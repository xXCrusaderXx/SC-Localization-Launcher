#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "backend/utils/UserCfg.hpp"
#include "protocol/State.hpp"

// Turns Protocol::GameSettings into user.cfg lines and finds out from Game.log whether the game took them.
// Release builds only accept whitelisted cvars from user.cfg and log the others as "Not Whitelisted".
namespace GameSettings
{

using Line = std::pair<std::string, std::string>;

inline std::string lower (std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// The own lines as key/value pairs; nullopt and the offending line in `error` if one is no "key = value" or takes a
// key the launcher sets itself. Blank lines and comments ("--") are skipped.
inline std::optional<std::vector<Line>> parseCustom (const std::string& text, std::string& error)
{
    static const std::regex keyValue(R"(\s*([A-Za-z_][A-Za-z0-9_.]*)\s*=\s*([^\r\n]*?)\s*)");
    static const std::set<std::string> reserved{"g_language", "g_languageaudio", "sys_maxfps", "sys_maxidlefps"};
    std::vector<Line> lines;
    std::istringstream in(text);
    std::string line;
    while(std::getline(in, line))
    {
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(UserCfg::keyOf(line).empty()) continue;
        std::smatch match;
        if(!std::regex_match(line, match, keyValue) || reserved.contains(lower(match[1].str())))
        {
            error = line;
            return std::nullopt;
        }
        lines.emplace_back(match[1].str(), match[2].str());
    }
    return lines;
}

// Every line the settings put into user.cfg
inline std::optional<std::vector<Line>> linesOf (const Protocol::GameSettings& settings, std::string& error)
{
    if(settings.maxFps < 0 || settings.maxFps > 1000 || settings.maxIdleFps < 0 || settings.maxIdleFps > 1000)
    {
        error = "frame limit out of range";
        return std::nullopt;
    }
    auto custom = parseCustom(settings.customLines, error);
    if(!custom) return std::nullopt;
    std::vector<Line> lines;
    if(settings.maxFps > 0) lines.emplace_back("sys_maxFps", std::to_string(settings.maxFps));
    if(settings.maxIdleFps > 0) lines.emplace_back("sys_maxIdleFps", std::to_string(settings.maxIdleFps));
    lines.insert(lines.end(), custom->begin(), custom->end());
    return lines;
}

// Sets the lines in user.cfg and removes the keys of `previousKeys` that are no longer wanted; nothing else changes.
// Returns the keys now managed (lowercase), or nullopt if user.cfg could not be written.
inline std::optional<std::vector<std::string>> apply (const std::filesystem::path& userCfg, const std::vector<Line>& lines,
                                                      const std::vector<std::string>& previousKeys)
{
    auto cfg = UserCfg::load(userCfg);
    std::vector<std::string> keys;
    for(const auto& [key, value] : lines) keys.push_back(lower(key));
    for(const auto& key : previousKeys)
        if(std::find(keys.begin(), keys.end(), key) == keys.end()) cfg.remove(key);
    for(const auto& [key, value] : lines) cfg.set(key, value);
    if(!cfg.save(userCfg)) return std::nullopt;
    return keys;
}

// Keys (lowercase) that Game.log reports as not whitelisted, i.e. ignored from user.cfg
inline std::set<std::string> notWhitelisted (const std::filesystem::path& gameLog)
{
    static const std::regex marked(R"(LoadConfigVar\(\): variable \[([^\]]+)\] is marked \[Not Whitelisted\])");
    std::set<std::string> keys;
    std::ifstream in(gameLog);
    std::string line;
    while(std::getline(in, line))
        if(line.find("Not Whitelisted") != std::string::npos)
            for(std::sregex_iterator it(line.begin(), line.end(), marked), end; it != end; ++it) keys.insert(lower((*it)[1].str()));
    return keys;
}

}  // namespace GameSettings
