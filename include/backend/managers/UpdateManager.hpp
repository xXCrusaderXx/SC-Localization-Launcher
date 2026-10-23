#pragma once

#include <curl/curl.h>
#include <strings.h>
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <thread>
#include <vector>

#include "backend/database/DataBase.hpp"
#include "backend/utils/utils.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

class UpdateManager
{
   public:
    using GameProcess = Database::GameProcess;

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

    struct Download
    {
        bool ok = false;
        // The server confirmed that the installed file is still current (HTTP 304), nothing was written
        bool notModified = false;
        std::string etag;
    };

    // Keeps the ETag of the last response; a redirect starts a new response with its own headers
    static size_t etagHeaderCallback (char* buffer, size_t size, size_t nitems, void* userdata)
    {
        auto* etag = static_cast<std::string*>(userdata);
        std::string line(buffer, size * nitems);
        if(line.starts_with("HTTP/")) etag->clear();
        if(line.size() > 5 && strncasecmp(line.c_str(), "etag:", 5) == 0)
        {
            auto start = line.find_first_not_of(" \t", 5);
            auto end = line.find_last_not_of(" \t\r\n");
            if(start != std::string::npos && end >= start) *etag = line.substr(start, end - start + 1);
        }
        return size * nitems;
    }

    // Common options; the caller sets what is specific to the request
    CURL* createRequest (const std::string& url, std::string& etag, curl_slist*& headers, const std::string& ifNoneMatch)
    {
        CURL* curl = curl_easy_init();
        if(!curl) return nullptr;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortOnShutdown);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, const_cast<std::atomic<bool>*>(&shuttingDown));
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, etagHeaderCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &etag);
        if(!ifNoneMatch.empty())
        {
            headers = curl_slist_append(headers, ("If-None-Match: " + ifNoneMatch).c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
        return curl;
    }

    // ifNoneMatch: ETag of the installed file; the server answers 304 if it is still current
    Download downloadAndWriteToFile (const std::string& url, const std::filesystem::path& outputFile, const std::string& ifNoneMatch)
    {
        Download download;
        curl_slist* headers = nullptr;
        CURL* curl = createRequest(url, download.etag, headers, ifNoneMatch);
        if(!curl) return download;

        // Download into a temp file first, so a failed download never destroys a working global.ini
        std::filesystem::path tmpFile = outputFile;
        tmpFile += ".download";

        std::ofstream file(tmpFile, std::ios::binary);
        if(!file.is_open())
        {
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
            return download;
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFileCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        file.close();

        std::error_code ec;
        if(res == CURLE_OK && httpCode == 304 && !ifNoneMatch.empty())
        {
            LOG_DEBUG(lg) << "not modified: " << url;
            std::filesystem::remove(tmpFile, ec);
            download.ok = download.notModified = true;
            download.etag = ifNoneMatch;
            return download;
        }
        if(res != CURLE_OK || httpCode != 200 || std::filesystem::file_size(tmpFile, ec) == 0)
        {
            LOG_ERROR(lg) << "download failed: " << url << " :: " << curl_easy_strerror(res) << " (HTTP " << httpCode << ")";
            std::filesystem::remove(tmpFile, ec);
            return download;
        }

        std::filesystem::rename(tmpFile, outputFile, ec);
        if(ec)
        {
            LOG_ERROR(lg) << "rename failed: " << tmpFile << " -> " << outputFile << " :: " << ec.message();
            std::filesystem::remove(tmpFile, ec);
            return download;
        }
        download.ok = true;
        return download;
    }

    Download downloadWithFallback (const std::vector<std::string>& urls, const std::filesystem::path& outputFile, const std::string& ifNoneMatch)
    {
        for(const auto& url : urls)
        {
            if(url.empty()) continue;
            LOG_DEBUG(lg) << "download: " << url;
            if(auto download = downloadAndWriteToFile(url, outputFile, ifNoneMatch); download.ok) return download;
        }
        return {};
    }

    // ETag of a file without downloading it, from the first URL that answers
    std::optional<std::string> fetchEtag (const std::vector<std::string>& urls)
    {
        for(const auto& url : urls)
        {
            std::string etag;
            curl_slist* headers = nullptr;
            CURL* curl = createRequest(url, etag, headers, "");
            if(!curl) continue;
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
            CURLcode res = curl_easy_perform(curl);
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            curl_easy_cleanup(curl);
            if(res == CURLE_OK && httpCode == 200) return etag;
            LOG_DEBUG(lg) << "fetchEtag() - " << url << " :: " << curl_easy_strerror(res) << " (HTTP " << httpCode << ")";
        }
        return std::nullopt;
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

    Download installTranslation (const std::filesystem::path& installPath, const std::string& folder, const std::vector<std::string>& urls,
                                 const std::string& ifNoneMatch)
    {
        std::filesystem::path targetDir = installPath / "data" / "Localization" / folder;
        std::error_code ec;
        std::filesystem::create_directories(targetDir, ec);
        if(ec)
        {
            LOG_ERROR(lg) << "cannot create " << targetDir << " :: " << ec.message();
            return {};
        }

        // Only ask for "not modified" when the file is really there
        std::string etag = std::filesystem::exists(targetDir / "global.ini") ? ifNoneMatch : "";
        Download download = downloadWithFallback(urls, targetDir / "global.ini", etag);
        if(!download.ok && std::filesystem::is_empty(targetDir, ec)) std::filesystem::remove(targetDir, ec);
        return download;
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

    // Inside a Flatpak the game and its launchers live on the host, not in the sandbox.
    // Returns the process id, or nothing if the program could not be started.
    static std::optional<qint64> startOnHost (const QString& program, const QStringList& arguments, const QString& workingDirectory = {})
    {
        QProcess process;
        if(inFlatpak())
        {
            QStringList hostArguments{"--host"};
            if(!workingDirectory.isEmpty()) hostArguments << "--directory=" + workingDirectory;
            hostArguments << program << arguments;
            process.setProgram("flatpak-spawn");
            process.setArguments(hostArguments);
        }
        else
        {
            process.setProgram(program);
            process.setArguments(arguments);
            process.setWorkingDirectory(workingDirectory);
        }
        // Set when the launcher runs from VS Code or another Electron app; the RSI Launcher is an Electron app
        // too and would then run as plain Node.js and quit with "bad option"
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.remove("ELECTRON_RUN_AS_NODE");
        process.setProcessEnvironment(environment);

        qint64 pid = 0;
        if(!process.startDetached(&pid)) return std::nullopt;
        return pid;
    }

    // Wine names a process after its Windows program: the first argument is e.g. "C:\\...\\RSI Launcher.exe"
    static std::string rsiLauncherProgram ()
    {
#ifdef SC_TESTING
        // Test builds only: a real RSI Launcher on the test machine must not count
        if(const char* program = std::getenv("SC_RSI_LAUNCHER_PROGRAM")) return program;
#endif
        return "RSI Launcher.exe";
    }

    static std::string gameProgram ()
    {
#ifdef SC_TESTING
        if(const char* program = std::getenv("SC_GAME_PROGRAM")) return program;
#endif
        return "StarCitizen.exe";
    }

    // The host's /proc is not visible inside the Flatpak; pgrep matches the joined command line instead
    static GameProcess runningOnFlatpakHost ()
    {
        auto running = [] (const std::string& program)
        {
            QProcess pgrep;
            pgrep.start("flatpak-spawn", {"--host", "pgrep", "-f", QString::fromStdString("(^|\\\\)" + program + "( |$)")});
            return pgrep.waitForFinished(5000) && pgrep.exitStatus() == QProcess::NormalExit && pgrep.exitCode() == 0;
        };
        if(running("StarCitizen\\.exe")) return GameProcess::Game;
        return running("RSI Launcher\\.exe") ? GameProcess::Launcher : GameProcess::None;
    }

    // A process that ended stays a zombie until its parent reaps it, which a container's PID 1 may never do
    static bool processExists (qint64 pid)
    {
        std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
        std::string line;
        if(!std::getline(stat, line)) return false;
        // "pid (name) state …"; the name may contain spaces and parentheses
        auto end = line.rfind(')');
        return end != std::string::npos && end + 2 < line.size() && line[end + 2] != 'Z' && line[end + 2] != 'X';
    }

    // Last lines of the LUG launch log; the RSI Launcher writes why it quit there
    static std::string tailOf (const std::filesystem::path& file, size_t lines)
    {
        std::ifstream in(file);
        std::vector<std::string> last;
        for(std::string line; std::getline(in, line);)
        {
            last.push_back(line);
            if(last.size() > lines) last.erase(last.begin());
        }
        std::string text;
        for(const auto& line : last) text += "\n    " + line;
        return text;
    }

   public:
    // What of Star Citizen is running, started by this launcher or not
    static GameProcess runningGameProcess ()
    {
        if(inFlatpak()) return runningOnFlatpakHost();
        auto launcher = rsiLauncherProgram();
        auto game = gameProgram();
        auto found = GameProcess::None;
        std::error_code ec;
        for(const auto& entry : std::filesystem::directory_iterator("/proc", ec))
        {
            std::ifstream cmdline(entry.path() / "cmdline");
            std::string program;
            if(!std::getline(cmdline, program, '\0')) continue;
            auto name = program.substr(program.find_last_of("\\/") + 1);
            if(strcasecmp(name.c_str(), game.c_str()) == 0) return GameProcess::Game;
            if(strcasecmp(name.c_str(), launcher.c_str()) == 0) found = GameProcess::Launcher;
        }
        return found;
    }

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
        // ETag of the installed file of this language; a source that still has it is not downloaded again
        std::string etag;
    };

    struct Result
    {
        std::string channel;
        std::string language;
        bool success = false;
        std::string etag;
        // SHA-256 of the installed global.ini, empty for English
        std::string fileHash;
        std::string folder;
    };

    static std::filesystem::path translationFile (const std::filesystem::path& installPath, const std::string& folder)
    {
        return installPath / "data" / "Localization" / folder / "global.ini";
    }

    static std::string fileSha256 (const std::filesystem::path& path)
    {
        QFile file(QString::fromStdString(path.string()));
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if(!file.open(QIODevice::ReadOnly) || !hash.addData(&file)) return "";
        return hash.result().toHex().toStdString();
    }

    // The game only uses the translation while user.cfg selects its folder
    static bool userCfgSelects (const std::filesystem::path& installPath, const std::string& folder)
    {
        std::ifstream in(installPath / "user.cfg");
        std::string line;
        while(std::getline(in, line))
        {
            if(!line.empty() && line.back() == '\r') line.pop_back();
            auto start = line.find_first_not_of(" \t");
            if(start == std::string::npos) continue;
            std::string key = line.substr(start, line.find_first_of(" \t=", start) - start);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            if(key != "g_language") continue;
            auto equals = line.find('=');
            if(equals == std::string::npos) continue;
            std::string value = line.substr(equals + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            if(value == folder) return true;
        }
        return false;
    }

    // Whether the translation the launcher installed is still in place as installed
    static bool isIntact (const std::filesystem::path& installPath, const std::string& folder, const std::string& fileHash)
    {
        auto file = translationFile(installPath, folder);
        std::error_code ec;
        if(!std::filesystem::exists(file, ec) || !userCfgSelects(installPath, folder)) return false;
        return fileHash.empty() || fileSha256(file) == fileHash;
    }

    UpdateManager (const std::atomic<bool>& shuttingDown)
        : lg("UpdateManager")
        , shuttingDown(shuttingDown)
    {
        LOG_DEBUG(lg) << "instanziated";
    };
    ~UpdateManager () { LOG_DEBUG(lg) << "destructed"; }

    // Blocks until all channels are done
    std::vector<Result> update (const std::vector<Job>& jobs)
    {
        LOG_DEBUG(lg) << "update() - START";
        std::vector<Result> results;
        for(const auto& job : jobs)
        {
            if(shuttingDown) break;
            LOG_DEBUG(lg) << "update() - Channel: " << job.channel << " :: Path: " << job.installPath << " :: Language: " << job.language;
            Result result{job.channel, job.language, false, "", "", job.folder};

            // Download first: if it fails, the translation installed so far stays in place
            if(!job.folder.empty())
            {
                Download download = installTranslation(job.installPath, job.folder, job.urls, job.etag);
                if(!download.ok)
                {
                    results.push_back(result);
                    continue;
                }
                result.etag = download.etag;
                result.fileHash = fileSha256(translationFile(job.installPath, job.folder));
            }
            removeTranslations(job.installPath, job.managedFolders, job.folder);
            patchUserCfg(job.installPath / "user.cfg", job.folder);
            result.success = true;
            results.push_back(result);
        }
        LOG_DEBUG(lg) << "update() - FINISHED";
        return results;
    }

    // Current ETag per channel, without downloading; channels whose source does not answer are missing
    std::map<std::string, std::string> remoteVersions (const std::map<std::string, std::vector<std::string>>& urlsPerChannel)
    {
        std::map<std::string, std::string> etags;
        for(const auto& [channel, urls] : urlsPerChannel)
        {
            if(shuttingDown) break;
            if(auto etag = fetchEtag(urls); etag && !etag->empty()) etags[channel] = *etag;
        }
        return etags;
    }

    // Starts the RSI Launcher through the LUG Helper's script or Lutris and waits until it keeps running
    Protocol::GameStart startGame (const std::filesystem::path& rsiLauncherPath)
    {
        LOG_DEBUG(lg) << "startGame() - START";

        std::optional<qint64> starter;
        // sc-launch.sh runs as long as the RSI Launcher, so its end means the start failed. Lutris may hand the
        // start over to an instance that is already running and quit right away.
        bool starterOutlivesGame = false;
        std::filesystem::path launchLog;
        auto prefix = findWinePrefix(rsiLauncherPath);
        if(prefix && std::filesystem::exists(*prefix / "sc-launch.sh"))
        {
            // LUG helper installation
            std::filesystem::path script = *prefix / "sc-launch.sh";
            LOG_DEBUG(lg) << "startGame() - LUG: " << script;
            starter = startOnHost("bash", {QString::fromStdString(script.string())}, QString::fromStdString(prefix->string()));
            starterOutlivesGame = true;
            launchLog = *prefix / "sc-launch.log";
        }
        else if(inFlatpak() || !QStandardPaths::findExecutable("lutris").isEmpty())
        {
            LOG_DEBUG(lg) << "startGame() - Lutris";
            starter = startOnHost("lutris", {"lutris:rungame/star-citizen"});
        }
        else
        {
            LOG_ERROR(lg) << "startGame() - neither sc-launch.sh nor lutris found (RSI Launcher path: " << rsiLauncherPath << ")";
            return Protocol::GameStart::NoStarter;
        }
        if(!starter)
        {
            LOG_ERROR(lg) << "startGame() - failed to start the LUG script or Lutris";
            return Protocol::GameStart::Failed;
        }

        // The RSI Launcher may also start and quit at once, so it only counts once it has been running for a moment
        using Clock = std::chrono::steady_clock;
        constexpr auto settled = std::chrono::seconds(3);
        // Wine may need a while, e.g. to set up its prefix after an update
        auto deadline = Clock::now() + std::chrono::seconds(90);
        std::optional<Clock::time_point> runningSince;
        while(!shuttingDown && Clock::now() < deadline)
        {
            if(runningGameProcess() == GameProcess::None)
                runningSince.reset();
            else if(!runningSince)
                runningSince = Clock::now();
            else if(Clock::now() - *runningSince >= settled)
            {
                LOG_INFO(lg) << "startGame() - RSI Launcher is running";
                return Protocol::GameStart::Running;
            }
            if(starterOutlivesGame && !runningSince && !processExists(*starter)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        if(shuttingDown) return Protocol::GameStart::Idle;
        LOG_ERROR(lg) << "startGame() - the RSI Launcher did not start" << (launchLog.empty() ? "" : ", end of " + launchLog.string() + ":")
                      << tailOf(launchLog, 10);
        return Protocol::GameStart::Failed;
    }
};
