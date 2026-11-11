#pragma once

#include <QStandardPaths>
#include <QSysInfo>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "backend/managers/UpdateManager.hpp"
#include "backend/utils/ZipWriter.hpp"
#include "backend/utils/paths.hpp"
#include "backend/utils/platform.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

#ifndef _WIN32
#include <sys/resource.h>
#endif

// Read-only checks of the installation for a problem report. Nothing here changes a file outside the report itself.
// Everything that leaves the machine (check details, settings, logs) has the home folder and the user name replaced.
class Diagnostics
{
   public:
    using Check = Protocol::DiagnosticCheck;
    using Status = Protocol::CheckStatus;

    // What the backend knows, copied on its thread
    struct Input
    {
        std::string version;
        std::string installType;
        bool catalogueLoaded = false;
        std::filesystem::path rsiLauncherPath;

        struct Channel
        {
            std::string name;
            std::filesystem::path installPath;
            // Localization folder of the chosen translation; empty for English
            std::string folder;
            // Protocol::ChannelStatus as text
            std::string status;
        };

        std::vector<Channel> channels;
        // settings.json as the launcher would write it
        std::string settings;
    };

   private:
    LoggerFramework::LogEx lg;
    std::filesystem::path home;
    std::string user;

    // The values the LUG Helper checks before an installation; lower ones make the game crash or stutter
    static constexpr uint64_t MIN_MAP_COUNT = 16777216;
    static constexpr uint64_t MIN_OPEN_FILES = 524288;

    // 2 GB: a translation needs little, but a full disk also breaks game updates and shader caches
    static constexpr uintmax_t LOW_SPACE = 2ull * 1024 * 1024 * 1024;

    static std::string readOsRelease ()
    {
        // In the Flatpak /etc/os-release describes the runtime; the host's is mounted here
        for(const char* file : {"/run/host/os-release", "/etc/os-release"})
        {
            std::ifstream in(file);
            std::string line;
            while(std::getline(in, line))
                if(line.starts_with("PRETTY_NAME="))
                {
                    std::string value = line.substr(12);
                    if(value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
                    return value;
                }
        }
        return "unknown";
    }

    static std::string environment (const char* name)
    {
        const char* value = std::getenv(name);
        return value && *value ? value : "?";
    }

    static std::string gigabytes (uintmax_t bytes)
    {
        std::ostringstream text;
        text << std::fixed << std::setprecision(1) << static_cast<double>(bytes) / (1024.0 * 1024 * 1024) << " GB";
        return text.str();
    }

    // The last `maxBytes` of a text file, starting at a line
    static std::string tail (const std::filesystem::path& file, std::streamoff maxBytes)
    {
        std::ifstream in(file, std::ios::binary | std::ios::ate);
        if(!in.is_open()) return "";
        std::streamoff size = in.tellg();
        std::streamoff start = std::max<std::streamoff>(0, size - maxBytes);
        in.seekg(start);
        std::string text((std::istreambuf_iterator<char>(in)), {});
        if(start > 0)
            if(size_t newline = text.find('\n'); newline != std::string::npos) text.erase(0, newline + 1);
        return text;
    }

    Check check (std::string id, Status status, std::string detail, std::string channel = "") const
    {
        return {std::move(id), std::move(channel), status, anonymize(detail)};
    }

   public:
    // Linux settings that often break Star Citizen, read below `root` ("/" outside tests). They only report: the launcher
    // changes nothing on the system. openFiles is the hard limit of open files, which Wine's esync and fsync raise the soft one to.
    // gameMode: nullopt when it cannot be looked up, as in the Flatpak.
    static std::vector<Check> systemChecks (const std::filesystem::path& root, uint64_t openFiles, std::optional<bool> gameMode)
    {
        std::vector<Check> checks;
        auto firstLine = [&root] (const char* file)
        {
            std::ifstream in(root / file);
            std::string line;
            std::getline(in, line);
            while(!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();
            return line;
        };

        std::string mapCount = firstLine("proc/sys/vm/max_map_count");
        if(!mapCount.empty() && std::all_of(mapCount.begin(), mapCount.end(), [] (unsigned char c) { return std::isdigit(c); }))
        {
            bool enough = mapCount.size() > 18 || std::stoull(mapCount) >= MIN_MAP_COUNT;
            checks.push_back({"mapCount", "", enough ? Status::Ok : Status::Warning, mapCount});
        }

        checks.push_back({"openFiles", "", openFiles >= MIN_OPEN_FILES ? Status::Ok : Status::Warning,
                          openFiles == UINT64_MAX ? "unlimited" : std::to_string(openFiles)});

        if(gameMode) checks.push_back({"gameMode", "", *gameMode ? Status::Ok : Status::Info, ""});

        if(std::string governor = firstLine("sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"); !governor.empty())
            checks.push_back({"cpuGovernor", "", Status::Info, governor});
        return checks;
    }

    Diagnostics ()
        : lg("Diagnostics")
        , home(PATHS::getUserRootPath())
        , user(home.filename().string())
    {
    }

    // Home folder -> "~", the user name as a path component (also in Wine's drive_c/users/<name>) -> "<user>".
    // Short names are left alone, they would also match ordinary words.
    std::string anonymize (std::string text) const
    {
        auto replaceAll = [&text] (const std::string& from, const std::string& to)
        {
            if(from.empty()) return;
            for(size_t pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos + to.size())) text.replace(pos, from.size(), to);
        };
        std::string homeText = home.string();
        while(homeText.size() > 1 && homeText.back() == '/') homeText.pop_back();
        if(homeText.size() > 1) replaceAll(homeText, "~");
        if(user.size() >= 3)
            for(char separator : {'/', '\\'})
            {
                std::string component = std::string(1, separator) + user;
                for(size_t pos = text.find(component); pos != std::string::npos; pos = text.find(component, pos + 1))
                {
                    size_t end = pos + component.size();
                    if(end == text.size() || text[end] == '/' || text[end] == '\\' || text[end] == '"' || text[end] == '\'' || text[end] == ' ')
                        text.replace(pos + 1, user.size(), "<user>");
                }
            }
        return text;
    }

    std::vector<Check> run (const Input& input) const
    {
        std::vector<Check> checks;

#ifdef _WIN32
        checks.push_back(
            check("system", Status::Info, QSysInfo::prettyProductName().toStdString() + " · " + QSysInfo::kernelVersion().toStdString()));
#else
        checks.push_back(check("system", Status::Info,
                               readOsRelease() + " · Linux " + QSysInfo::kernelVersion().toStdString() + " · " + environment("XDG_CURRENT_DESKTOP") +
                                   " (" + environment("XDG_SESSION_TYPE") + ")"));
#endif
        checks.push_back(check("launcher", Status::Info, input.version + " · " + input.installType));
        checks.push_back(check("catalogue", input.catalogueLoaded ? Status::Ok : Status::Warning, ""));

        std::error_code ec;
        std::optional<std::filesystem::path> prefix;
        if(input.rsiLauncherPath.empty())
            checks.push_back(check("rsiLauncher", Status::Warning, ""));
        else
        {
            bool exists = std::filesystem::is_directory(input.rsiLauncherPath, ec);
            checks.push_back(check("rsiLauncher", exists ? Status::Ok : Status::Error, input.rsiLauncherPath.string()));
            prefix = UpdateManager::findWinePrefix(input.rsiLauncherPath);
        }
#ifdef _WIN32
        // No Wine: the launcher starts the RSI Launcher itself
        if(!input.rsiLauncherPath.empty())
        {
            bool found = std::filesystem::is_regular_file(input.rsiLauncherPath / "RSI Launcher.exe", ec);
            checks.push_back(check("starter", found ? Status::Ok : Status::Error, "RSI Launcher.exe"));
        }
#else
        for(const auto& channel : input.channels)
            if(!prefix) prefix = UpdateManager::findWinePrefix(channel.installPath);
        checks.push_back(check("winePrefix", prefix ? Status::Ok : Status::Error, prefix ? prefix->string() : ""));

        // How the game is started: the LUG Helper's script in the prefix, otherwise Lutris
        if(prefix && std::filesystem::exists(*prefix / "sc-launch.sh", ec))
            checks.push_back(check("starter", Status::Ok, "sc-launch.sh"));
        else if(!QStandardPaths::findExecutable("lutris").isEmpty())
            checks.push_back(check("starter", Status::Ok, "lutris"));
        else
            checks.push_back(check("starter", std::filesystem::exists("/.flatpak-info") ? Status::Info : Status::Warning, ""));

        // The Flatpak sees the host's kernel settings and limits, but not its programs
        rlimit limit{};
        uint64_t openFiles =
            getrlimit(RLIMIT_NOFILE, &limit) == 0 ? (limit.rlim_max == RLIM_INFINITY ? UINT64_MAX : static_cast<uint64_t>(limit.rlim_max)) : 0;
        std::optional<bool> gameMode;
        if(!std::filesystem::exists("/.flatpak-info", ec)) gameMode = !QStandardPaths::findExecutable("gamemoderun").isEmpty();
        for(auto& system : systemChecks("/", openFiles, gameMode)) checks.push_back(std::move(system));
#endif

        for(const auto& channel : input.channels)
        {
            const auto& path = channel.installPath;
            if(!std::filesystem::is_directory(path, ec))
            {
                checks.push_back(check("folder", Status::Error, path.string(), channel.name));
                continue;
            }
            checks.push_back(check("folder", Status::Ok, path.string(), channel.name));
            checks.push_back(check("writable", Platform::isWritable(path) ? Status::Ok : Status::Error, "", channel.name));

            if(channel.folder.empty())
                checks.push_back(check("translation", Status::Info, "english · " + channel.status, channel.name));
            else
            {
                auto file = UpdateManager::translationFile(path, channel.folder);
                uintmax_t size = std::filesystem::exists(file, ec) ? std::filesystem::file_size(file, ec) : 0;
                Status status = size == 0 ? Status::Error : channel.status == "installed" ? Status::Ok : Status::Warning;
                checks.push_back(
                    check("translation", status, channel.folder + " · " + std::to_string(size) + " bytes · " + channel.status, channel.name));
                bool cfg = std::filesystem::exists(path / "user.cfg", ec);
                checks.push_back(check("userCfg", cfg && UpdateManager::userCfgSelects(path, channel.folder) ? Status::Ok : Status::Error,
                                       cfg ? "" : "missing", channel.name));
            }

            auto space = std::filesystem::space(path, ec);
            if(!ec)
                checks.push_back(
                    check("diskSpace", space.available < LOW_SPACE ? Status::Warning : Status::Ok, gigabytes(space.available), channel.name));
        }
        return checks;
    }

    // ZIP with report.txt, checks.json, settings.json, the newest launcher logs and the end of sc-launch.log.
    // Game.log is left out: it names the player's account. Returns the file, or nullopt if it could not be written.
    std::optional<std::filesystem::path> writeReport (const Input& input, const std::vector<Check>& checks, const std::filesystem::path& folder)
    {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local = Platform::localTime(now);
        std::ostringstream name;
        name << "sc-localization-launcher-report-" << std::put_time(&local, "%Y%m%d-%H%M%S") << ".zip";

        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        auto path = folder / name.str();
        ZipWriter zip(path);
        if(!zip.isOpen())
        {
            LOG_ERROR(lg) << "writeReport() - cannot write " << path;
            return std::nullopt;
        }

        std::ostringstream report;
        report << "SC Localization Launcher problem report\n\n";
        for(const auto& c : checks)
            report << "[" << nlohmann::json(c.status).get<std::string>() << "] " << (c.channel.empty() ? "" : c.channel + " ") << c.id
                   << (c.detail.empty() ? "" : ": " + c.detail) << "\n";
        zip.add("report.txt", report.str());
        zip.add("checks.json", nlohmann::json(checks).dump(2) + "\n");
        zip.add("settings.json", anonymize(input.settings));

        // The newest logs; the current run is among them
        std::vector<std::filesystem::path> logs;
        for(std::filesystem::directory_iterator it(PATHS::LOGPATH_USER / "logs", ec), end; !ec && it != end; it.increment(ec))
            if(it->path().extension() == ".log") logs.push_back(it->path());
        std::sort(logs.begin(), logs.end());
        for(size_t i = logs.size() > 3 ? logs.size() - 3 : 0; i < logs.size(); ++i)
        {
            // Versions up to 1.2 named them "debug_2026-10-24 18:21:37.762.log"; colons and spaces trip up unzip tools
            std::string name = logs[i].filename().string();
            std::replace(name.begin(), name.end(), ':', '-');
            std::replace(name.begin(), name.end(), ' ', '_');
            zip.add("logs/" + name, anonymize(tail(logs[i], 2 * 1024 * 1024)));
        }

        if(auto prefix = UpdateManager::findWinePrefix(input.rsiLauncherPath.empty() && !input.channels.empty() ? input.channels.front().installPath
                                                                                                                : input.rsiLauncherPath))
            if(std::filesystem::exists(*prefix / "sc-launch.log", ec))
                zip.add("sc-launch.log", anonymize(tail(*prefix / "sc-launch.log", 256 * 1024)));

        if(!zip.finish())
        {
            LOG_ERROR(lg) << "writeReport() - writing " << path << " failed";
            std::filesystem::remove(path, ec);
            return std::nullopt;
        }
        LOG_DEBUG(lg) << "writeReport() - " << path;
        return path;
    }

    // Where reports go: the Downloads folder, so a bug report can attach it right away
    static std::filesystem::path reportFolder ()
    {
        QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        return downloads.isEmpty() ? PATHS::getUserRootPath() : std::filesystem::path(downloads.toStdString());
    }
};
