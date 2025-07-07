#pragma once

#include <filesystem>

namespace PATHS
{

inline const std::filesystem::path ROOT = std::filesystem::current_path();

struct LIVE
{
    static inline const std::filesystem::path ORIGIN_DIR = ROOT / "LIVE";
    static inline const std::filesystem::path ORIGIN_USER_CFG = ORIGIN_DIR / "user.cfg";
    static inline const std::filesystem::path ORIGIN_GLOBAL_INI = ORIGIN_DIR / "data" / "global.ini";

    static inline const std::filesystem::path BACKUP_DIR = ROOT / "BackUp" / "LIVE";
    static inline const std::filesystem::path BACKUP_USER_CFG = BACKUP_DIR / "user.cfg";
    static inline const std::filesystem::path BACKUP_GLOBAL_INI = BACKUP_DIR / "global.ini";

    static inline const std::filesystem::path TRANSLATION_DE_DIR = ROOT / "StarCitizen-Deutsch-INI" / "live";
    static inline const std::filesystem::path TRANSLATION_DE_USER_CFG = TRANSLATION_DE_DIR / "user.cfg";
    static inline const std::filesystem::path TRANSLATION_DE_GLOBAL_INI = TRANSLATION_DE_DIR / "global.ini";

    static inline const std::filesystem::path TRANSLATION_DEVOLL_DIR = ROOT / "StarCitizen-Deutsch-INI" / "live" / "full";
    static inline const std::filesystem::path TRANSLATION_DEVOLL_USER_CFG = TRANSLATION_DEVOLL_DIR / "user.cfg";
    static inline const std::filesystem::path TRANSLATION_DEVOLL_GLOBAL_INI = TRANSLATION_DEVOLL_DIR / "global.ini";
};

struct PTU
{
    static inline const std::filesystem::path ORIGIN_DIR = ROOT / "PTU";
    static inline const std::filesystem::path ORIGIN_USER_CFG = ORIGIN_DIR / "user.cfg";
    static inline const std::filesystem::path ORIGIN_GLOBAL_INI = ORIGIN_DIR / "data" / "global.ini";

    static inline const std::filesystem::path BACKUP_DIR = ROOT / "BackUp" / "PTU";
    static inline const std::filesystem::path BACKUP_USER_CFG = BACKUP_DIR / "user.cfg";
    static inline const std::filesystem::path BACKUP_GLOBAL_INI = BACKUP_DIR / "global.ini";

    static inline const std::filesystem::path TRANSLATION_DE_DIR = ROOT / "StarCitizen-Deutsch-INI" / "ptu";
    static inline const std::filesystem::path TRANSLATION_DE_USER_CFG = TRANSLATION_DE_DIR / "user.cfg";
    static inline const std::filesystem::path TRANSLATION_DE_GLOBAL_INI = TRANSLATION_DE_DIR / "global.ini";
};

struct EPTU
{
    static inline const std::filesystem::path ORIGIN_DIR = ROOT / "EPTU";
    static inline const std::filesystem::path ORIGIN_USER_CFG = ORIGIN_DIR / "user.cfg";
    static inline const std::filesystem::path ORIGIN_GLOBAL_INI = ORIGIN_DIR / "data" / "global.ini";

    static inline const std::filesystem::path BACKUP_DIR = ROOT / "BackUp" / "EPTU";
    static inline const std::filesystem::path BACKUP_USER_CFG = BACKUP_DIR / "user.cfg";
    static inline const std::filesystem::path BACKUP_GLOBAL_INI = BACKUP_DIR / "global.ini";

    static inline const std::filesystem::path TRANSLATION_DE_DIR = ROOT / "StarCitizen-Deutsch-INI" / "eptu";
    static inline const std::filesystem::path TRANSLATION_DE_USER_CFG = TRANSLATION_DE_DIR / "user.cfg";
    static inline const std::filesystem::path TRANSLATION_DE_GLOBAL_INI = TRANSLATION_DE_DIR / "global.ini";
};

struct HOTFIX
{
    static inline const std::filesystem::path ORIGIN_DIR = ROOT / "HOTFIX";
    static inline const std::filesystem::path ORIGIN_USER_CFG = ORIGIN_DIR / "user.cfg";
    static inline const std::filesystem::path ORIGIN_GLOBAL_INI = ORIGIN_DIR / "data" / "global.ini";

    static inline const std::filesystem::path BACKUP_DIR = ROOT / "BackUp" / "HOTFIX";
    static inline const std::filesystem::path BACKUP_USER_CFG = BACKUP_DIR / "user.cfg";
    static inline const std::filesystem::path BACKUP_GLOBAL_INI = BACKUP_DIR / "global.ini";

    static inline const std::filesystem::path TRANSLATION_DE_DIR = ROOT / "StarCitizen-Deutsch-INI" / "hotfix";
    static inline const std::filesystem::path TRANSLATION_DE_USER_CFG = TRANSLATION_DE_DIR / "user.cfg";
    static inline const std::filesystem::path TRANSLATION_DE_GLOBAL_INI = TRANSLATION_DE_DIR / "global.ini";
};

struct TECH_PREVIEW
{
    static inline const std::filesystem::path ORIGIN_DIR = ROOT / "TECH_PREVIEW";
    static inline const std::filesystem::path ORIGIN_USER_CFG = ORIGIN_DIR / "user.cfg";
    static inline const std::filesystem::path ORIGIN_GLOBAL_INI = ORIGIN_DIR / "data" / "global.ini";

    static inline const std::filesystem::path BACKUP_DIR = ROOT / "BackUp" / "TECH_PREVIEW";
    static inline const std::filesystem::path BACKUP_USER_CFG = BACKUP_DIR / "user.cfg";
    static inline const std::filesystem::path BACKUP_GLOBAL_INI = BACKUP_DIR / "global.ini";

    static inline const std::filesystem::path TRANSLATION_DE_DIR = ROOT / "StarCitizen-Deutsch-INI" / "tech_preview";
    static inline const std::filesystem::path TRANSLATION_DE_USER_CFG = TRANSLATION_DE_DIR / "user.cfg";
    static inline const std::filesystem::path TRANSLATION_DE_GLOBAL_INI = TRANSLATION_DE_DIR / "global.ini";

    static inline const std::filesystem::path TRANSLATION_DEVOLL_DIR = ROOT / "StarCitizen-Deutsch-INI" / "tech_preview" / "full";
    static inline const std::filesystem::path TRANSLATION_DEVOLL_USER_CFG = TRANSLATION_DEVOLL_DIR / "user.cfg";
    static inline const std::filesystem::path TRANSLATION_DEVOLL_GLOBAL_INI = TRANSLATION_DEVOLL_DIR / "global.ini";
};

}  // namespace PATHS
