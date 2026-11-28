#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// The player's own texts for single entries of global.ini, per channel, e.g. a shorter name or a personal fix.
// They go into the installed file after the item values, so they win, and are written again after every update of
// the translation. A key the file does not have is added at its end. They never leave the player's computer.
namespace TextOverrides
{

// Key and text in the order the player wrote them
using Overrides = std::vector<std::pair<std::string, std::string>>;

// Per channel, more is surely no list of single fixes
inline constexpr size_t MAX_SIZE = 64 * 1024;

inline std::string trimmed (const std::string& text)
{
    auto first = text.find_first_not_of(" \t\r");
    if(first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

// Keys of global.ini are compared without case, like the game does
inline std::string lower (std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// "key=value" lines; blank lines and comments (";" or "#") are skipped, spaces around key and value removed.
// nullopt with the offending line in `error` if a line is no "key=value" or repeats a key, "too long" above MAX_SIZE.
inline std::optional<Overrides> parse (const std::string& text, std::string& error)
{
    if(text.size() > MAX_SIZE)
    {
        error = "too long";
        return std::nullopt;
    }
    Overrides overrides;
    std::vector<std::string> keys;
    std::istringstream in(text);
    std::string line;
    while(std::getline(in, line))
    {
        auto content = trimmed(line);
        if(content.empty() || content.front() == ';' || content.front() == '#') continue;
        auto equals = content.find('=');
        std::string key = equals == std::string::npos ? "" : trimmed(content.substr(0, equals));
        bool spaced = key.find_first_of(" \t") != std::string::npos;
        if(key.empty() || spaced || std::find(keys.begin(), keys.end(), lower(key)) != keys.end())
        {
            error = content;
            return std::nullopt;
        }
        keys.push_back(lower(key));
        overrides.emplace_back(key, trimmed(content.substr(equals + 1)));
    }
    return overrides;
}

// Changes whenever the result would change; empty for none. Stored with the installed file, which is downloaded
// again when it differs, so a removed text gets the translation's own text back.
inline std::string signature (const Overrides& overrides)
{
    if(overrides.empty()) return "";
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for(const auto& [key, value] : overrides) hash.addData(QByteArray::fromStdString(lower(key) + "=" + value + "\n"));
    return hash.result().toHex().left(16).toStdString();
}

// The signature of the texts as stored in the settings; text that no longer parses counts as none
inline std::string signatureOf (const std::string& text)
{
    std::string error;
    auto overrides = parse(text, error);
    return overrides ? signature(*overrides) : "";
}

// Writes the texts into a freshly downloaded global.ini, keeping its byte order mark and line endings; written next
// to it and moved over it. Returns the keys added at the end because the file does not have them.
inline std::optional<std::vector<std::string>> apply (const std::filesystem::path& file, const Overrides& overrides)
{
    std::map<std::string, size_t> index;
    for(size_t i = 0; i < overrides.size(); ++i) index[lower(overrides[i].first)] = i;
    std::vector<bool> used(overrides.size(), false);

    std::ifstream in(file, std::ios::binary);
    if(!in) return std::nullopt;
    std::string out;
    std::string line;
    // The ending of the first line, for the lines added at the end
    std::optional<bool> crlfFile;
    bool endsWithNewline = true;
    while(std::getline(in, line))
    {
        bool crlf = !line.empty() && line.back() == '\r';
        if(crlf) line.pop_back();
        if(!crlfFile) crlfFile = crlf;
        auto equals = line.find('=');
        if(equals != std::string::npos)
        {
            // global.ini starts with a byte order mark, which belongs to the first line, not to its key
            std::string key = line.substr(0, equals);
            if(key.starts_with("\xEF\xBB\xBF")) key.erase(0, 3);
            if(auto found = index.find(lower(key)); found != index.end())
            {
                line = line.substr(0, equals + 1) + overrides[found->second].second;
                used[found->second] = true;
            }
        }
        out += line;
        // The last line keeps its ending, also when it has none
        endsWithNewline = !in.eof();
        if(!in.eof())
            out += crlf ? "\r\n" : "\n";
        else if(crlf)
            out += "\r";
    }
    in.close();

    std::string newline = crlfFile.value_or(false) ? "\r\n" : "\n";
    std::vector<std::string> appended;
    for(size_t i = 0; i < overrides.size(); ++i)
    {
        if(used[i]) continue;
        if(!endsWithNewline && !out.empty()) out += out.back() == '\r' ? "\n" : newline;
        endsWithNewline = true;
        out += overrides[i].first + "=" + overrides[i].second + newline;
        appended.push_back(overrides[i].first);
    }

    auto temporary = file;
    temporary += ".overrides";
    {
        std::ofstream write(temporary, std::ios::binary | std::ios::trunc);
        write << out;
        if(!write) return std::nullopt;
    }
    std::error_code ec;
    std::filesystem::rename(temporary, file, ec);
    if(ec)
    {
        std::filesystem::remove(temporary, ec);
        return std::nullopt;
    }
    return appended;
}

// The player's texts replaced in settings or state as JSON, for the log and the problem report
inline void hideIn (nlohmann::json& channels, const char* field)
{
    for(auto& channel : channels)
        if(channel.is_object() && channel.contains(field) && channel[field].is_string() && !channel[field].get<std::string>().empty())
            channel[field] = "(hidden)";
}

}  // namespace TextOverrides
