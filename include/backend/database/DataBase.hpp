#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <nlohmann/json.hpp>
#include <string>

namespace Database
{

// Server mapping in channels.json:
// server1 = German (hybrid), server2 = English, server3 = German (full), server4 = Swiss German
struct WebInfo
{
    std::string description;
    std::string folder;
    int active = 0;
    std::string server1;
    std::string server2;
    std::string server3;
    std::string server4;
    std::string server1_fallback;
    std::string server2_fallback;
    std::string server3_fallback;
    std::string server4_fallback;
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

    // Guards all fields above. Recursive, because manager callbacks re-enter the backend
    // (e.g. initGui()) while processMassage() holds the lock.
    std::recursive_mutex mutex;
    // Set when the app closes; workers stop early and skip GUI callbacks
    std::atomic<bool> shuttingDown{false};
};

}  // namespace Database
