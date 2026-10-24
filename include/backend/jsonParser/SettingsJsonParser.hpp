#pragma once

#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "backend/database/DataBase.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

// Layout of settings.json. Missing fields get their default value.
struct ChannelSettings
{
    std::string installPath;
    // A language code from languages.json or Protocol::ENGLISH
    std::string language = Protocol::ENGLISH;
    Database::InstalledTranslation installed;
    Protocol::CustomSource custom;
    Protocol::GameSettings gameSettings;
    std::vector<std::string> managedKeys;
    int64_t settingsWritten = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ChannelSettings, installPath, language, installed, custom, gameSettings, managedKeys, settingsWritten)

struct SettingsFile
{
    std::map<std::string, ChannelSettings> channels;
    std::string rsiLauncherPath;
    Protocol::Options options;
    // Launcher version whose release notes were shown last
    std::string lastSeenVersion;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SettingsFile, channels, rsiLauncherPath, options, lastSeenVersion)

class SettingsJsonParser
{
   private:
    LoggerFramework::LogEx lg;
    std::filesystem::path filePath;

    // Language names written by earlier versions; unknown names are kept, the backend checks them against languages.json
    static std::string migrateLanguage (const std::string& language)
    {
        static const std::map<std::string, std::string> renamed{
            {"", Protocol::ENGLISH}, {"eng", Protocol::ENGLISH}, {"deFull", "de-full"}, {"deVoll", "de-full"}};
        auto it = renamed.find(language);
        return it != renamed.end() ? it->second : language;
    }

    // Format up to the protocol refactor: "selectedTrans" per channel and a "settings" node
    static SettingsFile fromLegacyJson (const nlohmann::json& json)
    {
        SettingsFile file;
        // Kept in a variable, items() only references the json it iterates
        const auto channels = json.value("channels", nlohmann::json::object());
        for(const auto& [name, channel] : channels.items())
        {
            file.channels[name].installPath = channel.value("installPath", "");
            file.channels[name].language = channel.value("selectedTrans", "");
        }

        const auto settings = json.value("settings", nlohmann::json::object());
        file.rsiLauncherPath = settings.value("rsiLauncherInstallPath", "");
        file.options.launchGameAfterUpdate = settings.value("LaunchScAfterTranslation", false);
        file.options.updateAtStart = settings.value("autoTranslationAtStart", false);
        file.options.installNewTranslations = settings.value("autoNewTranslation", false);
        file.options.minimizeAfterUpdate = settings.value("minimizeScdAfterUpdate", false);
        file.options.startWithSystem = settings.value("startScdWithSystemStart", false);
        file.options.showUpdateProgress = settings.value("showUpdateStatus", false);
        return file;
    }

    std::optional<SettingsFile> loadFrom (const std::filesystem::path& path)
    {
        std::ifstream stream(path);
        if(!stream.is_open()) return std::nullopt;

        try
        {
            nlohmann::json json = nlohmann::json::parse(stream);
            LOG_DEBUG(lg) << "load() - " << path;
            SettingsFile file = json.contains("settings") ? fromLegacyJson(json) : json.get<SettingsFile>();
            for(auto& [name, channel] : file.channels) channel.language = migrateLanguage(channel.language);
            return file;
        }
        catch(const std::exception& e)
        {
            LOG_ERROR(lg) << "load() - cannot read " << path << " :: " << e.what();
            return std::nullopt;
        }
    }

   public:
    SettingsJsonParser (std::filesystem::path filePath)
        : lg("SettingsJsonParser")
        , filePath(std::move(filePath))
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~SettingsJsonParser () { LOG_DEBUG(lg) << "destructed"; };

    // Falls back to the settings of the former SC Deutsch Launcher, the shipped settings.json and then to the
    // defaults, so a broken file never stops the app. The next save() writes to filePath.
    SettingsFile load ()
    {
        if(auto file = loadFrom(filePath)) return *file;
        if(auto file = loadFrom(PATHS::JSON_FILES::SETTINGS_LEGACY)) return *file;
        if(auto file = loadFrom(PATHS::JSON_FILES::SETTINGS)) return *file;
        LOG_DEBUG(lg) << "load() - no settings.json found, using defaults";
        return {};
    }

    void save (const SettingsFile& file)
    {
        // Write a temp file first, a crash while writing must not destroy the settings
        std::filesystem::path tmpFile = filePath;
        tmpFile += ".tmp";

        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);
        {
            std::ofstream stream(tmpFile, std::ios::trunc);
            if(!stream.is_open())
            {
                LOG_ERROR(lg) << "save() - cannot open " << tmpFile;
                return;
            }
            stream << nlohmann::json(file).dump(4) << std::endl;
        }

        std::filesystem::rename(tmpFile, filePath, ec);
        if(ec) LOG_ERROR(lg) << "save() - rename failed: " << tmpFile << " -> " << filePath << " :: " << ec.message();
    }
};
