#pragma once

#include <algorithm>
#include <functional>
#include <thread>

#include "backend/DirectoryFinder.hpp"
#include "backend/utils/TaskQueue.hpp"
#include "backend/utils/paths.hpp"
#include "database/DataBase.hpp"
#include "jsonParser/LanguageCatalogParser.hpp"
#include "jsonParser/SettingsJsonParser.hpp"
#include "logging/logging.h"
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
    DirectoryFinder directoryFinder;
    StateCallback stateCallback;

    std::thread languagesJob;
    std::thread updateJob;
    std::thread searchJob;
    std::thread versionCheckJob;

    // Declared last: its thread uses all members above
    TaskQueue queue;

    // Only called on the backend thread. A job that is not running anymore is joined right away,
    // it never waits for the backend thread.
    void startJob (std::thread& job, std::function<void()> function)
    {
        if(job.joinable()) job.join();
        job = std::thread(std::move(function));
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
        if(!channel.remoteEtag.empty() && channel.remoteEtag != channel.installed.etag) return ChannelStatus::UpdateAvailable;
        return ChannelStatus::Installed;
    }

    // Asks the sources whether the chosen translations changed, without downloading them
    void checkRemoteVersions ()
    {
        std::map<std::string, std::vector<std::string>> urls;
        std::map<std::string, std::string> checkedLanguage;
        for(const auto& [name, channel] : data.channels)
        {
            if(channel.installPath.empty() || channel.language == Protocol::ENGLISH || !isAvailable(name, channel.language)) continue;
            urls[name] = findLanguage(channel.language)->urls.at(name);
            checkedLanguage[name] = channel.language;
        }
        if(urls.empty()) return;

        startJob(versionCheckJob,
                 [this, urls, checkedLanguage]
                 {
                     auto etags = updateManager.remoteVersions(urls);
                     queue.post(
                         [this, etags, checkedLanguage]
                         {
                             for(const auto& [name, etag] : etags)
                             {
                                 auto& channel = data.channels[name];
                                 // The user may have picked another language meanwhile
                                 if(channel.language == checkedLanguage.at(name)) channel.remoteEtag = etag;
                             }
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
                             checkRemoteVersions();
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
            // The known version belonged to the previous language
            channel->remoteEtag.clear();
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

        // The job works on a copy, the user may change settings while it runs
        std::vector<UpdateManager::Job> jobs;
        for(const auto& [name, channel] : data.channels)
        {
            if(channel.installPath.empty()) continue;
            // Offline or not offered right now: leave the installed translation alone instead of removing it
            if(!isAvailable(name, channel.language))
            {
                LOG_DEBUG(lg) << "startUpdate - " << name << ": " << channel.language << " is not available, channel skipped";
                continue;
            }

            // Same language as installed: the source can answer "not modified" instead of sending the file again
            std::string etag = channel.installed.language == channel.language ? channel.installed.etag : "";
            UpdateManager::Job job{name, channel.installPath, channel.language, "", {}, managedFolders(), etag};
            if(const auto* language = findLanguage(channel.language))
            {
                job.folder = language->folder;
                job.urls = language->urls.at(name);
            }
            jobs.push_back(std::move(job));
        }

        if(jobs.empty())
        {
            LOG_DEBUG(lg) << "startUpdate - nothing to update";
            return;
        }

        data.update = Protocol::UpdateStatus::Running;
        bool launchGame = data.options.launchGameAfterUpdate;
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
                                 if(!result.success) continue;
                                 auto& channel = data.channels[result.channel];
                                 bool english = result.language == Protocol::ENGLISH;
                                 channel.installed = {english ? "" : result.language, result.etag};
                                 if(channel.language == result.language) channel.remoteEtag = result.etag;
                             }
                             data.update = success ? Protocol::UpdateStatus::Succeeded : Protocol::UpdateStatus::Failed;
                             saveSettings();
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
    explicit BackEnd (StateCallback stateCallback)
        : lg("BackEnd")
        , settingsJsonParser(PATHS::JSON_FILES::SETTINGS_USER)
        , updateManager(data.shuttingDown)
        , stateCallback(std::move(stateCallback))
    {
        // Settings first, so the GUI can show them while the language list is still downloading
        queue.post(
            [this]
            {
                loadSettings();
                publish();
                loadLanguages();
            });
        LOG_DEBUG(lg) << "instanziated";
    }

    ~BackEnd ()
    {
        data.shuttingDown = true;
        // Runs the commands still waiting, then no one starts new jobs
        queue.stop();
        for(auto* job : {&languagesJob, &updateJob, &searchJob, &versionCheckJob})
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
