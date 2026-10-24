#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

// Edits single settings in a game channel's user.cfg ("key = value" per line). Lines whose key is not touched stay
// exactly as they were, including their line endings, comments and spacing; a set key keeps its place in the file.
class UserCfg
{
    struct Line
    {
        std::string text;
        // "\n", "\r\n" or "" for a last line without one
        std::string end;
    };

    std::vector<Line> lines;

    static std::string lower (std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    // The ending new lines get: whatever the file uses
    std::string newLineEnd () const
    {
        for(const auto& line : lines)
            if(!line.end.empty()) return line.end;
        return "\n";
    }

   public:
    // The key of a line, lowercase; empty for blank lines and comments
    static std::string keyOf (const std::string& line)
    {
        auto start = line.find_first_not_of(" \t");
        if(start == std::string::npos || line.compare(start, 2, "--") == 0 || line[start] == ';' || line[start] == '#') return "";
        return lower(line.substr(start, line.find_first_of(" \t=", start) - start));
    }

    explicit UserCfg (const std::string& content = "")
    {
        size_t pos = 0;
        while(pos < content.size())
        {
            size_t newline = content.find('\n', pos);
            if(newline == std::string::npos)
            {
                lines.push_back({content.substr(pos), ""});
                break;
            }
            bool crlf = newline > pos && content[newline - 1] == '\r';
            lines.push_back({content.substr(pos, newline - pos - (crlf ? 1 : 0)), crlf ? "\r\n" : "\n"});
            pos = newline + 1;
        }
    }

    static UserCfg load (const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return UserCfg(std::string((std::istreambuf_iterator<char>(in)), {}));
    }

    std::string str () const
    {
        std::string text;
        for(const auto& line : lines) text += line.text + line.end;
        return text;
    }

    std::optional<std::string> value (const std::string& key) const
    {
        for(const auto& line : lines)
            if(keyOf(line.text) == lower(key))
            {
                auto equals = line.text.find('=');
                if(equals == std::string::npos) return "";
                std::string value = line.text.substr(equals + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                return value;
            }
        return std::nullopt;
    }

    // Replaces the first line of this key in place and drops further ones; appends it if the key is new
    void set (const std::string& key, const std::string& value)
    {
        std::string text = key + " = " + value;
        bool done = false;
        for(auto it = lines.begin(); it != lines.end();)
        {
            if(keyOf(it->text) != lower(key))
            {
                ++it;
                continue;
            }
            if(done)
            {
                it = lines.erase(it);
                continue;
            }
            if(it->text != text) it->text = text;
            done = true;
            ++it;
        }
        if(done) return;
        std::string end = newLineEnd();
        if(!lines.empty() && lines.back().end.empty()) lines.back().end = end;
        lines.push_back({text, end});
    }

    void remove (const std::string& key)
    {
        std::erase_if(lines, [&] (const Line& line) { return keyOf(line.text) == lower(key); });
    }

    bool onlyBlank () const
    {
        return std::all_of(lines.begin(), lines.end(), [] (const Line& l) { return l.text.find_first_not_of(" \t\r") == std::string::npos; });
    }

    // Writes only when the content changed; a file left with nothing but blank lines is removed
    bool save (const std::filesystem::path& path) const
    {
        std::error_code ec;
        if(onlyBlank())
        {
            std::filesystem::remove(path, ec);
            return !ec;
        }
        std::string content = str();
        if(load(path).str() == content) return true;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << content;
        return static_cast<bool>(out);
    }
};
