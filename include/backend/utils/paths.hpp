#pragma once

#include <limits.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace PATHS
{
inline std::filesystem::path getUserRootPath ()
{
    const char* homeDir = std::getenv("HOME");
    if(homeDir && *homeDir != '\0')
    {
        std::cout << "Home directory: " << homeDir << "\n";
        return std::filesystem::path(homeDir);
    }
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

inline const std::filesystem::path ROOT = getExecutablePath();
inline const std::filesystem::path ROOT_USER = getUserRootPath() / "SC-Deutsch-Launcher";
inline const std::filesystem::path LOGPATH = std::filesystem::current_path();
inline const std::filesystem::path LOGPATH_USER = ROOT_USER;

struct JSON_FILES
{
    static inline const std::filesystem::path SETTINGS_USER = ROOT_USER / "local" / "settings.json";
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
