#pragma once

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <future>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "logging/logging.h"
#include "protocol/State.hpp"

// Searches the disks for an RSI Launcher installation. Works only on its own data,
// so it can run on any thread.
class DirectoryFinder
{
   public:
    struct Result
    {
        std::filesystem::path rsiLauncherPath;
        std::map<std::string, std::filesystem::path> channelPaths;
    };

   private:
    mutable LoggerFramework::LogEx lg;

    std::string rootTarget = "/Roberts Space Industries/RSI Launcher";
    std::vector<std::string> executables{"RSI Launcher.exe", "StarCitizen_Launcher.exe"};

    bool hasLauncherExecutable (const std::filesystem::path& path) const
    {
        return std::any_of(executables.begin(), executables.end(),
                           [&] (const std::string& exe) { return std::filesystem::is_regular_file(path / exe); });
    }

    // The channel folders live next to the launcher: .../Roberts Space Industries/StarCitizen/<channel>
    Result readInstallation (const std::filesystem::path& launcherPath) const
    {
        Result result;
        if(!hasLauncherExecutable(launcherPath)) return result;

        result.rsiLauncherPath = launcherPath;
        for(const auto& channel : Protocol::CHANNEL_NAMES)
        {
            std::filesystem::path channelPath = launcherPath.parent_path() / "StarCitizen" / channel;
            LOG_DEBUG(lg) << "[check] " << channelPath;
            if(std::filesystem::exists(channelPath))
            {
                LOG_DEBUG(lg) << "[set] " << channel << " → " << channelPath;
                result.channelPaths[channel] = channelPath;
            }
        }
        return result;
    }

    std::optional<Result> searchPath (const std::string& basePath, const std::atomic<bool>& foundFlag, const std::atomic<bool>& shuttingDown) const
    {
        std::set<std::string> ignorePaths = {"/proc", "/sys", "/run", "/dev", "/tmp", "/var"};
        size_t counter = 0;

        try
        {
            for(const auto& entry :
                std::filesystem::recursive_directory_iterator(basePath, std::filesystem::directory_options::skip_permission_denied))
            {
                if(foundFlag.load() || shuttingDown.load()) break;  // Another search thread was faster

                if(!entry.is_directory()) continue;

                std::string path = entry.path().string();

                if(std::any_of(ignorePaths.begin(), ignorePaths.end(), [&] (const std::string& ignore) { return path.starts_with(ignore); }))
                    continue;

                if(++counter % 1000 == 0) LOG_DEBUG(lg) << "[scan] " << path;

                if(path.ends_with(rootTarget))
                {
                    LOG_DEBUG(lg) << "[FOUND] " << path;
                    return readInstallation(entry.path());
                }
            }
        }
        catch(const std::exception& e)
        {
            LOG_DEBUG(lg) << "[ERROR] " << e.what();
        }
        return std::nullopt;
    }

   public:
    DirectoryFinder ()
        : lg("DirectoryFinder")
    {
        LOG_DEBUG(lg) << "instanziated";
    };

    ~DirectoryFinder () { LOG_DEBUG(lg) << "destructed"; };

    // Blocks until the search is done. Returns nullopt if no installation was found.
    std::optional<Result> findRsiInstallation (const std::atomic<bool>& shuttingDown)
    {
        LOG_DEBUG(lg) << "findRsiInstallation() - target: " << rootTarget;

        std::vector<std::string> rootPaths = {"/home", "/opt", "/usr", "/mnt", "/media"};
        std::vector<std::future<std::optional<Result>>> tasks;
        std::atomic<bool> foundFlag{false};

        for(const auto& path : rootPaths)
        {
            if(!std::filesystem::exists(path)) continue;

            tasks.emplace_back(std::async(std::launch::async,
                                          [this, path, &foundFlag, &shuttingDown]
                                          {
                                              auto result = searchPath(path, foundFlag, shuttingDown);
                                              if(result) foundFlag = true;
                                              return result;
                                          }));
        }

        std::optional<Result> found;
        for(auto& task : tasks)
        {
            auto result = task.get();
            if(result && !found) found = result;
        }

        if(found)
            LOG_DEBUG(lg) << "[DONE] Pfad gefunden.";
        else
            LOG_DEBUG(lg) << "[RESULT] Kein Pfad mit '" << rootTarget << "' gefunden.";
        return found;
    }
};
