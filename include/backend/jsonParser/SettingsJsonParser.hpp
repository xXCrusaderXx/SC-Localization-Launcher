#pragma once

#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "backend/database/DataBase.hpp"
#include "logging/logging.h"

class SettingsJsonParser
{
   private:
    LoggerFramework::LogEx lg;
    nlohmann::json json;
    std::filesystem::path filePath;

    void validateChannelNode (nlohmann::json node)
    {
        if(!node.contains("installPath")) throw std::runtime_error("missing node: installPath");
        if(!node.contains("selectedTrans")) throw std::runtime_error("missing node: selectedTrans");
    }

   public:
    SettingsJsonParser (std::filesystem::path filePath)
        : filePath(filePath)
        , lg("SettingsJsonParser")
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~SettingsJsonParser () { LOG_DEBUG(lg) << "destructed"; };

    bool loadJsonFile ()
    {
        std::ifstream file(filePath);
        if(!std::filesystem::exists(filePath)) throw std::runtime_error("Datei existiert nicht");
        LOG_DEBUG(lg) << "loadJsonFile () - settings.json filePath: " << filePath;
        if(!file.is_open())
        {
            LOG_DEBUG(lg) << "loadJsonFile () - Datei konnte nicht geöffnet werden.";
            throw std::runtime_error("Datei konnte nicht geöffnet werden.\n");
        }

        try
        {
            file >> json;
        }
        catch(const nlohmann::json::parse_error& e)
        {
            LOG_DEBUG(lg) << "loadJsonFile () - Fehler beim Parsen : " << e.what();
            throw std::runtime_error(std::string("Fehler beim Parsen: ") + e.what());
        }
        return true;
    }

    void getChannelSettings (Database::DataBase& database)
    {
        loadJsonFile();

        if(json.empty())
        {
            LOG_DEBUG(lg) << "getChannelSettings () - settings.json is empty";
            throw std::runtime_error("settings.json is empty");
        }

        getChannels(database);
        getSettings(database);
    }

    void getChannels (Database::DataBase& database)
    {
        if(!json.contains("channels"))
        {
            LOG_DEBUG(lg) << "getChannelSettings () - missing node: channels";
            throw std::runtime_error("missing node: channels");
        }
        for(const auto& [key, value] : json.at("channels").items())
        {
            Database::Controls controls;

            if(!value.contains("selectedTrans"))
            {
                LOG_DEBUG(lg) << "getChannelSettings () - missing node: selectedTrans";
                throw std::runtime_error("missing node: selectedTrans");
            }
            if(value.at("selectedTrans") == "eng" || value.at("selectedTrans") == "") controls.buttonEng_Selected = true;
            if(value.at("selectedTrans") == "de") controls.buttonDe_Selected = true;
            if(value.at("selectedTrans") == "deVoll") controls.buttonDeFull_Selected = true;

            if(!value.contains("installPath"))
            {
                LOG_DEBUG(lg) << "getChannelSettings () - missing node: installPath";
                throw std::runtime_error("missing node: installPath");
            }
            Database::Paths paths;
            paths.installPath = std::filesystem::path(value.at("installPath"));
            database.channelData[key].controls = controls;
            database.channelData[key].paths = paths;
        }
    }

    void getSettings (Database::DataBase& database)
    {
        if(!json.contains("settings"))
        {
            LOG_DEBUG(lg) << "getSettings () - missing node: settings";
            throw std::runtime_error("missing node: settings");
        }

        if(!json.at("settings").contains("autoTranslationAtStart"))
        {
            LOG_DEBUG(lg) << "getSettings () - missing node: autoTranslationAtStart";
            throw std::runtime_error("missing node: autoTranslationAtStart");
        }
        else
        {
            try
            {
                json.at("settings")["autoTranslationAtStart"].get<bool>();
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(e.what());
            }
        }
        if(!json.at("settings").contains("autoNewTranslation"))
        {
            LOG_DEBUG(lg) << "getSettings () - missing node: autoNewTranslation";
            throw std::runtime_error("missing node: autoNewTranslation");
        }
        else
        {
            try
            {
                json.at("settings")["autoNewTranslation"].get<bool>();
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(e.what());
            }
        }
        if(!json.at("settings").contains("minimizeScdAfterUpdate"))
        {
            LOG_DEBUG(lg) << "getSettings () - missing node: minimizeScdAfterUpdate";
            throw std::runtime_error("missing node: minimizeScdAfterUpdate");
        }
        else
        {
            try
            {
                json.at("settings")["minimizeScdAfterUpdate"].get<bool>();
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(e.what());
            }
        }
        if(!json.at("settings").contains("LaunchScAfterTranslation"))
        {
            LOG_DEBUG(lg) << "getSettings () - missing node: LaunchScAfterTranslation";
            throw std::runtime_error("missing node: LaunchScAfterTranslation");
        }
        else
        {
            try
            {
                json.at("settings")["LaunchScAfterTranslation"].get<bool>();
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(e.what());
            }
        }
        if(!json.at("settings").contains("startScdWithSystemStart"))
        {
            LOG_DEBUG(lg) << "getSettings () - missing node: startScdWithSystemStart";
            throw std::runtime_error("missing node: startScdWithSystemStart");
        }
        else
        {
            try
            {
                json.at("settings")["startScdWithSystemStart"].get<bool>();
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(e.what());
            }
        }
        if(!json.at("settings").contains("showUpdateStatus"))
        {
            LOG_DEBUG(lg) << "getSettings () - missing node: showUpdateStatus";
            throw std::runtime_error("missing node: showUpdateStatus");
        }
        else
        {
            try
            {
                json.at("settings")["showUpdateStatus"].get<bool>();
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(e.what());
            }
        }
        if(!json.at("settings").contains("rsiLauncherInstallPath"))
        {
            LOG_DEBUG(lg) << "getSettings () - missing node: rsiLauncherInstallPath";
            throw std::runtime_error("missing node: rsiLauncherInstallPath");
        }
        else
        {
            try
            {
                json.at("settings")["rsiLauncherInstallPath"].get<std::string>();
            }
            catch(const std::exception& e)
            {
                throw std::runtime_error(e.what());
            }
        }

        database.settings.autoTranslationAtStart = json["settings"]["autoTranslationAtStart"];
        database.settings.autoNewTranslation = json["settings"]["autoNewTranslation"];
        database.settings.minimizeScdAfterUpdate = json["settings"]["minimizeScdAfterUpdate"];
        database.settings.LaunchScAfterTranslation = json["settings"]["LaunchScAfterTranslation"];
        database.settings.startScdWithSystemStart = json["settings"]["startScdWithSystemStart"];
        database.settings.showUpdateStatus = json["settings"]["showUpdateStatus"];
        database.settings.rsiLauncherInstallPath = std::filesystem::path(json["settings"]["rsiLauncherInstallPath"]);
    }

    void saveChannelSettings (const Database::DataBase& database)
    {
        nlohmann::json newChannelsJson;

        for(const auto& [key, channel] : database.channelData)
        {
            nlohmann::json value;

            if(channel.controls.buttonEng_Selected)
                value["selectedTrans"] = "eng";
            else if(channel.controls.buttonDe_Selected)
                value["selectedTrans"] = "de";
            else if(channel.controls.buttonDeFull_Selected)
                value["selectedTrans"] = "deVoll";
            else
                value["selectedTrans"] = "";

            value["installPath"] = channel.paths.installPath.string();

            newChannelsJson[key] = value;
        }

        json["channels"] = newChannelsJson;

        json["settings"]["autoTranslationAtStart"] = database.settings.autoTranslationAtStart;
        json["settings"]["autoNewTranslation"] = database.settings.autoNewTranslation;
        json["settings"]["minimizeScdAfterUpdate"] = database.settings.minimizeScdAfterUpdate;
        json["settings"]["LaunchScAfterTranslation"] = database.settings.LaunchScAfterTranslation;
        json["settings"]["startScdWithSystemStart"] = database.settings.startScdWithSystemStart;
        json["settings"]["showUpdateStatus"] = database.settings.showUpdateStatus;
        json["settings"]["rsiLauncherInstallPath"] = database.settings.rsiLauncherInstallPath;

        std::ofstream file(filePath);
        if(file.is_open())
        {
            file << std::setw(4) << json << std::endl;
            file.close();
        }
        else
        {
            LOG_DEBUG(lg) << "saveChannelSettings () - Fehler beim Öffnen der Datei zum Schreiben!";
        }
    }
};
