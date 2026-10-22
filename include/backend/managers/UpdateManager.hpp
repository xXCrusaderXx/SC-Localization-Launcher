#pragma once

#include <curl/curl.h>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

#include "backend/database/DataBase.hpp"
#include "backend/utils/utils.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

class UpdateManager
{
   private:
    LoggerFramework::LogEx lg;
    const std::atomic<bool>& shuttingDown;

    static size_t writeToFileCallback (void* contents, size_t size, size_t nmemb, void* userp)
    {
        std::ofstream* outFile = static_cast<std::ofstream*>(userp);
        size_t totalSize = size * nmemb;
        outFile->write(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    // Aborts a running download as soon as the app closes
    static int abortOnShutdown (void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
        return static_cast<const std::atomic<bool>*>(clientp)->load() ? 1 : 0;
    }

    bool downloadAndWriteToFile (const std::string& url, const std::filesystem::path& outputFile)
    {
        CURL* curl = curl_easy_init();
        if(!curl) return false;

        // Download into a temp file first, so a failed download never destroys a working global.ini
        std::filesystem::path tmpFile = outputFile;
        tmpFile += ".download";

        std::ofstream file(tmpFile, std::ios::binary);
        if(!file.is_open())
        {
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFileCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Redirects erlauben
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortOnShutdown);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(&shuttingDown));

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);
        file.close();

        std::error_code ec;
        if(res != CURLE_OK || httpCode != 200 || std::filesystem::file_size(tmpFile, ec) == 0)
        {
            LOG_ERROR(lg) << "download failed: " << url << " :: " << curl_easy_strerror(res) << " (HTTP " << httpCode << ")";
            std::filesystem::remove(tmpFile, ec);
            return false;
        }

        std::filesystem::rename(tmpFile, outputFile, ec);
        if(ec)
        {
            LOG_ERROR(lg) << "rename failed: " << tmpFile << " -> " << outputFile << " :: " << ec.message();
            std::filesystem::remove(tmpFile, ec);
            return false;
        }
        return true;
    }

    bool downloadWithFallback (const std::vector<std::string>& urls, const std::filesystem::path& outputFile)
    {
        for(const auto& url : urls)
        {
            if(url.empty()) continue;
            LOG_DEBUG(lg) << "download: " << url;
            if(downloadAndWriteToFile(url, outputFile)) return true;
        }
        return false;
    }

    static bool isLanguageLine (const std::string& line)
    {
        auto start = line.find_first_not_of(" \t");
        if(start == std::string::npos) return false;
        std::string key = line.substr(start, line.find_first_of(" \t=", start) - start);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        return key == "g_language" || key == "g_languageaudio";
    }

    // Only touches the language lines, all other user.cfg settings of the user are kept.
    // An empty folder removes the language setting, so the game uses its original English text.
    void patchUserCfg (const std::filesystem::path& userCfg, const std::string& folder)
    {
        std::vector<std::string> lines;
        {
            std::ifstream in(userCfg);
            std::string line;
            while(std::getline(in, line))
            {
                if(!line.empty() && line.back() == '\r') line.pop_back();
                if(!isLanguageLine(line)) lines.push_back(line);
            }
        }

        if(!folder.empty())
        {
            lines.push_back("g_language = " + folder);
            lines.push_back("g_languageAudio = english");
        }

        bool onlyBlank = std::all_of(lines.begin(), lines.end(), [] (const std::string& l) { return l.find_first_not_of(" \t") == std::string::npos; });
        std::error_code ec;
        if(onlyBlank)
        {
            std::filesystem::remove(userCfg, ec);
            return;
        }

        std::ofstream out(userCfg, std::ios::trunc);
        for(const auto& l : lines) out << l << "\n";
    }

    // Removes the translations of all managed folders except `keep`
    void removeTranslations (const std::filesystem::path& installPath, const std::vector<std::string>& managedFolders, const std::string& keep)
    {
        std::error_code ec;
        std::filesystem::path localization = installPath / "data" / "Localization";
        for(const auto& folder : managedFolders)
            if(folder != keep) std::filesystem::remove_all(localization / folder, ec);

        // Clean up the folders we created, but only if nothing else lives in them
        if(std::filesystem::is_empty(localization, ec)) std::filesystem::remove(localization, ec);
        if(std::filesystem::is_empty(installPath / "data", ec)) std::filesystem::remove(installPath / "data", ec);
    }

    bool installTranslation (const std::filesystem::path& installPath, const std::string& folder, const std::vector<std::string>& urls)
    {
        std::filesystem::path targetDir = installPath / "data" / "Localization" / folder;
        std::error_code ec;
        std::filesystem::create_directories(targetDir, ec);
        if(ec)
        {
            LOG_ERROR(lg) << "cannot create " << targetDir << " :: " << ec.message();
            return false;
        }

        if(downloadWithFallback(urls, targetDir / "global.ini")) return true;
        if(std::filesystem::is_empty(targetDir, ec)) std::filesystem::remove(targetDir, ec);
        return false;
    }

    // The wine prefix is the folder that contains drive_c
    static std::optional<std::filesystem::path> findWinePrefix (std::filesystem::path path)
    {
        while(!path.empty() && path != path.root_path())
        {
            if(std::filesystem::is_directory(path / "drive_c")) return path;
            path = path.parent_path();
        }
        return std::nullopt;
    }

    static bool inFlatpak () { return std::filesystem::exists("/.flatpak-info"); }

    // Inside a Flatpak the game and its launchers live on the host, not in the sandbox
    static bool startOnHost (const QString& program, const QStringList& arguments, const QString& workingDirectory = {})
    {
        if(!inFlatpak()) return QProcess::startDetached(program, arguments, workingDirectory);

        QStringList hostArguments{"--host"};
        if(!workingDirectory.isEmpty()) hostArguments << "--directory=" + workingDirectory;
        hostArguments << program << arguments;
        return QProcess::startDetached("flatpak-spawn", hostArguments);
    }

   public:
    // One channel to update, prepared by the backend
    struct Job
    {
        std::string channel;
        std::filesystem::path installPath;
        std::string language = Protocol::ENGLISH;
        // Target localization folder and download URLs, both empty for English
        std::string folder;
        std::vector<std::string> urls;
        // Folders of all known languages; the ones not in use are removed
        std::vector<std::string> managedFolders;
    };

    UpdateManager (const std::atomic<bool>& shuttingDown)
        : lg("UpdateManager")
        , shuttingDown(shuttingDown)
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~UpdateManager () { LOG_DEBUG(lg) << "destructed"; }

    // Blocks until all channels are done. Returns false if at least one channel failed.
    bool update (const std::vector<Job>& jobs)
    {
        LOG_DEBUG(lg) << "update() - START";
        bool success = true;
        for(const auto& job : jobs)
        {
            if(shuttingDown) return false;
            LOG_DEBUG(lg) << "update() - Channel: " << job.channel << " :: Path: " << job.installPath << " :: Language: " << job.language;

            // Download first: if it fails, the translation installed so far stays in place
            if(!job.folder.empty() && !installTranslation(job.installPath, job.folder, job.urls))
            {
                success = false;
                continue;
            }
            removeTranslations(job.installPath, job.managedFolders, job.folder);
            patchUserCfg(job.installPath / "user.cfg", job.folder);
        }

        if(!success) LOG_ERROR(lg) << "update() - at least one channel could not be updated";
        LOG_DEBUG(lg) << "update() - FINISHED";
        return success;
    }

    void startGame (const std::filesystem::path& rsiLauncherPath)
    {
        LOG_DEBUG(lg) << "startGame() - START";

        auto prefix = findWinePrefix(rsiLauncherPath);
        if(prefix && std::filesystem::exists(*prefix / "sc-launch.sh"))
        {
            // LUG helper installation
            std::filesystem::path script = *prefix / "sc-launch.sh";
            LOG_DEBUG(lg) << "startGame() - LUG: " << script;
            if(!startOnHost("bash", {QString::fromStdString(script.string())}, QString::fromStdString(prefix->string())))
                LOG_ERROR(lg) << "startGame() - failed to start " << script;
            return;
        }

        if(inFlatpak() || !QStandardPaths::findExecutable("lutris").isEmpty())
        {
            LOG_DEBUG(lg) << "startGame() - Lutris";
            if(!startOnHost("lutris", {"lutris:rungame/star-citizen"})) LOG_ERROR(lg) << "startGame() - failed to start lutris";
            return;
        }

        LOG_ERROR(lg) << "startGame() - neither sc-launch.sh nor lutris found (RSI Launcher path: " << rsiLauncherPath << ")";
    }
};
