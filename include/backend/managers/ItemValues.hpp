#pragma once

#include <curl/curl.h>
#include <QCryptographicHash>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "backend/jsonParser/LanguageCatalogParser.hpp"
#include "backend/managers/WikiApiClient.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"

// Adds values from the Star Citizen Wiki API (CC BY-SA) to item names in a downloaded global.ini, e.g.
// "Holdstrong (S4 Military A)". It works by key, so it fits the global.ini of every language: which key belongs to
// which Wiki item comes from item-mapping.json, built weekly from the English texts (tools/build-item-mapping.py).
// Building a plan downloads data and blocks, so it runs in a background job.
class ItemValues
{
   public:
    // What an update adds: global.ini key -> text appended to that name in parentheses
    struct Plan
    {
        std::map<std::string, std::string> nameSuffixes;
        // Changes whenever the result would change; stored with the installed file, which is downloaded again
        // instead of reused when it differs, so a file is never edited twice
        std::string signature;
    };

    explicit ItemValues (const std::atomic<bool>& shuttingDown, std::filesystem::path cacheDir = PATHS::CACHE_USER)
        : lg("ItemValues")
        , shuttingDown(shuttingDown)
        , cacheDir(std::move(cacheDir))
        , wiki(shuttingDown, this->cacheDir / "wiki")
    {
    }

    // Nothing when neither the mapping nor the Wiki data can be loaded or is cached; the translation is then
    // installed without values. language: catalogue code; German codes get German class names.
    std::optional<Plan> build (const std::string& language)
    {
        auto mapping = loadMapping();
        if(!mapping) return std::nullopt;

        std::map<std::string, nlohmann::json> items;
        for(const auto& type : SIZED_TYPES) addItems(items, type);
        for(const auto& type : CLASSED_TYPES) addItems(items, type);
        if(shuttingDown || items.empty()) return std::nullopt;

        bool german = language.starts_with("de");
        Plan plan;
        for(const auto& [key, uuid] : mapping->items())
        {
            if(!isNameKey(key) || !uuid.is_string()) continue;
            auto item = items.find(uuid.get<std::string>());
            if(item == items.end()) continue;
            if(auto suffix = nameSuffix(item->second, german); !suffix.empty()) plan.nameSuffixes[key] = suffix;
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        for(const auto& [key, suffix] : plan.nameSuffixes) hash.addData(QByteArray::fromStdString(key + "=" + suffix + "\n"));
        plan.signature = "names:" + hash.result().toHex().left(16).toStdString();
        LOG_INFO(lg) << "build() - " << plan.nameSuffixes.size() << " names for " << language;
        return plan;
    }

    // Appends the plan's values to a freshly downloaded global.ini; written next to it and moved over it
    static bool apply (const std::filesystem::path& file, const Plan& plan)
    {
        std::ifstream in(file, std::ios::binary);
        if(!in) return false;
        std::string out;
        std::string line;
        while(std::getline(in, line))
        {
            bool crlf = !line.empty() && line.back() == '\r';
            if(crlf) line.pop_back();
            auto equals = line.find('=');
            if(equals != std::string::npos)
            {
                // global.ini starts with a byte order mark, which belongs to the first line, not to its key
                std::string key = line.substr(0, equals);
                if(key.starts_with("\xEF\xBB\xBF")) key.erase(0, 3);
                if(auto suffix = plan.nameSuffixes.find(key); suffix != plan.nameSuffixes.end() && equals + 1 < line.size())
                    line += " (" + suffix->second + ")";
            }
            out += line;
            // The last line keeps its ending, also when it has none
            if(!in.eof())
                out += crlf ? "\r\n" : "\n";
            else if(crlf)
                out += "\r";
        }
        in.close();

        auto temporary = file;
        temporary += ".items";
        {
            std::ofstream write(temporary, std::ios::binary | std::ios::trunc);
            write << out;
            if(!write) return false;
        }
        std::error_code ec;
        std::filesystem::rename(temporary, file, ec);
        return !ec;
    }

   private:
    inline static const std::string MAPPING_URL = "https://raw.githubusercontent.com/xXCrusaderXx/SC-Localization-Launcher/main/item-mapping.json";
    // Size only: grade and class say nothing for these
    inline static const std::vector<std::string> SIZED_TYPES{"WeaponGun", "WeaponPersonal", "Missile", "MissileLauncher", "Turret"};
    // Size, class and grade
    inline static const std::vector<std::string> CLASSED_TYPES{"Shield", "PowerPlant", "Cooler", "QuantumDrive", "Radar"};
    inline static const std::map<std::string, std::string> GERMAN_CLASSES{
        {"Civilian", "Zivil"}, {"Industrial", "Industrie"}, {"Military", "Militär"}, {"Stealth", "Tarnung"}, {"Competition", "Wettkampf"}};

    LoggerFramework::LogEx lg;
    const std::atomic<bool>& shuttingDown;
    std::filesystem::path cacheDir;
    WikiApiClient wiki;

    static std::string mappingUrl ()
    {
#ifdef SC_TESTING
        // Test builds only: the tests serve the mapping from a local server
        if(const char* url = std::getenv("SC_ITEM_MAPPING_URL")) return url;
#endif
        return MAPPING_URL;
    }

    static bool isNameKey (const std::string& key)
    {
        std::string lower = key;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower.find("name") != std::string::npos && lower.find("desc") == std::string::npos;
    }

    void addItems (std::map<std::string, nlohmann::json>& items, const std::string& type)
    {
        if(shuttingDown) return;
        auto reduce = [] (const nlohmann::json& item)
        {
            nlohmann::json kept;
            for(const char* field : {"uuid", "type", "size", "grade", "class"}) kept[field] = item.value(field, nlohmann::json());
            return kept;
        };
        auto result = wiki.fetchAll("items", {{"filter[type]", type}}, reduce);
        for(const auto& item : result.entries)
            if(item["uuid"].is_string()) items[item["uuid"].get<std::string>()] = item;
    }

    static std::string nameSuffix (const nlohmann::json& item, bool german)
    {
        std::string type = item.value("type", "");
        if(!item["size"].is_number_integer()) return "";
        std::string suffix = "S" + std::to_string(item["size"].get<int>());
        if(std::find(CLASSED_TYPES.begin(), CLASSED_TYPES.end(), type) == CLASSED_TYPES.end()) return suffix;
        if(item["class"].is_string())
        {
            std::string itemClass = item["class"].get<std::string>();
            auto translated = GERMAN_CLASSES.find(itemClass);
            suffix += " " + (german && translated != GERMAN_CLASSES.end() ? translated->second : itemClass);
        }
        if(item["grade"].is_string()) suffix += " " + item["grade"].get<std::string>();
        return suffix;
    }

    static size_t writeCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    // item-mapping.json from the repository, kept for a day; the cached copy is used when GitHub cannot be reached
    std::optional<nlohmann::json> loadMapping ()
    {
        auto file = cacheDir / "item-mapping.json";
        std::error_code ec;
        bool fresh = std::filesystem::exists(file, ec) &&
                     std::filesystem::last_write_time(file, ec) > std::filesystem::file_time_type::clock::now() - std::chrono::hours(24);
        if(!fresh)
        {
            if(auto data = download(mappingUrl()))
            {
                auto json = nlohmann::json::parse(*data, nullptr, false);
                if(!json.is_discarded() && json.contains("keys") && json["keys"].is_object())
                {
                    std::filesystem::create_directories(cacheDir, ec);
                    auto temporary = file;
                    temporary += ".download";
                    std::ofstream(temporary) << *data;
                    std::filesystem::rename(temporary, file, ec);
                    return json["keys"];
                }
                LOG_ERROR(lg) << "loadMapping() - unexpected content";
            }
        }
        std::ifstream in(file);
        if(!in) return std::nullopt;
        auto json = nlohmann::json::parse(in, nullptr, false);
        if(json.is_discarded() || !json.contains("keys") || !json["keys"].is_object()) return std::nullopt;
        return json["keys"];
    }

    std::optional<std::string> download (const std::string& url)
    {
        if(!LanguageCatalogParser::isAllowedUrl(url)) return std::nullopt;
        CURL* curl = curl_easy_init();
        if(!curl) return std::nullopt;
        std::string data;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        CURLcode code = curl_easy_perform(curl);
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_cleanup(curl);
        if(code != CURLE_OK || status != 200)
        {
            LOG_ERROR(lg) << "download() - " << url << " failed: " << curl_easy_strerror(code) << " (HTTP " << status << ")";
            return std::nullopt;
        }
        return data;
    }
};
