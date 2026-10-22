#pragma once

#include <curl/curl.h>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>

#include "backend/database/DataBase.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"

class ChannelsJsonParser
{
   public:
    using Channels = std::map<std::string, Database::WebInfo>;

   private:
    LoggerFramework::LogEx lg;
    nlohmann::json newJson;

    inline static const std::string CHANNELS_URL = "https://www.sc-deutsch-launcher.de/inc/channels.json";

    static std::string channelsUrl ()
    {
#ifdef SC_TESTING
        // Test builds only: the tests serve channels.json from a local server
        if(const char* url = std::getenv("SC_CHANNELS_URL")) return url;
#endif
        return CHANNELS_URL;
    }

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
        std::string url = channelsUrl();
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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

    static void readChannel (const nlohmann::json& channel, const std::string& fallbackName, Channels& channels)
    {
        auto info = channel.get<Database::WebInfo>();
        std::string channelName = normalizeChannelName(info.description.empty() ? fallbackName : info.description);
        if(channelName.empty() || channels.contains(channelName)) return;

        info.description = channelName;
        channels[channelName] = info;
    }

   public:
    ChannelsJsonParser ()
        : lg("ChannelsJsonParser")
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~ChannelsJsonParser () { LOG_DEBUG(lg) << "destructed"; };

    // Returns nullopt if neither the server nor the cache is available. The app stays usable,
    // only translations cannot be downloaded.
    std::optional<Channels> getChannels ()
    {
        if(!loadJsonFromWeb() && !loadCache())
        {
            LOG_ERROR(lg) << "getChannels() - no channel information available";
            return std::nullopt;
        }

        if(!newJson.contains("channels") || !newJson.at("channels").is_array())
        {
            LOG_ERROR(lg) << "getChannels() - channels not found in JSON";
            return std::nullopt;
        }

        LOG_DEBUG(lg) << "getChannels() - channels found, processing";
        Channels channels;
        try
        {
            for(const auto& channel : newJson.at("channels")) readChannel(channel, "", channels);

            // Channels the server currently does not offer (e.g. EPTU) are listed under "backup"
            if(newJson.contains("backup") && newJson.at("backup").is_object())
                for(const auto& [key, channel] : newJson.at("backup").items()) readChannel(channel, key, channels);
        }
        catch(const std::exception& e)
        {
            LOG_ERROR(lg) << "getChannels() - unexpected channels.json format: " << e.what();
            return std::nullopt;
        }
        return channels;
    }
};
