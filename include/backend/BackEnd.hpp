#pragma once

#include <nlohmann/json.hpp>

#include "backend/utils/paths.hpp"
#include "database/DataBase.hpp"
#include "jsonParser/ChannelsJsonParser.hpp"
#include "jsonParser/SettingsJsonParser.hpp"
#include "logging/logging.h"
#include "updateHandler/UpdateHandler.hpp"

class BackEnd
{
   private:
    LoggerFramework::LogEx lg;
    Database::Channels database;
    ChannelsJsonParser channelsJsonParser;
    SettingsJsonParser settingsJsonParser;
    UpdateHandler updateHandler;

    using Callback = std::function<void(const nlohmann::json&)>;
    std::map<std::string, Callback> guiCallbacks;

    void updateControlsByDataBase ()
    {
        for(auto& [channel, data] : database)
        {
            data.controls.buttonEng_Enabled = true;

            if(data.info.active)
            {
                if(!data.info.server1.empty()) data.controls.buttonDe_Enabled = true;
                if(!data.info.server2.empty()) data.controls.buttonDeFull_Enabled = true;
            }
            if(!data.paths.installPath.empty()) data.controls.installPathIsSet = true;
        }
    }

   public:
    BackEnd ()
        : settingsJsonParser(PATHS::JSON_FILES::SETTINGS)
        , lg("BackEnd")
    {
        settingsJsonParser.getChannelSettings(database);
        channelsJsonParser.getChannels(database);

        updateControlsByDataBase();
        LOG_DEBUG(lg) << "instanziated";
    }
    virtual ~BackEnd ()
    {
        settingsJsonParser.saveChannelSettings(database);
        LOG_DEBUG(lg) << "destructed";
    }

    void setGuiCallback (const std::string& key, Callback cb) { guiCallbacks[key] = cb; };

    void processMassage (const std::string& key, const nlohmann::json& msg)
    {
        LOG_DEBUG(lg) << "processMassage() - " << key << "-msg:\n" << msg.dump(4);

        if(key == "LIVE" || key == "PTU" || key == "EPTU" || key == "HOTFIX" || key == "TECH-PREVIEW")
        {
            if(msg.contains("buttonEngSelected")) database[key].controls.buttonEng_Selected = msg["buttonEngSelected"];
            if(msg.contains("buttonDeSelected")) database[key].controls.buttonDe_Selected = msg["buttonDeSelected"];
            if(msg.contains("buttonDeFullSelected")) database[key].controls.buttonDeFull_Selected = msg["buttonDeFullSelected"];
            if(msg.contains("installPath"))
            {
                database[key].paths.installPath = std::filesystem::path(msg["installPath"]);
                nlohmann::json responseMsg;
                updateControlsByDataBase();
                responseMsg["installPathIsSet"] = true;
                responseMsg["buttonEngSelected"] = true;
                responseMsg["buttonDeEnabled"] = database[key].controls.buttonDe_Enabled;
                responseMsg["buttonDeFullEnabled"] = database[key].controls.buttonDeFull_Enabled;
                guiCallbacks[key](responseMsg);

                nlohmann::json updateButtonMassage;
                updateButtonMassage["buttonEnabled"] = true;
                updateButtonMassage["buttonReady"] = true;
                if(guiCallbacks[key]) guiCallbacks[key](updateButtonMassage);
            }
        }
        if(key == "UPDATE")
        {
            if(msg.contains("start"))
            {
                nlohmann::json responseMsg;
                responseMsg["buttonBusy"] = true;
                guiCallbacks[key](responseMsg);

                updateHandler.update(database);

                nlohmann::json responseMsg2;
                responseMsg2["buttonReady"] = true;
                guiCallbacks[key](responseMsg2);
            }
        }
    }

    void initGui ()
    {
        LOG_DEBUG(lg) << "initGui()";
        bool readyForUpdate = false;

        for(const auto& [channel, data] : database)
        {
            nlohmann::json msg;

            if(database[channel].controls.installPathIsSet == true)
            {
                readyForUpdate = true;
                msg["buttonEngEnabled"] = database[channel].controls.buttonDe_Enabled;
                msg["buttonDeEnabled"] = database[channel].controls.buttonDe_Enabled;
                msg["buttonDeFullEnabled"] = database[channel].controls.buttonDeFull_Enabled;
                msg["buttonEngSelected"] = database[channel].controls.buttonEng_Selected;
                msg["buttonDeSelected"] = database[channel].controls.buttonDe_Selected;
                msg["buttonDeFullSelected"] = database[channel].controls.buttonDeFull_Selected;
            }
            else
            {
                msg["buttonEngEnabled"] = database[channel].controls.buttonEng_Enabled;
            }
            msg["installPathIsSet"] = database[channel].controls.installPathIsSet;

            if(guiCallbacks[channel]) guiCallbacks[channel](msg);
        }

        if(readyForUpdate)
        {
            nlohmann::json updateButtonMassage;
            updateButtonMassage["buttonEnabled"] = true;
            updateButtonMassage["buttonReady"] = true;
            if(guiCallbacks["UPDATE"]) guiCallbacks["UPDATE"](updateButtonMassage);
        }
    }
};
