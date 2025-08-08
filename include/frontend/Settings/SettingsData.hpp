#pragma once

#include <filesystem>
#include "protocol/SettingsPayload.hpp"

struct SettingsData
{
    // SETTINGS
    bool autoNewTranslation;
    bool autoTranslationAtStart;
    bool LaunchScAfterTranslation;
    bool minimizeScdAfterUpdate;
    bool showUpdateStatus;
    bool startScdWithSystemStart;
    // PATHS
    std::filesystem::path liveInstallPath;
    std::filesystem::path ptuInstallPath;
    std::filesystem::path eptuInstallPath;
    std::filesystem::path hotfixInstallPath;
    std::filesystem::path techPrevieInstallPath;
    std::filesystem::path rsiLauncherInstallPath;

    // AUTO-SEARCH-BUTTON
    bool autoSearchButtonBusy;
    bool autoSearchButtonEnabled;

    SettingsData () = default;
    SettingsData (const SettingsData &settingsData) { updateSettingsData(settingsData); }

    void updateSettingsData (const SettingsData &settingsData)
    {
        // SETTINGS
        autoNewTranslation = settingsData.autoNewTranslation;
        autoTranslationAtStart = settingsData.autoTranslationAtStart;
        LaunchScAfterTranslation = settingsData.LaunchScAfterTranslation;
        minimizeScdAfterUpdate = settingsData.minimizeScdAfterUpdate;
        showUpdateStatus = settingsData.showUpdateStatus;
        startScdWithSystemStart = settingsData.startScdWithSystemStart;
        // PATHS
        liveInstallPath = settingsData.liveInstallPath;
        ptuInstallPath = settingsData.ptuInstallPath;
        eptuInstallPath = settingsData.eptuInstallPath;
        hotfixInstallPath = settingsData.hotfixInstallPath;
        techPrevieInstallPath = settingsData.techPrevieInstallPath;
        rsiLauncherInstallPath = settingsData.rsiLauncherInstallPath;

        // AUTO-SEARCH-BUTTON
        autoSearchButtonBusy = settingsData.autoSearchButtonBusy;
        autoSearchButtonEnabled = settingsData.autoSearchButtonEnabled;
    }

    void updateSettingsData (const Protocol::SettingsPayload &settingsPayload)
    {
        // SETTINGS
        if(settingsPayload.settings.autoNewTranslation) autoNewTranslation = settingsPayload.settings.autoNewTranslation.value();
        if(settingsPayload.settings.autoTranslationAtStart) autoTranslationAtStart = settingsPayload.settings.autoTranslationAtStart.value();
        if(settingsPayload.settings.LaunchScAfterTranslation) LaunchScAfterTranslation = settingsPayload.settings.LaunchScAfterTranslation.value();
        if(settingsPayload.settings.minimizeScdAfterUpdate) minimizeScdAfterUpdate = settingsPayload.settings.minimizeScdAfterUpdate.value();
        if(settingsPayload.settings.showUpdateStatus) showUpdateStatus = settingsPayload.settings.showUpdateStatus.value();
        if(settingsPayload.settings.startScdWithSystemStart) startScdWithSystemStart = settingsPayload.settings.startScdWithSystemStart.value();

        // PATHS-
        if(settingsPayload.paths.liveInstallPath) liveInstallPath = settingsPayload.paths.liveInstallPath.value();
        if(settingsPayload.paths.ptuInstallPath) ptuInstallPath = settingsPayload.paths.ptuInstallPath.value();
        if(settingsPayload.paths.eptuInstallPath) eptuInstallPath = settingsPayload.paths.eptuInstallPath.value();
        if(settingsPayload.paths.hotfixInstallPath) hotfixInstallPath = settingsPayload.paths.hotfixInstallPath.value();
        if(settingsPayload.paths.techPrevieInstallPath) techPrevieInstallPath = settingsPayload.paths.techPrevieInstallPath.value();
        if(settingsPayload.paths.rsiLauncherInstallPath) rsiLauncherInstallPath = settingsPayload.paths.rsiLauncherInstallPath.value();

        // AUTO-SEARCH-BUTTON
        if(settingsPayload.autoSearchButton.busy) autoSearchButtonBusy = settingsPayload.autoSearchButton.busy.value();
        if(settingsPayload.autoSearchButton.enabled) autoSearchButtonEnabled = settingsPayload.autoSearchButton.enabled.value();
    }
};
