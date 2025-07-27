#pragma once

#include <filesystem>

namespace PATHS
{

inline const std::filesystem::path ROOT = std::filesystem::current_path();

struct JSON_FILES
{
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
