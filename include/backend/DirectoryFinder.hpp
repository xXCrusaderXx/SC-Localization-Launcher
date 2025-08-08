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
    Database::BackendData& backendData;

    std::string rootTarget = "/Roberts Space Industries/RSI Launcher";

    std::map<std::string, std::string> targets{{"LIVE", "StarCitizen/LIVE"},
                                               {"PTU", "StarCitizen/PTU"},
                                               {"EPTU", "StarCitizen/EPTU"},
                                               {"HOTFIX", "StarCitizen/HOTFIX"},
                                               {"TECH-PREVIEW", "StarCitizen/TECH-PREVIEW"}};

    std::vector<std::string> executables{"RSI Launcher.exe", "StarCitizen_Launcher.exe"};

   public:
    DirectoryFinder (Database::BackendData& backendData)
        : lg("DirectoryFinder")
        , backendData(backendData)
    {
        LOG_DEBUG(lg) << "instanziated";
    };

    ~DirectoryFinder () { LOG_DEBUG(lg) << "destructed"; };

    void findRsiInstallation ()
    {
        LOG_DEBUG(lg) << "findRsiInstallation()";
        find(rootTarget);
    }

    void find (std::string target)
    {
        LOG_DEBUG(lg) << "find() - find target: " << target;

        std::vector<std::string> rootPaths = {"/home", "/opt", "/usr", "/mnt", "/media"};
        std::vector<std::future<void>> tasks;
        std::atomic<bool> foundFlag{false};

        for(const auto& path : rootPaths)
        {
            if(!std::filesystem::exists(path)) continue;

            tasks.emplace_back(std::async(std::launch::async, [this, path, target, &foundFlag] { searchPath(path, target, foundFlag); }));
        }

        for(auto& t : tasks)
        {
            t.wait();
        }

        if(!foundFlag)
        {
            LOG_DEBUG(lg) << "[RESULT] Kein Pfad mit '" << target << "' gefunden.";
        }
        else
        {
            LOG_DEBUG(lg) << "[DONE] Pfad gefunden.";
        }
    }

    void searchPath (const std::string& basePath, const std::string& target, std::atomic<bool>& foundFlag)
    {
        std::set<std::string> ignorePaths = {"/proc", "/sys", "/run", "/dev", "/tmp", "/var"};
        size_t counter = 0;

        try
        {
            for(const auto& entry :
                std::filesystem::recursive_directory_iterator(basePath, std::filesystem::directory_options::skip_permission_denied))
            {
                if(foundFlag.load()) break;  // Frühzeitiger Abbruch

                if(!entry.is_directory()) continue;

                std::string path = entry.path().string();

                if(std::any_of(ignorePaths.begin(), ignorePaths.end(), [&] (const std::string& ignore) { return path.starts_with(ignore); }))
                    continue;

                if(++counter % 1000 == 0)
                {
                    LOG_DEBUG(lg) << "[scan] " << path;
                }

                if(path.ends_with(target))
                {
                    {
                        LOG_DEBUG(lg) << "[FOUND] " << path;
                    }

                    for(const auto& exe : executables)
                    {
                        if(std::filesystem::is_regular_file(path + "/" + exe))
                        {
                            backendData.rsiLauncherInstallPath = path;
                        }
                    }

                    for(const auto& [key, suffix] : targets)
                    {
                        std::filesystem::path targetPath = entry.path().parent_path() / suffix;
                        {
                            LOG_DEBUG(lg) << "[check] " << targetPath;
                        }

                        if(std::filesystem::exists(targetPath))
                        {
                            for(const auto& exe : executables)
                            {
                                if(std::filesystem::is_regular_file(path + "/" + exe))
                                {
                                    LOG_DEBUG(lg) << "[set] " << key << " → " << targetPath;
                                    backendData.channelData[key].installPath = targetPath;
                                }
                            }
                        }
                    }

                    foundFlag = true;
                    break;
                }
            }
        }
        catch(const std::exception& e)
        {
            LOG_DEBUG(lg) << "[ERROR] " << e.what();
        }
    }
};
