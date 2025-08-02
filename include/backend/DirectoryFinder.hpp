#pragma once

#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>

#include "backend/database/DataBase.hpp"
#include "logging/logging.h"

class DirectoryFinder
{
   private:
    LoggerFramework::LogEx lg;
    nlohmann::json json;
    Database::DataBase& database;

    std::string rootTarget = "/Roberts Space Industries/RSI Launcher";

    std::map<std::string, std::string> targets{{"LIVE", "StarCitizen/LIVE"},
                                               {"PTU", "StarCitizen/PTU"},
                                               {"EPTU", "StarCitizen/EPTU"},
                                               {"HOTFIX", "StarCitizen/HOTFIX"},
                                               {"TECH-PREVIEW", "StarCitizen/TECH-PREVIEW"}};

   public:
    DirectoryFinder (Database::DataBase& database)
        : lg("DirectoryFinder")
        , database(database)
    {
        LOG_DEBUG(lg) << "instanziated";
    };

    ~DirectoryFinder () { LOG_DEBUG(lg) << "destructed"; };

    void findRsiInstallation ()
    {
        LOG_DEBUG(lg) << "findRsiInstallation()";

        bool anyPathInstallPath = false;
        for(const auto& [key, data] : database.channelData)
        {
            // if(!data.paths.installPath.empty()) anyPathInstallPath = true;
        }

        if(anyPathInstallPath == false) find(rootTarget);
    }

    void find (std::string target)
    {
        LOG_DEBUG(lg) << "find() - find target: " << target;

        try
        {
            std::set<std::string> ignorePaths = {"/proc", "/sys", "/run", "/dev", "/tmp", "/var"};

            for(const auto& entry : std::filesystem::recursive_directory_iterator("/", std::filesystem::directory_options::skip_permission_denied))
            {
                if(!entry.is_directory()) continue;

                std::string path = entry.path().string();

                // Ignorieren, wenn Pfad mit einem der verbotenen beginnt
                if(std::any_of(ignorePaths.begin(), ignorePaths.end(), [&] (const std::string& ignore) { return path.starts_with(ignore); }))
                {
                    continue;
                }

                if(path.ends_with(target))
                {
                    LOG_DEBUG(lg) << "find() - found: " << path;

                    database.settings.rsiLauncherInstallPath = path;

                    for(const auto& [key, suffix] : targets)
                    {
                        std::filesystem::path targetPath = entry.path().parent_path() / suffix;

                        LOG_DEBUG(lg) << "find() - targetPath: " << targetPath;

                        if(std::filesystem::exists(targetPath))
                        {
                            LOG_DEBUG(lg) << "find() - set Path: " << targetPath;
                            database.channelData[key].paths.installPath = targetPath.string();
                            database.channelData[key].controls.installPathIsSet = true;
                        }
                    }

                    break;
                }
            }
        }
        catch(const std::exception& e)
        {
            LOG_DEBUG(lg) << "find() - Error: " << e.what();
        }
    }
};
