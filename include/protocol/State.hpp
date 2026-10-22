#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

// The complete backend state. The backend sends a fresh snapshot after every change,
// the frontend only displays it and never has to merge anything.
namespace Protocol
{

// Channels in display order
inline const std::vector<std::string> CHANNEL_NAMES{"LIVE", "PTU", "EPTU", "HOTFIX", "TECH-PREVIEW"};

enum class Language
{
    English,
    German,
    GermanFull
};

NLOHMANN_JSON_SERIALIZE_ENUM(Language, {{Language::English, "eng"}, {Language::German, "de"}, {Language::GermanFull, "deFull"}})

// Unlike from_json, which falls back to English, unknown names are rejected
inline std::optional<Language> languageFromString (const std::string& name)
{
    for(auto language : {Language::English, Language::German, Language::GermanFull})
        if(nlohmann::json(language) == name) return language;
    return std::nullopt;
}

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
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Options, launchGameAfterUpdate, updateAtStart, installNewTranslations, minimizeAfterUpdate,
                                                startWithSystem, showUpdateProgress)

struct Channel
{
    std::string name;
    std::string installPath;
    Language language = Language::English;
    // What the translation server currently offers, independent of the install path
    std::vector<Language> availableLanguages;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Channel, name, installPath, language, availableLanguages)

struct State
{
    std::vector<Channel> channels;
    std::string rsiLauncherPath;
    Options options;
    UpdateStatus update = UpdateStatus::Idle;
    bool searching = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(State, channels, rsiLauncherPath, options, update, searching)

}  // namespace Protocol
