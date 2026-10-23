#pragma once

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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
    std::vector<std::filesystem::path> mountRoots;

    static std::vector<std::filesystem::path> defaultMountRoots ()
    {
        std::vector<std::filesystem::path> roots{"/mnt", "/media"};
        if(const char* user = std::getenv("USER"); user && *user)
        {
            roots.push_back(std::filesystem::path("/run/media") / user);
            roots.push_back(std::filesystem::path("/media") / user);
        }
        return roots;
    }

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

    // Wine prefixes from the Lutris game configs ("prefix: /path"); placeholders like $GAMEDIR are skipped
    static std::vector<std::filesystem::path> lutrisPrefixes (const std::filesystem::path& home)
    {
        std::vector<std::filesystem::path> prefixes;
        for(const auto& dir : {home / ".config/lutris/games", home / ".local/share/lutris/games"})
        {
            std::error_code ec;
            for(const auto& entry : std::filesystem::directory_iterator(dir, ec))
            {
                if(entry.path().extension() != ".yml") continue;
                std::ifstream file(entry.path());
                std::string line;
                while(std::getline(file, line))
                {
                    auto start = line.find_first_not_of(" \t");
                    if(start == std::string::npos || line.compare(start, 7, "prefix:") != 0) continue;
                    std::string value = line.substr(start + 7);
                    value.erase(0, value.find_first_not_of(" \t'\""));
                    value.erase(value.find_last_not_of(" \t'\"\r") + 1);
                    if(value.starts_with("/")) prefixes.emplace_back(value);
                }
            }
        }
        return prefixes;
    }

    // The same folder names on every mounted disk, e.g. /mnt/Games_1T/star-citizen
    static std::vector<std::filesystem::path> prefixesOnMountedDisks (const std::vector<std::filesystem::path>& mountRoots)
    {
        std::vector<std::filesystem::path> prefixes;
        for(const auto& root : mountRoots)
        {
            std::error_code ec;
            for(const auto& disk : std::filesystem::directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec))
                for(const char* name : {"star-citizen", "Games/star-citizen", "Lutris/star-citizen", "StarCitizen"})
                    prefixes.push_back(disk.path() / name);
        }
        return prefixes;
    }

    // Where the LUG Helper and Lutris install Star Citizen by default, the prefixes Lutris knows about and
    // the same names on mounted disks
    static std::vector<std::filesystem::path> knownLauncherPaths (const std::filesystem::path& home,
                                                                  const std::vector<std::filesystem::path>& mountRoots)
    {
        std::vector<std::filesystem::path> prefixes{home / "Games/star-citizen", home / ".wine"};
        for(const auto& prefix : lutrisPrefixes(home)) prefixes.push_back(prefix);
        for(const auto& prefix : prefixesOnMountedDisks(mountRoots)) prefixes.push_back(prefix);

        std::vector<std::filesystem::path> paths;
        for(const auto& prefix : prefixes) paths.push_back(prefix / "drive_c/Program Files/Roberts Space Industries/RSI Launcher");
        return paths;
    }

   public:
    // mountRoots: where other disks are mounted; the tests pass their own
    explicit DirectoryFinder (std::vector<std::filesystem::path> mountRoots = defaultMountRoots())
        : lg("DirectoryFinder")
        , mountRoots(std::move(mountRoots))
    {
        LOG_DEBUG(lg) << "instanziated";
    };

    ~DirectoryFinder () { LOG_DEBUG(lg) << "destructed"; };

    // Blocks until the search is done. Returns nullopt if no installation was found.
    std::optional<Result> findRsiInstallation (const std::atomic<bool>& shuttingDown)
    {
        LOG_DEBUG(lg) << "findRsiInstallation() - target: " << rootTarget;

        // The usual places take milliseconds, the full scan below can take minutes on large disks
        if(const char* home = std::getenv("HOME"); home && *home)
        {
            for(const auto& path : knownLauncherPaths(home, mountRoots))
            {
                std::error_code ec;
                if(!std::filesystem::is_directory(path, ec) || !hasLauncherExecutable(path)) continue;
                LOG_DEBUG(lg) << "[FOUND] at a known place: " << path;
                return readInstallation(path);
            }
        }

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
