#pragma once

#include <nlohmann/json.hpp>
#include <thread>

#include "backend/utils/paths.hpp"
#include "database/DataBase.hpp"
#include "jsonParser/ChannelsJsonParser.hpp"
#include "jsonParser/SettingsJsonParser.hpp"
#include "logging/logging.h"
#include "managers/ChannelManager.hpp"
#include "managers/SettingsManager.hpp"
#include "managers/UpdateManager.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/UpdateButtonPayload.hpp"

class BackEnd
{
   private:
    LoggerFramework::LogEx lg;
    Database::BackendData backendData;
    ChannelsJsonParser channelsJsonParser;
    SettingsJsonParser settingsJsonParser;
    ChannelManager channelManager;
    UpdateManager updateHandler;
    SettingsManager settingsManager;

    using Callback = std::function<void(const nlohmann::json&)>;
    std::map<std::string, Callback> guiCallbacks;

   public:
    BackEnd ()
        : settingsJsonParser(PATHS::JSON_FILES::SETTINGS_USER)
        , lg("BackEnd")
        , settingsManager(backendData)
    {
        settingsJsonParser.getChannelSettings(backendData);
        channelsJsonParser.getChannels(backendData);

        channelManager.initialSetup(backendData);

        LOG_DEBUG(lg) << "instanziated";
    }
    virtual ~BackEnd ()
    {
        settingsJsonParser.saveChannelSettings(backendData);
        LOG_DEBUG(lg) << "destructed";
    }

    void setGuiCallback (const std::string& key, Callback cb) { guiCallbacks[key] = cb; };

    void processMassage (const nlohmann::json& msg)
    {
        LOG_DEBUG(lg) << "processMassage() - msg:\n" << msg.dump(4);

        Protocol::Request request;
        request.fromJson(msg);

        if(request.type == Protocol::PayloadType::ChannelLive || request.type == Protocol::PayloadType::ChannelPTU ||
           request.type == Protocol::PayloadType::ChannelEPTU || request.type == Protocol::PayloadType::ChannelHotfix ||
           request.type == Protocol::PayloadType::ChannelTechPeview)
        {
            auto response = channelManager.processRequest(backendData, request);

            std::string key = "";
            if(request.type == Protocol::PayloadType::ChannelLive)
            {
                key = "LIVE";
            }
            else if(request.type == Protocol::PayloadType::ChannelPTU)
            {
                key = "PTU";
            }
            else if(request.type == Protocol::PayloadType::ChannelEPTU)
            {
                key = "EPTU";
            }
            else if(request.type == Protocol::PayloadType::ChannelHotfix)
            {
                key = "HOTFIX";
            }
            else if(request.type == Protocol::PayloadType::ChannelTechPeview)
            {
                key = "TECH-PREVIEW";
            }
            if(guiCallbacks[key]) guiCallbacks[key](response->toJson());
        }

        if(request.type == Protocol::PayloadType::Settings)
        {
            settingsManager.processRequest(request);
        }

        if(request.type == Protocol::PayloadType::UpdateButton)
        {
            Protocol::Response response;
            Protocol::UpdateButtonPayload::Response updateButtonResponse;

            updateButtonResponse.updateButton.busy = true;
            updateButtonResponse.updateButton.enabled = true;
            updateButtonResponse.updateButton.LaunchScAfterTranslation = backendData.settings.checkboxes.LaunchScAfterTranslation;

            response.payload = updateButtonResponse.to_json();
            response.type = Protocol::PayloadType::UpdateButton;
            guiCallbacks["UPDATE"](response.toJson());

            updateHandler.processRequest(backendData, request);

            updateButtonResponse.updateButton.busy = false;
            updateButtonResponse.updateButton.enabled = true;
            response.payload = updateButtonResponse.to_json();
            guiCallbacks["UPDATE"](response.toJson());

            if(backendData.settings.checkboxes.LaunchScAfterTranslation)
            {
                bool lutris_installation = true;
                bool lug_installation = true;

                if(lutris_installation)
                {
                    chdir(backendData.rsiLauncherInstallPath.string().c_str());
                    LOG_DEBUG(lg) << "processMassage() - EXEC_PATH: " << backendData.rsiLauncherInstallPath.string().c_str();
                    std::string command = "/usr/bin/lutris lutris:rungame/star-citizen";
                    std::system(command.c_str());
                    chdir(PATHS::ROOT.c_str());
                }

                if(lug_installation)
                {
                    std::string bashPath = backendData.rsiLauncherInstallPath.parent_path().parent_path().parent_path().parent_path().string();
                    chdir(bashPath.c_str());
                    LOG_DEBUG(lg) << "processMassage() - EXEC_PATH: " << bashPath << "/sc-launch.sh";
                    std::string command = "sh " + bashPath + "/sc-launch.sh";
                    std::system(command.c_str());
                    chdir(PATHS::ROOT.c_str());
                }

                LOG_DEBUG(lg) << "processMassage() - SC_LAUNCH - FINISHED!";
            }
        }
    }

    void initGui ()
    {
        LOG_DEBUG(lg) << "initGui()";
        bool readyForUpdate = false;

        nlohmann::json updateButtonInit;
        nlohmann::json updateSettingsInit;

        for(const auto& [channel, data] : backendData.channelData)
        {
            Protocol::Response response;
            Protocol::ChannelPayload::Response channelResponse;

            if(!backendData.channelData[channel].installPath.empty())
            {
                readyForUpdate = true;
                channelResponse.buttonChannel.enabled = true;
                channelResponse.buttonChannel.acitve = false;
                channelResponse.buttonEng.enabled = true;
                channelResponse.buttonEng.selected = true;
                channelResponse.buttonDe.enabled = backendData.channelData[channel].buttonDe.enabled;
                channelResponse.buttonDe.selected = backendData.channelData[channel].buttonDe.selected;
                channelResponse.buttonDeFull.enabled = backendData.channelData[channel].buttonDeFull.enabled;
                channelResponse.buttonDeFull.selected = backendData.channelData[channel].buttonDeFull.selected;
            }
            else
            {
                channelResponse.buttonChannel.enabled = backendData.channelData[channel].buttonChannel.enabled;
            }

            channelResponse.installPathIsSet = !backendData.channelData[channel].installPath.empty();
            response.payload = channelResponse.to_json();
            response.type = Protocol::payloadTypeFromString(channel);
            if(guiCallbacks[channel]) guiCallbacks[channel](response.toJson());
        }

        if(readyForUpdate)
        {
            Protocol::Response response;
            Protocol::UpdateButtonPayload::Response updateButtonResponse;
            updateButtonResponse.updateButton.enabled = true;
            updateButtonResponse.updateButton.busy = false;
            updateButtonResponse.updateButton.LaunchScAfterTranslation = backendData.settings.checkboxes.LaunchScAfterTranslation;
            if(guiCallbacks["UPDATE"]) guiCallbacks["UPDATE"](updateButtonResponse.to_json());
        }

        Protocol::Response response;
        Protocol::SettingsPayload::Response settingsResponse;
        settingsResponse.autoSearchButton.enabled = true;
        settingsResponse.autoSearchButton.busy = false;
        settingsResponse.paths.liveInstallPath = backendData.channelData["LIVE"].installPath;
        settingsResponse.paths.ptuInstallPath = backendData.channelData["PTU"].installPath;
        settingsResponse.paths.eptuInstallPath = backendData.channelData["EPTU"].installPath;
        settingsResponse.paths.hotfixInstallPath = backendData.channelData["HOTFIX"].installPath;
        settingsResponse.paths.techprevieInstallPath = backendData.channelData["TECH-PREVIEW"].installPath;
        settingsResponse.paths.rsiLauncherInstallPath = backendData.rsiLauncherInstallPath;

        settingsResponse.settings.autoTranslationAtStart = backendData.settings.checkboxes.autoTranslationAtStart;
        settingsResponse.settings.autoNewTranslation = backendData.settings.checkboxes.autoNewTranslation;
        settingsResponse.settings.minimizeScdAfterUpdate = backendData.settings.checkboxes.minimizeScdAfterUpdate;
        settingsResponse.settings.LaunchScAfterTranslation = backendData.settings.checkboxes.LaunchScAfterTranslation;
        settingsResponse.settings.startScdWithSystemStart = backendData.settings.checkboxes.startScdWithSystemStart;
        settingsResponse.settings.showUpdateStatus = backendData.settings.checkboxes.showUpdateStatus;
        response.payload = settingsResponse.to_json();
        response.type = Protocol::PayloadType::Settings;

        if(guiCallbacks["SETTINGS"]) guiCallbacks["SETTINGS"](response.toJson());

        LOG_DEBUG(lg) << "initGui() - finished";
    }
};
