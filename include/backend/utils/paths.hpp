#pragma once

#include <limits.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace PATHS
{
inline std::filesystem::path getUserRootPath ()
{
    const char* homeDir = std::getenv("HOME");
    if(homeDir && *homeDir != '\0')
        return std::filesystem::path(homeDir);
    else
    {
        std::cerr << "HOME environment variable is not gesetzt oder leer!\n";
        throw std::runtime_error("Umgebungsvariable HOME nicht gesetzt");
    }
}

inline std::filesystem::path getExecutablePath ()
{
    // The AppImage runtime sets APPDIR to the mounted AppDir, which holds resources/ and local/
    const char* appDir = std::getenv("APPDIR");
    if(appDir && *appDir != '\0') return std::filesystem::path(appDir);

    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if(count != -1)
    {
        // readlink() does not null-terminate
        return std::filesystem::path(std::string(result, count)).parent_path();
    }
    throw std::runtime_error("Kann Pfad zur ausführbaren Datei nicht ermitteln");
}

// $XDG_<name> if set, otherwise $HOME/<fallback> (XDG Base Directory spec); Flatpak sets these to its sandbox dirs
inline std::filesystem::path xdgDir (const char* variable, const char* fallback)
{
    const char* dir = std::getenv(variable);
    if(dir && *dir != '\0') return std::filesystem::path(dir);
    return getUserRootPath() / fallback;
}

inline const std::string APP_DIR_NAME = "sc-localization-launcher";

inline const std::filesystem::path ROOT = getExecutablePath();
inline const std::filesystem::path CONFIG_USER = xdgDir("XDG_CONFIG_HOME", ".config") / APP_DIR_NAME;
inline const std::filesystem::path CACHE_USER = xdgDir("XDG_CACHE_HOME", ".cache") / APP_DIR_NAME;
inline const std::filesystem::path LOGPATH = std::filesystem::current_path();
inline const std::filesystem::path LOGPATH_USER = xdgDir("XDG_STATE_HOME", ".local/state") / APP_DIR_NAME;
// Used up to the rename to SC Localization Launcher, only read to migrate the settings
inline const std::filesystem::path LEGACY_ROOT_USER = getUserRootPath() / "SC-Deutsch-Launcher";

struct JSON_FILES
{
    static inline const std::filesystem::path SETTINGS_USER = CONFIG_USER / "settings.json";
    static inline const std::filesystem::path SETTINGS_LEGACY = LEGACY_ROOT_USER / "local" / "settings.json";
    static inline const std::filesystem::path SETTINGS = ROOT / "local" / "settings.json";
};

struct BACKUP
{
    static inline const std::filesystem::path BACKUP_DIR = ROOT / "backup";
    static inline const std::filesystem::path BACKUP_LIVE = BACKUP_DIR / "LIVE";
    static inline const std::filesystem::path BACKUP_PTU = BACKUP_DIR / "PTU";
    static inline const std::filesystem::path BACKUP_EPTU = BACKUP_DIR / "EPTU";
    static inline const std::filesystem::path BACKUP_HOTFIX = BACKUP_DIR / "HOTFIX";
    static inline const std::filesystem::path BACKUP_TECH_PREVIEW = BACKUP_DIR / "TECH-PREVIEW";
};

}  // namespace PATHS
