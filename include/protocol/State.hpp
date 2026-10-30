#pragma once
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// The complete backend state. The backend sends a fresh snapshot after every change,
// the frontend only displays it and never has to merge anything.
namespace Protocol
{

// Channels in display order
inline const std::vector<std::string> CHANNEL_NAMES{"LIVE", "PTU", "EPTU", "HOTFIX", "TECH-PREVIEW"};

// The original game text: no translation installed. Every other language code comes from languages.json,
// except CUSTOM: the source the user entered for one channel.
inline const std::string ENGLISH = "en";
inline const std::string CUSTOM = "custom";

// A global.ini the user chose for one channel instead of a catalogue language; empty url = none
struct CustomSource
{
    std::string url;
    // Star Citizen localization folder the file goes into, e.g. "german_(germany)"
    std::string folder;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CustomSource, url, folder)

// Who made a translation and under which license; shown wherever the translation is offered
struct Credit
{
    std::string project;
    std::string authors;
    std::string url;
    std::string license;
    std::string licenseUrl;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Credit, project, authors, url, license, licenseUrl)

struct Language
{
    std::string code;
    // In the language itself, e.g. "Deutsch"
    std::string name;
    // Empty for ENGLISH, the original game text
    Credit credit;
    // Star Citizen localization folder, empty for ENGLISH
    std::string folder;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Language, code, name, credit, folder)

enum class UpdateStatus
{
    Idle,
    Running,
    Succeeded,
    Failed
};

NLOHMANN_JSON_SERIALIZE_ENUM(UpdateStatus, {{UpdateStatus::Idle, "idle"},
                                            {UpdateStatus::Running, "running"},
                                            {UpdateStatus::Succeeded, "succeeded"},
                                            {UpdateStatus::Failed, "failed"}})

// The RSI Launcher: started by this launcher, or seen running
enum class GameStart
{
    Idle,
    Starting,
    // The RSI Launcher is open, also when started without this launcher
    Running,
    // StarCitizen.exe runs; translations are not updated meanwhile
    Playing,
    // Started, but the RSI Launcher did not appear or quit right away
    Failed,
    // Neither the LUG Helper's sc-launch.sh nor Lutris was found
    NoStarter
};

NLOHMANN_JSON_SERIALIZE_ENUM(GameStart, {{GameStart::Idle, "idle"},
                                         {GameStart::Starting, "starting"},
                                         {GameStart::Running, "running"},
                                         {GameStart::Playing, "playing"},
                                         {GameStart::Failed, "failed"},
                                         {GameStart::NoStarter, "noStarter"}})

struct Options
{
    bool launchGameAfterUpdate = false;
    bool updateAtStart = false;
    // Installs new translation versions found by the background check, see StartBackgroundChecks
    bool installNewTranslations = false;
    bool minimizeAfterUpdate = false;
    bool startWithSystem = false;
    bool showUpdateProgress = false;
    // Asks GitHub for a newer launcher version at start
    bool checkLauncherUpdatesAtStart = false;
    // Closing the window keeps the launcher in the system tray, so the background checks go on; only with a tray
    bool closeToTray = false;
    // Adds size, class and grade from the Star Citizen Wiki API to item names in global.ini
    bool itemNameDetails = false;
    // Puts a line of stats from the Star Citizen Wiki API in front of item descriptions in global.ini
    bool itemDescriptionStats = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Options, launchGameAfterUpdate, updateAtStart, installNewTranslations, minimizeAfterUpdate,
                                                startWithSystem, showUpdateProgress, checkLauncherUpdatesAtStart, closeToTray, itemNameDetails,
                                                itemDescriptionStats)

enum class ChannelStatus
{
    NoPath,
    // The chosen translation is not in the game folder yet
    NotInstalled,
    // The chosen translation is installed and no newer version is known
    Installed,
    // A newer version exists, or the installed language differs from the chosen one
    UpdateAvailable,
    // The installed global.ini or the language line in user.cfg was changed, e.g. by a game update
    Damaged,
    // The last update of this channel failed
    Failed
};

NLOHMANN_JSON_SERIALIZE_ENUM(ChannelStatus, {{ChannelStatus::NoPath, "noPath"},
                                             {ChannelStatus::NotInstalled, "notInstalled"},
                                             {ChannelStatus::Installed, "installed"},
                                             {ChannelStatus::UpdateAvailable, "updateAvailable"},
                                             {ChannelStatus::Damaged, "damaged"},
                                             {ChannelStatus::Failed, "failed"}})

// Settings the launcher writes into a channel's user.cfg, see GameSettings.hpp
struct GameSettings
{
    // sys_maxFps; 0 = not set by the launcher
    int maxFps = 0;
    // sys_maxIdleFps, for menus and loading screens; 0 = not set by the launcher
    int maxIdleFps = 0;
    // Own "key = value" lines, one per line
    std::string customLines;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GameSettings, maxFps, maxIdleFps, customLines)

// A program the user starts together with the game, see CompanionPrograms
struct Program
{
    // Program and arguments, split like a shell does (quotes, backslash) but without variables or pipes
    std::string command;
    bool enabled = true;
    // Ends the program when Star Citizen quits
    bool stopWithGame = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Program, command, enabled, stopWithGame)

struct Channel
{
    std::string name;
    std::string installPath;
    std::string language = ENGLISH;
    // Codes the translation sources currently offer for this channel, always including ENGLISH,
    // independent of the install path
    std::vector<std::string> availableLanguages;
    ChannelStatus status = ChannelStatus::NoPath;
    // Language code -> short notice of the translation project for this channel; plain text
    std::map<std::string, std::string> notices;
    // The user's own source for this channel, offered as language CUSTOM when set
    CustomSource custom;
    GameSettings gameSettings;
    // A Game.log newer than the last change of gameSettings was read
    bool settingsChecked = false;
    // Keys of gameSettings that Game.log reports as "Not Whitelisted": the game ignores them
    std::vector<std::string> ignoredSettings;
    // Why the last SetGameSettings was refused, e.g. a custom line that is no "key = value"
    std::string settingsError;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Channel, name, installPath, language, availableLanguages, status, notices, custom, gameSettings,
                                                settingsChecked, ignoredSettings, settingsError)

// Updates of the launcher itself, from the GitHub releases of the project
enum class SelfUpdateStatus
{
    Idle,
    Checking,
    UpToDate,
    Available,
    Downloading,
    Installing,
    // Installed; the new version runs after a restart of the launcher
    Installed,
    Error
};

NLOHMANN_JSON_SERIALIZE_ENUM(SelfUpdateStatus, {{SelfUpdateStatus::Idle, "idle"},
                                                {SelfUpdateStatus::Checking, "checking"},
                                                {SelfUpdateStatus::UpToDate, "upToDate"},
                                                {SelfUpdateStatus::Available, "available"},
                                                {SelfUpdateStatus::Downloading, "downloading"},
                                                {SelfUpdateStatus::Installing, "installing"},
                                                {SelfUpdateStatus::Installed, "installed"},
                                                {SelfUpdateStatus::Error, "error"}})

struct SelfUpdate
{
    SelfUpdateStatus status = SelfUpdateStatus::Idle;
    std::string currentVersion;
    std::string latestVersion;
    // Release notes in Markdown
    std::string notes;
    // Release page with the downloads
    std::string url;
    std::string error;
    // How this launcher was installed: appimage, deb, rpm, arch, flatpak or source
    std::string installType;
    // A signed package for this installation is in the release, so the launcher can update itself
    bool canInstall = false;
    // Download progress, 0..1
    double progress = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SelfUpdate, status, currentVersion, latestVersion, notes, url, error, installType, canInstall,
                                                progress)

// Release notes shown once after the launcher was updated, by whatever means
struct WhatsNew
{
    // The running version; empty while there is nothing to show
    std::string version;
    // Markdown, one "## <version>" section per release since the last version seen, newest first
    std::string notes;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WhatsNew, version, notes)

enum class CacheResult
{
    None,
    Deleted,
    // Gone already, e.g. deleted by hand since the scan
    NotFound,
    // Not deleted, e.g. no permission
    Failed,
    // Not deleted because Star Citizen runs
    GameRunning
};

NLOHMANN_JSON_SERIALIZE_ENUM(CacheResult, {{CacheResult::None, "none"},
                                           {CacheResult::Deleted, "deleted"},
                                           {CacheResult::NotFound, "notFound"},
                                           {CacheResult::Failed, "failed"},
                                           {CacheResult::GameRunning, "gameRunning"}})

// A shader cache found by ScanCaches, see CacheCleaner
struct CacheEntry
{
    // game, dxvk, prefix (Mesa/NVIDIA cache in the Wine prefix) or user (Mesa/NVIDIA cache shared by all programs)
    std::string kind;
    std::string path;
    std::uint64_t bytes = 0;
    // Of the last ClearCaches
    CacheResult result = CacheResult::None;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CacheEntry, kind, path, bytes, result)

struct Caches
{
    // Scanning or deleting
    bool busy = false;
    // A scan finished, so entries is the current list
    bool scanned = false;
    std::vector<CacheEntry> entries;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Caches, busy, scanned, entries)

enum class CheckStatus
{
    Info,
    Ok,
    Warning,
    Error
};

NLOHMANN_JSON_SERIALIZE_ENUM(CheckStatus,
                             {{CheckStatus::Info, "info"}, {CheckStatus::Ok, "ok"}, {CheckStatus::Warning, "warning"}, {CheckStatus::Error, "error"}})

// One result of RunDiagnostics, see Diagnostics. The UI words it by id; detail holds paths (home shown as ~) or values.
struct DiagnosticCheck
{
    // system, launcher, catalogue, rsiLauncher, winePrefix, starter; per channel: folder, writable, translation,
    // userCfg, diskSpace
    std::string id;
    // Empty unless the check belongs to a game channel
    std::string channel;
    CheckStatus status = CheckStatus::Info;
    std::string detail;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DiagnosticCheck, id, channel, status, detail)

struct Diagnostics
{
    bool busy = false;
    std::vector<DiagnosticCheck> checks;
    // Written by SaveReport; empty until then
    std::string reportPath;
    // Why the report could not be written
    std::string error;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Diagnostics, busy, checks, reportPath, error)

// A profile backup of a game channel, see ProfileBackup
struct BackupEntry
{
    std::string channel;
    // File name, e.g. LIVE-20261024-203000.zip; also tells the time
    std::string name;
    std::uint64_t bytes = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BackupEntry, channel, name, bytes)

struct Backups
{
    bool busy = false;
    // Newest first, all channels
    std::vector<BackupEntry> entries;
    // Of the last CreateBackup or RestoreBackup: "created" or "restored", or an error such as "Star Citizen is running"
    std::string result;
    std::string resultChannel;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Backups, busy, entries, result, resultChannel)

struct State
{
    // All known languages in display order, ENGLISH first
    std::vector<Language> languages;
    std::vector<Channel> channels;
    std::string rsiLauncherPath;
    Options options;
    UpdateStatus update = UpdateStatus::Idle;
    GameStart game = GameStart::Idle;
    bool searching = false;
    // The sources are asked at start whether the translations changed; channel statuses are final once this is false
    bool checking = false;
    SelfUpdate selfUpdate;
    // Commands handled so far; a client that counts what it sent knows when its last command was answered
    std::uint64_t handledCommands = 0;
    WhatsNew whatsNew;
    Caches caches;
    Diagnostics diagnostics;
    Backups backups;
    // Started when StarCitizen.exe starts, also when the game was started without this launcher
    std::vector<Program> programs;
    // Commands that could not be started at the last start of the game
    std::vector<std::string> failedPrograms;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(State, languages, channels, rsiLauncherPath, options, update, game, searching, checking, selfUpdate,
                                                handledCommands, whatsNew, caches, diagnostics, backups, programs, failedPrograms)

}  // namespace Protocol
