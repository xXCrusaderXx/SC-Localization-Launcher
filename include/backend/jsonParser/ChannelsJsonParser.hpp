#pragma once

#include <curl/curl.h>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

#include "backend/database/DataBase.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"

class ChannelsJsonParser
{
   private:
    LoggerFramework::LogEx lg;
    nlohmann::json newJson;

    inline static const std::string CHANNELS_URL = "https://www.sc-deutsch-launcher.de/inc/channels.json";

    // Some server channel names differ from the installation folder names
    inline static const std::map<std::string, std::string> CHANNEL_ALIASES{{"TECH-PREV", "TECH-PREVIEW"}};

    static size_t WriteCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        std::string* s = (std::string*) userp;
        size_t totalSize = size * nmemb;
        s->append((char*) contents, totalSize);
        return totalSize;
    }

    static std::string normalizeChannelName (const std::string& name)
    {
        auto it = CHANNEL_ALIASES.find(name);
        return it != CHANNEL_ALIASES.end() ? it->second : name;
    }

    bool loadJsonFromWeb ()
    {
        CURL* curl = curl_easy_init();
        if(!curl) return false;

        std::string jsonData;
        curl_easy_setopt(curl, CURLOPT_URL, CHANNELS_URL.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &jsonData);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);

        if(res != CURLE_OK || httpCode != 200)
        {
            LOG_ERROR(lg) << "loadJsonFromWeb() - HTTP request failed: " << curl_easy_strerror(res) << " (HTTP " << httpCode << ")";
            return false;
        }

        try
        {
            newJson = nlohmann::json::parse(jsonData);
            LOG_DEBUG(lg) << "JSON:\n" << newJson.dump(4);
        }
        catch(std::exception& e)
        {
            LOG_ERROR(lg) << "loadJsonFromWeb() - failed to parse JSON: " << e.what();
            return false;
        }

        saveCache();
        return true;
    }

    std::filesystem::path cacheFile () const { return PATHS::ROOT_USER / "local" / "channels.json"; }

    void saveCache ()
    {
        try
        {
            std::filesystem::create_directories(cacheFile().parent_path());
            std::ofstream file(cacheFile());
            file << newJson.dump(4);
        }
        catch(const std::exception& e)
        {
            LOG_ERROR(lg) << "saveCache() - " << e.what();
        }
    }

    bool loadCache ()
    {
        std::ifstream file(cacheFile());
        if(!file.is_open()) return false;

        try
        {
            file >> newJson;
            LOG_DEBUG(lg) << "loadCache() - using cached channels.json";
            return true;
        }
        catch(const std::exception& e)
        {
            LOG_ERROR(lg) << "loadCache() - " << e.what();
            return false;
        }
    }

    void readChannel (const nlohmann::json& channel, Database::BackendData& backendData)
    {
        std::string channelName = normalizeChannelName(channel.value("description", ""));
        if(channelName.empty()) return;

        auto& info = backendData.channelData[channelName].info;
        info.description = channelName;
        info.folder = channel.value("folder", "");
        info.active = channel.value("active", 0);
        info.server1 = channel.value("server1", "");
        info.server2 = channel.value("server2", "");
        info.server3 = channel.value("server3", "");
        info.server4 = channel.value("server4", "");
        info.server1_fallback = channel.value("server1_fallback", "");
        info.server2_fallback = channel.value("server2_fallback", "");
        info.server3_fallback = channel.value("server3_fallback", "");
        info.server4_fallback = channel.value("server4_fallback", "");
    }

   public:
    ChannelsJsonParser ()
        : lg("ChannelsJsonParser")
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~ChannelsJsonParser () { LOG_DEBUG(lg) << "destructed"; };

    // Returns false if neither the server nor the cache is available. The app stays usable,
    // only translations cannot be downloaded.
    bool getChannels (Database::BackendData& backendData)
    {
        if(!loadJsonFromWeb() && !loadCache())
        {
            LOG_ERROR(lg) << "getChannels() - no channel information available";
            return false;
        }

        if(!newJson.contains("channels") || !newJson.at("channels").is_array())
        {
            LOG_ERROR(lg) << "getChannels() - channels not found in JSON";
            return false;
        }

        LOG_DEBUG(lg) << "getChannels() - channels found, processing";
        for(const auto& channel : newJson.at("channels")) readChannel(channel, backendData);

        // Channels the server currently does not offer (e.g. EPTU) are listed under "backup"
        if(newJson.contains("backup") && newJson.at("backup").is_object())
        {
            for(const auto& [key, channel] : newJson.at("backup").items())
            {
                std::string channelName = normalizeChannelName(channel.value("description", key));
                if(!backendData.channelData[channelName].info.description.empty()) continue;
                readChannel(channel, backendData);
            }
        }
        return true;
    }
};
