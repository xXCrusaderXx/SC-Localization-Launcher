#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
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
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LanguageDefinition, code, name, folder, source, credit)

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
};

// What the launcher last installed into a channel. Empty language: nothing, the game shows English.
struct InstalledTranslation
{
    std::string language;
    // ETag of the downloaded global.ini, to find out whether a newer version exists
    std::string etag;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InstalledTranslation, language, etag)

struct ChannelData
{
    std::filesystem::path installPath;
    // The user's choice. While no source offers it, updates skip the channel instead of removing anything
    std::string language = Protocol::ENGLISH;
    InstalledTranslation installed;
    // ETag the source reports for the chosen language right now; empty until checked
    std::string remoteEtag;
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
    bool searching = false;
    Protocol::SelfUpdate selfUpdate;

    // Set when the app closes; background jobs read it to stop early
    std::atomic<bool> shuttingDown{false};
};

}  // namespace Database
