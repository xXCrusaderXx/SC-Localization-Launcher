#pragma once

#include <curl/curl.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "BundledLanguages.hpp"
#include "backend/database/DataBase.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"

// Loads languages.json and the channel lists it refers to, and works out which language
// is available on which channel. Works only on its own data, so it can run on any thread.
class LanguageCatalogParser
{
   private:
    mutable LoggerFramework::LogEx lg;

    inline static const std::string LANGUAGES_URL = "https://raw.githubusercontent.com/xXCrusaderXx/SC-Localization-Launcher/main/languages.json";

    // Some server channel names differ from the installation folder names
    inline static const std::map<std::string, std::string> CHANNEL_ALIASES{{"TECH-PREV", "TECH-PREVIEW"}};

    static std::string languagesUrl ()
    {
#ifdef SC_TESTING
        // Test builds only: the tests serve languages.json from a local server
        if(const char* url = std::getenv("SC_LANGUAGES_URL")) return url;
#endif
        return LANGUAGES_URL;
    }

    static std::string normalizeChannelName (const std::string& name)
    {
        auto it = CHANNEL_ALIASES.find(name);
        return it != CHANNEL_ALIASES.end() ? it->second : name;
    }

    static size_t writeCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    std::optional<std::string> download (const std::string& url) const
    {
        CURL* curl = curl_easy_init();
        if(!curl) return std::nullopt;

        std::string data;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);

        if(res != CURLE_OK || httpCode != 200)
        {
            LOG_ERROR(lg) << "download() - " << url << " failed: " << curl_easy_strerror(res) << " (HTTP " << httpCode << ")";
            return std::nullopt;
        }
        return data;
    }

    static std::filesystem::path cacheFile (const std::string& url)
    {
        std::ostringstream name;
        name << std::hex << std::hash<std::string>{}(url) << ".json";
        return PATHS::CACHE_USER / name.str();
    }

    // Downloads a JSON file and caches it; falls back to the cache when offline
    std::optional<nlohmann::json> loadJson (const std::string& url) const
    {
        if(auto data = download(url))
        {
            auto json = nlohmann::json::parse(*data, nullptr, false);
            if(!json.is_discarded())
            {
                std::error_code ec;
                std::filesystem::create_directories(PATHS::CACHE_USER, ec);
                std::ofstream(cacheFile(url)) << *data;
                return json;
            }
            LOG_ERROR(lg) << "loadJson() - " << url << " is not valid JSON";
        }

        std::ifstream cache(cacheFile(url));
        if(!cache.is_open()) return std::nullopt;
        auto json = nlohmann::json::parse(cache, nullptr, false);
        if(json.is_discarded()) return std::nullopt;
        LOG_DEBUG(lg) << "loadJson() - using cached " << url;
        return json;
    }

    Database::LanguageCatalog loadCatalog () const
    {
        if(auto json = loadJson(languagesUrl()))
        {
            try
            {
                return json->get<Database::LanguageCatalog>();
            }
            catch(const std::exception& e)
            {
                LOG_ERROR(lg) << "loadCatalog() - unexpected languages.json format: " << e.what();
            }
        }
        LOG_DEBUG(lg) << "loadCatalog() - using the built-in languages.json";
        return nlohmann::json::parse(BUNDLED_LANGUAGES_JSON).get<Database::LanguageCatalog>();
    }

    bool isValid (const Database::LanguageDefinition& language, const std::vector<Database::AvailableLanguage>& accepted) const
    {
        auto reject = [&] (const std::string& reason)
        {
            LOG_ERROR(lg) << "language '" << language.code << "' ignored: " << reason;
            return false;
        };
        if(language.code.empty() || language.code == Protocol::ENGLISH) return reject("invalid code");
        if(std::any_of(accepted.begin(), accepted.end(), [&] (const auto& l) { return l.code == language.code; })) return reject("duplicate code");
        if(language.name.empty()) return reject("missing name");
        if(!isSafeFolderName(language.folder)) return reject("invalid folder name");
        if(language.source.channelList.empty() == language.source.channels.empty()) return reject("needs either channelList or channels");
        if(!language.source.channelList.empty() && (!isAllowedUrl(language.source.channelList) || language.source.server.empty()))
            return reject("invalid channelList");
        // Translations are only offered with a visible credit and license
        const auto& credit = language.credit;
        if(credit.project.empty() || credit.authors.empty() || credit.license.empty()) return reject("incomplete credit");
        if(!isAllowedUrl(credit.url) || (!credit.licenseUrl.empty() && !isAllowedUrl(credit.licenseUrl))) return reject("invalid credit URL");
        return true;
    }

    // Channel name -> entry of a channels.json in the sc-deutsch-launcher.de format
    std::map<std::string, nlohmann::json> readChannelList (const nlohmann::json& json) const
    {
        std::map<std::string, nlohmann::json> channels;
        auto add = [&] (const nlohmann::json& channel, const std::string& fallbackName)
        {
            if(!channel.is_object()) return;
            std::string name = normalizeChannelName(channel.value("description", fallbackName));
            if(!name.empty() && !channels.contains(name)) channels[name] = channel;
        };

        if(json.contains("channels") && json.at("channels").is_array())
            for(const auto& channel : json.at("channels")) add(channel, "");
        // Channels the server currently does not offer (e.g. EPTU) are listed under "backup"
        if(json.contains("backup") && json.at("backup").is_object())
            for(const auto& [key, channel] : json.at("backup").items()) add(channel, key);
        return channels;
    }

    // Notices come from the internet and are shown in the UI: plain text, one line, short
    static std::string cleanNotice (std::string text)
    {
        for(char& c : text)
            if(c == '\n' || c == '\r' || c == '\t') c = ' ';
        text.erase(0, text.find_first_not_of(' '));
        text.erase(text.find_last_not_of(' ') + 1);
        constexpr size_t maxLength = 120;
        if(text.size() > maxLength)
        {
            text.resize(maxLength);
            // Do not cut a UTF-8 character in half
            while(!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) text.pop_back();
            if(!text.empty() && (static_cast<unsigned char>(text.back()) & 0x80)) text.pop_back();
            text += "…";
        }
        return text;
    }

    std::vector<std::string> allowedUrls (const std::vector<std::string>& urls) const
    {
        std::vector<std::string> result;
        for(const auto& url : urls)
        {
            if(url.empty()) continue;
            if(isAllowedUrl(url))
                result.push_back(url);
            else
                LOG_ERROR(lg) << "URL ignored, only HTTPS is allowed: " << url;
        }
        return result;
    }

   public:
    // Also used for the sources users enter themselves
    static bool isAllowedUrl (const std::string& url)
    {
#ifdef SC_TESTING
        if(url.starts_with("http://127.0.0.1:")) return true;
#endif
        return url.starts_with("https://");
    }

    // The folder becomes a path component inside the game directory, which is written and deleted
    static bool isSafeFolderName (const std::string& folder)
    {
        static const std::regex allowed("[A-Za-z0-9_()-]+");
        return std::regex_match(folder, allowed);
    }

    LanguageCatalogParser ()
        : lg("LanguageCatalogParser")
    {
        LOG_DEBUG(lg) << "instanziated";
    }
    ~LanguageCatalogParser () { LOG_DEBUG(lg) << "destructed"; }

    // Blocks while downloading. Never fails: without network it uses the cache or the built-in list.
    std::vector<Database::AvailableLanguage> getLanguages () const
    {
        std::vector<Database::AvailableLanguage> languages;
        std::map<std::string, std::map<std::string, nlohmann::json>> channelLists;

        for(const auto& definition : loadCatalog().languages)
        {
            if(!isValid(definition, languages)) continue;
            Database::AvailableLanguage language{definition.code, definition.name, definition.folder, {}, definition.credit, {}};
            for(const auto& [channel, notice] : definition.notices)
                if(auto text = cleanNotice(notice); !text.empty()) language.notices[channel == "*" ? channel : normalizeChannelName(channel)] = text;

            if(!definition.source.channelList.empty())
            {
                const auto& url = definition.source.channelList;
                if(!channelLists.contains(url)) channelLists[url] = readChannelList(loadJson(url).value_or(nlohmann::json::object()));

                const std::string& server = definition.source.server;
                for(const auto& [channel, info] : channelLists.at(url))
                {
                    if(info.value("active", 0) == 0) continue;
                    auto urls = allowedUrls({info.value(server, ""), info.value(server + "_fallback", "")});
                    if(!urls.empty()) language.urls[channel] = urls;
                }
            }
            else
            {
                for(const auto& [channel, urls] : definition.source.channels)
                    if(auto allowed = allowedUrls(urls); !allowed.empty()) language.urls[normalizeChannelName(channel)] = allowed;
                if(language.urls.empty())
                {
                    LOG_ERROR(lg) << "language '" << definition.code << "' ignored: no usable URL";
                    continue;
                }
            }
            languages.push_back(std::move(language));
        }
        return languages;
    }
};
