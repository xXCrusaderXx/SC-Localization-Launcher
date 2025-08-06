#pragma once
#include <optional>
#include <thread>

#include "backend/DirectoryFinder.hpp"
#include "backend/database/DataBase.hpp"
#include "logging/logging.h"
#include "protocol/Protocol.hpp"
#include "protocol/SettingsPayload.hpp"
#include "protocol/UpdateButtonPayload.hpp"

class SettingsManager
{
   private:
    LoggerFramework::LogEx lg;
    DirectoryFinder dirFinder;
    Database::BackendData& backendData;

   public:
    SettingsManager (Database::BackendData& backendData)
        : lg("SettingsManager")
        , backendData(backendData)
        , dirFinder(backendData)
    {
        LOG_DEBUG(lg) << "instanziated";
    };

    ~SettingsManager () { LOG_DEBUG(lg) << "destructed"; }

    void processRequest (const Protocol::Massage& request, Protocol::Massage& response)
    {
        LOG_DEBUG(lg) << "processRequest() - Start";
        LOG_DEBUG(lg) << "processRequest() - msg:\n" << request.getJson().dump(4);

        if(request.moduleExist("Settings"))
        {
            LOG_DEBUG(lg) << "processRequest() - Settings module found";
        }
        else
        {
            LOG_DEBUG(lg) << "processRequest() - Settings module not found";
            return;
        }
        Protocol::SettingsPayload settingsRequest(request.getModuleNode("Settings"));

        if(settingsRequest.settings.LaunchScAfterTranslation)
        {
            backendData.settings.checkboxes.LaunchScAfterTranslation = settingsRequest.settings.LaunchScAfterTranslation.value();
            processLaunchAfterUpdate(settingsRequest, response);
        }

        if(settingsRequest.settings.autoTranslationAtStart)
        {
            backendData.settings.checkboxes.autoTranslationAtStart = settingsRequest.settings.autoTranslationAtStart.value();
            processAutoTranslationAtStart(settingsRequest, response);
        }
        if(settingsRequest.settings.autoNewTranslation)
        {
            backendData.settings.checkboxes.autoNewTranslation = settingsRequest.settings.autoNewTranslation.value();
            processAutoNewTranslation(settingsRequest, response);
        }
        if(backendData.settings.checkboxes.minimizeScdAfterUpdate != settingsRequest.settings.minimizeScdAfterUpdate)
        {
            backendData.settings.checkboxes.minimizeScdAfterUpdate = settingsRequest.settings.minimizeScdAfterUpdate.value();
            processMinimizeScdAfterUpdate(settingsRequest, response);
        }
        if(backendData.settings.checkboxes.startScdWithSystemStart != settingsRequest.settings.startScdWithSystemStart)
        {
            backendData.settings.checkboxes.startScdWithSystemStart = settingsRequest.settings.startScdWithSystemStart.value();
            processStartScdWithSystemStart(settingsRequest, response);
        }
        if(backendData.settings.checkboxes.showUpdateStatus != settingsRequest.settings.showUpdateStatus)
        {
            backendData.settings.checkboxes.showUpdateStatus = settingsRequest.settings.showUpdateStatus.value();
            processShowUpdateStatus(settingsRequest, response);
        }

        if(settingsRequest.autoSearchButton.autoSearchStart)
        {
            processAutoSearch(response);

            LOG_DEBUG(lg) << "processRequest() - finished";
        }
    }

    void processLaunchAfterUpdate (Protocol::SettingsPayload& settingsRequest, Protocol::Massage& response)
    {
        if(!settingsRequest.settings.LaunchScAfterTranslation)
        {
            LOG_DEBUG(lg) << "processLaunchAfterUpdate() - Skipping LaunchScAfterTranslation";
            return;
        }

        LOG_DEBUG(lg) << "processLaunchAfterUpdate() - LaunchScAfterTranslation is "
                      << (settingsRequest.settings.LaunchScAfterTranslation ? "enabled" : "disabled");

        Protocol::UpdateButtonPayload updateButtonResponse;
        updateButtonResponse.updateButton.LaunchScAfterTranslation = settingsRequest.settings.LaunchScAfterTranslation.value();

        response.AddModuleNode("UPDATE", updateButtonResponse.toJson());
    }

    void processAutoTranslationAtStart (Protocol::SettingsPayload& settingsRequest, Protocol::Massage& response) {}
    void processAutoNewTranslation (Protocol::SettingsPayload& settingsRequest, Protocol::Massage& response) {}
    void processMinimizeScdAfterUpdate (Protocol::SettingsPayload& settingsRequest, Protocol::Massage& response) {}
    void processStartScdWithSystemStart (Protocol::SettingsPayload& settingsRequest, Protocol::Massage& response) {}
    void processShowUpdateStatus (Protocol::SettingsPayload& settingsRequest, Protocol::Massage& response) {}

    void processAutoSearch (Protocol::Massage& response, std::string cmd = "START")
    {
        Protocol::SettingsPayload settingsResponse;

        if(cmd == "START")
        {
            LOG_DEBUG(lg) << "processAutoSearch() - Start";
            std::thread(
                [&response, this] ()
                {
                    dirFinder.findRsiInstallation();

                    processAutoSearch(response, "FINISHED");
                })
                .detach();
            settingsResponse.autoSearchButton.busy = true;
            settingsResponse.paths.liveInstallPath = backendData.channelData["LIVE"].installPath;
            settingsResponse.paths.ptuInstallPath = backendData.channelData["PTU"].installPath;
            settingsResponse.paths.eptuInstallPath = backendData.channelData["EPTU"].installPath;
            settingsResponse.paths.hotfixInstallPath = backendData.channelData["HOTIX"].installPath;
            settingsResponse.paths.techPrevieInstallPath = backendData.channelData["TECH-PREVIEW"].installPath;
            settingsResponse.paths.rsiLauncherInstallPath = backendData.rsiLauncherInstallPath;
            response.AddModuleNode("SETTINGS", settingsResponse.toJson());
        }
        else if(cmd == "FINISHED")
        {
            LOG_DEBUG(lg) << "processAutoSearch() - Finished";
            settingsResponse.autoSearchButton.busy = false;
            settingsResponse.paths.liveInstallPath = backendData.channelData["LIVE"].installPath;
            settingsResponse.paths.ptuInstallPath = backendData.channelData["PTU"].installPath;
            settingsResponse.paths.eptuInstallPath = backendData.channelData["EPTU"].installPath;
            settingsResponse.paths.hotfixInstallPath = backendData.channelData["HOTIX"].installPath;
            settingsResponse.paths.techPrevieInstallPath = backendData.channelData["TECH-PREVIEW"].installPath;
            settingsResponse.paths.rsiLauncherInstallPath = backendData.rsiLauncherInstallPath;
            response.AddModuleNode("SETTINGS", settingsResponse.toJson());
        }
    }

    void updateSettingsResponse (Protocol::Massage& response)
    {
        Protocol::SettingsPayload settingsResponse;
        settingsResponse.autoSearchButton.enabled = true;
        settingsResponse.autoSearchButton.busy = false;
        settingsResponse.paths.liveInstallPath = backendData.channelData["LIVE"].installPath;
        settingsResponse.paths.ptuInstallPath = backendData.channelData["PTU"].installPath;
        settingsResponse.paths.eptuInstallPath = backendData.channelData["EPTU"].installPath;
        settingsResponse.paths.hotfixInstallPath = backendData.channelData["HOTFIX"].installPath;
        settingsResponse.paths.techPrevieInstallPath = backendData.channelData["TECH-PREVIEW"].installPath;
        settingsResponse.paths.rsiLauncherInstallPath = backendData.rsiLauncherInstallPath;

        settingsResponse.settings.autoTranslationAtStart = backendData.settings.checkboxes.autoTranslationAtStart;
        settingsResponse.settings.autoNewTranslation = backendData.settings.checkboxes.autoNewTranslation;
        settingsResponse.settings.minimizeScdAfterUpdate = backendData.settings.checkboxes.minimizeScdAfterUpdate;
        settingsResponse.settings.LaunchScAfterTranslation = backendData.settings.checkboxes.LaunchScAfterTranslation;
        settingsResponse.settings.startScdWithSystemStart = backendData.settings.checkboxes.startScdWithSystemStart;
        settingsResponse.settings.showUpdateStatus = backendData.settings.checkboxes.showUpdateStatus;

        response.AddModuleNode("SETTINGS", settingsResponse.toJson());
    }
};
