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

    void getChannelSettings (Database::Channels& channels)
    {
        loadJsonFile();

        if(json.empty())
        {
            LOG_DEBUG(lg) << "getChannelSettings () - settings.json is empty";
            throw std::runtime_error("settings.json is empty");
        }

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
            if(value.at("selectedTrans") == "eng") controls.buttonEng_Selected = true;
            if(value.at("selectedTrans") == "de") controls.buttonDe_Selected = true;
            if(value.at("selectedTrans") == "deVoll") controls.buttonDeFull_Selected = true;

            if(!value.contains("installPath"))
            {
                LOG_DEBUG(lg) << "getChannelSettings () - missing node: installPath";
                throw std::runtime_error("missing node: installPath");
            }
            Database::Paths paths;
            paths.installPath = std::filesystem::path(value.at("installPath"));
            channels[key].controls = controls;
            channels[key].paths = paths;
        }
    }

    void saveChannelSettings (const Database::Channels& channels)
    {
        nlohmann::json newChannelsJson;

        for(const auto& [key, channel] : channels)
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

    bool compare (const nlohmann::json& oldJson, const nlohmann::json& newJson) { return true; }
};
