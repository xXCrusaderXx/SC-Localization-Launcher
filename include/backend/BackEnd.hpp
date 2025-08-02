#pragma once

#include <nlohmann/json.hpp>
#include <thread>

#include "backend/DirectoryFinder.hpp"
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
    Database::DataBase database;
    ChannelsJsonParser channelsJsonParser;
    SettingsJsonParser settingsJsonParser;
    UpdateHandler updateHandler;
    DirectoryFinder dirFinder;

    using Callback = std::function<void(const nlohmann::json&)>;
    std::map<std::string, Callback> guiCallbacks;

    void updateControlsByDataBase ()
    {
        for(auto& [channel, data] : database.channelData)
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
        , dirFinder(database)
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
        LOG_DEBUG(lg) << "processMassage() - >" << key << "< - msg:\n" << msg.dump(4);

        if(key == "LIVE" || key == "PTU" || key == "EPTU" || key == "HOTFIX" || key == "TECH-PREVIEW")
        {
            if(msg.contains("buttonEngSelected")) database.channelData[key].controls.buttonEng_Selected = msg["buttonEngSelected"];
            if(msg.contains("buttonDeSelected")) database.channelData[key].controls.buttonDe_Selected = msg["buttonDeSelected"];
            if(msg.contains("buttonDeFullSelected")) database.channelData[key].controls.buttonDeFull_Selected = msg["buttonDeFullSelected"];
            if(msg.contains("installPath"))
            {
                database.channelData[key].paths.installPath = std::filesystem::path(msg["installPath"]);
                nlohmann::json responseMsg;
                updateControlsByDataBase();
                responseMsg["installPathIsSet"] = true;
                responseMsg["buttonEngSelected"] = true;
                responseMsg["buttonDeEnabled"] = database.channelData[key].controls.buttonDe_Enabled;
                responseMsg["buttonDeFullEnabled"] = database.channelData[key].controls.buttonDeFull_Enabled;
                guiCallbacks[key](responseMsg);

                nlohmann::json updateButtonMassage;
                updateButtonMassage["buttonEnabled"] = true;
                updateButtonMassage["buttonReady"] = true;
                if(guiCallbacks["UPDATE"]) guiCallbacks["UPDATE"](updateButtonMassage);
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

        if(key == "SETTINGS")
        {
            if(msg.contains("autoTranslationAtStart"))
            {
                database.settings.autoTranslationAtStart = msg["autoTranslationAtStart"];
            }
            if(msg.contains("autoNewTranslation"))
            {
                database.settings.autoNewTranslation = msg["autoNewTranslation"];
            }
            if(msg.contains("minimizeScdAfterUpdate"))
            {
                database.settings.minimizeScdAfterUpdate = msg["minimizeScdAfterUpdate"];
            }
            if(msg.contains("LaunchScAfterTranslation"))
            {
                database.settings.LaunchScAfterTranslation = msg["LaunchScAfterTranslation"];
            }
            if(msg.contains("startScdWithSystemStart"))
            {
                database.settings.startScdWithSystemStart = msg["startScdWithSystemStart"];
            }
            if(msg.contains("showUpdateStatus"))
            {
                database.settings.showUpdateStatus = msg["showUpdateStatus"];
            }
            if(msg.contains("autoSearch"))
            {
                if(msg["autoSearch"] == "start")
                {
                    nlohmann::json re;
                    re["autoSearch"] = "running";
                    guiCallbacks[key](re);

                    std::thread(
                        [this] ()
                        {
                            dirFinder.findRsiInstallation();
                            nlohmann::json finMsg;
                            finMsg["autoSearch"] = "finished";
                            processMassage("SETTINGS", finMsg);
                        })
                        .detach();
                }
                if(msg["autoSearch"] == "finished")
                {
                    updateControlsByDataBase();
                    initGui();
                    nlohmann::json re;
                    re["autoSearch"] = "ready";
                    guiCallbacks[key](re);
                }
            }
        }
    }

    void initGui ()
    {
        LOG_DEBUG(lg) << "initGui()";
        bool readyForUpdate = false;

        nlohmann::json updateButtonInit;
        nlohmann::json updateSettingsInit;

        for(const auto& [channel, data] : database.channelData)
        {
            nlohmann::json channelsInit;

            if(database.channelData[channel].controls.installPathIsSet == true)
            {
                readyForUpdate = true;
                channelsInit["buttonEngEnabled"] = database.channelData[channel].controls.buttonDe_Enabled;
                channelsInit["buttonDeEnabled"] = database.channelData[channel].controls.buttonDe_Enabled;
                channelsInit["buttonDeFullEnabled"] = database.channelData[channel].controls.buttonDeFull_Enabled;
                channelsInit["buttonEngSelected"] = database.channelData[channel].controls.buttonEng_Selected;
                channelsInit["buttonDeSelected"] = database.channelData[channel].controls.buttonDe_Selected;
                channelsInit["buttonDeFullSelected"] = database.channelData[channel].controls.buttonDeFull_Selected;
                updateSettingsInit[channel] = database.channelData[channel].paths.installPath;
            }
            else
            {
                channelsInit["buttonEngEnabled"] = database.channelData[channel].controls.buttonEng_Enabled;
            }
            channelsInit["installPathIsSet"] = database.channelData[channel].controls.installPathIsSet;

            if(guiCallbacks[channel]) guiCallbacks[channel](channelsInit);
        }

        if(readyForUpdate)
        {
            updateButtonInit["buttonEnabled"] = true;
            updateButtonInit["buttonReady"] = true;
            if(guiCallbacks["UPDATE"]) guiCallbacks["UPDATE"](updateButtonInit);
        }

        updateSettingsInit["autoTranslationAtStart"] = database.settings.autoTranslationAtStart;
        updateSettingsInit["autoNewTranslation"] = database.settings.autoNewTranslation;
        updateSettingsInit["minimizeScdAfterUpdate"] = database.settings.minimizeScdAfterUpdate;
        updateSettingsInit["LaunchScAfterTranslation"] = database.settings.LaunchScAfterTranslation;
        updateSettingsInit["startScdWithSystemStart"] = database.settings.startScdWithSystemStart;
        updateSettingsInit["showUpdateStatus"] = database.settings.showUpdateStatus;
        updateSettingsInit["autoSearch"] = "ready";
        updateSettingsInit["rsiLauncherInstallPath"] = database.settings.rsiLauncherInstallPath;
        if(guiCallbacks["SETTINGS"]) guiCallbacks["SETTINGS"](updateSettingsInit);
    }
};
