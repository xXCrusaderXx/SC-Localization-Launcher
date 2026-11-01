#pragma once

#include <curl/curl.h>
#include <QCryptographicHash>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
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

// Adds values from the Star Citizen Wiki API (CC BY-NC-SA 4.0) to a downloaded global.ini: size, class and grade to
// item names, e.g. "Holdstrong (S4 Military A)", the weight class to armour, e.g. "ADP Core (Heavy)", and a line of
// stats in front of item descriptions, e.g.
// "HP 100,000 · Regen 19,000/s". It works by key, so it fits the global.ini of every language: which key belongs to
// which Wiki item comes from item-mapping.json, built weekly from the English texts (tools/build-item-mapping.py).
// Building a plan downloads data and blocks, so it runs in a background job.
class ItemValues
{
   public:
    // What an update adds: global.ini key -> text appended to that name in parentheses, or put as its own line in
    // front of that description
    struct Plan
    {
        std::map<std::string, std::string> nameSuffixes;
        std::map<std::string, std::string> descriptionPrefixes;
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

    // What a plan adds, as named at the start of its signature; empty for nothing
    static std::string kinds (bool names, bool stats) { return names && stats ? "names+stats" : names ? "names" : stats ? "stats" : ""; }
    static std::string kindsOf (const std::string& signature) { return signature.substr(0, signature.find(':')); }

    // Nothing when neither the mapping nor the Wiki data can be loaded or is cached; the translation is then
    // installed without values. language: catalogue code; German codes get German class names and labels.
    std::optional<Plan> build (const std::string& language, bool names = true, bool stats = false)
    {
        auto mapping = loadMapping();
        if(!mapping) return std::nullopt;

        std::map<std::string, nlohmann::json> items;
        for(const auto& type : SIZED_TYPES) addItems(items, type);
        for(const auto& type : CLASSED_TYPES) addItems(items, type);
        // Armour only adds to names; its descriptions in the game already list its values
        if(names)
            for(const auto& type : ARMOUR_TYPES) addItems(items, type);
        if(shuttingDown || items.empty()) return std::nullopt;

        bool german = language.starts_with("de");
        Plan plan;
        for(const auto& [key, uuid] : mapping->items())
        {
            if(!uuid.is_string()) continue;
            auto item = items.find(uuid.get<std::string>());
            if(item == items.end()) continue;
            if(isDescriptionKey(key))
            {
                if(auto line = statsLine(item->second, german); stats && !line.empty()) plan.descriptionPrefixes[key] = line;
            }
            else if(isNameKey(key))
            {
                if(auto suffix = nameSuffix(item->second, german); names && !suffix.empty()) plan.nameSuffixes[key] = suffix;
            }
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        for(const auto& [key, suffix] : plan.nameSuffixes) hash.addData(QByteArray::fromStdString(key + "=" + suffix + "\n"));
        for(const auto& [key, line] : plan.descriptionPrefixes) hash.addData(QByteArray::fromStdString(key + ":" + line + "\n"));
        plan.signature = kinds(names, stats) + ":" + hash.result().toHex().left(16).toStdString();
        LOG_INFO(lg) << "build() - " << plan.nameSuffixes.size() << " names, " << plan.descriptionPrefixes.size() << " descriptions for " << language;
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
                bool empty = equals + 1 >= line.size();
                if(auto suffix = plan.nameSuffixes.find(key); suffix != plan.nameSuffixes.end() && !empty) line += " (" + suffix->second + ")";
                // The game shows "\n" in a text as a line break
                if(auto prefix = plan.descriptionPrefixes.find(key); prefix != plan.descriptionPrefixes.end() && !empty)
                    line.insert(equals + 1, prefix->second + "\\n");
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
    inline static const std::vector<std::string> SIZED_TYPES{"WeaponGun", "WeaponPersonal", "Missile", "MissileLauncher", "Turret", "WeaponMining"};
    // Size, class and grade
    inline static const std::vector<std::string> CLASSED_TYPES{"Shield", "PowerPlant", "Cooler", "QuantumDrive", "Radar"};
    inline static const std::map<std::string, std::string> GERMAN_CLASSES{
        {"Civilian", "Zivil"}, {"Industrial", "Industrie"}, {"Military", "Militär"}, {"Stealth", "Tarnung"}, {"Competition", "Wettkampf"}};
    // Weight class only
    inline static const std::vector<std::string> ARMOUR_TYPES{"Char_Armor_Torso",  "Char_Armor_Arms",      "Char_Armor_Legs",
                                                              "Char_Armor_Helmet", "Char_Armor_Undersuit", "Char_Armor_Backpack"};
    // Wiki sub type -> English and German weight class; backpacks name the armour they fit
    inline static const std::map<std::string, std::pair<std::string, std::string>> WEIGHT_CLASSES{
        {"Light", {"Light", "Leicht"}},      {"Medium", {"Medium", "Mittel"}},      {"Heavy", {"Heavy", "Schwer"}},
        {"LightArmor", {"Light", "Leicht"}}, {"MediumArmor", {"Medium", "Mittel"}}, {"HeavyArmor", {"Heavy", "Schwer"}}};

    // One value of a stats line: "{}" in the label stands for the number
    struct Stat
    {
        const char* pointer;
        const char* english;
        const char* german;
        double scale = 1;
    };
    // Radars and turrets have no single value that says much, so they keep their description as it is
    static const std::map<std::string, std::vector<Stat>>& statsTable ()
    {
        static const std::map<std::string, std::vector<Stat>> stats{
            {"Shield", {{"/shield/max_health", "HP {}", "HP {}"}, {"/shield/regen_rate", "Regen {}/s", "Regen {}/s"}}},
            {"PowerPlant", {{"/power_plant/power_segment_generation", "Output {}", "Leistung {}"}}},
            {"Cooler", {{"/cooler/coolant_segment_generation", "Cooling {}", "Kühlung {}"}}},
            {"QuantumDrive",
             {{"/quantum_drive/standard_jump/drive_speed", "Speed {} km/s", "Tempo {} km/s", 0.001},
              {"/quantum_drive/standard_jump/spool_up_time", "Spool {} s", "Aufladen {} s"}}},
            {"WeaponGun",
             {{"/vehicle_weapon/damage/burst", "DPS {}", "DPS {}"},
              {"/vehicle_weapon/damage/alpha_total", "Alpha {}", "Alpha {}"},
              {"/vehicle_weapon/range", "Range {} m", "Reichweite {} m"}}},
            {"WeaponPersonal",
             {{"/personal_weapon/damage/dps_total", "DPS {}", "DPS {}"}, {"/personal_weapon/damage/alpha_total", "Alpha {}", "Alpha {}"}}},
            {"Missile", {{"/missile/damage_total", "Damage {}", "Schaden {}"}, {"/missile/lock_range_max", "Lock {} m", "Erfassung {} m"}}},
            {"MissileLauncher", {{"/missile_rack/missile_count", "Missiles {}", "Raketen {}"}}}};
        return stats;
    }
    // Changes when the reducer keeps other fields, so caches from older versions are not used
    inline static const std::string CACHE_FORMAT = "item-values-3";

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

    static std::string lowered (std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), ::tolower);
        return text;
    }

    // item-mapping.json holds names and descriptions only, and some name keys have no "name" in them
    static bool isDescriptionKey (const std::string& key) { return lowered(key).find("desc") != std::string::npos; }
    static bool isNameKey (const std::string& key) { return !isDescriptionKey(key); }

    void addItems (std::map<std::string, nlohmann::json>& items, const std::string& type)
    {
        if(shuttingDown) return;
        auto reduce = [] (const nlohmann::json& item)
        {
            nlohmann::json kept;
            for(const char* field : {"uuid", "type", "sub_type", "size", "grade", "class"}) kept[field] = item.value(field, nlohmann::json());
            kept["stats"] = nlohmann::json::object();
            if(auto stats = statsTable().find(item.value("type", "")); stats != statsTable().end())
                for(const auto& stat : stats->second)
                    if(auto value = item.value(nlohmann::json::json_pointer(stat.pointer), nlohmann::json()); value.is_number())
                        kept["stats"][stat.pointer] = value;
            return kept;
        };
        auto result = wiki.fetchAll("items", {{"filter[type]", type}}, reduce, std::chrono::hours(24), CACHE_FORMAT);
        for(const auto& item : result.entries)
            if(item["uuid"].is_string()) items[item["uuid"].get<std::string>()] = item;
    }

    static std::string nameSuffix (const nlohmann::json& item, bool german)
    {
        std::string type = item.value("type", "");
        if(std::find(ARMOUR_TYPES.begin(), ARMOUR_TYPES.end(), type) != ARMOUR_TYPES.end())
        {
            auto weight = item["sub_type"].is_string() ? WEIGHT_CLASSES.find(item["sub_type"].get<std::string>()) : WEIGHT_CLASSES.end();
            if(weight == WEIGHT_CLASSES.end()) return "";
            return german ? weight->second.second : weight->second.first;
        }
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

    // e.g. 19,000 or 8.5 in English, 19.000 or 8,5 in German; one decimal below 100, none above
    static std::string formatNumber (double value, bool german)
    {
        bool decimal = std::abs(value) < 100 && std::round(value * 10) != std::round(value) * 10;
        auto tenths = static_cast<long long>(std::llround(std::abs(value) * (decimal ? 10 : 1)));
        auto whole = std::to_string(decimal ? tenths / 10 : tenths);
        for(int at = static_cast<int>(whole.size()) - 3; at > 0; at -= 3) whole.insert(at, german ? "." : ",");
        std::string text = (value < 0 && tenths != 0 ? "-" : "") + whole;
        if(decimal) text += (german ? "," : ".") + std::to_string(tenths % 10);
        return text;
    }

    static std::string statsLine (const nlohmann::json& item, bool german)
    {
        auto stats = statsTable().find(item.value("type", ""));
        if(stats == statsTable().end() || !item.contains("stats")) return "";
        std::string line;
        for(const auto& stat : stats->second)
        {
            auto value = item["stats"].find(stat.pointer);
            if(value == item["stats"].end() || !value->is_number() || value->get<double>() <= 0) continue;
            std::string label = german ? stat.german : stat.english;
            label.replace(label.find("{}"), 2, formatNumber(value->get<double>() * stat.scale, german));
            line += (line.empty() ? "" : " · ") + label;
        }
        return line;
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
