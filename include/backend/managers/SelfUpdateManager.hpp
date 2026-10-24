#pragma once

#include <curl/curl.h>
#include <sodium.h>
#include <QProcess>
#include <QStandardPaths>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>

#include "backend/database/DataBase.hpp"
#include "backend/managers/UpdateManager.hpp"
#include "backend/utils/Minisign.hpp"
#include "backend/utils/Zsync.hpp"
#include "backend/utils/paths.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

// Finds newer launcher releases on GitHub and installs them. Nothing is installed unless SHA256SUMS carries a
// valid minisign signature of the project's key and the downloaded package matches its checksum.
class SelfUpdateManager
{
   public:
    using ReleaseAssets = Database::ReleaseAssets;
    using Installation = Database::Installation;

    struct CheckResult
    {
        Protocol::SelfUpdate update;
        std::optional<ReleaseAssets> assets;
    };

    using ProgressCallback = std::function<void(double)>;

    // Flatpak app id; also used to restart the Flatpak after an update
    inline static const QString FLATPAK_APP_ID = "io.github.xXCrusaderXx.SCLocalizationLauncher";

   private:
    LoggerFramework::LogEx lg;
    const std::atomic<bool>& shuttingDown;
    std::optional<Minisign::PublicKey> signingKey;

    inline static const std::string LATEST_RELEASE_URL = "https://api.github.com/repos/xXCrusaderXx/SC-Localization-Launcher/releases/latest";
    inline static const std::string DOWNLOAD_URL_PREFIX = "https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/download/";
    inline static const std::string FLATPAK_REPO_URL = "https://xxcrusaderxx.github.io/SC-Localization-Launcher/repo/";
    inline static const std::string FLATPAKREF_URL =
        "https://xxcrusaderxx.github.io/SC-Localization-Launcher/io.github.xXCrusaderXx.SCLocalizationLauncher.flatpakref";
    // A delta update downloads its changed blocks with at most this many requests
    static constexpr size_t MAX_DELTA_REQUESTS = 64;

    static std::string latestReleaseUrl ()
    {
#ifdef SC_TESTING
        // Test builds only: the tests serve the release from a local server
        if(const char* url = std::getenv("SC_RELEASES_URL")) return url;
#endif
        return LATEST_RELEASE_URL;
    }

    // Downloads only from this project's releases
    static bool isAllowedDownloadUrl (const std::string& url)
    {
#ifdef SC_TESTING
        if(url.starts_with("http://127.0.0.1:")) return true;
#endif
        return url.starts_with(DOWNLOAD_URL_PREFIX);
    }

    static size_t writeToString (void* contents, size_t size, size_t nmemb, void* userp)
    {
        static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    static size_t writeToFile (void* contents, size_t size, size_t nmemb, void* userp)
    {
        static_cast<std::ofstream*>(userp)->write(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    struct Transfer
    {
        const std::atomic<bool>* shuttingDown;
        const ProgressCallback* progress;
    };

    static int onProgress (void* clientp, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t)
    {
        auto* transfer = static_cast<Transfer*>(clientp);
        if(transfer->shuttingDown->load()) return 1;
        if(transfer->progress && *transfer->progress && total > 0) (*transfer->progress)(static_cast<double>(now) / static_cast<double>(total));
        return 0;
    }

    // Writes the body to `body` or, if `file` is set, into that file. Returns the HTTP code, 0 on network errors.
    // api: a GitHub API request, which wants its own Accept header
    long download (const std::string& url, std::string* body, std::ofstream* file, const std::string& userAgent, bool api = false,
                   const ProgressCallback* progress = nullptr)
    {
        CURL* curl = curl_easy_init();
        if(!curl) return 0;

        Transfer transfer{&shuttingDown, progress};
        curl_slist* headers = api ? curl_slist_append(nullptr, "Accept: application/vnd.github+json") : nullptr;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file ? writeToFile : writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file ? static_cast<void*>(file) : static_cast<void*>(body));
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, file ? 600L : 30L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onProgress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &transfer);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        if(res != CURLE_OK)
        {
            LOG_ERROR(lg) << "download() - " << url << " :: " << curl_easy_strerror(res);
            return 0;
        }
        return httpCode;
    }

    // "v1.2.3", "1.2.3" or "1.2.3-rc1" -> {1, 2, 3}; nullopt if it is no version at all
    static std::optional<std::array<int, 3>> parseVersion (std::string text)
    {
        if(!text.empty() && (text[0] == 'v' || text[0] == 'V')) text.erase(0, 1);
        std::array<int, 3> parts{0, 0, 0};
        size_t pos = 0;
        for(int i = 0; i < 3; ++i)
        {
            size_t end = pos;
            while(end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) ++end;
            if(end == pos) return std::nullopt;
            parts[i] = std::stoi(text.substr(pos, end - pos));
            if(i < 2)
            {
                if(end >= text.size() || text[end] != '.') return std::nullopt;
                pos = end + 1;
            }
        }
        return parts;
    }

    // Key/value pairs of /etc/os-release (ID, ID_LIKE, VERSION_ID, UBUNTU_CODENAME, ...)
    static std::map<std::string, std::string> osRelease ()
    {
        std::map<std::string, std::string> values;
        std::ifstream file("/etc/os-release");
        std::string line;
        while(std::getline(file, line))
        {
            auto equals = line.find('=');
            if(equals == std::string::npos) continue;
            std::string value = line.substr(equals + 1);
            if(value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
            values[line.substr(0, equals)] = value;
        }
        return values;
    }

    // Asks the package manager whether it owns the running binary
    static bool ownedBy (const QString& program, const QStringList& arguments)
    {
        if(QStandardPaths::findExecutable(program).isEmpty()) return false;
        QProcess process;
        process.start(program, arguments);
        return process.waitForFinished(5000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

    struct HostResult
    {
        int exitCode = -1;
        std::string output;
    };

    // Runs a command on the host from inside the Flatpak sandbox (needs --talk-name=org.freedesktop.Flatpak).
    // timeoutMs -1 waits as long as it takes, e.g. while a password dialog is open.
    static HostResult runOnHost (const QStringList& command, int timeoutMs = 15000)
    {
        QProcess process;
        // English output, so the fields of `flatpak info` can be parsed
        process.start("flatpak-spawn", QStringList{"--host", "--env=LC_ALL=C"} + command);
        if(!process.waitForStarted(5000)) return {};
        if(!process.waitForFinished(timeoutMs) || process.exitStatus() != QProcess::NormalExit) return {};
        return {process.exitCode(), process.readAllStandardOutput().toStdString()};
    }

    struct RangeTarget
    {
        CURL* curl;
        std::fstream* file;
        uint64_t remaining;
    };

    // Accepts only a partial answer of at most the requested size; a server that ignores the range would overwrite
    // the reused blocks
    static size_t writeRange (void* contents, size_t size, size_t nmemb, void* userp)
    {
        auto* target = static_cast<RangeTarget*>(userp);
        long httpCode = 0;
        curl_easy_getinfo(target->curl, CURLINFO_RESPONSE_CODE, &httpCode);
        size_t bytes = size * nmemb;
        if(httpCode != 206 || bytes > target->remaining) return 0;
        target->file->write(static_cast<char*>(contents), static_cast<std::streamsize>(bytes));
        target->remaining -= bytes;
        return bytes;
    }

    // Builds the new AppImage in `package` from the blocks of the installed one and downloads only the blocks that
    // changed. False if that is not possible or not worth it; the caller then downloads the whole package.
    bool downloadDelta (const ReleaseAssets& release, const std::filesystem::path& appImage, const std::filesystem::path& package,
                        const std::string& userAgent, const ProgressCallback& progress)
    {
        std::string body;
        if(download(release.zsyncUrl, &body, nullptr, userAgent) != 200) return false;
        auto control = Zsync::parse(body);
        if(!control)
        {
            LOG_ERROR(lg) << "downloadDelta() - cannot read " << release.zsyncUrl;
            return false;
        }
        auto missing = Zsync::reuse(*control, appImage, package);
        if(!missing)
        {
            LOG_ERROR(lg) << "downloadDelta() - cannot reuse " << appImage;
            return false;
        }
        auto ranges = Zsync::merge(*missing, MAX_DELTA_REQUESTS);
        uint64_t total = 0;
        for(const auto& range : ranges) total += range.end - range.begin;
        LOG_DEBUG(lg) << "downloadDelta() - " << total << " of " << control->length << " bytes to download in " << ranges.size() << " requests";
        if(total > control->length / 2) return false;

        std::fstream file(package, std::ios::binary | std::ios::in | std::ios::out);
        CURL* curl = curl_easy_init();
        if(!file.is_open() || !curl)
        {
            if(curl) curl_easy_cleanup(curl);
            return false;
        }
        // One handle for all requests, so the connection is reused
        curl_easy_setopt(curl, CURLOPT_URL, release.packageUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeRange);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onProgress);

        bool complete = true;
        uint64_t done = 0;
        for(const auto& range : ranges)
        {
            uint64_t size = range.end - range.begin;
            RangeTarget target{curl, &file, size};
            ProgressCallback rangeProgress = [&] (double fraction)
            {
                if(progress) progress((static_cast<double>(done) + fraction * static_cast<double>(size)) / static_cast<double>(total));
            };
            Transfer transfer{&shuttingDown, &rangeProgress};
            std::string bytes = std::to_string(range.begin) + "-" + std::to_string(range.end - 1);
            file.seekp(static_cast<std::streamoff>(range.begin));
            curl_easy_setopt(curl, CURLOPT_RANGE, bytes.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &target);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &transfer);
            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK || target.remaining != 0 || !file)
            {
                long httpCode = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
                LOG_ERROR(lg) << "downloadDelta() - bytes " << bytes << " :: " << curl_easy_strerror(res) << " (HTTP " << httpCode << ")";
                complete = false;
                break;
            }
            done += size;
        }
        curl_easy_cleanup(curl);
        file.close();
        return complete && file;
    }

    // sha256sum format: "<64 hex digits>  <file name>", a '*' before the name marks binary mode
    static std::optional<std::string> checksumFor (const std::string& checksums, const std::string& name)
    {
        std::istringstream stream(checksums);
        std::string line;
        while(std::getline(stream, line))
        {
            if(!line.empty() && line.back() == '\r') line.pop_back();
            if(line.size() < 66) continue;
            std::string file = line.substr(66);
            if(!file.empty() && file.front() == '*') file.erase(0, 1);
            if(file == name) return line.substr(0, 64);
        }
        return std::nullopt;
    }

    static std::optional<std::string> sha256Hex (const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if(!file.is_open()) return std::nullopt;
        crypto_hash_sha256_state state;
        crypto_hash_sha256_init(&state);
        std::array<char, 64 * 1024> buffer;
        while(file.read(buffer.data(), buffer.size()) || file.gcount() > 0)
            crypto_hash_sha256_update(&state, reinterpret_cast<unsigned char*>(buffer.data()), static_cast<unsigned long long>(file.gcount()));
        std::array<unsigned char, crypto_hash_sha256_BYTES> hash;
        crypto_hash_sha256_final(&state, hash.data());
        std::array<char, crypto_hash_sha256_BYTES * 2 + 1> hex;
        sodium_bin2hex(hex.data(), hex.size(), hash.data(), hash.size());
        return std::string(hex.data());
    }

    // Runs a package manager through pkexec, which asks for the password
    std::optional<std::string> installPackage (const std::string& type, const std::filesystem::path& package)
    {
        QStringList command;
        QString file = QString::fromStdString(package.string());
        if(type == "deb") command = {"apt-get", "install", "-y", file};
        if(type == "rpm") command = {"dnf", "install", "-y", file};
        if(type == "arch") command = {"pacman", "-U", "--noconfirm", file};
        if(command.isEmpty()) return "unsupported installation";

        QProcess process;
        process.start("pkexec", command);
        if(!process.waitForStarted(5000)) return "pkexec is not available";
        // The password dialog may stay open for a while
        process.waitForFinished(-1);
        int code = process.exitCode();
        if(process.exitStatus() != QProcess::NormalExit) return "the package manager crashed";
        // pkexec: 126 = dialog dismissed, 127 = not authorized
        if(code == 126 || code == 127) return "cancelled";
        if(code != 0)
        {
            LOG_ERROR(lg) << "installPackage() - " << command.join(' ').toStdString() << " failed (" << code
                          << "): " << process.readAllStandardError().toStdString();
            return "the package manager failed";
        }
        return std::nullopt;
    }

    // Replaces the AppImage atomically: written next to it, then renamed over it
    std::optional<std::string> installAppImage (const std::filesystem::path& appImage, const std::filesystem::path& download)
    {
        std::filesystem::path next = appImage;
        next += ".new";
        std::error_code ec;
        std::filesystem::copy_file(download, next, std::filesystem::copy_options::overwrite_existing, ec);
        if(ec)
        {
            LOG_ERROR(lg) << "installAppImage() - cannot write " << next << " :: " << ec.message();
            return "no write permission for the AppImage folder";
        }
        std::filesystem::permissions(next,
                                     std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                                         std::filesystem::perms::others_read | std::filesystem::perms::others_exec,
                                     ec);
        std::filesystem::rename(next, appImage, ec);
        if(ec)
        {
            std::filesystem::remove(next, ec);
            return "cannot replace the AppImage";
        }
        return std::nullopt;
    }

    // Updates the Flatpak through Flatpak itself, which checks the repository's GPG signature. A bundle install is
    // reinstalled from the repository once, so later updates also arrive through `flatpak update`.
    std::optional<std::string> installFlatpak (const std::string& version, const Installation& installation)
    {
        QString where = QString::fromStdString("--" + installation.flatpakInstallation);
        QStringList command =
            installation.flatpakFromRepository
                ? QStringList{"flatpak", "update", "-y", "--noninteractive", where, FLATPAK_APP_ID}
                : QStringList{"flatpak", "install", "-y", "--noninteractive", "--reinstall", where, QString::fromStdString(FLATPAKREF_URL)};
        auto result = runOnHost(command, -1);
        if(result.exitCode != 0)
        {
            LOG_ERROR(lg) << "installFlatpak() - " << command.join(' ').toStdString() << " failed (" << result.exitCode << ")";
            return "flatpak failed";
        }

        // The repository is published a moment after the GitHub release, so it may not offer the new version yet
        auto info = runOnHost({"flatpak", "info", where, FLATPAK_APP_ID});
        std::string installed = flatpakInfoField(info.output, "Version");
        if(isNewer(version, installed))
        {
            LOG_ERROR(lg) << "installFlatpak() - repository still offers " << installed << ", release is " << version;
            return "the Flatpak repository does not offer the new version yet";
        }
        return std::nullopt;
    }

   public:
    // signingKey: text of a minisign .pub file; without a valid key nothing is ever installed
    SelfUpdateManager (const std::atomic<bool>& shuttingDown, const std::string& signingKey)
        : lg("SelfUpdateManager")
        , shuttingDown(shuttingDown)
        , signingKey(Minisign::parsePublicKey(signingKey))
    {
        if(!this->signingKey) LOG_DEBUG(lg) << "no update signing key, the launcher cannot update itself";
        LOG_DEBUG(lg) << "instanziated";
    }
    ~SelfUpdateManager () { LOG_DEBUG(lg) << "destructed"; }

    static bool isNewer (const std::string& latest, const std::string& current)
    {
        auto latestVersion = parseVersion(latest);
        auto currentVersion = parseVersion(current);
        return latestVersion && currentVersion && *latestVersion > *currentVersion;
    }

    // Which release package fits this distribution, also for derivatives: Linux Mint, Pop!_OS, Zorin (Ubuntu 24.04 base),
    // LMDE (Debian 12 base), Nobara (Fedora base). Empty if none fits. Image-based systems (Fedora Atomic, Bazzite, ...)
    // cannot install packages with dnf.
    static std::string packageVariant (const std::string& type, const std::map<std::string, std::string>& os, bool imageBased)
    {
        auto get = [&os] (const char* key)
        {
            auto it = os.find(key);
            return it == os.end() ? std::string() : it->second;
        };
        auto basedOn = [&get] (const std::string& id)
        { return (" " + get("ID") + " " + get("ID_LIKE") + " ").find(" " + id + " ") != std::string::npos; };

        if(type == "deb")
        {
            if((get("ID") == "ubuntu" && get("VERSION_ID") == "24.04") || get("UBUNTU_CODENAME") == "noble") return "ubuntu24.04";
            // Other Ubuntu releases and their derivatives have other library versions than Debian 12
            if(basedOn("ubuntu") || !get("UBUNTU_CODENAME").empty()) return "";
            if((get("ID") == "debian" && get("VERSION_ID") == "12") ||
               (basedOn("debian") && (get("VERSION_CODENAME") == "bookworm" || get("DEBIAN_CODENAME") == "bookworm")))
                return "debian12";
            return "";
        }
        if(type == "rpm") return !imageBased && basedOn("fedora") ? "fedora" : "";
        if(type == "arch") return "arch";
        return "";
    }

    // Value of "Key: value" in the output of `flatpak info` (run with LC_ALL=C)
    static std::string flatpakInfoField (const std::string& info, const std::string& key)
    {
        std::istringstream stream(info);
        std::string line;
        while(std::getline(stream, line))
        {
            auto start = line.find_first_not_of(" \t");
            if(start == std::string::npos || line.compare(start, key.size() + 1, key + ":") != 0) continue;
            auto value = line.find_first_not_of(" \t", start + key.size() + 1);
            return value == std::string::npos ? std::string() : line.substr(value);
        }
        return "";
    }

    // URL of a remote in the output of `flatpak remotes --columns=name,url` (tab separated)
    static std::string flatpakRemoteUrl (const std::string& remotes, const std::string& name)
    {
        std::istringstream stream(remotes);
        std::string line;
        while(std::getline(stream, line))
        {
            auto tab = line.find('\t');
            if(tab != std::string::npos && line.substr(0, tab) == name) return line.substr(tab + 1);
        }
        return "";
    }

    static Installation detectInstallation ()
    {
        Installation installation;
        if(const char* appImage = std::getenv("APPIMAGE"); appImage && *appImage)
        {
            installation.type = "appimage";
            installation.appImage = appImage;
            return installation;
        }
        if(std::filesystem::exists("/.flatpak-info"))
        {
            installation.type = "flatpak";
            // Without access to the host's flatpak the launcher cannot update itself; the dialog then explains how
            auto info = runOnHost({"flatpak", "info", FLATPAK_APP_ID});
            std::string where = flatpakInfoField(info.output, "Installation");
            if(info.exitCode == 0 && (where == "user" || where == "system"))
            {
                installation.flatpakInstallation = where;
                std::string origin = flatpakInfoField(info.output, "Origin");
                auto remotes = runOnHost({"flatpak", "remotes", "--" + QString::fromStdString(where), "--columns=name,url"});
                installation.flatpakFromRepository = flatpakRemoteUrl(remotes.output, origin) == FLATPAK_REPO_URL;
            }
            return installation;
        }

        std::error_code ec;
        QString binary = QString::fromStdString(std::filesystem::canonical("/proc/self/exe", ec).string());
        bool imageBased = std::filesystem::exists("/run/ostree-booted");
        if(ownedBy("dpkg", {"-S", binary}))
            installation.type = "deb";
        else if(ownedBy("rpm", {"-qf", binary}))
            installation.type = "rpm";
        else if(ownedBy("pacman", {"-Qo", binary}))
            installation.type = "arch";
        installation.variant = packageVariant(installation.type, osRelease(), imageBased);
        return installation;
    }

    // Release notes as Markdown: one "## <version>" section per published release after `since` up to `current`, newest
    // first. Without `since` only the notes of `current`. nullopt if GitHub cannot be asked; the section on verifying
    // the download is left out, it is meant for the release page.
    std::optional<std::string> notesSince (const std::string& since, const std::string& current)
    {
        std::string url = latestReleaseUrl();
        if(url.ends_with("/latest")) url.erase(url.size() - 7);
        std::string body;
        long httpCode = download(url, &body, nullptr, "sc-localization-launcher/" + current, true);
        if(httpCode != 200)
        {
            LOG_ERROR(lg) << "notesSince() - " << url << " :: HTTP " << httpCode;
            return std::nullopt;
        }
        auto json = nlohmann::json::parse(body, nullptr, false);
        if(json.is_discarded() || !json.is_array())
        {
            LOG_ERROR(lg) << "notesSince() - unexpected answer from GitHub";
            return std::nullopt;
        }

        std::vector<std::pair<std::array<int, 3>, std::string>> sections;
        for(const auto& release : json)
        {
            if(!release.is_object() || release.value("draft", false) || release.value("prerelease", false)) continue;
            std::string tag = release.value("tag_name", "");
            auto version = parseVersion(tag);
            if(!version || isNewer(tag, current)) continue;
            if(since.empty() ? isNewer(current, tag) : !isNewer(tag, since)) continue;

            std::string notes = release.value("body", "");
            if(size_t verify = notes.find("### Verify the download"); verify != std::string::npos) notes.erase(verify);
            while(!notes.empty() && std::isspace(static_cast<unsigned char>(notes.back()))) notes.pop_back();
            std::string name = (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
            sections.emplace_back(*version, "## " + name + "\n\n" + notes);
        }
        std::sort(sections.begin(), sections.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
        std::string markdown;
        for(const auto& [version, section] : sections) markdown += (markdown.empty() ? "" : "\n\n") + section;
        return markdown;
    }

    // Release asset with the package for this installation; empty if the launcher cannot update itself
    static std::string packageName (const Installation& installation, const std::string& version)
    {
        if(installation.type == "appimage") return "SC-Localization-Launcher-" + version + "-x86_64.AppImage";
        if(installation.variant.empty()) return "";
        if(installation.type == "deb") return "sc-localization-launcher-" + version + "-" + installation.variant + "-x86_64.deb";
        if(installation.type == "rpm") return "sc-localization-launcher-" + version + "-" + installation.variant + "-x86_64.rpm";
        if(installation.type == "arch") return "sc-localization-launcher-" + version + "-arch-x86_64.pkg.tar.zst";
        return "";
    }

    // Blocks while asking GitHub. Never throws; problems end up in SelfUpdate::error.
    CheckResult check (const std::string& currentVersion, const Installation& installation)
    {
        CheckResult check;
        Protocol::SelfUpdate& result = check.update;
        result.currentVersion = currentVersion;
        result.installType = installation.type;
        result.status = Protocol::SelfUpdateStatus::Error;

        std::string body;
        std::string url = latestReleaseUrl();
        long httpCode = download(url, &body, nullptr, "sc-localization-launcher/" + currentVersion, true);
        if(httpCode == 0)
        {
            result.error = "network error";
            return check;
        }
        // GitHub allows 60 unauthenticated API requests per hour and address
        if(httpCode == 403 || httpCode == 429)
        {
            LOG_ERROR(lg) << "check() - rate limited (HTTP " << httpCode << ")";
            result.error = "rate limit";
            return check;
        }
        // No release published yet
        if(httpCode == 404)
        {
            result.status = Protocol::SelfUpdateStatus::UpToDate;
            return check;
        }
        if(httpCode != 200)
        {
            LOG_ERROR(lg) << "check() - " << url << " :: HTTP " << httpCode;
            result.error = "HTTP " + std::to_string(httpCode);
            return check;
        }

        auto json = nlohmann::json::parse(body, nullptr, false);
        if(json.is_discarded() || !json.is_object() || !json.contains("tag_name") || !json.at("tag_name").is_string())
        {
            LOG_ERROR(lg) << "check() - unexpected answer from GitHub";
            result.error = "unexpected answer";
            return check;
        }

        std::string tag = json.at("tag_name").get<std::string>();
        result.latestVersion = (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
        result.notes = json.value("body", "");
        result.url = json.value("html_url", "");
        // The release page is opened in the browser, so it must not point anywhere but GitHub
        if(!result.url.starts_with("https://github.com/")) result.url = "https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/latest";
        result.status = isNewer(tag, currentVersion) ? Protocol::SelfUpdateStatus::Available : Protocol::SelfUpdateStatus::UpToDate;
        result.error.clear();
        LOG_DEBUG(lg) << "check() - current " << currentVersion << ", latest " << result.latestVersion << ", installed as " << installation.type
                      << (installation.variant.empty() ? "" : " (" + installation.variant + ")")
                      << (installation.type == "flatpak"
                              ? " (" + installation.flatpakInstallation + ", " + (installation.flatpakFromRepository ? "repository" : "bundle") + ")"
                              : "");

        // Installing needs the package for this installation plus the signed checksums
        std::map<std::string, std::string> assets;
        if(json.contains("assets") && json.at("assets").is_array())
            for(const auto& asset : json.at("assets"))
                if(asset.is_object()) assets[asset.value("name", "")] = asset.value("browser_download_url", "");

        // Flatpak updates come from the signed Flatpak repository, not from the release assets
        if(installation.type == "flatpak")
        {
            if(!installation.flatpakInstallation.empty())
            {
                result.canInstall = result.status == Protocol::SelfUpdateStatus::Available;
                check.assets = ReleaseAssets{result.latestVersion, "", "", "", ""};
            }
            return check;
        }

        ReleaseAssets release{result.latestVersion, packageName(installation, result.latestVersion), "", "", ""};
        if(signingKey && !release.packageName.empty() && assets.contains(release.packageName) && assets.contains("SHA256SUMS") &&
           assets.contains("SHA256SUMS.minisig"))
        {
            release.packageUrl = assets.at(release.packageName);
            release.checksumsUrl = assets.at("SHA256SUMS");
            release.signatureUrl = assets.at("SHA256SUMS.minisig");
            if(isAllowedDownloadUrl(release.packageUrl) && isAllowedDownloadUrl(release.checksumsUrl) && isAllowedDownloadUrl(release.signatureUrl))
            {
                result.canInstall = result.status == Protocol::SelfUpdateStatus::Available;
                std::string zsync = release.packageName + ".zsync";
                if(installation.type == "appimage" && assets.contains(zsync) && isAllowedDownloadUrl(assets.at(zsync)))
                    release.zsyncUrl = assets.at(zsync);
                check.assets = release;
            }
            else
                LOG_ERROR(lg) << "check() - release assets are not hosted by the project's GitHub releases";
        }
        return check;
    }

    // Blocks until the update is installed. Returns an error message, nullopt on success.
    std::optional<std::string> install (const ReleaseAssets& release, const Installation& installation, const std::string& currentVersion,
                                        const ProgressCallback& progress)
    {
        // The game reads its files while running and the launcher must not be replaced under it
        if(UpdateManager::runningGameProcess() == UpdateManager::GameProcess::Game) return "Star Citizen is running";
        if(installation.type == "flatpak") return installFlatpak(release.version, installation);
        if(!signingKey) return "no signing key";

        std::string userAgent = "sc-localization-launcher/" + currentVersion;
        std::string checksums;
        std::string signature;
        if(download(release.checksumsUrl, &checksums, nullptr, userAgent) != 200 ||
           download(release.signatureUrl, &signature, nullptr, userAgent) != 200)
            return "download failed";

        // The signature covers the checksum list; its trusted comment must name this very release
        auto verified = Minisign::verify(*signingKey, signature, checksums);
        if(!verified.valid)
        {
            LOG_ERROR(lg) << "install() - signature check failed: " << verified.error;
            return "invalid signature";
        }
        if(!verified.trustedComment.ends_with(" v" + release.version))
        {
            LOG_ERROR(lg) << "install() - signature belongs to another release: " << verified.trustedComment;
            return "invalid signature";
        }
        auto expected = checksumFor(checksums, release.packageName);
        if(!expected) return "package missing from the checksums";

        std::error_code ec;
        std::filesystem::path folder = PATHS::CACHE_USER / "launcher-update";
        std::filesystem::remove_all(folder, ec);
        std::filesystem::create_directories(folder, ec);
        std::filesystem::path package = folder / release.packageName;
        // A new AppImage shares most of its blocks with the installed one
        bool downloaded = false;
        if(installation.type == "appimage" && !release.zsyncUrl.empty() &&
           downloadDelta(release, installation.appImage, package, userAgent, progress))
        {
            downloaded = sha256Hex(package) == expected;
            if(!downloaded) LOG_ERROR(lg) << "install() - delta update does not match the checksum, downloading the whole package";
        }
        if(!downloaded)
        {
            {
                std::ofstream file(package, std::ios::binary | std::ios::trunc);
                if(!file.is_open()) return "cannot write the download";
                if(download(release.packageUrl, nullptr, &file, userAgent, false, &progress) != 200) return "download failed";
            }
            if(sha256Hex(package) != expected)
            {
                LOG_ERROR(lg) << "install() - checksum mismatch for " << release.packageName;
                std::filesystem::remove_all(folder, ec);
                return "checksum mismatch";
            }
        }

        auto error = installation.type == "appimage" ? installAppImage(installation.appImage, package) : installPackage(installation.type, package);
        std::filesystem::remove_all(folder, ec);
        if(!error) LOG_DEBUG(lg) << "install() - installed " << release.packageName;
        return error;
    }
};
