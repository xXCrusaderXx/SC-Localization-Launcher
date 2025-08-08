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
    UpdateManager updateManager;
    SettingsManager settingsManager;

    using Callback = std::function<void(const nlohmann::json&)>;
    Callback guiCallback;

   public:
    BackEnd ()
        : settingsJsonParser(PATHS::JSON_FILES::SETTINGS_USER)
        , lg("BackEnd")
        , channelManager(backendData)
        , settingsManager(backendData)
        , updateManager(backendData)
    {
        settingsManager.setGuiCallback(
            [this] (const nlohmann::json& msg)
            {
                this->guiCallback(msg);
                this->initGui();
            });
        updateManager.setGuiCallback(
            [this] (const nlohmann::json& msg)
            {
                this->guiCallback(msg);
                this->initGui();
            });

        settingsJsonParser.getChannelSettings(backendData);
        channelsJsonParser.getChannels(backendData);

        channelManager.initialSetup();

        LOG_DEBUG(lg) << "instanziated";
    }
    virtual ~BackEnd ()
    {
        settingsJsonParser.saveChannelSettings(backendData);
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
        LOG_DEBUG(lg) << "processMassage() - msg:\n" << msg.dump(4);

        Protocol::Massage request(msg);
        Protocol::Massage response(Protocol::MassageType::Response);

        if(request.moduleExist("LIVE") || request.moduleExist("PTU") || request.moduleExist("EPTU") || request.moduleExist("HOTFIX") ||
           request.moduleExist("TECH-PREVIEW"))
        {
            channelManager.processRequest(request, response);
        }

        if(request.moduleExist("SETTINGS")) settingsManager.processRequest(request, response);
        if(request.moduleExist("UPDATE")) updateManager.processRequest(request, response);

        guiCallback(response.getJson());
    }
    void initGui ()
    {
        LOG_DEBUG(lg) << "initGui()";

        Protocol::Massage response(Protocol::MassageType::Response);

        channelManager.updateChannelResponse(response);
        settingsManager.updateSettingsResponse(response);
        updateManager.updateUpdateResponse(response);

        if(guiCallback) guiCallback(response.getJson());

        LOG_DEBUG(lg) << "initGui() - finished";
    }
};
