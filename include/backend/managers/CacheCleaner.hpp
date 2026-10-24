#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "backend/utils/paths.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

// Finds and deletes shader caches that can go stale after a game or driver update. Only these known places are ever
// touched; everything is rebuilt by the game or the driver on the next start (which then takes longer once).
//   game    <prefix>/drive_c/users/*/AppData/Local/Star Citizen/*/{shaders,vulkanshadercache}; the folder next to
//           them also holds GraphicsSettings, so only these two subfolders go
//   dxvk    *.dxvk-cache and vkd3d-proton.cache* in the game channel folder and its Bin64
//   prefix  Mesa and NVIDIA caches the LUG Helper's sc-launch.sh puts into the prefix
//   user    the Mesa and NVIDIA caches in ~/.cache, shared by all games and programs
class CacheCleaner
{
   public:
    using Entry = Protocol::CacheEntry;

   private:
    LoggerFramework::LogEx lg;

    static std::string lower (std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    static bool isRealDirectory (const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::symlink_status(path, ec).type() == std::filesystem::file_type::directory;
    }

    static bool isRealFile (const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::symlink_status(path, ec).type() == std::filesystem::file_type::regular;
    }

    // Subdirectories without following symlinks, sorted for a stable list
    static std::vector<std::filesystem::path> subdirectories (const std::filesystem::path& path)
    {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        for(std::filesystem::directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec))
            if(isRealDirectory(it->path())) result.push_back(it->path());
        std::sort(result.begin(), result.end());
        return result;
    }

    // Size on disk of a file or folder; symlinks inside are not followed. Stops early when the app closes.
    static uint64_t size (const std::filesystem::path& path, const std::atomic<bool>& stop)
    {
        std::error_code ec;
        if(isRealFile(path)) return std::filesystem::file_size(path, ec);
        uint64_t total = 0;
        for(std::filesystem::recursive_directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, ec), end;
            !ec && it != end && !stop; it.increment(ec))
        {
            std::error_code fileError;
            if(it->is_regular_file(fileError) && !it->is_symlink(fileError)) total += it->file_size(fileError);
        }
        return total;
    }

    static void add (std::vector<Entry>& entries, const std::string& kind, const std::filesystem::path& path, const std::atomic<bool>& stop)
    {
        if(!isRealDirectory(path) && !isRealFile(path)) return;
        if(std::any_of(entries.begin(), entries.end(), [&] (const Entry& e) { return e.path == path.string(); })) return;
        entries.push_back({kind, path.string(), size(path, stop), Protocol::CacheResult::None});
    }

    static std::filesystem::path userCacheDir ()
    {
        // The Flatpak's XDG_CACHE_HOME is its sandbox; the drivers write to the host's ~/.cache
        if(std::filesystem::exists("/.flatpak-info")) return PATHS::getUserRootPath() / ".cache";
        return PATHS::xdgDir("XDG_CACHE_HOME", ".cache");
    }

   public:
    CacheCleaner ()
        : lg("CacheCleaner")
    {
    }

    // prefixes: Wine prefixes (folders with drive_c); gameFolders: the channel folders, e.g. .../StarCitizen/LIVE
    std::vector<Entry> find (const std::vector<std::filesystem::path>& prefixes, const std::vector<std::filesystem::path>& gameFolders,
                             const std::atomic<bool>& stop) const
    {
        std::vector<Entry> entries;
        for(const auto& prefix : prefixes)
        {
            for(const auto& user : subdirectories(prefix / "drive_c/users"))
                for(const auto& local : subdirectories(user / "AppData/Local"))
                {
                    if(lower(local.filename().string()) != "star citizen") continue;
                    for(const auto& version : subdirectories(local))
                        for(const char* cache : {"shaders", "vulkanshadercache"}) add(entries, "game", version / cache, stop);
                }
            for(const char* cache : {"mesa_shader_cache", "mesa_shader_cache_db", "radv_builtin_shaders", "GLCache", "nvidia/GLCache"})
                add(entries, "prefix", prefix / cache, stop);
        }

        for(const auto& folder : gameFolders)
            for(const auto& dir : {folder, folder / "Bin64"})
            {
                std::error_code ec;
                for(std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
                {
                    std::string name = lower(it->path().filename().string());
                    if(name.ends_with(".dxvk-cache") || name.starts_with("vkd3d-proton.cache")) add(entries, "dxvk", it->path(), stop);
                }
            }

        auto cache = userCacheDir();
        for(const char* dir : {"mesa_shader_cache", "mesa_shader_cache_db", "nvidia/GLCache"}) add(entries, "user", cache / dir, stop);
        add(entries, "user", PATHS::getUserRootPath() / ".nv/GLCache", stop);
        return entries;
    }

    // Deletes the entries whose path is in `paths`; anything not found by find() is never touched.
    // Each entry gets its result; deleted entries keep their former size, so the dialog can show what was freed.
    void clear (std::vector<Entry>& entries, const std::set<std::string>& paths, const std::atomic<bool>& shuttingDown)
    {
        for(auto& entry : entries)
        {
            if(!paths.contains(entry.path) || shuttingDown) continue;
            std::filesystem::path path(entry.path);
            std::error_code ec;
            if(!isRealDirectory(path) && !isRealFile(path))
            {
                entry.result = Protocol::CacheResult::NotFound;
                continue;
            }
            std::filesystem::remove_all(path, ec);
            if(ec)
            {
                // Typically a file owned by another user, e.g. from running the game with sudo once
                LOG_ERROR(lg) << "clear() - " << path << " :: " << ec.message();
                entry.result = Protocol::CacheResult::Failed;
                continue;
            }
            LOG_DEBUG(lg) << "clear() - deleted " << path << " (" << entry.bytes << " bytes)";
            entry.result = Protocol::CacheResult::Deleted;
        }
    }
};
