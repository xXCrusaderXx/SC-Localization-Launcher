#pragma once

#include <algorithm>
#include <functional>
#include <thread>

#include "UpdateSigningKey.hpp"
#include "backend/DirectoryFinder.hpp"
#include "backend/utils/TaskQueue.hpp"
#include "backend/utils/paths.hpp"
#include "database/DataBase.hpp"
#include "jsonParser/LanguageCatalogParser.hpp"
#include "jsonParser/SettingsJsonParser.hpp"
#include "logging/logging.h"
#include "managers/SelfUpdateManager.hpp"
#include "managers/UpdateManager.hpp"
#include "protocol/Commands.hpp"
#include "protocol/State.hpp"

// Commands go in, the complete state comes out.
//
// All backend data lives on one thread (the TaskQueue): commands run there one after another,
// so no locking is needed. Slow work (downloads, disk search) runs in a background job that only
// sees copies of the data and posts its result back to the queue.
class BackEnd
{
   public:
    using StateCallback = std::function<void(const Protocol::State&)>;

   private:
    LoggerFramework::LogEx lg;
    Database::BackendData data;
    SettingsJsonParser settingsJsonParser;
    LanguageCatalogParser languageCatalogParser;
    UpdateManager updateManager;
    SelfUpdateManager selfUpdateManager;
    DirectoryFinder directoryFinder;
    StateCallback stateCallback;

    std::thread languagesJob;
    std::thread updateJob;
    std::thread searchJob;
    std::thread versionCheckJob;
    std::thread selfUpdateJob;

    // Declared last: its thread uses all members above
    TaskQueue queue;

    // Only called on the backend thread. A job that is not running anymore is joined right away,
    // it never waits for the backend thread.
    void startJob (std::thread& job, std::function<void()> function)
    {
        if(job.joinable()) job.join();
        job = std::thread(std::move(function));
    }

    static std::string updateSigningKey ()
    {
#ifdef SC_TESTING
        // Test builds only: the tests sign their releases with a key of their own
        if(const char* key = std::getenv("SC_UPDATE_SIGNING_KEY")) return key;
#endif
        return UPDATE_SIGNING_KEY;
    }

    const Database::AvailableLanguage* findLanguage (const std::string& code) const
    {
        auto it = std::find_if(data.languages.begin(), data.languages.end(), [&] (const auto& l) { return l.code == code; });
        return it != data.languages.end() ? &*it : nullptr;
    }

    std::vector<std::string> availableLanguages (const std::string& channel) const
    {
        std::vector<std::string> codes{Protocol::ENGLISH};
        for(const auto& language : data.languages)
            if(language.urls.contains(channel)) codes.push_back(language.code);
        return codes;
    }

    bool isAvailable (const std::string& channel, const std::string& code) const
    {
        auto codes = availableLanguages(channel);
        return std::find(codes.begin(), codes.end(), code) != codes.end();
    }

    Database::ChannelData* findChannel (const std::string& name)
    {
        auto it = data.channels.find(name);
        if(it != data.channels.end()) return &it->second;
        LOG_ERROR(lg) << "unknown channel: " << name;
        return nullptr;
    }

    std::vector<std::string> managedFolders () const
    {
        std::vector<std::string> folders;
        for(const auto& language : data.languages)
            if(std::find(folders.begin(), folders.end(), language.folder) == folders.end()) folders.push_back(language.folder);
        return folders;
    }

    static bool hasTranslationFile (const std::filesystem::path& installPath, const std::string& folder)
    {
        std::error_code ec;
        return std::filesystem::exists(installPath / "data" / "Localization" / folder / "global.ini", ec);
    }

    Protocol::ChannelStatus channelStatus (const Database::ChannelData& channel) const
    {
        using Protocol::ChannelStatus;
        if(channel.installPath.empty()) return ChannelStatus::NoPath;
        if(channel.lastUpdateFailed) return ChannelStatus::Failed;

        if(channel.language == Protocol::ENGLISH)
        {
            // English means no translation files; any left over still has to be removed
            auto folders = managedFolders();
            bool leftOver =
                std::any_of(folders.begin(), folders.end(), [&] (const auto& folder) { return hasTranslationFile(channel.installPath, folder); });
            return leftOver ? ChannelStatus::UpdateAvailable : ChannelStatus::Installed;
        }

        const auto* language = findLanguage(channel.language);
        if(!language || !hasTranslationFile(channel.installPath, language->folder)) return ChannelStatus::NotInstalled;
        // Installed by hand, by the former SC Deutsch Launcher or as another language of the same folder: version unknown
        if(channel.installed.language != channel.language) return ChannelStatus::UpdateAvailable;
        if(channel.damaged) return ChannelStatus::Damaged;
        if(!channel.remoteEtag.empty() && channel.remoteEtag != channel.installed.etag) return ChannelStatus::UpdateAvailable;
        return ChannelStatus::Installed;
    }

    // Asks the sources whether the chosen translations changed, without downloading them, and checks that the
    // installed files are still as installed (a game update may overwrite global.ini or user.cfg)
    void checkChannels ()
    {
        struct Integrity
        {
            std::filesystem::path installPath;
            std::string folder;
            std::string fileHash;
        };
        std::map<std::string, std::vector<std::string>> urls;
        std::map<std::string, Integrity> installed;
        std::map<std::string, std::string> checkedLanguage;
        for(const auto& [name, channel] : data.channels)
        {
            if(channel.installPath.empty() || channel.language == Protocol::ENGLISH) continue;
            const auto* language = findLanguage(channel.language);
            if(!language) continue;
            checkedLanguage[name] = channel.language;
            if(isAvailable(name, channel.language)) urls[name] = language->urls.at(name);
            if(channel.installed.language == channel.language) installed[name] = {channel.installPath, language->folder, channel.installed.fileHash};
        }
        if(checkedLanguage.empty()) return;

        startJob(versionCheckJob,
                 [this, urls, installed, checkedLanguage]
                 {
                     auto etags = updateManager.remoteVersions(urls);
                     std::map<std::string, bool> damaged;
                     for(const auto& [name, files] : installed)
                         damaged[name] = !UpdateManager::isIntact(files.installPath, files.folder, files.fileHash);
                     queue.post(
                         [this, etags, damaged, checkedLanguage]
                         {
                             for(const auto& [name, language] : checkedLanguage)
                             {
                                 auto& channel = data.channels[name];
                                 // The user may have picked another language meanwhile
                                 if(channel.language != language) continue;
                                 if(etags.contains(name)) channel.remoteEtag = etags.at(name);
                                 if(damaged.contains(name)) channel.damaged = damaged.at(name);
                             }
                             publish();
                         });
                 });
    }

    // force: download even if the source reports "not modified", e.g. to repair a changed file
    std::optional<UpdateManager::Job> makeUpdateJob (const std::string& name, const Database::ChannelData& channel, bool force)
    {
        if(channel.installPath.empty()) return std::nullopt;
        // Offline or not offered right now: leave the installed translation alone instead of removing it
        if(!isAvailable(name, channel.language))
        {
            LOG_DEBUG(lg) << "update - " << name << ": " << channel.language << " is not available, channel skipped";
            return std::nullopt;
        }

        // Same language as installed and intact: the source can answer "not modified" instead of sending the file again
        bool reuse = !force && !channel.damaged && channel.installed.language == channel.language;
        UpdateManager::Job job{name, channel.installPath, channel.language, "", {}, managedFolders(), reuse ? channel.installed.etag : ""};
        if(const auto* language = findLanguage(channel.language))
        {
            job.folder = language->folder;
            job.urls = language->urls.at(name);
        }
        return job;
    }

    void runUpdate (std::vector<UpdateManager::Job> jobs, bool launchGame)
    {
        data.update = Protocol::UpdateStatus::Running;
        startJob(updateJob,
                 [this, jobs, launchGame, rsiLauncherPath = data.rsiLauncherPath]
                 {
                     auto results = updateManager.update(jobs);
                     // Also after a partial failure, the other channels are still playable
                     if(launchGame && !data.shuttingDown) updateManager.startGame(rsiLauncherPath);
                     queue.post(
                         [this, results, jobCount = jobs.size()]
                         {
                             bool success = results.size() == jobCount;
                             for(const auto& result : results)
                             {
                                 success &= result.success;
                                 auto& channel = data.channels[result.channel];
                                 channel.lastUpdateFailed = !result.success;
                                 if(!result.success) continue;
                                 bool english = result.language == Protocol::ENGLISH;
                                 channel.installed = {english ? "" : result.language, result.etag, result.fileHash};
                                 channel.damaged = false;
                                 if(channel.language == result.language) channel.remoteEtag = result.etag;
                             }
                             data.update = success ? Protocol::UpdateStatus::Succeeded : Protocol::UpdateStatus::Failed;
                             saveSettings();
                             publish();
                         });
                 });
    }

    Protocol::State snapshot () const
    {
        Protocol::State state;

        state.languages.push_back({Protocol::ENGLISH, "English", {}});
        for(const auto& language : data.languages) state.languages.push_back({language.code, language.name, language.credit});

        // Known channels in display order, then whatever else the sources list
        std::vector<std::string> names = Protocol::CHANNEL_NAMES;
        for(const auto& [name, channel] : data.channels)
            if(std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);

        for(const auto& name : names)
        {
            const auto& channel = data.channels.at(name);
            state.channels.push_back({name, channel.installPath.string(), channel.language, availableLanguages(name), channelStatus(channel)});
        }

        state.rsiLauncherPath = data.rsiLauncherPath.string();
        state.options = data.options;
        state.update = data.update;
        state.searching = data.searching;
        state.selfUpdate = data.selfUpdate;
        return state;
    }

    void publish ()
    {
        if(data.shuttingDown) return;
        Protocol::State state = snapshot();
        LOG_DEBUG(lg) << "publish() - state:\n" << nlohmann::json(state).dump(4);
        stateCallback(state);
    }

    void loadSettings ()
    {
        SettingsFile file = settingsJsonParser.load();
        for(const auto& name : Protocol::CHANNEL_NAMES) data.channels[name];
        for(const auto& [name, settings] : file.channels)
        {
            data.channels[name].installPath = settings.installPath;
            data.channels[name].language = settings.language;
            data.channels[name].installed = settings.installed;
        }
        data.rsiLauncherPath = file.rsiLauncherPath;
        data.options = file.options;
        // Moves settings of the former SC Deutsch Launcher to the new location right away
        saveSettings();
    }

    void saveSettings ()
    {
        SettingsFile file;
        for(const auto& [name, channel] : data.channels) file.channels[name] = {channel.installPath.string(), channel.language, channel.installed};
        file.rsiLauncherPath = data.rsiLauncherPath.string();
        file.options = data.options;
        settingsJsonParser.save(file);
    }

    void loadLanguages ()
    {
        startJob(languagesJob,
                 [this]
                 {
                     auto languages = languageCatalogParser.getLanguages();
                     queue.post(
                         [this, languages = std::move(languages)]
                         {
                             data.languages = languages;
                             // Channels only a source knows about (e.g. a new test channel) get a row too
                             for(const auto& language : data.languages)
                                 for(const auto& [channel, urls] : language.urls) data.channels[channel];
                             publish();
                             checkChannels();
                         });
                 });
    }

    // --- Command handlers, all on the backend thread ---

    void handle (const Protocol::SetLanguage& command)
    {
        if(command.language != Protocol::ENGLISH && !findLanguage(command.language))
        {
            LOG_ERROR(lg) << "unknown language: " << command.language;
            return;
        }
        if(auto* channel = findChannel(command.channel))
        {
            channel->language = command.language;
            // The known version and the last result belonged to the previous language
            channel->remoteEtag.clear();
            channel->damaged = false;
            channel->lastUpdateFailed = false;
        }
    }

    void handle (const Protocol::SetInstallPath& command)
    {
        if(auto* channel = findChannel(command.channel))
        {
            channel->installPath = command.path;
            // The record described the previous folder
            channel->installed = {};
            channel->remoteEtag.clear();
            channel->damaged = false;
            channel->lastUpdateFailed = false;
        }
    }

    void handle (const Protocol::SetRsiLauncherPath& command) { data.rsiLauncherPath = command.path; }

    void handle (const Protocol::SetOption& command)
    {
        // The field names of Options are the valid option names
        nlohmann::json options = data.options;
        if(!options.contains(command.option))
        {
            LOG_ERROR(lg) << "unknown option: " << command.option;
            return;
        }
        options[command.option] = command.value;
        data.options = options.get<Protocol::Options>();
    }

    void handle (const Protocol::StartUpdate&)
    {
        if(data.update == Protocol::UpdateStatus::Running)
        {
            LOG_DEBUG(lg) << "startUpdate - update already running";
            return;
        }

        // The jobs work on copies, the user may change settings while they run
        std::vector<UpdateManager::Job> jobs;
        for(const auto& [name, channel] : data.channels)
            if(auto job = makeUpdateJob(name, channel, false)) jobs.push_back(std::move(*job));

        if(jobs.empty())
        {
            LOG_DEBUG(lg) << "startUpdate - nothing to update";
            return;
        }
        runUpdate(std::move(jobs), data.options.launchGameAfterUpdate);
    }

    void handle (const Protocol::RepairChannel& command)
    {
        if(data.update == Protocol::UpdateStatus::Running)
        {
            LOG_DEBUG(lg) << "repairChannel - an update is running";
            return;
        }
        const auto* channel = findChannel(command.channel);
        if(!channel) return;
        auto job = makeUpdateJob(command.channel, *channel, true);
        if(!job) return;
        // A repair never starts the game
        runUpdate({std::move(*job)}, false);
    }

    void handle (const Protocol::CheckForUpdate&)
    {
        using Protocol::SelfUpdateStatus;
        auto status = data.selfUpdate.status;
        if(status == SelfUpdateStatus::Checking || status == SelfUpdateStatus::Downloading || status == SelfUpdateStatus::Installing) return;
        data.selfUpdate.status = SelfUpdateStatus::Checking;
        startJob(selfUpdateJob,
                 [this, version = data.selfUpdate.currentVersion]
                 {
                     auto installation = SelfUpdateManager::detectInstallation();
                     auto result = selfUpdateManager.check(version, installation);
                     queue.post(
                         [this, result, installation]
                         {
                             data.selfUpdate = result.update;
                             data.releaseAssets = result.assets;
                             data.installation = installation;
                             publish();
                         });
                 });
    }

    void handle (const Protocol::InstallUpdate&)
    {
        using Protocol::SelfUpdateStatus;
        if(data.selfUpdate.status != SelfUpdateStatus::Available || !data.selfUpdate.canInstall || !data.releaseAssets)
        {
            LOG_ERROR(lg) << "installUpdate - no installable update";
            return;
        }

        data.selfUpdate.status = SelfUpdateStatus::Downloading;
        data.selfUpdate.progress = 0;
        data.selfUpdate.error.clear();
        startJob(selfUpdateJob,
                 [this, release = *data.releaseAssets, installation = data.installation, version = data.selfUpdate.currentVersion]
                 {
                     // Whole percents are enough for the GUI and keep the state updates few
                     auto lastPercent = std::make_shared<int>(-1);
                     auto error = selfUpdateManager.install(release, installation, version,
                                                            [this, lastPercent] (double progress)
                                                            {
                                                                int percent = static_cast<int>(progress * 100);
                                                                if(percent == *lastPercent) return;
                                                                *lastPercent = percent;
                                                                queue.post(
                                                                    [this, progress]
                                                                    {
                                                                        data.selfUpdate.progress = progress;
                                                                        // After the download only the installation is left
                                                                        if(progress >= 1)
                                                                            data.selfUpdate.status = Protocol::SelfUpdateStatus::Installing;
                                                                        publish();
                                                                    });
                                                            });
                     queue.post(
                         [this, error]
                         {
                             data.selfUpdate.status = error ? Protocol::SelfUpdateStatus::Error : Protocol::SelfUpdateStatus::Installed;
                             data.selfUpdate.error = error.value_or("");
                             // A failed attempt can be retried after checking again
                             data.selfUpdate.canInstall = false;
                             publish();
                         });
                 });
    }

    void handle (const Protocol::AutoSearch&)
    {
        if(data.searching)
        {
            LOG_DEBUG(lg) << "autoSearch - search already running";
            return;
        }

        data.searching = true;
        startJob(searchJob,
                 [this]
                 {
                     auto result = directoryFinder.findRsiInstallation(data.shuttingDown);
                     queue.post(
                         [this, result]
                         {
                             if(result)
                             {
                                 data.rsiLauncherPath = result->rsiLauncherPath;
                                 for(const auto& [name, path] : result->channelPaths) data.channels[name].installPath = path;
                                 saveSettings();
                             }
                             data.searching = false;
                             publish();
                         });
                 });
    }

   public:
    // The callback is called on the backend thread
    // version: of this launcher, compared with the published releases
    BackEnd (StateCallback stateCallback, std::string version)
        : lg("BackEnd")
        , settingsJsonParser(PATHS::JSON_FILES::SETTINGS_USER)
        , updateManager(data.shuttingDown)
        , selfUpdateManager(data.shuttingDown, updateSigningKey())
        , stateCallback(std::move(stateCallback))
    {
        data.selfUpdate.currentVersion = std::move(version);
        // Settings first, so the GUI can show them while the language list is still downloading
        queue.post(
            [this]
            {
                loadSettings();
                publish();
                loadLanguages();
                if(data.options.checkLauncherUpdatesAtStart) handle(Protocol::CheckForUpdate{});
            });
        LOG_DEBUG(lg) << "instanziated";
    }

    ~BackEnd ()
    {
        data.shuttingDown = true;
        // Runs the commands still waiting, then no one starts new jobs
        queue.stop();
        for(auto* job : {&languagesJob, &updateJob, &searchJob, &versionCheckJob, &selfUpdateJob})
            if(job->joinable()) job->join();
        LOG_DEBUG(lg) << "destructed";
    }

    // Thread safe, returns immediately
    void execute (Protocol::Command command)
    {
        queue.post(
            [this, command = std::move(command)]
            {
                LOG_DEBUG(lg) << "execute() - " << Protocol::toString(command);
                std::visit([this] (const auto& c) { handle(c); }, command);
                saveSettings();
                publish();
            });
    }
};
