#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "backend/utils/platform.hpp"
#include "logging/logging.h"

// Creates a test channel as a copy of LIVE and removes channel folders, so the RSI Launcher only downloads what
// differs. Works only on whole channel folders directly inside the Star Citizen library
// (.../Roberts Space Industries/StarCitizen/<channel>) and never changes a game file:
//   copy    file by file as a reflink where the file system can share the data (Btrfs, XFS, bcachefs), otherwise
//           as a plain copy. Never a hard link. It goes into a hidden folder next to the target, which is renamed
//           only when everything was copied, so an aborted copy never leaves a half channel under the real name.
//   remove  the folder is renamed to a hidden name first and then deleted; symlinks inside are removed, never
//           followed.
// The RSI Launcher verifies and patches the copied files afterwards.
class ChannelFolderManager
{
   public:
    // Channels that can be created from LIVE
    inline static const std::vector<std::string> CREATABLE = {"PTU", "EPTU", "TECH-PREVIEW"};
    // Top-level entries of LIVE that belong to that channel only: its logs (counted by the statistics) and its login
    inline static const std::set<std::string> SKIPPED = {"Game.log", "logbackups", "Crashes", "loginData.json"};
    // Free space a copy leaves at least, for the RSI Launcher's patch
    static constexpr std::uint64_t SPACE_MARGIN = 1024ull * 1024 * 1024;

    struct Entry
    {
        // Relative to the channel folder
        std::filesystem::path path;
        std::filesystem::file_type type = std::filesystem::file_type::none;
        std::uint64_t bytes = 0;
    };

    // What a copy of LIVE writes, parents before their contents
    struct Plan
    {
        std::vector<Entry> entries;
        std::uint64_t bytes = 0;
    };

   private:
    LoggerFramework::LogEx lg;

    static std::filesystem::file_type typeOf (const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::symlink_status(path, ec).type();
    }

    static std::filesystem::path hidden (const std::filesystem::path& folder, const std::string& purpose)
    {
        return folder.parent_path() / (".sc-launcher-" + folder.filename().string() + "." + purpose);
    }

    // Whether `inner` is `outer` or lies below it, compared by their real locations
    static bool contains (const std::filesystem::path& outer, const std::filesystem::path& inner)
    {
        std::error_code ec;
        auto a = std::filesystem::weakly_canonical(outer, ec);
        if(ec) return true;
        auto b = std::filesystem::weakly_canonical(inner, ec);
        if(ec) return false;
        auto [end, ignored] = std::mismatch(a.begin(), a.end(), b.begin(), b.end());
        return end == a.end();
    }

    // Copies the bytes in chunks, so a cancel is noticed within a large file too
    static bool copyBytes (const std::filesystem::path& source, const std::filesystem::path& target, std::uint64_t& done,
                           const std::function<void(std::uint64_t)>& progress, const std::function<bool()>& stopped)
    {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        if(!in || !out) return false;
        std::vector<char> buffer(4 * 1024 * 1024);
        while(in)
        {
            if(stopped()) return false;
            in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            auto count = in.gcount();
            if(count <= 0) break;
            if(!out.write(buffer.data(), count)) return false;
            done += static_cast<std::uint64_t>(count);
            progress(done);
        }
        return in.eof() && out.flush();
    }

    bool copyEntry (const Entry& entry, const std::filesystem::path& from, const std::filesystem::path& to, std::uint64_t& done,
                    const std::function<void(std::uint64_t)>& progress, const std::function<bool()>& stopped)
    {
        using std::filesystem::file_type;
        auto source = from / entry.path;
        auto target = to / entry.path;
        std::error_code ec;
        switch(entry.type)
        {
            case file_type::directory:
                std::filesystem::create_directory(target, ec);
                return !ec;
            case file_type::symlink:
                // As a link again; the copy never reads what it points to
                std::filesystem::copy_symlink(source, target, ec);
                return !ec;
            case file_type::regular:
                break;
            default:
                return true;
        }
        if(Platform::cloneFile(source, target))
        {
            done += entry.bytes;
            progress(done);
        }
        else
        {
            std::uint64_t start = done;
            if(!copyBytes(source, target, done, progress, stopped)) return false;
            // A file that changed while it was copied
            if(done - start != entry.bytes) LOG_DEBUG(lg) << "copy - size changed: " << source;
        }
        // The RSI Launcher's verification compares the files; their times and permissions stay those of LIVE
        auto status = std::filesystem::status(source, ec);
        if(!ec) std::filesystem::permissions(target, status.permissions(), ec);
        auto time = std::filesystem::last_write_time(source, ec);
        if(!ec) std::filesystem::last_write_time(target, time, ec);
        return true;
    }

   public:
    ChannelFolderManager ()
        : lg("ChannelFolderManager")
    {
    }

    // The library a channel folder sits in: .../StarCitizen/<name>, the layout of the RSI Launcher; nullopt otherwise
    static std::optional<std::filesystem::path> libraryOf (const std::filesystem::path& folder, const std::string& name)
    {
        auto clean = folder.lexically_normal();
        if(clean.filename().empty()) clean = clean.parent_path();
        if(name.empty() || clean.filename() != name || clean.parent_path().filename() != "StarCitizen") return std::nullopt;
        return clean.parent_path();
    }

    // A folder a copy may go to: not there yet, or an empty real folder
    static bool isFreeTarget (const std::filesystem::path& folder)
    {
        auto type = typeOf(folder);
        if(type == std::filesystem::file_type::not_found) return true;
        std::error_code ec;
        return type == std::filesystem::file_type::directory && std::filesystem::is_empty(folder, ec) && !ec;
    }

    // Why `folder` must not be removed as channel `name`, or empty if it may. `keep`: folders that must survive, such as
    // the other channels and the RSI Launcher.
    static std::string whyNotRemovable (const std::filesystem::path& folder, const std::string& name, const std::vector<std::filesystem::path>& keep)
    {
        if(name == "LIVE") return "live";
        if(!libraryOf(folder, name)) return "notAllowed";
        auto type = typeOf(folder);
        if(type == std::filesystem::file_type::not_found) return "notFound";
        // A symlink would lead the deletion somewhere else
        if(type != std::filesystem::file_type::directory) return "notAllowed";
        for(const auto& other : keep)
            if(!other.empty() && contains(folder, other)) return "notAllowed";
        return "";
    }

    // Everything of LIVE that a copy takes, symlinks inside not followed
    static Plan plan (const std::filesystem::path& live, const std::function<bool()>& stopped)
    {
        Plan result;
        std::error_code ec;
        for(std::filesystem::recursive_directory_iterator it(live, ec), end; !ec && it != end && !stopped(); it.increment(ec))
        {
            auto relative = it->path().lexically_relative(live);
            if(it.depth() == 0 && SKIPPED.contains(relative.string()))
            {
                it.disable_recursion_pending();
                continue;
            }
            std::error_code fileError;
            auto type = it->symlink_status(fileError).type();
            Entry entry{relative, type, 0};
            if(type == std::filesystem::file_type::regular) entry.bytes = it->file_size(fileError);
            if(type != std::filesystem::file_type::directory && type != std::filesystem::file_type::regular &&
               type != std::filesystem::file_type::symlink)
                continue;
            result.bytes += entry.bytes;
            result.entries.push_back(std::move(entry));
        }
        return result;
    }

    // Size of the files in a folder, symlinks inside not followed
    static std::uint64_t size (const std::filesystem::path& folder, const std::function<bool()>& stopped)
    {
        std::uint64_t total = 0;
        std::error_code ec;
        for(std::filesystem::recursive_directory_iterator it(folder, ec), end; !ec && it != end && !stopped(); it.increment(ec))
        {
            std::error_code fileError;
            if(it->symlink_status(fileError).type() == std::filesystem::file_type::regular) total += it->file_size(fileError);
        }
        return total;
    }

    // Whether files copied from `live` into `library` can be reflinks; tried with one small file of the plan
    static bool canReflink (const std::filesystem::path& live, const Plan& plan, const std::filesystem::path& library)
    {
        const Entry* smallest = nullptr;
        for(const auto& entry : plan.entries)
            if(entry.type == std::filesystem::file_type::regular && (!smallest || entry.bytes < smallest->bytes)) smallest = &entry;
        if(!smallest) return false;
        auto probe = library / ".sc-launcher-reflink-probe";
        std::error_code ec;
        if(typeOf(probe) == std::filesystem::file_type::regular) std::filesystem::remove(probe, ec);
        bool cloned = Platform::cloneFile(live / smallest->path, probe);
        if(cloned) std::filesystem::remove(probe, ec);
        return cloned;
    }

    // Free space for a copy: with reflinks the data is shared, so only the margin is needed
    static bool enoughSpace (std::uint64_t copyBytes, std::int64_t freeBytes, bool reflink)
    {
        if(freeBytes < 0) return false;
        return static_cast<std::uint64_t>(freeBytes) >= (reflink ? 0 : copyBytes) + SPACE_MARGIN;
    }

    // Copies LIVE to `target` as planned. Returns created, cancelled, exists or failed; anything but created leaves
    // neither the target nor the hidden folder behind. progress(done, total) is called from this thread.
    std::string copy (const std::filesystem::path& live, const Plan& plan, const std::filesystem::path& target,
                      const std::function<void(std::uint64_t, std::uint64_t)>& progress, const std::function<bool()>& stopped)
    {
        std::error_code ec;
        auto temp = hidden(target, "copying");
        // Left over by a launcher that was killed during a copy
        if(typeOf(temp) == std::filesystem::file_type::directory) std::filesystem::remove_all(temp, ec);
        if(!isFreeTarget(target)) return "exists";
        if(!std::filesystem::create_directory(temp, ec))
        {
            LOG_ERROR(lg) << "copy - cannot create " << temp << ": " << ec.message();
            return "failed";
        }

        LOG_INFO(lg) << "copy - " << live << " -> " << target << ", " << plan.bytes << " bytes";
        std::uint64_t done = 0;
        auto report = [&] (std::uint64_t bytes) { progress(bytes, plan.bytes); };
        std::string result = "created";
        for(const auto& entry : plan.entries)
        {
            if(stopped())
            {
                result = "cancelled";
                break;
            }
            if(!copyEntry(entry, live, temp, done, report, stopped))
            {
                result = stopped() ? "cancelled" : "failed";
                if(result == "failed") LOG_ERROR(lg) << "copy - cannot copy " << live / entry.path;
                break;
            }
        }
        if(result == "created")
        {
            // An empty target folder is replaced; rename() cannot replace one on Windows
            if(typeOf(target) == std::filesystem::file_type::directory) std::filesystem::remove(target, ec);
            std::filesystem::rename(temp, target, ec);
            if(ec)
            {
                LOG_ERROR(lg) << "copy - cannot rename " << temp << ": " << ec.message();
                result = "failed";
            }
        }
        if(result != "created") std::filesystem::remove_all(temp, ec);
        LOG_INFO(lg) << "copy - " << result;
        return result;
    }

    // Deletes a channel folder that whyNotRemovable() accepted. Returns removed or failed; a failed rename leaves the
    // folder as it was.
    std::string remove (const std::filesystem::path& folder)
    {
        std::error_code ec;
        auto temp = hidden(folder, "removing");
        if(typeOf(temp) == std::filesystem::file_type::directory) std::filesystem::remove_all(temp, ec);
        std::filesystem::rename(folder, temp, ec);
        if(ec)
        {
            LOG_ERROR(lg) << "remove - cannot rename " << folder << ": " << ec.message();
            return "failed";
        }
        // Does not follow symlinks
        auto count = std::filesystem::remove_all(temp, ec);
        if(ec) LOG_ERROR(lg) << "remove - " << temp << " is left: " << ec.message();
        LOG_INFO(lg) << "remove - " << folder << ", " << count << " entries";
        return "removed";
    }
};
