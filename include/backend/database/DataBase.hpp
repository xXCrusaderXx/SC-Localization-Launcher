#pragma once

#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace Database
{

struct Info
{
    std::string description;
    std::string folder;
    int active;
    std::string server1;
    std::string server2;
    std::string server1_fallback;
    std::string server2_fallback;
};

struct Controls
{
    bool installPathIsSet = false;
    bool buttonEng_Enabled = false;
    bool buttonDe_Enabled = false;
    bool buttonDeFull_Enabled = false;
    bool buttonEng_Selected = false;
    bool buttonDe_Selected = false;
    bool buttonDeFull_Selected = false;
};

struct Settings
{
    bool autoTranslationAtStart = false;
    bool autoNewTranslation = false;
    bool minimizeScdAfterUpdate = false;
    bool LaunchScAfterTranslation = false;
    bool startScdWithSystemStart = false;
    bool showUpdateStatus = false;
};

struct Paths
{
    std::filesystem::path installPath = "";
    std::filesystem::path backupPath = "";
};

struct ChannelData
{
    Info info;
    Controls controls;
    Paths paths;
};

struct DataBase
{
    std::map<std::string, ChannelData> channelData;
    Settings settings;
};
}  // namespace Database
