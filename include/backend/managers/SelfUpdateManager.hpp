#pragma once

#include <curl/curl.h>
#include <sodium.h>
#include <QProcess>
#include <QStandardPaths>
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
#include "backend/utils/Minisign.hpp"
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

   private:
    LoggerFramework::LogEx lg;
    const std::atomic<bool>& shuttingDown;
    std::optional<Minisign::PublicKey> signingKey;

    inline static const std::string LATEST_RELEASE_URL = "https://api.github.com/repos/xXCrusaderXx/SC-Localization-Launcher/releases/latest";
    inline static const std::string DOWNLOAD_URL_PREFIX = "https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/download/";

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

    // ID and VERSION_ID from /etc/os-release
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

    // The game reads its files while running and the launcher must not be replaced under a running update
    static bool gameIsRunning ()
    {
        std::error_code ec;
        for(const auto& entry : std::filesystem::directory_iterator("/proc", ec))
        {
            std::ifstream comm(entry.path() / "comm");
            std::string name;
            if(std::getline(comm, name) && name == "StarCitizen.exe") return true;
        }
        return false;
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
            return installation;
        }

        std::error_code ec;
        QString binary = QString::fromStdString(std::filesystem::canonical("/proc/self/exe", ec).string());
        auto os = osRelease();
        if(ownedBy("dpkg", {"-S", binary}))
        {
            installation.type = "deb";
            if(os["ID"] == "ubuntu" && os["VERSION_ID"] == "24.04") installation.variant = "ubuntu24.04";
            if(os["ID"] == "debian" && os["VERSION_ID"] == "12") installation.variant = "debian12";
        }
        else if(ownedBy("rpm", {"-qf", binary}))
        {
            installation.type = "rpm";
            if(os["ID"] == "fedora") installation.variant = "fedora";
        }
        else if(ownedBy("pacman", {"-Qo", binary}))
        {
            installation.type = "arch";
            installation.variant = "arch";
        }
        return installation;
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
        LOG_DEBUG(lg) << "check() - current " << currentVersion << ", latest " << result.latestVersion << ", installed as " << installation.type;

        // Installing needs the package for this installation plus the signed checksums
        std::map<std::string, std::string> assets;
        if(json.contains("assets") && json.at("assets").is_array())
            for(const auto& asset : json.at("assets"))
                if(asset.is_object()) assets[asset.value("name", "")] = asset.value("browser_download_url", "");

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
        if(!signingKey) return "no signing key";
        if(gameIsRunning()) return "Star Citizen is running";

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
        {
            std::ofstream file(package, std::ios::binary);
            if(!file.is_open()) return "cannot write the download";
            if(download(release.packageUrl, nullptr, &file, userAgent, false, &progress) != 200) return "download failed";
        }
        if(sha256Hex(package) != expected)
        {
            LOG_ERROR(lg) << "install() - checksum mismatch for " << release.packageName;
            std::filesystem::remove_all(folder, ec);
            return "checksum mismatch";
        }

        auto error = installation.type == "appimage" ? installAppImage(installation.appImage, package) : installPackage(installation.type, package);
        std::filesystem::remove_all(folder, ec);
        if(!error) LOG_DEBUG(lg) << "install() - installed " << release.packageName;
        return error;
    }
};
