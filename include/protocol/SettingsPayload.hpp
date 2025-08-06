#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>

namespace Protocol
{

struct AutoSearchButton
{
    std::optional<bool> enabled = false;
    std::optional<bool> busy = false;
    std::optional<bool> autoSearchStart = false;
};

struct Paths
{
    std::optional<std::filesystem::path> liveInstallPath;
    std::optional<std::filesystem::path> ptuInstallPath;
    std::optional<std::filesystem::path> eptuInstallPath;
    std::optional<std::filesystem::path> hotfixInstallPath;
    std::optional<std::filesystem::path> techPrevieInstallPath;
    std::optional<std::filesystem::path> rsiLauncherInstallPath;
};

struct Settings
{
    std::optional<bool> autoTranslationAtStart = false;
    std::optional<bool> autoNewTranslation = false;
    std::optional<bool> minimizeScdAfterUpdate = false;
    std::optional<bool> LaunchScAfterTranslation = false;
    std::optional<bool> startScdWithSystemStart = false;
    std::optional<bool> showUpdateStatus = false;
};

struct SettingsPayload
{
    Paths paths;
    Settings settings;
    AutoSearchButton autoSearchButton;

    SettingsPayload () = default;

    SettingsPayload (const nlohmann::json& j)
    {
        if(j.contains("paths"))
        {
            if(j.at("paths").contains("liveInstallPath")) paths.liveInstallPath = j.at("paths").at("liveInstallPath").get<std::filesystem::path>();
            if(j.at("paths").contains("ptuInstallPath")) paths.ptuInstallPath = j.at("paths").at("ptuInstallPath").get<std::filesystem::path>();
            if(j.at("paths").contains("eptuInstallPath")) paths.eptuInstallPath = j.at("paths").at("eptuInstallPath").get<std::filesystem::path>();
            if(j.at("paths").contains("hotfixInstallPath"))
                paths.hotfixInstallPath = j.at("paths").at("hotfixInstallPath").get<std::filesystem::path>();
            if(j.at("paths").contains("techPrevieInstallPath"))
                paths.techPrevieInstallPath = j.at("paths").at("techPrevieInstallPath").get<std::filesystem::path>();
            if(j.at("paths").contains("rsiLauncherInstallPath"))
                paths.rsiLauncherInstallPath = j.at("paths").at("rsiLauncherInstallPath").get<std::filesystem::path>();
        }
        if(j.contains("settings"))
        {
            if(j.at("settings").contains("autoTranslationAtStart"))
                settings.autoTranslationAtStart = j.at("settings").at("autoTranslationAtStart").get<bool>();
            if(j.at("settings").contains("autoNewTranslation")) settings.autoNewTranslation = j.at("settings").at("autoNewTranslation").get<bool>();
            if(j.at("settings").contains("minimizeScdAfterUpdate"))
                settings.minimizeScdAfterUpdate = j.at("settings").at("minimizeScdAfterUpdate").get<bool>();
            if(j.at("settings").contains("LaunchScAfterTranslation"))
                settings.LaunchScAfterTranslation = j.at("settings").at("LaunchScAfterTranslation").get<bool>();
            if(j.at("settings").contains("startScdWithSystemStart"))
                settings.startScdWithSystemStart = j.at("settings").at("startScdWithSystemStart").get<bool>();
            if(j.at("settings").contains("showUpdateStatus")) settings.showUpdateStatus = j.at("settings").at("showUpdateStatus").get<bool>();
        }
        if(j.contains("autoSearchButton"))
        {
            if(j.at("autoSearchButton").contains("enabled")) autoSearchButton.enabled = j.at("autoSearchButton").at("enabled").get<bool>();
            if(j.at("autoSearchButton").contains("busy")) autoSearchButton.busy = j.at("autoSearchButton").at("busy").get<bool>();
            if(j.at("autoSearchButton").contains("autoSearchStart"))
                autoSearchButton.autoSearchStart = j.at("autoSearchButton").at("autoSearchStart").get<bool>();
        }
    }

    nlohmann::json toJson () const
    {
        nlohmann::json j;
        if(paths.liveInstallPath) j["paths"]["liveInstallPath"] = paths.liveInstallPath->string();
        if(paths.ptuInstallPath) j["paths"]["ptuInstallPath"] = paths.ptuInstallPath->string();
        if(paths.eptuInstallPath) j["paths"]["eptuInstallPath"] = paths.eptuInstallPath->string();
        if(paths.hotfixInstallPath) j["paths"]["hotfixInstallPath"] = paths.hotfixInstallPath->string();
        if(paths.techPrevieInstallPath) j["paths"]["techPrevieInstallPath"] = paths.techPrevieInstallPath->string();
        if(paths.rsiLauncherInstallPath) j["paths"]["rsiLauncherInstallPath"] = paths.rsiLauncherInstallPath->string();
        if(settings.autoTranslationAtStart) j["autoTranslationAtStart"] = settings.autoTranslationAtStart.value();
        if(settings.autoNewTranslation) j["autoNewTranslation"] = settings.autoNewTranslation.value();
        if(settings.minimizeScdAfterUpdate) j["minimizeScdAfterUpdate"] = settings.minimizeScdAfterUpdate.value();
        if(settings.LaunchScAfterTranslation) j["LaunchScAfterTranslation"] = settings.LaunchScAfterTranslation.value();
        if(settings.startScdWithSystemStart) j["startScdWithSystemStart"] = settings.startScdWithSystemStart.value();
        if(settings.showUpdateStatus) j["showUpdateStatus"] = settings.showUpdateStatus.value();
        if(autoSearchButton.enabled) j["autoSearchButton"]["enabled"] = autoSearchButton.enabled.value();
        if(autoSearchButton.busy) j["autoSearchButton"]["busy"] = autoSearchButton.busy.value();
        if(autoSearchButton.autoSearchStart) j["autoSearchButton"]["autoSearchStart"] = autoSearchButton.autoSearchStart.value();
        return j;
    };
};

}  // namespace Protocol
