#pragma once

#include <mutex>
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
    UpdateManager updateManager;
    SettingsManager settingsManager;

    using Callback = std::function<void(const nlohmann::json&)>;
    Callback guiCallback;

    void saveSettings ()
    {
        try
        {
            settingsJsonParser.saveChannelSettings(backendData);
        }
        catch(const std::exception& e)
        {
            LOG_ERROR(lg) << "saveSettings() - " << e.what();
        }
    }

   public:
    BackEnd ()
        : lg("BackEnd")
        , settingsJsonParser(PATHS::JSON_FILES::SETTINGS_USER)
        , channelManager(backendData)
        , updateManager(backendData)
        , settingsManager(backendData)
    {
        auto workerCallback = [this] (const nlohmann::json& msg)
        {
            if(backendData.shuttingDown || !guiCallback) return;
            this->guiCallback(msg);
            this->initGui();
        };
        settingsManager.setGuiCallback(workerCallback);
        updateManager.setGuiCallback(workerCallback);

        settingsJsonParser.getChannelSettings(backendData);
        if(!channelsJsonParser.getChannels(backendData)) LOG_ERROR(lg) << "no channel information, translations cannot be downloaded";

        channelManager.initialSetup();

        LOG_DEBUG(lg) << "instanziated";
    }
    virtual ~BackEnd ()
    {
        // Workers still use backendData and the callbacks, so stop them before anything is destroyed
        backendData.shuttingDown = true;
        updateManager.waitForWorkers();
        settingsManager.waitForWorkers();
        saveSettings();
        LOG_DEBUG(lg) << "destructed";
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

    void processMassage (const nlohmann::json& msg)
    {
        std::lock_guard<std::recursive_mutex> lock(backendData.mutex);
        LOG_DEBUG(lg) << "processMassage() - msg:\n" << msg.dump(4);

        Protocol::Massage request(msg);
        Protocol::Massage response(Protocol::MassageType::Response);

        if(request.moduleExist("LIVE") || request.moduleExist("PTU") || request.moduleExist("EPTU") || request.moduleExist("HOTFIX") ||
           request.moduleExist("TECH-PREVIEW"))
        {
            channelManager.processRequest(request, response);
            // A new install path can make the update button ready
            updateManager.updateUpdateResponse(response);
        }

        if(request.moduleExist("SETTINGS")) settingsManager.processRequest(request, response);
        if(request.moduleExist("UPDATE")) updateManager.processRequest(request, response);

        saveSettings();
        if(guiCallback) guiCallback(response.getJson());
    }
    void initGui ()
    {
        std::lock_guard<std::recursive_mutex> lock(backendData.mutex);
        LOG_DEBUG(lg) << "initGui()";

        Protocol::Massage response(Protocol::MassageType::Response);

        // Paths may have changed in the background (auto search), so recompute the button states
        channelManager.initialSetup();
        channelManager.updateChannelResponse(response);
        settingsManager.updateSettingsResponse(response);
        updateManager.updateUpdateResponse(response);

        if(guiCallback) guiCallback(response.getJson());

        LOG_DEBUG(lg) << "initGui() - finished";
    }
};
