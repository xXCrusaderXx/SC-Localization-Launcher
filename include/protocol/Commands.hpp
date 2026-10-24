#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <variant>

#include "protocol/State.hpp"

// Commands from the frontend to the backend. They only describe what the user wants,
// the answer is always the next State snapshot.
namespace Protocol
{

struct SetLanguage
{
    static constexpr const char* name = "setLanguage";
    std::string channel;
    // A code from State::languages
    std::string language;
};

struct SetInstallPath
{
    static constexpr const char* name = "setInstallPath";
    std::string channel;
    std::string path;
};

struct SetRsiLauncherPath
{
    static constexpr const char* name = "setRsiLauncherPath";
    std::string path;
};

// option is a field name of Options, checked by the backend
struct SetOption
{
    static constexpr const char* name = "setOption";
    std::string option;
    bool value = false;
};

struct StartUpdate
{
    static constexpr const char* name = "startUpdate";
    // Only these channels; empty = all
    std::vector<std::string> channels;
    // False for the command line: a script never starts the game, whatever the option says
    bool allowGameStart = true;
};

// Starts the RSI Launcher without updating anything
struct StartGame
{
    static constexpr const char* name = "startGame";
};

struct AutoSearch
{
    static constexpr const char* name = "autoSearch";
};

// Downloads the chosen translation of one channel again and rewrites its user.cfg line,
// e.g. after a game update overwrote global.ini
struct RepairChannel
{
    static constexpr const char* name = "repairChannel";
    std::string channel;
};

// Sets the user's own global.ini source for one channel; an empty url removes it
struct SetCustomSource
{
    static constexpr const char* name = "setCustomSource";
    std::string channel;
    std::string url;
    std::string folder;
};

// Asks GitHub whether a newer launcher version exists; nothing is installed
struct CheckForUpdate
{
    static constexpr const char* name = "checkForUpdate";
};

// Downloads, verifies and installs the launcher update found by checkForUpdate
struct InstallUpdate
{
    static constexpr const char* name = "installUpdate";
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetLanguage, channel, language)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetInstallPath, channel, path)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetRsiLauncherPath, path)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetOption, option, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StartUpdate, channels, allowGameStart)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RepairChannel, channel)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetCustomSource, channel, url, folder)
inline void to_json (nlohmann::json& j, const StartGame&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const AutoSearch&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const CheckForUpdate&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const InstallUpdate&) { j = nlohmann::json::object(); }

using Command = std::variant<SetLanguage, SetInstallPath, SetRsiLauncherPath, SetOption, StartUpdate, StartGame, AutoSearch, CheckForUpdate,
                             InstallUpdate, RepairChannel, SetCustomSource>;

// For the log, e.g. setLanguage {"channel":"LIVE","language":"de"}
inline std::string toString (const Command& command)
{
    return std::visit([] (const auto& c) { return std::string(c.name) + " " + nlohmann::json(c).dump(); }, command);
}

}  // namespace Protocol
