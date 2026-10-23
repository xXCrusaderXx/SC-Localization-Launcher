#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// The complete backend state. The backend sends a fresh snapshot after every change,
// the frontend only displays it and never has to merge anything.
namespace Protocol
{

// Channels in display order
inline const std::vector<std::string> CHANNEL_NAMES{"LIVE", "PTU", "EPTU", "HOTFIX", "TECH-PREVIEW"};

// The original game text: no translation installed. Every other language code comes from languages.json.
inline const std::string ENGLISH = "en";

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
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Language, code, name, credit)

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

struct Options
{
    bool launchGameAfterUpdate = false;
    bool updateAtStart = false;
    bool installNewTranslations = false;
    bool minimizeAfterUpdate = false;
    bool startWithSystem = false;
    bool showUpdateProgress = false;
    // Asks GitHub for a newer launcher version at start
    bool checkLauncherUpdatesAtStart = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Options, launchGameAfterUpdate, updateAtStart, installNewTranslations, minimizeAfterUpdate,
                                                startWithSystem, showUpdateProgress, checkLauncherUpdatesAtStart)

enum class ChannelStatus
{
    NoPath,
    // The chosen translation is not in the game folder yet
    NotInstalled,
    // The chosen translation is installed and no newer version is known
    Installed,
    // A newer version exists, or the installed language differs from the chosen one
    UpdateAvailable
};

NLOHMANN_JSON_SERIALIZE_ENUM(ChannelStatus, {{ChannelStatus::NoPath, "noPath"},
                                             {ChannelStatus::NotInstalled, "notInstalled"},
                                             {ChannelStatus::Installed, "installed"},
                                             {ChannelStatus::UpdateAvailable, "updateAvailable"}})

struct Channel
{
    std::string name;
    std::string installPath;
    std::string language = ENGLISH;
    // Codes the translation sources currently offer for this channel, always including ENGLISH,
    // independent of the install path
    std::vector<std::string> availableLanguages;
    ChannelStatus status = ChannelStatus::NoPath;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Channel, name, installPath, language, availableLanguages, status)

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

struct State
{
    // All known languages in display order, ENGLISH first
    std::vector<Language> languages;
    std::vector<Channel> channels;
    std::string rsiLauncherPath;
    Options options;
    UpdateStatus update = UpdateStatus::Idle;
    bool searching = false;
    SelfUpdate selfUpdate;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(State, languages, channels, rsiLauncherPath, options, update, searching, selfUpdate)

}  // namespace Protocol
