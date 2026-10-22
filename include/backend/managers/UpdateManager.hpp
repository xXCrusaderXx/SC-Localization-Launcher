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
#include <thread>
#include <vector>

#include "backend/database/DataBase.hpp"
#include "backend/utils/utils.hpp"
#include "logging/logging.h"
#include "protocol/Protocol.hpp"
#include "protocol/UpdateButtonPayload.hpp"

class UpdateManager
{
   private:
    LoggerFramework::LogEx lg;
    Database::BackendData& backendData;
    using Callback = std::function<void(const nlohmann::json&)>;
    Callback guiCallback;
    std::atomic<bool> updateRunning{false};
    // Finished workers are only joined on shutdown, joining inside makeUpdate() could deadlock
    // with a worker that is still waiting for backendData.mutex in its GUI callback.
    std::vector<std::thread> workers;

    inline static const std::string GERMAN_LOCALIZATION = "german_(germany)";

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
        return static_cast<std::atomic<bool>*>(clientp)->load() ? 1 : 0;
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
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &backendData.shuttingDown);

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

    // Only touches the language lines, all other user.cfg settings of the user are kept
    void patchUserCfg (const std::filesystem::path& userCfg, bool german)
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

        if(german)
        {
            lines.push_back("g_language = " + GERMAN_LOCALIZATION);
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

    void removeGermanTranslation (const std::filesystem::path& installPath)
    {
        std::error_code ec;
        std::filesystem::path localization = installPath / "data" / "Localization";
        std::filesystem::remove_all(localization / GERMAN_LOCALIZATION, ec);

        // Clean up the folders we created, but only if nothing else lives in them
        if(std::filesystem::is_empty(localization, ec)) std::filesystem::remove(localization, ec);
        if(std::filesystem::is_empty(installPath / "data", ec)) std::filesystem::remove(installPath / "data", ec);

        patchUserCfg(installPath / "user.cfg", false);
    }

    bool installGermanTranslation (const std::filesystem::path& installPath, const std::vector<std::string>& urls)
    {
        std::filesystem::path targetDir = installPath / "data" / "Localization" / GERMAN_LOCALIZATION;
        std::error_code ec;
        std::filesystem::create_directories(targetDir, ec);
        if(ec)
        {
            LOG_ERROR(lg) << "cannot create " << targetDir << " :: " << ec.message();
            return false;
        }

        if(!downloadWithFallback(urls, targetDir / "global.ini")) return false;

        patchUserCfg(installPath / "user.cfg", true);
        return true;
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

    void startGame (const std::filesystem::path& rsiLauncherInstallPath)
    {
        LOG_DEBUG(lg) << "startGame() - START";

        auto prefix = findWinePrefix(rsiLauncherInstallPath);
        if(prefix && std::filesystem::exists(*prefix / "sc-launch.sh"))
        {
            // LUG helper installation
            std::filesystem::path script = *prefix / "sc-launch.sh";
            LOG_DEBUG(lg) << "startGame() - LUG: " << script;
            if(!QProcess::startDetached("bash", {QString::fromStdString(script.string())}, QString::fromStdString(prefix->string())))
                LOG_ERROR(lg) << "startGame() - failed to start " << script;
            return;
        }

        QString lutris = QStandardPaths::findExecutable("lutris");
        if(!lutris.isEmpty())
        {
            LOG_DEBUG(lg) << "startGame() - Lutris: " << lutris.toStdString();
            if(!QProcess::startDetached(lutris, {"lutris:rungame/star-citizen"})) LOG_ERROR(lg) << "startGame() - failed to start lutris";
            return;
        }

        LOG_ERROR(lg) << "startGame() - neither sc-launch.sh nor lutris found (RSI Launcher path: " << rsiLauncherInstallPath << ")";
    }

   public:
    UpdateManager (Database::BackendData& backendData)
        : lg("UpdateManager")
        , backendData(backendData)
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~UpdateManager ()
    {
        waitForWorkers();
        LOG_DEBUG(lg) << "destructed";
    }

    void waitForWorkers ()
    {
        for(auto& worker : workers)
            if(worker.joinable()) worker.join();
        workers.clear();
    }

    void setGuiCallback (Callback cb)
    {
        LOG_DEBUG(lg) << "setGuiCallback()";
        if(!guiCallback)
        {
            LOG_DEBUG(lg) << "Setting new gui callback.";
            guiCallback = cb;
        }
    }

    void initialSetup ()
    {
        LOG_DEBUG(lg) << "initialSetup() - Start";
        LOG_DEBUG(lg) << "initialSetup() - Finished";
    }

    void processRequest (Protocol::Massage& request, Protocol::Massage& response)
    {
        LOG_DEBUG(lg) << "processRequest() - START";
        Protocol::UpdateButtonPayload updateButtonRequest(request.getModuleNode("UPDATE"));
        Protocol::UpdateButtonPayload updateButtonResponse;

        if(updateButtonRequest.updateButton.updateButtonClicked.value_or(false))
        {
            makeUpdate(updateButtonResponse);
            response.AddModuleNode("UPDATE", updateButtonResponse.toJson());
        }
        LOG_DEBUG(lg) << "processRequest() - FINISHED";
    }

    void makeUpdate (Protocol::UpdateButtonPayload& response)
    {
        LOG_DEBUG(lg) << "makeUpdate() - Start";
        if(updateRunning.exchange(true))
        {
            LOG_DEBUG(lg) << "makeUpdate() - update already running";
            response.updateButton.busy = true;
            return;
        }

        // Work on a copy, the GUI may change backendData while the download is running
        auto channelData = backendData.channelData;
        bool launchAfterUpdate = backendData.settings.checkboxes.LaunchScAfterTranslation;
        auto rsiLauncherInstallPath = backendData.rsiLauncherInstallPath;

        workers.emplace_back(
            [this, channelData, launchAfterUpdate, rsiLauncherInstallPath] ()
            {
                bool success = true;
                for(const auto& [channel, data] : channelData)
                {
                    if(backendData.shuttingDown) break;
                    if(data.installPath.empty())
                    {
                        LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Path is empty, skip!";
                        continue;
                    }

                    LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Path: " << data.installPath;

                    if(data.buttonDe.selected)
                    {
                        LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: De-selected";
                        success &= installGermanTranslation(data.installPath, {data.info.server1, data.info.server1_fallback});
                    }
                    else if(data.buttonDeFull.selected)
                    {
                        LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: DeVoll-selected";
                        success &= installGermanTranslation(data.installPath, {data.info.server3, data.info.server3_fallback});
                    }
                    else
                    {
                        LOG_DEBUG(lg) << "update() - Channel: " << channel << " :: Eng-selected";
                        removeGermanTranslation(data.installPath);
                    }
                }

                if(!success) LOG_ERROR(lg) << "makeUpdate() - at least one channel could not be updated";

                Protocol::UpdateButtonPayload updateUpdateResponse;
                updateUpdateResponse.updateButton.busy = false;
                Protocol::Massage response(Protocol::MassageType::Response);
                response.AddModuleNode("UPDATE", updateUpdateResponse.toJson());

                updateRunning = false;
                LOG_DEBUG(lg) << "makeUpdate() - FINISHED";
                if(guiCallback)
                    guiCallback(response.getJson());
                else
                    LOG_DEBUG(lg) << "makeUpdate() - GuiCallBack nullptr";

                if(launchAfterUpdate && !backendData.shuttingDown) startGame(rsiLauncherInstallPath);
            });

        LOG_DEBUG(lg) << "makeUpdate() - RUNNING";
        response.updateButton.busy = true;
    }

    void updateUpdateResponse (Protocol::Massage& response)
    {
        Protocol::UpdateButtonPayload updateButtonResponse;

        updateButtonResponse.updateButton.enabled = true;
        if(updateRunning)
        {
            // Keep the button blocked until the running update reports back
            updateButtonResponse.updateButton.busy = true;
            response.AddModuleNode("UPDATE", updateButtonResponse.toJson());
            return;
        }
        updateButtonResponse.updateButton.active = backendData.anyChannelPathSet;
        updateButtonResponse.updateButton.LaunchScAfterTranslation = backendData.settings.checkboxes.LaunchScAfterTranslation;

        response.AddModuleNode("UPDATE", updateButtonResponse.toJson());
    }
};
