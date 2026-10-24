#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "backend/utils/ZipReader.hpp"
#include "backend/utils/ZipWriter.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

// Backs up and restores what a player set up in a game channel: user.cfg, and per client profile the keybindings
// and settings (Profiles: actionmaps.xml, attributes.xml, ...), exported control mappings and saved characters.
// One ZIP per backup in <data>/backups/<channel>/, the newest KEEP per channel are kept. Only these paths are ever
// written back, and a restore first backs up what it replaces.
class ProfileBackup
{
   public:
    using Entry = Protocol::BackupEntry;
    static constexpr size_t KEEP = 10;

   private:
    LoggerFramework::LogEx lg;
    std::filesystem::path root;

    // Below user/client/<n>/
    inline static const std::vector<std::string> CLIENT_FOLDERS = {"Profiles", "controls/mappings", "CustomCharacters"};

    static bool validChannel (const std::string& channel)
    {
        return !channel.empty() &&
               std::all_of(channel.begin(), channel.end(), [] (unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
    }

    static bool isRegularFile (const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::symlink_status(path, ec).type() == std::filesystem::file_type::regular;
    }

    // With milliseconds, so backups in quick succession still sort by time
    static std::string timestamp ()
    {
        auto now = std::chrono::system_clock::now();
        auto seconds = std::chrono::system_clock::to_time_t(now);
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        std::tm local{};
        localtime_r(&seconds, &local);
        std::ostringstream text;
        text << std::put_time(&local, "%Y%m%d-%H%M%S") << "-" << std::setw(3) << std::setfill('0') << millis;
        return text.str();
    }

    // By name without ".zip": "LIVE-…-123" before "LIVE-…-123-2"
    static bool older (const std::string& a, const std::string& b)
    {
        return std::filesystem::path(a).stem().string() < std::filesystem::path(b).stem().string();
    }

    std::vector<std::filesystem::path> zipsOf (const std::string& channel) const
    {
        std::vector<std::filesystem::path> zips;
        std::error_code ec;
        for(std::filesystem::directory_iterator it(root / channel, ec), end; !ec && it != end; it.increment(ec))
            if(it->path().extension() == ".zip" && isRegularFile(it->path())) zips.push_back(it->path());
        // Names carry the time, so this is oldest first
        std::sort(zips.begin(), zips.end(), [] (const auto& a, const auto& b) { return older(a.filename().string(), b.filename().string()); });
        return zips;
    }

    void rotate (const std::string& channel)
    {
        auto zips = zipsOf(channel);
        std::error_code ec;
        for(size_t i = 0; i + KEEP < zips.size(); ++i)
        {
            std::filesystem::remove(zips[i], ec);
            LOG_DEBUG(lg) << "rotate() - removed " << zips[i];
        }
    }

    static bool writeFile (const std::filesystem::path& path, const std::string& data)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        auto tmp = path;
        tmp += ".restore-tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if(!out.is_open()) return false;
            out.write(data.data(), static_cast<std::streamsize>(data.size()));
            if(!out) return false;
        }
        std::filesystem::rename(tmp, path, ec);
        if(ec) std::filesystem::remove(tmp, ec);
        return !ec;
    }

   public:
    explicit ProfileBackup (std::filesystem::path root = PATHS::DATA_USER / "backups")
        : lg("ProfileBackup")
        , root(std::move(root))
    {
    }

    const std::filesystem::path& folder () const { return root; }

    // A path inside a backup that may be written back into the channel folder
    static bool isProfilePath (const std::string& path)
    {
        if(path == "user.cfg") return true;
        static const std::regex allowed(R"(user/client/[A-Za-z0-9_]+/(Profiles|controls/mappings|CustomCharacters)/[^\\:]+)");
        if(!std::regex_match(path, allowed)) return false;
        for(auto part : std::filesystem::path(path))
            if(part == ".." || part == ".") return false;
        return true;
    }

    // Relative paths of the files a backup of this channel folder contains; symlinks are left out
    static std::vector<std::string> profileFiles (const std::filesystem::path& installPath)
    {
        std::vector<std::string> files;
        if(isRegularFile(installPath / "user.cfg")) files.push_back("user.cfg");
        std::error_code ec;
        for(std::filesystem::directory_iterator client(installPath / "user/client", ec), end; !ec && client != end; client.increment(ec))
        {
            if(std::filesystem::symlink_status(client->path(), ec).type() != std::filesystem::file_type::directory) continue;
            for(const auto& folder : CLIENT_FOLDERS)
            {
                std::error_code walkError;
                for(std::filesystem::recursive_directory_iterator it(client->path() / folder, walkError), last; !walkError && it != last;
                    it.increment(walkError))
                {
                    if(!isRegularFile(it->path())) continue;
                    std::string relative = std::filesystem::relative(it->path(), installPath, walkError).generic_string();
                    if(isProfilePath(relative)) files.push_back(relative);
                }
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    // Newest first, all channels
    std::vector<Entry> list () const
    {
        std::vector<Entry> entries;
        std::error_code ec;
        for(std::filesystem::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
        {
            std::string channel = it->path().filename().string();
            if(!validChannel(channel)) continue;
            for(const auto& zip : zipsOf(channel))
            {
                std::error_code sizeError;
                entries.push_back({channel, zip.filename().string(), std::filesystem::file_size(zip, sizeError)});
            }
        }
        std::sort(entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) { return older(b.name, a.name); });
        return entries;
    }

    // Returns the name of the new backup, or nullopt if there is nothing to back up or it could not be written
    std::optional<std::string> create (const std::string& channel, const std::filesystem::path& installPath, const std::string& version)
    {
        if(!validChannel(channel)) return std::nullopt;
        auto files = profileFiles(installPath);
        if(files.empty())
        {
            LOG_DEBUG(lg) << "create() - nothing to back up in " << installPath;
            return std::nullopt;
        }
        std::error_code ec;
        std::filesystem::create_directories(root / channel, ec);
        std::string name = channel + "-" + timestamp();
        auto path = root / channel / (name + ".zip");
        for(int i = 2; std::filesystem::exists(path, ec); ++i) path = root / channel / (name + "-" + std::to_string(i) + ".zip");

        ZipWriter zip(path);
        if(!zip.isOpen()) return std::nullopt;
        nlohmann::json about = {{"channel", channel}, {"launcherVersion", version}, {"files", files}};
        zip.add("backup.json", about.dump(2) + "\n");
        for(const auto& file : files)
        {
            std::ifstream in(installPath / file, std::ios::binary);
            zip.add(file, std::string((std::istreambuf_iterator<char>(in)), {}));
        }
        if(!zip.finish())
        {
            std::filesystem::remove(path, ec);
            LOG_ERROR(lg) << "create() - cannot write " << path;
            return std::nullopt;
        }
        LOG_DEBUG(lg) << "create() - " << path << " with " << files.size() << " files";
        rotate(channel);
        return path.filename().string();
    }

    // Writes the files of a backup back into the channel folder; files not in the backup stay as they are.
    // Returns an error message, nullopt on success.
    std::optional<std::string> restore (const std::string& channel, const std::string& name, const std::filesystem::path& installPath,
                                        const std::string& version)
    {
        if(!validChannel(channel) || name.find('/') != std::string::npos || !name.ends_with(".zip")) return "unknown backup";
        auto path = root / channel / name;
        if(!isRegularFile(path)) return "unknown backup";
        if(installPath.empty() || !std::filesystem::is_directory(installPath)) return "no game folder";

        auto files = ZipReader::read(path);
        if(!files) return "damaged backup";
        for(const auto& file : *files)
            if(file.name != "backup.json" && !isProfilePath(file.name))
            {
                LOG_ERROR(lg) << "restore() - " << path << " contains " << file.name;
                return "damaged backup";
            }

        // What is replaced now can be restored in turn
        create(channel, installPath, version);
        for(const auto& file : *files)
        {
            if(file.name == "backup.json") continue;
            if(!writeFile(installPath / file.name, file.data))
            {
                LOG_ERROR(lg) << "restore() - cannot write " << installPath / file.name;
                return "cannot write";
            }
        }
        LOG_DEBUG(lg) << "restore() - " << path << " into " << installPath;
        return std::nullopt;
    }
};
