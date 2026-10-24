#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "protocol/State.hpp"

namespace Database
{

// Where a language's global.ini files come from, see languages.json. Exactly one of the two is set:
// channelList: a channels.json in the sc-deutsch-launcher.de format, `server` names the URL field per channel
// channels: fixed download URLs per channel, the first one that works wins
struct LanguageSource
{
    std::string channelList;
    std::string server;
    std::map<std::string, std::vector<std::string>> channels;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LanguageSource, channelList, server, channels)

struct LanguageDefinition
{
    std::string code;
    std::string name;
    // Star Citizen localization folder, also the value of g_language in user.cfg
    std::string folder;
    LanguageSource source;
    Protocol::Credit credit;
    // Short notice per channel, e.g. "translation in preparation"; "*" applies to every channel
    std::map<std::string, std::string> notices;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LanguageDefinition, code, name, folder, source, credit, notices)

struct LanguageCatalog
{
    int version = 1;
    std::vector<LanguageDefinition> languages;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LanguageCatalog, version, languages)

// A language with the download URLs it currently offers per channel
struct AvailableLanguage
{
    std::string code;
    std::string name;
    std::string folder;
    std::map<std::string, std::vector<std::string>> urls;
    Protocol::Credit credit;
    // Channel name or "*" -> notice, already checked and shortened
    std::map<std::string, std::string> notices;
};

// What the launcher last installed into a channel. Empty language: nothing, the game shows English.
struct InstalledTranslation
{
    std::string language;
    // ETag of the downloaded global.ini, to find out whether a newer version exists
    std::string etag;
    // SHA-256 of the installed global.ini, to notice when something else changed it
    std::string fileHash;
    // Localization folder it went into; needed to remove an own source after its folder changed
    std::string folder;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InstalledTranslation, language, etag, fileHash, folder)

struct ChannelData
{
    std::filesystem::path installPath;
    // The user's choice. While no source offers it, updates skip the channel instead of removing anything
    std::string language = Protocol::ENGLISH;
    InstalledTranslation installed;
    // ETag the source reports for the chosen language right now; empty until checked
    std::string remoteEtag;
    // The installed files no longer match what was installed; found by the check at start
    bool damaged = false;
    // The last update of this channel failed
    bool lastUpdateFailed = false;
    // The user's own source, offered as language Protocol::CUSTOM
    Protocol::CustomSource custom;
};

// What SelfUpdateManager::install() needs from a release; filled by its check() when a matching signed package exists
struct ReleaseAssets
{
    std::string version;
    std::string packageName;
    std::string packageUrl;
    std::string checksumsUrl;
    std::string signatureUrl;
    // AppImage only: block checksums for a delta update, empty if the release has none
    std::string zsyncUrl;
};

// How this launcher was installed, found by SelfUpdateManager::detectInstallation()
struct Installation
{
    // appimage, deb, rpm, arch, flatpak or source
    std::string type = "source";
    // Release asset for this installation, e.g. "ubuntu24.04" for the matching .deb
    std::string variant;
    // The AppImage file itself, only for type appimage
    std::filesystem::path appImage;
    // Flatpak only: "user" or "system", and whether it updates from the project's Flatpak repository
    // (otherwise it was installed from a bundle and is switched to the repository on its first update)
    std::string flatpakInstallation;
    bool flatpakFromRepository = false;
};

// What of Star Citizen is running, see UpdateManager::runningGameProcess()
enum class GameProcess
{
    None,
    // Only the RSI Launcher is open
    Launcher,
    // StarCitizen.exe runs and reads the translation files
    Game
};

// Only touched by the backend thread, see BackEnd
struct BackendData
{
    std::map<std::string, ChannelData> channels;
    // In languages.json order
    std::vector<AvailableLanguage> languages;
    std::filesystem::path rsiLauncherPath;
    Protocol::Options options;
    Protocol::UpdateStatus update = Protocol::UpdateStatus::Idle;
    Protocol::GameStart game = Protocol::GameStart::Idle;
    // Last seen by the game watcher
    GameProcess gameProcess = GameProcess::None;
    bool searching = false;
    // Set from the start until the first check of the sources is done
    bool checking = true;
    // The window asked for hourly checks and automatic updates (StartBackgroundChecks)
    bool backgroundChecks = false;
    std::uint64_t handledCommands = 0;
    Protocol::SelfUpdate selfUpdate;
    // Set by the last check; what an installation of the launcher update needs
    std::optional<ReleaseAssets> releaseAssets;
    Installation installation;
    std::string lastSeenVersion;
    Protocol::WhatsNew whatsNew;

    // Set when the app closes; background jobs read it to stop early
    std::atomic<bool> shuttingDown{false};
};

}  // namespace Database
