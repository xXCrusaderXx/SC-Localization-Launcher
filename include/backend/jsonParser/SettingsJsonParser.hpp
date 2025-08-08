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
        if(!std::filesystem::exists(filePath))
        {
            if(!std::filesystem::exists(PATHS::JSON_FILES::SETTINGS))
            {
                LOG_DEBUG(lg) << "loadJsonFile () - " << filePath << " existiert nicht";
                throw std::runtime_error("settings.json Datei existiert nicht");
            }
            else
            {
                filePath = PATHS::JSON_FILES::SETTINGS;
            }
        }
        std::ifstream file(filePath);
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

    void getChannelSettings (Database::BackendData& backendData)
    {
        loadJsonFile();

        if(json.empty())
        {
            LOG_DEBUG(lg) << "getChannelSettings () - settings.json is empty";
            throw std::runtime_error("settings.json is empty");
        }

        getChannels(backendData);
        getSettings(backendData);
    }

    void getChannels (Database::BackendData& backendData)
    {
        if(!json.contains("channels"))
        {
            LOG_DEBUG(lg) << "getChannelSettings () - missing node: channels";
            throw std::runtime_error("missing node: channels");
        }
        if(!json.at("channels").is_object())
        {
            LOG_DEBUG(lg) << "getChannelSettings () - channels is not an object";
            throw std::runtime_error("channels is not an object");
        }
        else
        {
            LOG_DEBUG(lg) << "getChannelSettings () - channels is an object";

            for(const auto& [key, channel] : json.at("channels").items())
            {
                validateChannelNode(channel);

                if(channel.at("selectedTrans").get<std::string>() == "eng" || channel.at("selectedTrans").get<std::string>() == "")
                {
                    backendData.channelData[key].buttonEng.selected = true;
                }
                else if(channel.at("selectedTrans").get<std::string>() == "de")
                {
                    backendData.channelData[key].buttonDe.selected = true;
                }
                else if(channel.at("selectedTrans").get<std::string>() == "deVoll")
                {
                    backendData.channelData[key].buttonDeFull.selected = true;
                }
                backendData.channelData[key].installPath = std::filesystem::path(channel.at("installPath").get<std::string>());
            }
        }
    }

    void getSettings (Database::BackendData& backendData)
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

        backendData.settings.checkboxes.autoTranslationAtStart = json["settings"]["autoTranslationAtStart"];
        backendData.settings.checkboxes.autoNewTranslation = json["settings"]["autoNewTranslation"];
        backendData.settings.checkboxes.minimizeScdAfterUpdate = json["settings"]["minimizeScdAfterUpdate"];
        backendData.settings.checkboxes.LaunchScAfterTranslation = json["settings"]["LaunchScAfterTranslation"];
        backendData.settings.checkboxes.startScdWithSystemStart = json["settings"]["startScdWithSystemStart"];
        backendData.settings.checkboxes.showUpdateStatus = json["settings"]["showUpdateStatus"];
        backendData.rsiLauncherInstallPath = std::filesystem::path(json["settings"]["rsiLauncherInstallPath"]);
    }

    void saveChannelSettings (const Database::BackendData& backendData)
    {
        nlohmann::json json;

        for(const auto& [key, channelData] : backendData.channelData)
        {
            nlohmann::json channelJson;
            channelJson["installPath"] = channelData.installPath.string();
            if(channelData.buttonEng.selected)
                channelJson["selectedTrans"] = "eng";
            else if(channelData.buttonDe.selected)
                channelJson["selectedTrans"] = "de";
            else if(channelData.buttonDeFull.selected)
                channelJson["selectedTrans"] = "deVoll";
            else
                channelJson["selectedTrans"] = "";

            json["channels"][key] = channelJson;
        }

        json["settings"]["autoTranslationAtStart"] = backendData.settings.checkboxes.autoTranslationAtStart;
        json["settings"]["autoNewTranslation"] = backendData.settings.checkboxes.autoNewTranslation;
        json["settings"]["minimizeScdAfterUpdate"] = backendData.settings.checkboxes.minimizeScdAfterUpdate;
        json["settings"]["LaunchScAfterTranslation"] = backendData.settings.checkboxes.LaunchScAfterTranslation;
        json["settings"]["startScdWithSystemStart"] = backendData.settings.checkboxes.startScdWithSystemStart;
        json["settings"]["showUpdateStatus"] = backendData.settings.checkboxes.showUpdateStatus;
        json["settings"]["rsiLauncherInstallPath"] = backendData.rsiLauncherInstallPath;

        if(!std::filesystem::exists(PATHS::JSON_FILES::SETTINGS_USER))
        {
            try
            {
                LOG_DEBUG(lg) << "Erstelle Settings.Json in USER Dir: " << PATHS::JSON_FILES::SETTINGS_USER;
                std::filesystem::create_directory(PATHS::JSON_FILES::SETTINGS_USER.parent_path());
            }
            catch(const std::filesystem::filesystem_error& e)
            {
                throw std::runtime_error(std::string("Fehler beim Erstellen des Verzeichnisses: ") + e.what());
            }
        }

        LOG_DEBUG(lg) << "SETTINGSFILE:" << json.dump(4);

        std::ofstream file(PATHS::JSON_FILES::SETTINGS_USER);
        if(file.is_open())
        {
            file << std::setw(4) << json << std::endl;
            file.close();
        }
        else
        {
            LOG_DEBUG(lg) << "saveChannelSettings () - Fehler beim Öffnen von: " << filePath;
        }

        LOG_DEBUG(lg) << "saveChannelSettings () - settings.json filePath: " << filePath;
    }
};
