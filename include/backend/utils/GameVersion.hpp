#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <istream>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// Game versions such as "4.10.2": the one a channel's game files have, and the one a translation states it was made for
namespace GameVersion
{
// Limits for a file that is not what it claims to be
constexpr size_t MAX_LINE_LENGTH = 64 * 1024;
constexpr size_t MAX_HEADER_LINES = 50;
constexpr std::uintmax_t MAX_MANIFEST_SIZE = 1024 * 1024;

// The first "major.minor" or "major.minor.patch" in the text, e.g. "4.10.2" in "Star Citizen Alpha 4.10.2 LIVE"; empty if there is none
inline std::string in (const std::string& text)
{
    static const std::regex version(R"((\d{1,3})\.(\d{1,3})(?:\.(\d{1,3}))?)");
    std::smatch match;
    if(!std::regex_search(text, match, version)) return {};
    return match[3].matched ? match[1].str() + "." + match[2].str() + "." + match[3].str() : match[1].str() + "." + match[2].str();
}

inline std::vector<int> parts (const std::string& version)
{
    std::vector<int> result;
    size_t start = 0;
    while(start < version.size())
    {
        size_t dot = version.find('.', start);
        result.push_back(std::stoi(version.substr(start, dot == std::string::npos ? std::string::npos : dot - start)));
        if(dot == std::string::npos) break;
        start = dot + 1;
    }
    return result;
}

// The translation is made for an earlier game version. A translation that names only "4.10" fits every 4.10.x.
inline bool older (const std::string& translation, const std::string& game)
{
    if(translation.empty() || game.empty()) return false;
    auto made = parts(translation);
    auto current = parts(game);
    current.resize(std::max(current.size(), made.size()), 0);
    return std::lexicographical_compare(made.begin(), made.end(), current.begin(), current.begin() + made.size());
}

// From the channel's build_manifest.id, whose "Branch" is e.g. "sc-alpha-4.10.2". Its "Version" counts builds, not patches.
inline std::string ofGame (const std::filesystem::path& installPath)
{
    auto file = installPath / "build_manifest.id";
    std::error_code ec;
    auto size = std::filesystem::file_size(file, ec);
    if(ec || size > MAX_MANIFEST_SIZE) return {};
    std::ifstream stream(file, std::ios::binary);
    auto manifest = nlohmann::json::parse(stream, nullptr, false);
    if(manifest.is_discarded() || !manifest.is_object()) return {};
    auto data = manifest.find("Data");
    if(data == manifest.end() || !data->is_object()) return {};
    auto branch = data->find("Branch");
    if(branch == data->end() || !branch->is_string()) return {};
    return in(branch->get<std::string>());
}

// From a "; Version : 4.10.2-…" line in the header, otherwise from the text the game shows as its version (Frontend_PU_Version).
// Projects write their game version into one of the two; empty if neither names one.
inline std::string ofTranslationIn (std::istream& stream)
{
    std::string line;
    bool header = true;
    for(size_t number = 0; std::getline(stream, line); ++number)
    {
        if(number == 0 && line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
        if(line.size() > MAX_LINE_LENGTH) continue;
        if(!line.empty() && line.back() == '\r') line.pop_back();

        if(header && number < MAX_HEADER_LINES && line.starts_with(";"))
        {
            auto colon = line.find(':');
            if(colon == std::string::npos) continue;
            std::string label = line.substr(1, colon - 1);
            label.erase(std::remove_if(label.begin(), label.end(), [] (unsigned char c) { return std::isspace(c); }), label.end());
            std::transform(label.begin(), label.end(), label.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if(label == "version")
                if(auto version = in(line.substr(colon + 1)); !version.empty()) return version;
            continue;
        }
        header = false;
        // The key may carry a suffix such as ",P"
        if(line.starts_with("Frontend_PU_Version=") || line.starts_with("Frontend_PU_Version,"))
            if(auto equals = line.find('='); equals != std::string::npos) return in(line.substr(equals + 1));
    }
    return {};
}

inline std::string ofTranslation (const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    if(!stream.is_open()) return {};
    return ofTranslationIn(stream);
}
}  // namespace GameVersion
