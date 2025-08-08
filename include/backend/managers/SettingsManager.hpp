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
    using Callback = std::function<void(const nlohmann::json&)>;
    Callback guiCallback;

   public:
    SettingsManager (Database::BackendData& backendData)
        : lg("SettingsManager")
        , backendData(backendData)
        , dirFinder(backendData)
    {
        LOG_DEBUG(lg) << "instanziated";
    };

    ~SettingsManager () { LOG_DEBUG(lg) << "destructed"; }

    void initialSetup ()
    {
        LOG_DEBUG(lg) << "initialSetup() - Start";
        LOG_DEBUG(lg) << "initialSetup() - Finished";
    }
    void setGuiCallback (Callback cb)
    {
        LOG_DEBUG(lg) << "setGuiCallback()";
        if(!guiCallback)
        {
            LOG_DEBUG(lg) << "Setting new gui callback.";
            guiCallback = cb;
        }
    }

    void processRequest (const Protocol::Massage& request, Protocol::Massage& response)
    {
        LOG_DEBUG(lg) << "processRequest() - Start";
        LOG_DEBUG(lg) << "processRequest() - msg:\n" << request.getJson().dump(4);

        Protocol::SettingsPayload settingsRequest(request.getModuleNode("SETTINGS"));
        Protocol::SettingsPayload settingsResponse;

        if(settingsRequest.settings.LaunchScAfterTranslation)
        {
            backendData.settings.checkboxes.LaunchScAfterTranslation = settingsRequest.settings.LaunchScAfterTranslation.value();
            processLaunchAfterUpdate(settingsRequest, settingsResponse);
        }

        if(settingsRequest.settings.autoTranslationAtStart)
        {
            backendData.settings.checkboxes.autoTranslationAtStart = settingsRequest.settings.autoTranslationAtStart.value();
            processAutoTranslationAtStart(settingsRequest, settingsResponse);
        }
        if(settingsRequest.settings.autoNewTranslation)
        {
            backendData.settings.checkboxes.autoNewTranslation = settingsRequest.settings.autoNewTranslation.value();
            processAutoNewTranslation(settingsRequest, settingsResponse);
        }
        if(backendData.settings.checkboxes.minimizeScdAfterUpdate)
        {
            backendData.settings.checkboxes.minimizeScdAfterUpdate = settingsRequest.settings.minimizeScdAfterUpdate.value();
            processMinimizeScdAfterUpdate(settingsRequest, settingsResponse);
        }
        if(backendData.settings.checkboxes.startScdWithSystemStart)
        {
            backendData.settings.checkboxes.startScdWithSystemStart = settingsRequest.settings.startScdWithSystemStart.value();
            processStartScdWithSystemStart(settingsRequest, settingsResponse);
        }
        if(backendData.settings.checkboxes.showUpdateStatus)
        {
            backendData.settings.checkboxes.showUpdateStatus = settingsRequest.settings.showUpdateStatus.value();
            processShowUpdateStatus(settingsRequest, settingsResponse);
        }

        if(settingsRequest.autoSearchButton.autoSearchStart)
        {
            if(settingsRequest.autoSearchButton.autoSearchStart.value()) processAutoSearch(settingsResponse);

            LOG_DEBUG(lg) << "processRequest() - finished";
        }
        response.AddModuleNode("SETTINGS", settingsResponse.toJson());
    }

    void processLaunchAfterUpdate (Protocol::SettingsPayload& settingsRequest, Protocol::SettingsPayload& response)
    {
        LOG_DEBUG(lg) << "processLaunchAfterUpdate() - LaunchScAfterTranslation is "
                      << (settingsRequest.settings.LaunchScAfterTranslation ? "enabled" : "disabled");

        Protocol::UpdateButtonPayload updateButtonResponse;
        updateButtonResponse.updateButton.LaunchScAfterTranslation = backendData.settings.checkboxes.LaunchScAfterTranslation;

        Protocol::Massage updateResponse(Protocol::MassageType::Response);
        updateResponse.AddModuleNode("UPDATE", updateButtonResponse.toJson());
        guiCallback(updateResponse.getJson());
    }

    void processAutoTranslationAtStart (Protocol::SettingsPayload& settingsRequest, Protocol::SettingsPayload& response) {}
    void processAutoNewTranslation (Protocol::SettingsPayload& settingsRequest, Protocol::SettingsPayload& response) {}
    void processMinimizeScdAfterUpdate (Protocol::SettingsPayload& settingsRequest, Protocol::SettingsPayload& response) {}
    void processStartScdWithSystemStart (Protocol::SettingsPayload& settingsRequest, Protocol::SettingsPayload& response) {}
    void processShowUpdateStatus (Protocol::SettingsPayload& settingsRequest, Protocol::SettingsPayload& response) {}

    void processAutoSearch (Protocol::SettingsPayload& response)
    {
        LOG_DEBUG(lg) << "processAutoSearch() - START";
        std::thread(
            [this] ()
            {
                dirFinder.findRsiInstallation();
                Protocol::SettingsPayload settingsResponse;
                settingsResponse.autoSearchButton.busy = false;
                settingsResponse.paths.liveInstallPath = backendData.channelData["LIVE"].installPath;
                settingsResponse.paths.ptuInstallPath = backendData.channelData["PTU"].installPath;
                settingsResponse.paths.eptuInstallPath = backendData.channelData["EPTU"].installPath;
                settingsResponse.paths.hotfixInstallPath = backendData.channelData["HOTFIX"].installPath;
                settingsResponse.paths.techPrevieInstallPath = backendData.channelData["TECH-PREVIEW"].installPath;
                settingsResponse.paths.rsiLauncherInstallPath = backendData.rsiLauncherInstallPath;
                Protocol::Massage response(Protocol::MassageType::Response);
                response.AddModuleNode("SETTINGS", settingsResponse.toJson());

                LOG_DEBUG(lg) << "processAutoSearch() - FINISHED";
                if(guiCallback)
                    guiCallback(response.getJson());
                else
                    LOG_DEBUG(lg) << "processAutoSearch() - GuiCallBack nullptr";
            })
            .detach();
        LOG_DEBUG(lg) << "processAutoSearch() - RUNNING";
        response.autoSearchButton.busy = true;
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
