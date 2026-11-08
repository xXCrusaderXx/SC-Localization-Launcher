#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Reads the comment lines (";") at the top of a translation's global.ini. Some projects name their contributors there,
// e.g. "; Contributeurs : Alice - Bob - Carol".
namespace TranslationHeader
{
// Limits for a file that is not what it claims to be
constexpr size_t MAX_HEADER_LINES = 50;
constexpr size_t MAX_LINE_LENGTH = 64 * 1024;
constexpr size_t MAX_CONTRIBUTORS = 2000;
constexpr size_t MAX_NAME_LENGTH = 100;

namespace detail
{
inline std::string trim (const std::string& text)
{
    auto begin = std::find_if_not(text.begin(), text.end(), [] (unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(text.rbegin(), text.rend(), [] (unsigned char c) { return std::isspace(c); }).base();
    return begin < end ? std::string(begin, end) : std::string();
}

inline std::string lower (std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

inline bool isContributorLabel (const std::string& label)
{
    const std::string name = lower(trim(label));
    return name == "contributors" || name == "contributeurs" || name == "mitwirkende";
}
}  // namespace detail

// Names from a "Contributors:" line in the given header text; empty if there is none
inline std::vector<std::string> contributorsIn (std::istream& stream)
{
    std::vector<std::string> names;
    std::string line;
    for(size_t number = 0; number < MAX_HEADER_LINES && std::getline(stream, line); ++number)
    {
        if(number == 0 && line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(!line.starts_with(";")) break;
        if(line.size() > MAX_LINE_LENGTH) continue;

        auto colon = line.find(':');
        if(colon == std::string::npos || !detail::isContributorLabel(line.substr(1, colon - 1))) continue;
        std::string list = line.substr(colon + 1);
        size_t start = 0;
        while(start <= list.size() && names.size() < MAX_CONTRIBUTORS)
        {
            size_t separator = list.find(" - ", start);
            std::string name = detail::trim(list.substr(start, separator == std::string::npos ? std::string::npos : separator - start));
            if(!name.empty() && name.size() <= MAX_NAME_LENGTH) names.push_back(name);
            if(separator == std::string::npos) break;
            start = separator + 3;
        }
        break;
    }
    return names;
}

inline std::vector<std::string> contributors (const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    if(!stream.is_open()) return {};
    return contributorsIn(stream);
}
}  // namespace TranslationHeader
