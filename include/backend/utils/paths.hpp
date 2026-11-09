#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "backend/utils/platform.hpp"

namespace PATHS
{
inline std::filesystem::path getUserRootPath ()
{
#ifdef _WIN32
    auto home = Platform::environmentPath("USERPROFILE");
#else
    auto home = Platform::environmentPath("HOME");
#endif
    if(home.empty()) throw std::runtime_error("The home folder is not set");
    return home;
}

inline std::filesystem::path getExecutablePath ()
{
    // The AppImage runtime sets APPDIR to the mounted AppDir, which holds resources/ and local/
    const char* appDir = std::getenv("APPDIR");
    if(appDir && *appDir != '\0') return std::filesystem::path(appDir);
    if(auto folder = Platform::executableFolder()) return *folder;
    throw std::runtime_error("Cannot find the folder of the program");
}

// $XDG_<name> if set, otherwise $HOME/<fallback> (XDG Base Directory spec); Flatpak sets these to its sandbox dirs
inline std::filesystem::path xdgDir (const char* variable, const char* fallback)
{
    auto dir = Platform::environmentPath(variable);
    if(!dir.empty()) return dir;
    return getUserRootPath() / fallback;
}

#ifdef _WIN32
// %APPDATA% roams with the user's profile, %LOCALAPPDATA% stays on this computer
inline std::filesystem::path windowsDir (const char* variable, const char* fallback)
{
    auto dir = Platform::environmentPath(variable);
    return dir.empty() ? getUserRootPath() / fallback : dir;
}
#endif

inline const std::string APP_DIR_NAME = "sc-localization-launcher";

inline const std::filesystem::path ROOT = getExecutablePath();
#ifdef _WIN32
inline const std::filesystem::path CONFIG_USER = windowsDir("APPDATA", "AppData/Roaming") / APP_DIR_NAME;
inline const std::filesystem::path CACHE_USER = windowsDir("LOCALAPPDATA", "AppData/Local") / APP_DIR_NAME / "cache";
inline const std::filesystem::path DATA_USER = windowsDir("LOCALAPPDATA", "AppData/Local") / APP_DIR_NAME / "data";
inline const std::filesystem::path LOGPATH_USER = windowsDir("LOCALAPPDATA", "AppData/Local") / APP_DIR_NAME;
#else
inline const std::filesystem::path CONFIG_USER = xdgDir("XDG_CONFIG_HOME", ".config") / APP_DIR_NAME;
inline const std::filesystem::path CACHE_USER = xdgDir("XDG_CACHE_HOME", ".cache") / APP_DIR_NAME;
inline const std::filesystem::path DATA_USER = xdgDir("XDG_DATA_HOME", ".local/share") / APP_DIR_NAME;
inline const std::filesystem::path LOGPATH_USER = xdgDir("XDG_STATE_HOME", ".local/state") / APP_DIR_NAME;
#endif
inline const std::filesystem::path LOGPATH = std::filesystem::current_path();
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
