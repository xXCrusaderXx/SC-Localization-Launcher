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
};

struct AutoSearch
{
    static constexpr const char* name = "autoSearch";
};

// Asks GitHub whether a newer launcher version exists; nothing is installed
struct CheckForUpdate
{
    static constexpr const char* name = "checkForUpdate";
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetLanguage, channel, language)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetInstallPath, channel, path)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetRsiLauncherPath, path)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetOption, option, value)
inline void to_json (nlohmann::json& j, const StartUpdate&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const AutoSearch&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const CheckForUpdate&) { j = nlohmann::json::object(); }

using Command = std::variant<SetLanguage, SetInstallPath, SetRsiLauncherPath, SetOption, StartUpdate, AutoSearch, CheckForUpdate>;

// For the log, e.g. setLanguage {"channel":"LIVE","language":"de"}
inline std::string toString (const Command& command)
{
    return std::visit([] (const auto& c) { return std::string(c.name) + " " + nlohmann::json(c).dump(); }, command);
}

}  // namespace Protocol
