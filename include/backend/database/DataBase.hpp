#pragma once

#include <atomic>
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

#include "protocol/State.hpp"

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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WebInfo, description, folder, active, server1, server2, server3, server4, server1_fallback,
                                                server2_fallback, server3_fallback, server4_fallback)

struct ChannelData
{
    WebInfo info;
    std::filesystem::path installPath;
    // The user's choice. It is kept even while the server does not offer it, see BackEnd::effectiveLanguage()
    Protocol::Language language = Protocol::Language::English;
};

// Only touched by the backend thread, see BackEnd
struct BackendData
{
    std::map<std::string, ChannelData> channels;
    std::filesystem::path rsiLauncherPath;
    Protocol::Options options;
    Protocol::UpdateStatus update = Protocol::UpdateStatus::Idle;
    bool searching = false;

    // Set when the app closes; background jobs read it to stop early
    std::atomic<bool> shuttingDown{false};
};

}  // namespace Database
