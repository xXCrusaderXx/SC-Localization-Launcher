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

// Sent by the window, never by the command line: checks the sources every hour and, with the option
// installNewTranslations, installs what changed (after Star Citizen has been closed, if it runs)
struct StartBackgroundChecks
{
    static constexpr const char* name = "startBackgroundChecks";
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

// Asks for the release notes if the launcher was updated since the last start. Only the GUI sends it, so a run on
// the command line neither downloads them nor marks them as seen.
struct LoadWhatsNew
{
    static constexpr const char* name = "loadWhatsNew";
};

// The user has read the release notes; they are not shown again for this version
struct DismissWhatsNew
{
    static constexpr const char* name = "dismissWhatsNew";
};

// Looks for shader caches of the configured game folders and their Wine prefixes, see State.caches
struct ScanCaches
{
    static constexpr const char* name = "scanCaches";
};

// Deletes these caches; only paths of the last scan are accepted. Refused while Star Citizen runs.
struct ClearCaches
{
    static constexpr const char* name = "clearCaches";
    std::vector<std::string> paths;
};

// Checks the installation without changing anything, see State.diagnostics
struct RunDiagnostics
{
    static constexpr const char* name = "runDiagnostics";
};

// Runs the checks again and saves them with settings and logs as a ZIP for a bug report, home paths anonymized
struct SaveReport
{
    static constexpr const char* name = "saveReport";
};

// Lists the profile backups, see State.backups
struct LoadBackups
{
    static constexpr const char* name = "loadBackups";
};

// Backs up keybindings, profile settings, characters and user.cfg of a channel
struct CreateBackup
{
    static constexpr const char* name = "createBackup";
    std::string channel;
};

// Writes a backup back into its channel after backing up the current files. Refused while Star Citizen runs.
struct RestoreBackup
{
    static constexpr const char* name = "restoreBackup";
    std::string channel;
    // BackupEntry.name
    std::string backup;
};

// Writes frame limits and own lines into the channel's user.cfg; the game reads them at its next start
struct SetGameSettings
{
    static constexpr const char* name = "setGameSettings";
    std::string channel;
    GameSettings settings;
};

// Replaces the programs started with the game; entries without a command are dropped
struct SetPrograms
{
    static constexpr const char* name = "setPrograms";
    std::vector<Program> programs;
};

// The launcher's own interface language, e.g. "fr"; empty = the system language. The window applies it.
struct SetInterfaceLanguage
{
    static constexpr const char* name = "setInterfaceLanguage";
    std::string language;
};

// "dark", "light", or empty to follow the system
struct SetTheme
{
    static constexpr const char* name = "setTheme";
    std::string theme;
};

// Reads the Game.log files of every channel with a folder, see State.statistics
struct LoadStatistics
{
    static constexpr const char* name = "loadStatistics";
};

// Saves a page the window built (HTML, in the interface language) into the Downloads folder
struct SaveStatisticsReport
{
    static constexpr const char* name = "saveStatisticsReport";
    std::string html;
    // Part of the file name: "statistics" or "system-info"
    std::string kind = "statistics";
};

// The dashboard's tiles in their order; unknown ids and repeats are dropped, at most MAX_DASHBOARD_TILES
struct SetDashboardTiles
{
    static constexpr const char* name = "setDashboardTiles";
    std::vector<std::string> tiles;
};

// The setup assistant was finished or skipped; it is not offered again
struct FinishSetup
{
    static constexpr const char* name = "finishSetup";
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetLanguage, channel, language)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetInstallPath, channel, path)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetRsiLauncherPath, path)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetOption, option, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StartUpdate, channels, allowGameStart)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RepairChannel, channel)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetCustomSource, channel, url, folder)
inline void to_json (nlohmann::json& j, const StartGame&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const FinishSetup&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const LoadStatistics&) { j = nlohmann::json::object(); }
// The page itself is not logged
inline void to_json (nlohmann::json& j, const SaveStatisticsReport& c) { j = {{"bytes", c.html.size()}, {"kind", c.kind}}; }
inline void to_json (nlohmann::json& j, const AutoSearch&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const StartBackgroundChecks&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const CheckForUpdate&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const InstallUpdate&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const LoadWhatsNew&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const DismissWhatsNew&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const ScanCaches&) { j = nlohmann::json::object(); }
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClearCaches, paths)
inline void to_json (nlohmann::json& j, const RunDiagnostics&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const SaveReport&) { j = nlohmann::json::object(); }
inline void to_json (nlohmann::json& j, const LoadBackups&) { j = nlohmann::json::object(); }
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CreateBackup, channel)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RestoreBackup, channel, backup)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetGameSettings, channel, settings)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetPrograms, programs)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetInterfaceLanguage, language)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetTheme, theme)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SetDashboardTiles, tiles)

using Command = std::variant<SetLanguage, SetInstallPath, SetRsiLauncherPath, SetOption, StartUpdate, StartGame, AutoSearch, CheckForUpdate,
                             InstallUpdate, RepairChannel, SetCustomSource, StartBackgroundChecks, LoadWhatsNew, DismissWhatsNew, ScanCaches,
                             ClearCaches, RunDiagnostics, SaveReport, LoadBackups, CreateBackup, RestoreBackup, SetGameSettings, SetPrograms,
                             SetInterfaceLanguage, FinishSetup, SetTheme, LoadStatistics, SaveStatisticsReport, SetDashboardTiles>;

// For the log, e.g. setLanguage {"channel":"LIVE","language":"de"}
inline std::string toString (const Command& command)
{
    return std::visit([] (const auto& c) { return std::string(c.name) + " " + nlohmann::json(c).dump(); }, command);
}

}  // namespace Protocol
