#pragma once

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace Database
{

struct WebInfo
{
    std::string description;
    std::string folder;
    int active;
    std::string server1;
    std::string server2;
    std::string server1_fallback;
    std::string server2_fallback;
};

struct ChannelButton
{
    std::optional<bool> enabled;
    std::optional<bool> active;
};

struct TranslationTargetButton
{
    bool enabled = false;
    bool selected = false;
};

struct ChannelData
{
    WebInfo info;
    std::filesystem::path installPath = "";
    ChannelButton buttonChannel;
    TranslationTargetButton buttonEng;
    TranslationTargetButton buttonDe;
    TranslationTargetButton buttonDeFull;
};

struct Checkboxes
{
    bool autoTranslationAtStart = false;
    bool autoNewTranslation = false;
    bool minimizeScdAfterUpdate = false;
    bool LaunchScAfterTranslation = false;
    bool startScdWithSystemStart = false;
    bool showUpdateStatus = false;
};

struct Settings
{
    Checkboxes checkboxes;
};

struct BackendData
{
    std::map<std::string, ChannelData> channelData;
    Settings settings;
    std::filesystem::path rsiLauncherInstallPath = "";
    bool anyChannelPathSet = false;
};

}  // namespace Database
