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

    std::vector<std::optional<Protocol::Response>> processRequest (const Protocol::Request& msg)
    {
        LOG_DEBUG(lg) << "processRequest() - Start";

        Protocol::SettingsPayload::Request request;
        request.from_json(msg.payload);

        std::vector<std::optional<Protocol::Response>> responses;

        if(backendData.settings.checkboxes.LaunchScAfterTranslation != request.settings.LaunchScAfterTranslation)
        {
            backendData.settings.checkboxes.LaunchScAfterTranslation = request.settings.LaunchScAfterTranslation;
            auto response = processLauchAfterUpdate();
            if(response) responses.push_back(response);
        }

        if(backendData.settings.checkboxes.autoTranslationAtStart != request.settings.autoTranslationAtStart)
        {
            backendData.settings.checkboxes.autoTranslationAtStart = request.settings.autoTranslationAtStart;
            auto response = processAutoTranslationAtStart();
            if(response) responses.push_back(response);
        }
        if(backendData.settings.checkboxes.autoNewTranslation != request.settings.autoNewTranslation)
        {
            backendData.settings.checkboxes.autoNewTranslation = request.settings.autoNewTranslation;
            auto response = processAutoNewTranslation();
            if(response) responses.push_back(response);
        }
        if(backendData.settings.checkboxes.minimizeScdAfterUpdate != request.settings.minimizeScdAfterUpdate)
        {
            backendData.settings.checkboxes.minimizeScdAfterUpdate = request.settings.minimizeScdAfterUpdate;
            auto response = processMinimizeScdAfterUpdate();
            if(response) responses.push_back(response);
        }
        if(backendData.settings.checkboxes.startScdWithSystemStart != request.settings.startScdWithSystemStart)
        {
            backendData.settings.checkboxes.startScdWithSystemStart = request.settings.startScdWithSystemStart;
            auto response = processStartScdWithSystemStart();
            if(response) responses.push_back(response);
        }
        if(backendData.settings.checkboxes.showUpdateStatus != request.settings.showUpdateStatus)
        {
            backendData.settings.checkboxes.showUpdateStatus = request.settings.showUpdateStatus;
            auto response = processShowUpdateStatus();
            if(response) responses.push_back(response);
        }

        if(request.autoSearch)
        {
            auto response = processAutoSearch();
            if(response) responses.push_back(response);
        }

        LOG_DEBUG(lg) << "processRequest() - finished";

        return responses;
    }

    std::optional<Protocol::Response> processLauchAfterUpdate ()
    {
        Protocol::Response response;
        Protocol::UpdateButtonPayload::Response updateButtonRequest;
        LOG_DEBUG(lg) << "processLauchAfterUpdate() - LaunchScAfterTranslation enabled";
        updateButtonRequest.updateButton.LaunchScAfterTranslation = true;
        response.payload = updateButtonRequest.to_json();
        response.type = Protocol::PayloadType::UpdateButton;
        return response;
    }

    std::optional<Protocol::Response> processAutoTranslationAtStart () { return std::nullopt; }
    std::optional<Protocol::Response> processAutoNewTranslation () { return std::nullopt; }
    std::optional<Protocol::Response> processMinimizeScdAfterUpdate () { return std::nullopt; }
    std::optional<Protocol::Response> processStartScdWithSystemStart () { return std::nullopt; }
    std::optional<Protocol::Response> processShowUpdateStatus () { return std::nullopt; }

    std::optional<Protocol::Response> processAutoSearch (std::string cmd = "START")
    {
        Protocol::Response response;
        Protocol::SettingsPayload::Response settingsResponse;

        if(cmd == "START")
        {
            LOG_DEBUG(lg) << "processAutoSearch() - Start";
            std::thread(
                [this] ()
                {
                    dirFinder.findRsiInstallation();

                    processAutoSearch("FINISHED");
                })
                .detach();
            settingsResponse.autoSearchButton.busy = true;
            settingsResponse.paths.liveInstallPath = backendData.channelData["LIVE"].installPath;
            settingsResponse.paths.ptuInstallPath = backendData.channelData["PTU"].installPath;
            settingsResponse.paths.eptuInstallPath = backendData.channelData["EPTU"].installPath;
            settingsResponse.paths.hotfixInstallPath = backendData.channelData["HOTIX"].installPath;
            settingsResponse.paths.techprevieInstallPath = backendData.channelData["TECH-PREVIEW"].installPath;
            settingsResponse.paths.rsiLauncherInstallPath = backendData.rsiLauncherInstallPath;
            response.payload = settingsResponse.to_json();
            response.type = Protocol::PayloadType::Settings;
            return response;
        }
        else if(cmd == "FINISHED")
        {
            LOG_DEBUG(lg) << "processAutoSearch() - Finished";
            // initGui();
            settingsResponse.autoSearchButton.busy = false;
            settingsResponse.paths.liveInstallPath = backendData.channelData["LIVE"].installPath;
            settingsResponse.paths.ptuInstallPath = backendData.channelData["PTU"].installPath;
            settingsResponse.paths.eptuInstallPath = backendData.channelData["EPTU"].installPath;
            settingsResponse.paths.hotfixInstallPath = backendData.channelData["HOTIX"].installPath;
            settingsResponse.paths.techprevieInstallPath = backendData.channelData["TECH-PREVIEW"].installPath;
            settingsResponse.paths.rsiLauncherInstallPath = backendData.rsiLauncherInstallPath;
            response.payload = settingsResponse.to_json();
            response.type = Protocol::PayloadType::Settings;
            return response;
        }
        LOG_DEBUG(lg) << "processAutoSearch() - Finished";

        return std::nullopt;
    }
};
