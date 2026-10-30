#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
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
    if(!settings.graphicsApi.empty() && settings.graphicsApi != "vulkan" && settings.graphicsApi != "dx11")
    {
        error = "unknown graphics API";
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

// The game version a channel ran last, e.g. "sc-alpha-4.8.0", from the "Branch:" line at the top of Game.log
inline std::string branchOf (const std::filesystem::path& gameLog)
{
    static const std::regex branch(R"(\bBranch: (\S+))");
    std::ifstream in(gameLog);
    std::string line;
    for(int i = 0; i < 200 && std::getline(in, line); ++i)
    {
        std::smatch match;
        if(std::regex_search(line, match, branch)) return match[1].str();
    }
    return "";
}

// Sets GraphicsRenderer (0 = DirectX 11, 1 = Vulkan) in GraphicsSettings.json of every folder the game made for
// `branch` in the Wine prefix, <user>/AppData/Local/star citizen/starcitizen_(<branch>)_<id>/; the other values stay.
// The game makes such a folder with its defaults at the first start of a version. False if there is none yet, or if a
// file could not be read or written.
inline bool applyGraphicsApi (const std::filesystem::path& prefix, const std::string& branch, const std::string& api)
{
    if(branch.empty()) return false;
    int renderer = api == "dx11" ? 0 : 1;
    bool found = false;
    bool ok = true;
    std::error_code userError, localError, versionError;
    for(std::filesystem::directory_iterator user(prefix / "drive_c/users", userError), end; !userError && user != end; user.increment(userError))
        for(std::filesystem::directory_iterator local(user->path() / "AppData/Local", localError), end; !localError && local != end;
            local.increment(localError))
        {
            if(lower(local->path().filename().string()) != "star citizen") continue;
            for(std::filesystem::directory_iterator version(local->path(), versionError), end; !versionError && version != end;
                version.increment(versionError))
            {
                if(!version->is_directory() || version->path().filename().string().find("(" + branch + ")") == std::string::npos) continue;
                found = true;
                auto file = version->path() / "GraphicsSettings/GraphicsSettings.json";
                nlohmann::ordered_json json = {{"GraphicsSettings", {{"SettingsVersion", 1}}}};
                if(std::filesystem::exists(file))
                {
                    std::ifstream in(file);
                    json = nlohmann::ordered_json::parse(in, nullptr, false);
                    // An unknown layout is left alone
                    if(!json.is_object() || !json.contains("GraphicsSettings") || !json["GraphicsSettings"].is_object())
                    {
                        ok = false;
                        continue;
                    }
                }
                auto& settings = json["GraphicsSettings"];
                if(settings.contains("GraphicsRenderer") && settings["GraphicsRenderer"] == renderer) continue;
                settings["GraphicsRenderer"] = renderer;
                std::error_code mkdir;
                std::filesystem::create_directories(file.parent_path(), mkdir);
                auto temporary = file;
                temporary += ".tmp";
                {
                    std::ofstream out(temporary, std::ios::trunc);
                    out << json.dump(2);
                    if(!out)
                    {
                        ok = false;
                        continue;
                    }
                }
                std::error_code rename;
                std::filesystem::rename(temporary, file, rename);
                ok &= !rename;
            }
        }
    return found && ok;
}

}  // namespace GameSettings
