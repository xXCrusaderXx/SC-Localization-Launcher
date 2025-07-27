#pragma once

#include <curl/curl.h>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "backend/database/DataBase.hpp"
#include "logging/logging.h"

class ChannelsJsonParser
{
   private:
    LoggerFramework::LogEx lg;
    CURL* curl = curl_easy_init();
    std::string jsonData;

    nlohmann::json newJson;
    nlohmann::json oldJson;

    static size_t WriteCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        std::string* s = (std::string*) userp;
        size_t totalSize = size * nmemb;
        s->append((char*) contents, totalSize);
        return totalSize;
    }

    void loadJsonFromWeb ()
    {
        if(curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, "https://www.sc-deutsch-launcher.de/inc/channels.json");
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &jsonData);

            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK)
            {
                LOG_DEBUG(lg) << "loadJsonFromWeb() - Fehler beim HTTP-Request: " << curl_easy_strerror(res);
                throw std::runtime_error("Fehler beim HTTP-Request");
            }

            curl_easy_cleanup(curl);

            try
            {
                newJson = nlohmann::json::parse(jsonData);
                LOG_DEBUG(lg) << "JSON:\n" << newJson.dump(4);
            }
            catch(std::exception& e)
            {
                LOG_DEBUG(lg) << "loadJsonFromWeb() - Fehler beim Parsen der JSON-Daten: " << e.what();
                throw std::runtime_error("Fehler beim Parsen der JSON-Daten:");
            }
        }
    }

   public:
    ChannelsJsonParser ()
        : lg("ChannelsJsonParser")
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~ChannelsJsonParser () { LOG_DEBUG(lg) << "destructed"; };

    void getChannels (Database::Channels& channels)
    {
        loadJsonFromWeb();
        for(const auto& [key, value] : newJson.at("channels").items())
        {
            Database::Info info;
            info.description = value.at("description");
            info.folder = value.at("folder");
            info.active = value.at("active");
            info.server1 = value.at("server1");
            info.server2 = value.at("server2");
            info.server1_fallback = value.at("server1_fallback");
            info.server2_fallback = value.at("server2_fallback");

            channels[info.description].info = info;
        }
    }

    bool compare (const nlohmann::json& oldJson, const nlohmann::json& newJson) { return true; }
};
