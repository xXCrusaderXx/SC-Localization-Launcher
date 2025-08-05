#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Protocol
{
namespace SettingsPayload
{

struct AutoSearchButton
{
    bool enabled = false;
    bool busy = false;
};

struct Paths
{
    std::filesystem::path liveInstallPath;
    std::filesystem::path ptuInstallPath;
    std::filesystem::path eptuInstallPath;
    std::filesystem::path hotfixInstallPath;
    std::filesystem::path techprevieInstallPath;
    std::filesystem::path rsiLauncherInstallPath;
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

struct Request
{
    Paths paths;
    Settings settings;
    bool autoSearch = false;

    nlohmann::json to_json () const
    {
        nlohmann::json j;
        j["paths"] = {{"liveInstallPath", paths.liveInstallPath.string()},
                      {"ptuInstallPath", paths.ptuInstallPath.string()},
                      {"eptuInstallPath", paths.eptuInstallPath.string()},
                      {"hotfixInstallPath", paths.hotfixInstallPath.string()},
                      {"techprevieInstallPath", paths.techprevieInstallPath.string()},
                      {"rsiLauncherInstallPath", paths.rsiLauncherInstallPath.string()}};
        j["autoTranslationAtStart"] = settings.autoTranslationAtStart;
        j["autoNewTranslation"] = settings.autoNewTranslation;
        j["minimizeScdAfterUpdate"] = settings.minimizeScdAfterUpdate;
        j["LaunchScAfterTranslation"] = settings.LaunchScAfterTranslation;
        j["startScdWithSystemStart"] = settings.startScdWithSystemStart;
        j["showUpdateStatus"] = settings.showUpdateStatus;
        j["autoSearch"] = autoSearch;
        return j;
    };

    void from_json (const nlohmann::json& j)
    {
        paths.liveInstallPath = j.at("paths").at("liveInstallPath").get<std::filesystem::path>();
        paths.ptuInstallPath = j.at("paths").at("ptuInstallPath").get<std::filesystem::path>();
        paths.eptuInstallPath = j.at("paths").at("eptuInstallPath").get<std::filesystem::path>();
        paths.hotfixInstallPath = j.at("paths").at("hotfixInstallPath").get<std::filesystem::path>();
        paths.techprevieInstallPath = j.at("paths").at("techprevieInstallPath").get<std::filesystem::path>();
        paths.rsiLauncherInstallPath = j.at("paths").at("rsiLauncherInstallPath").get<std::filesystem::path>();

        settings.autoTranslationAtStart = j.at("autoTranslationAtStart").get<bool>();
        settings.autoNewTranslation = j.at("autoNewTranslation").get<bool>();
        settings.minimizeScdAfterUpdate = j.at("minimizeScdAfterUpdate").get<bool>();
        settings.LaunchScAfterTranslation = j.at("LaunchScAfterTranslation").get<bool>();
        settings.startScdWithSystemStart = j.at("startScdWithSystemStart").get<bool>();
        settings.showUpdateStatus = j.at("showUpdateStatus").get<bool>();
        autoSearch = j.at("autoSearch").get<bool>();
    };
};
struct Response
{
    Paths paths;
    Settings settings;
    AutoSearchButton autoSearchButton;

    nlohmann::json to_json () const
    {
        nlohmann::json j;
        j["paths"] = {{"liveInstallPath", paths.liveInstallPath.string()},
                      {"ptuInstallPath", paths.ptuInstallPath.string()},
                      {"eptuInstallPath", paths.eptuInstallPath.string()},
                      {"hotfixInstallPath", paths.hotfixInstallPath.string()},
                      {"techprevieInstallPath", paths.techprevieInstallPath.string()},
                      {"rsiLauncherInstallPath", paths.rsiLauncherInstallPath.string()}};
        j["settings"] = {
            {"autoTranslationAtStart", settings.autoTranslationAtStart},   {"autoNewTranslation", settings.autoNewTranslation},
            {"minimizeScdAfterUpdate", settings.minimizeScdAfterUpdate},   {"LaunchScAfterTranslation", settings.LaunchScAfterTranslation},
            {"startScdWithSystemStart", settings.startScdWithSystemStart}, {"showUpdateStatus", settings.showUpdateStatus}};
        j["autoSearchButton"] = {{"enabled", autoSearchButton.enabled}, {"busy", autoSearchButton.busy}};
        return j;
    }

    void from_json (const nlohmann::json& j)
    {
        paths.liveInstallPath = j.at("paths").at("liveInstallPath").get<std::filesystem::path>();
        paths.ptuInstallPath = j.at("paths").at("ptuInstallPath").get<std::filesystem::path>();
        paths.eptuInstallPath = j.at("paths").at("eptuInstallPath").get<std::filesystem::path>();
        paths.hotfixInstallPath = j.at("paths").at("hotfixInstallPath").get<std::filesystem::path>();
        paths.techprevieInstallPath = j.at("paths").at("techprevieInstallPath").get<std::filesystem::path>();
        paths.rsiLauncherInstallPath = j.at("paths").at("rsiLauncherInstallPath").get<std::filesystem::path>();
        settings.autoTranslationAtStart = j.at("settings").at("autoTranslationAtStart").get<bool>();
        settings.autoNewTranslation = j.at("settings").at("autoNewTranslation").get<bool>();
        settings.minimizeScdAfterUpdate = j.at("settings").at("minimizeScdAfterUpdate").get<bool>();
        settings.LaunchScAfterTranslation = j.at("settings").at("LaunchScAfterTranslation").get<bool>();
        settings.startScdWithSystemStart = j.at("settings").at("startScdWithSystemStart").get<bool>();
        settings.showUpdateStatus = j.at("settings").at("showUpdateStatus").get<bool>();
        autoSearchButton.enabled = j.at("autoSearchButton").at("enabled").get<bool>();
    }
};

}  // namespace SettingsPayload
}  // namespace Protocol
