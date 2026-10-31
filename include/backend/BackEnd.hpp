#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <regex>
#include <set>
#include <thread>

#include "UpdateSigningKey.hpp"
#include "backend/DirectoryFinder.hpp"
#include "backend/utils/TaskQueue.hpp"
#include "backend/utils/paths.hpp"
#include "database/DataBase.hpp"
#include "jsonParser/LanguageCatalogParser.hpp"
#include "jsonParser/SettingsJsonParser.hpp"
#include "logging/logging.h"
#include "managers/CacheCleaner.hpp"
#include "managers/CompanionPrograms.hpp"
#include "managers/Diagnostics.hpp"
#include "managers/GameSettings.hpp"
#include "managers/ProfileBackup.hpp"
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
    // Only used by the update job
    ItemValues itemValues;
    SelfUpdateManager selfUpdateManager;
    DirectoryFinder directoryFinder;
    CacheCleaner cacheCleaner;
    Diagnostics diagnostics;
    ProfileBackup profileBackup;
    CompanionPrograms companionPrograms;
    StateCallback stateCallback;

    std::thread languagesJob;
    std::thread updateJob;
    std::thread searchJob;
    std::thread versionCheckJob;
    std::thread selfUpdateJob;
    std::thread gameJob;
    std::thread gameWatchJob;
    std::thread checkTimerJob;
    std::thread whatsNewJob;
    std::thread cacheJob;
    std::thread diagnosticsJob;
    std::thread backupJob;

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

    // A catalogue language, or for Protocol::CUSTOM the channel's own source as a language of this channel only
    std::optional<Database::AvailableLanguage> languageFor (const std::string& channel, const std::string& code) const
    {
        if(code == Protocol::CUSTOM)
        {
            auto it = data.channels.find(channel);
            if(it == data.channels.end() || it->second.custom.url.empty()) return std::nullopt;
            const auto& custom = it->second.custom;
            return Database::AvailableLanguage{Protocol::CUSTOM, "", custom.folder, {{channel, {custom.url}}}, {}, {}};
        }
        if(const auto* language = findLanguage(code)) return *language;
        return std::nullopt;
    }

    std::vector<std::string> availableLanguages (const std::string& channel) const
    {
        std::vector<std::string> codes{Protocol::ENGLISH};
        for(const auto& language : data.languages)
            if(language.urls.contains(channel)) codes.push_back(language.code);
        if(languageFor(channel, Protocol::CUSTOM)) codes.push_back(Protocol::CUSTOM);
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

    // Folders the launcher may have installed into on this channel: all catalogue folders and its own source's
    std::vector<std::string> managedFolders (const Database::ChannelData& channel) const
    {
        std::vector<std::string> folders;
        auto add = [&] (const std::string& folder)
        {
            if(!folder.empty() && std::find(folders.begin(), folders.end(), folder) == folders.end()) folders.push_back(folder);
        };
        for(const auto& language : data.languages) add(language.folder);
        add(channel.custom.folder);
        // A former own source of this channel may still be installed
        if(channel.installed.language == Protocol::CUSTOM) add(channel.installed.folder);
        return folders;
    }

    static bool hasTranslationFile (const std::filesystem::path& installPath, const std::string& folder)
    {
        std::error_code ec;
        return std::filesystem::exists(installPath / "data" / "Localization" / folder / "global.ini", ec);
    }

    Protocol::ChannelStatus channelStatus (const std::string& name, const Database::ChannelData& channel) const
    {
        using Protocol::ChannelStatus;
        if(channel.installPath.empty()) return ChannelStatus::NoPath;
        if(channel.lastUpdateFailed) return ChannelStatus::Failed;

        if(channel.language == Protocol::ENGLISH)
        {
            // English means no translation files; any left over still has to be removed
            auto folders = managedFolders(channel);
            bool leftOver =
                std::any_of(folders.begin(), folders.end(), [&] (const auto& folder) { return hasTranslationFile(channel.installPath, folder); });
            return leftOver ? ChannelStatus::UpdateAvailable : ChannelStatus::Installed;
        }

        auto language = languageFor(name, channel.language);
        if(!language || !hasTranslationFile(channel.installPath, language->folder)) return ChannelStatus::NotInstalled;
        // Installed by hand, by the former SC Deutsch Launcher or as another language of the same folder: version unknown
        if(channel.installed.language != channel.language) return ChannelStatus::UpdateAvailable;
        if(channel.damaged) return ChannelStatus::Damaged;
        // Switched on or off since the file was installed
        if(ItemValues::kinds(data.options.itemNameDetails, data.options.itemDescriptionStats) != ItemValues::kindsOf(channel.installed.itemValues))
            return ChannelStatus::UpdateAvailable;
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
            auto language = languageFor(name, channel.language);
            if(!language) continue;
            checkedLanguage[name] = channel.language;
            if(isAvailable(name, channel.language)) urls[name] = language->urls.at(name);
            if(channel.installed.language == channel.language) installed[name] = {channel.installPath, language->folder, channel.installed.fileHash};
        }
        if(checkedLanguage.empty())
        {
            data.checking = false;
            return;
        }

        data.checking = true;
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
                             data.checking = false;
                             publish();
                             updateInBackground();
                         });
                 });
    }

    static std::chrono::seconds checkInterval ()
    {
#ifdef SC_TESTING
        // Test builds only: a test cannot wait an hour
        if(const char* seconds = std::getenv("SC_CHECK_INTERVAL_SECONDS")) return std::chrono::seconds(std::atoi(seconds));
#endif
        return std::chrono::hours(1);
    }

    // With the option on, installs what the last check found, unless the game reads the files right now;
    // the game watcher checks again once it has been closed
    void updateInBackground ()
    {
        if(!data.backgroundChecks || !data.options.installNewTranslations) return;
        if(data.update == Protocol::UpdateStatus::Running || data.game == Protocol::GameStart::Starting) return;
        bool pending = std::any_of(data.channels.begin(), data.channels.end(),
                                   [this] (const auto& entry)
                                   {
                                       auto status = channelStatus(entry.first, entry.second);
                                       // A failed channel is tried again with every check
                                       return status == Protocol::ChannelStatus::NotInstalled || status == Protocol::ChannelStatus::UpdateAvailable ||
                                              status == Protocol::ChannelStatus::Damaged || status == Protocol::ChannelStatus::Failed;
                                   });
        if(!pending) return;
        if(gamePlaying())
        {
            LOG_INFO(lg) << "background update - waiting until Star Citizen has been closed";
            return;
        }
        LOG_INFO(lg) << "background update";
        handle(Protocol::StartUpdate{.allowGameStart = false});
        publish();
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
        UpdateManager::Job job{name, channel.installPath, channel.language, "", {}, managedFolders(channel), reuse ? channel.installed.etag : ""};
        if(channel.installed.language == channel.language) job.installedItemValues = channel.installed.itemValues;
        if(auto language = languageFor(name, channel.language))
        {
            job.folder = language->folder;
            job.urls = language->urls.at(name);
        }
        return job;
    }

    void startGame ()
    {
        data.game = Protocol::GameStart::Starting;
        startJob(gameJob,
                 [this, rsiLauncherPath = data.rsiLauncherPath]
                 {
                     auto result = updateManager.startGame(rsiLauncherPath);
                     queue.post(
                         [this, result]
                         {
                             // It may have quit again or the game may run already; the watcher saw that
                             data.game = result == Protocol::GameStart::Running ? gameStatus() : result;
                             publish();
                         });
                 });
    }

    // Notices when the RSI Launcher or the game starts and quits, also when it was started without this launcher
    void watchGame ()
    {
        // Known right away, so a command line update never starts while the game runs
        data.gameProcess = UpdateManager::runningGameProcess();
        data.game = gameStatus();
        startJob(gameWatchJob,
                 [this, seen = data.gameProcess] () mutable
                 {
                     while(!data.shuttingDown)
                     {
                         if(auto now = UpdateManager::runningGameProcess(); now != seen)
                         {
                             seen = now;
                             queue.post([this, now] { gameProcessChanged(now); });
                         }
                         for(int i = 0; i < 8 && !data.shuttingDown; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(250));
                     }
                 });
    }

    // What the watcher saw, as shown to the user
    Protocol::GameStart gameStatus () const
    {
        switch(data.gameProcess)
        {
            case Database::GameProcess::Game:
                return Protocol::GameStart::Playing;
            case Database::GameProcess::Launcher:
                return Protocol::GameStart::Running;
            default:
                return Protocol::GameStart::Idle;
        }
    }

    void gameProcessChanged (Database::GameProcess process)
    {
        // A game update may have replaced the translation, and an update may have waited for the game to close
        bool gameClosed = data.gameProcess == Database::GameProcess::Game && process != Database::GameProcess::Game;
        bool gameStarted = data.gameProcess != Database::GameProcess::Game && process == Database::GameProcess::Game;
        data.gameProcess = process;
        if(gameStarted) data.failedPrograms = companionPrograms.start(data.programs);
        if(gameClosed) companionPrograms.stop();
        if(gameClosed) checkGameSettings();
        if(gameClosed)
            for(auto& [name, channel] : data.channels) applyGraphicsApi(channel);
        if(gameClosed && data.backgroundChecks) checkChannels();
        // A start in progress decides itself whether the RSI Launcher keeps running; a failed start stays visible
        bool shownRunning = data.game == Protocol::GameStart::Running || data.game == Protocol::GameStart::Playing;
        if(data.game != Protocol::GameStart::Starting && (process != Database::GameProcess::None || shownRunning)) data.game = gameStatus();
        publish();
    }

    bool gamePlaying () const { return data.gameProcess == Database::GameProcess::Game; }

    // Runs in the update job: one plan per language, and a file whose values change is downloaded again instead of
    // reused, so values are only ever added to a file as downloaded
    void prepareItemValues (std::vector<UpdateManager::Job>& jobs, bool names, bool stats)
    {
        std::map<std::string, std::shared_ptr<const ItemValues::Plan>> plans;
        for(auto& job : jobs)
        {
            if(job.folder.empty()) continue;
            std::shared_ptr<const ItemValues::Plan> plan;
            if(names || stats)
            {
                if(!plans.contains(job.language))
                {
                    auto built = itemValues.build(job.language, names, stats);
                    plans[job.language] = built ? std::make_shared<const ItemValues::Plan>(std::move(*built)) : nullptr;
                }
                plan = plans[job.language];
                // Wiki or mapping unreachable: an unchanged file keeps the values it has
                if(!plan) continue;
            }
            job.itemValues = plan;
            if((plan ? plan->signature : "") != job.installedItemValues) job.etag.clear();
        }
    }

    void runUpdate (std::vector<UpdateManager::Job> jobs, bool launchGame)
    {
        data.update = Protocol::UpdateStatus::Running;
        // A repair may run while the RSI Launcher is still starting; that start reports its own result
        if(data.game != Protocol::GameStart::Starting) data.game = Protocol::GameStart::Idle;
        startJob(updateJob,
                 [this, jobs, launchGame, names = data.options.itemNameDetails, stats = data.options.itemDescriptionStats] () mutable
                 {
                     prepareItemValues(jobs, names, stats);
                     auto results = updateManager.update(jobs);
                     queue.post(
                         [this, results, launchGame, jobCount = jobs.size()]
                         {
                             bool success = results.size() == jobCount;
                             for(const auto& result : results)
                             {
                                 success &= result.success;
                                 auto& channel = data.channels[result.channel];
                                 channel.lastUpdateFailed = !result.success;
                                 if(!result.success) continue;
                                 bool english = result.language == Protocol::ENGLISH;
                                 channel.installed = {english ? "" : result.language, result.etag, result.fileHash, result.folder, result.itemValues};
                                 channel.damaged = false;
                                 if(channel.language == result.language) channel.remoteEtag = result.etag;
                             }
                             data.update = success ? Protocol::UpdateStatus::Succeeded : Protocol::UpdateStatus::Failed;
                             // Also after a partial failure, the other channels are still playable
                             if(launchGame && !data.shuttingDown) startGame();
                             saveSettings();
                             publish();
                         });
                 });
    }

    // Language code -> notice for this channel; a channel's own notice wins over the one for all channels
    std::map<std::string, std::string> notices (const std::string& channel) const
    {
        std::map<std::string, std::string> result;
        for(const auto& language : data.languages)
        {
            if(auto it = language.notices.find(channel); it != language.notices.end())
                result[language.code] = it->second;
            else if(auto all = language.notices.find("*"); all != language.notices.end())
                result[language.code] = all->second;
        }
        return result;
    }

    Protocol::State snapshot () const
    {
        Protocol::State state;

        state.languages.push_back({Protocol::ENGLISH, "English", {}, ""});
        for(const auto& language : data.languages) state.languages.push_back({language.code, language.name, language.credit, language.folder});

        // Known channels in display order, then whatever else the sources list
        std::vector<std::string> names = Protocol::CHANNEL_NAMES;
        for(const auto& [name, channel] : data.channels)
            if(std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);

        for(const auto& name : names)
        {
            const auto& channel = data.channels.at(name);
            state.channels.push_back({name, channel.installPath.string(), channel.language, availableLanguages(name), channelStatus(name, channel),
                                      notices(name), channel.custom, channel.gameSettings, channel.settingsChecked, channel.ignoredSettings,
                                      channel.settingsError, channel.graphicsApiWaiting});
        }

        state.rsiLauncherPath = data.rsiLauncherPath.string();
        state.options = data.options;
        state.update = data.update;
        state.game = data.game;
        state.searching = data.searching;
        state.checking = data.checking;
        state.handledCommands = data.handledCommands;
        state.selfUpdate = data.selfUpdate;
        state.whatsNew = data.whatsNew;
        state.caches = data.caches;
        state.diagnostics = data.diagnostics;
        state.backups = data.backups;
        state.programs = data.programs;
        state.failedPrograms = data.failedPrograms;
        state.interfaceLanguage = data.interfaceLanguage;
        state.setupNeeded = !data.setupDone;
        state.theme = data.theme;
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
            data.channels[name].custom = settings.custom;
            data.channels[name].gameSettings = settings.gameSettings;
            data.channels[name].managedKeys = settings.managedKeys;
            data.channels[name].settingsWritten = settings.settingsWritten;
        }
        data.rsiLauncherPath = file.rsiLauncherPath;
        data.options = file.options;
        data.lastSeenVersion = file.lastSeenVersion;
        data.programs = file.programs;
        data.interfaceLanguage = file.interfaceLanguage;
        data.theme = file.theme;
        // Settings from before the assistant: whoever set a game folder is set up already
        data.setupDone = file.setupDone || std::any_of(file.channels.begin(), file.channels.end(),
                                                       [] (const auto& channel) { return !channel.second.installPath.empty(); });
        // Moves settings of the former SC Deutsch Launcher to the new location right away
        saveSettings();
        checkGameSettings();
        for(auto& [name, channel] : data.channels) applyGraphicsApi(channel);
    }

    SettingsFile settingsFile () const
    {
        SettingsFile file;
        for(const auto& [name, channel] : data.channels)
            file.channels[name] = {channel.installPath.string(), channel.language,    channel.installed,      channel.custom,
                                   channel.gameSettings,         channel.managedKeys, channel.settingsWritten};
        file.rsiLauncherPath = data.rsiLauncherPath.string();
        file.options = data.options;
        file.lastSeenVersion = data.lastSeenVersion;
        file.programs = data.programs;
        file.interfaceLanguage = data.interfaceLanguage;
        file.setupDone = data.setupDone;
        file.theme = data.theme;
        return file;
    }

    void saveSettings () { settingsJsonParser.save(settingsFile()); }

    // Reads Game.log of every channel with own settings: once the game started after they were written, it tells which
    // of them it ignored. Game.log is rewritten at every game start.
    void checkGameSettings ()
    {
        for(auto& [name, channel] : data.channels)
        {
            if(channel.installPath.empty() || channel.managedKeys.empty()) continue;
            std::error_code ec;
            auto log = channel.installPath / "Game.log";
            auto modified = std::filesystem::last_write_time(log, ec);
            if(ec) continue;
            // file_clock::to_sys, not clock_cast: GCC 12 (Debian 12) lacks the latter for file_clock
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::file_clock::to_sys(modified).time_since_epoch()).count();
            if(seconds <= channel.settingsWritten) continue;
            auto ignored = GameSettings::notWhitelisted(log);
            channel.ignoredSettings.clear();
            for(const auto& key : channel.managedKeys)
                if(ignored.contains(key)) channel.ignoredSettings.push_back(key);
            channel.settingsChecked = true;
        }
    }

    // The game makes a new GraphicsSettings.json with its defaults for every game version, and its own menu may change
    // it; so the choice is written again whenever the game quits. Never while the game runs.
    void applyGraphicsApi (Database::ChannelData& channel)
    {
        channel.graphicsApiWaiting = false;
        if(channel.gameSettings.graphicsApi.empty() || channel.installPath.empty()) return;
        auto prefix = UpdateManager::findWinePrefix(channel.installPath);
        channel.graphicsApiWaiting =
            gamePlaying() || !prefix ||
            !GameSettings::applyGraphicsApi(*prefix, GameSettings::branchOf(channel.installPath / "Game.log"), channel.gameSettings.graphicsApi);
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
                             // Before publishing: without channels to ask, the check is done right here
                             checkChannels();
                             publish();
                         });
                 });
    }

    // --- Command handlers, all on the backend thread ---

    void handle (const Protocol::SetLanguage& command)
    {
        if(command.language != Protocol::ENGLISH && !languageFor(command.channel, command.language))
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
        // Switched on: what the last check found is installed right away
        if((command.option == "installNewTranslations" && command.value) || command.option == "itemNameDetails" ||
           command.option == "itemDescriptionStats")
            updateInBackground();
    }

    void handle (const Protocol::StartUpdate& command)
    {
        if(data.update == Protocol::UpdateStatus::Running || data.game == Protocol::GameStart::Starting)
        {
            LOG_DEBUG(lg) << "startUpdate - update or game start already running";
            return;
        }
        // The running game reads the translation files
        if(gamePlaying())
        {
            LOG_DEBUG(lg) << "startUpdate - Star Citizen is running";
            return;
        }

        // The jobs work on copies, the user may change settings while they run
        std::vector<UpdateManager::Job> jobs;
        for(const auto& [name, channel] : data.channels)
        {
            bool chosen = command.channels.empty() || std::find(command.channels.begin(), command.channels.end(), name) != command.channels.end();
            if(!chosen) continue;
            if(auto job = makeUpdateJob(name, channel, false)) jobs.push_back(std::move(*job));
        }

        if(jobs.empty())
        {
            LOG_DEBUG(lg) << "startUpdate - nothing to update";
            return;
        }
        runUpdate(std::move(jobs), command.allowGameStart && data.options.launchGameAfterUpdate);
    }

    void handle (const Protocol::StartGame&)
    {
        if(data.update == Protocol::UpdateStatus::Running || data.game == Protocol::GameStart::Starting ||
           data.gameProcess != Database::GameProcess::None)
        {
            LOG_DEBUG(lg) << "startGame - update, game start or game already running";
            return;
        }
        startGame();
    }

    void handle (const Protocol::StartBackgroundChecks&)
    {
        if(data.backgroundChecks) return;
        data.backgroundChecks = true;
        // The first check runs when the languages have loaded; an earlier one has found something to install
        if(!data.checking) updateInBackground();
        startJob(checkTimerJob,
                 [this, interval = checkInterval()]
                 {
                     auto next = std::chrono::steady_clock::now() + interval;
                     while(!data.shuttingDown)
                     {
                         std::this_thread::sleep_for(std::chrono::milliseconds(250));
                         if(std::chrono::steady_clock::now() < next) continue;
                         next += interval;
                         queue.post([this] { checkChannels(); });
                     }
                 });
    }

    void handle (const Protocol::SetCustomSource& command)
    {
        auto* channel = findChannel(command.channel);
        if(!channel) return;
        if(!command.url.empty() && (command.url.size() > 2048 || !LanguageCatalogParser::isAllowedUrl(command.url) ||
                                    !LanguageCatalogParser::isSafeFolderName(command.folder)))
        {
            LOG_ERROR(lg) << "setCustomSource - rejected: " << command.url << " -> " << command.folder;
            return;
        }

        channel->custom = {command.url, command.url.empty() ? "" : command.folder};
        if(channel->language != Protocol::CUSTOM) return;
        // Removing the source while it is chosen falls back to English, so the next update removes its files
        if(command.url.empty()) channel->language = Protocol::ENGLISH;
        channel->remoteEtag.clear();
        channel->damaged = false;
        channel->lastUpdateFailed = false;
    }

    void handle (const Protocol::RepairChannel& command)
    {
        if(data.update == Protocol::UpdateStatus::Running || gamePlaying())
        {
            LOG_DEBUG(lg) << "repairChannel - an update or Star Citizen is running";
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

    void handle (const Protocol::LoadWhatsNew&)
    {
        const std::string current = data.selfUpdate.currentVersion;
        const std::string since = data.lastSeenVersion;
        if(!data.whatsNew.version.empty()) return;
        // A local development build (0.9.0-dev+<commit>) is no release: it must not mark 0.9.0's notes as seen
        if(current.find("-dev") != std::string::npos) return;
        if(since.empty())
        {
            // A new installation has nothing to catch up on; a launcher that is already set up came from a version
            // before release notes were shown and gets the notes of this one
            bool setUp = !data.rsiLauncherPath.empty() ||
                         std::any_of(data.channels.begin(), data.channels.end(), [] (const auto& c) { return !c.second.installPath.empty(); });
            if(!setUp)
            {
                data.lastSeenVersion = current;
                return;
            }
        }
        // Also after a downgrade or with a build from source, which is not a release
        else if(!SelfUpdateManager::isNewer(current, since))
            return;

        startJob(whatsNewJob,
                 [this, since, current]
                 {
                     auto notes = selfUpdateManager.notesSince(since, current);
                     queue.post(
                         [this, notes, current]
                         {
                             // Offline: asked again at the next start
                             if(!notes) return;
                             if(notes->empty())
                             {
                                 // No release with notes for this version, e.g. a test build
                                 data.lastSeenVersion = current;
                                 saveSettings();
                             }
                             else
                                 data.whatsNew = {current, *notes};
                             publish();
                         });
                 });
    }

    void handle (const Protocol::ScanCaches&)
    {
        if(data.caches.busy) return;
        // Every configured channel folder and the Wine prefixes around them and the RSI Launcher
        std::vector<std::filesystem::path> folders;
        std::vector<std::filesystem::path> prefixes;
        auto addPrefix = [&prefixes] (const std::filesystem::path& path)
        {
            if(path.empty()) return;
            auto prefix = UpdateManager::findWinePrefix(path);
            if(prefix && std::find(prefixes.begin(), prefixes.end(), *prefix) == prefixes.end()) prefixes.push_back(*prefix);
        };
        for(const auto& [name, channel] : data.channels)
            if(!channel.installPath.empty())
            {
                folders.push_back(channel.installPath);
                addPrefix(channel.installPath);
            }
        addPrefix(data.rsiLauncherPath);

        data.caches.busy = true;
        startJob(cacheJob,
                 [this, folders, prefixes]
                 {
                     auto entries = cacheCleaner.find(prefixes, folders, data.shuttingDown);
                     queue.post(
                         [this, entries = std::move(entries)]
                         {
                             data.caches = {false, true, entries};
                             publish();
                         });
                 });
    }

    void handle (const Protocol::ClearCaches& command)
    {
        if(data.caches.busy || !data.caches.scanned) return;
        std::set<std::string> paths(command.paths.begin(), command.paths.end());
        // The game reads its shader cache while it runs
        if(gamePlaying())
        {
            for(auto& entry : data.caches.entries)
                if(paths.contains(entry.path)) entry.result = Protocol::CacheResult::GameRunning;
            return;
        }

        data.caches.busy = true;
        startJob(cacheJob,
                 [this, entries = data.caches.entries, paths] () mutable
                 {
                     cacheCleaner.clear(entries, paths, data.shuttingDown);
                     queue.post(
                         [this, entries = std::move(entries)]
                         {
                             data.caches.entries = entries;
                             data.caches.busy = false;
                             publish();
                         });
                 });
    }

    // Copies what the checks need, so they can run on their own thread
    Diagnostics::Input diagnosticsInput () const
    {
        Diagnostics::Input input;
        input.version = data.selfUpdate.currentVersion;
        input.catalogueLoaded = !data.languages.empty();
        input.rsiLauncherPath = data.rsiLauncherPath;
        for(const auto& [name, channel] : data.channels)
        {
            if(channel.installPath.empty()) continue;
            std::string folder;
            if(channel.language != Protocol::ENGLISH)
            {
                auto language = languageFor(name, channel.language);
                folder = language ? language->folder : channel.installed.folder;
            }
            input.channels.push_back({name, channel.installPath, folder, nlohmann::json(channelStatus(name, channel)).get<std::string>()});
        }
        input.settings = nlohmann::json(settingsFile()).dump(2) + "\n";
        return input;
    }

    // saveReport: also writes the ZIP
    void runDiagnostics (bool saveReport)
    {
        if(data.diagnostics.busy) return;
        data.diagnostics.busy = true;
        data.diagnostics.reportPath.clear();
        data.diagnostics.error.clear();
        startJob(diagnosticsJob,
                 [this, input = diagnosticsInput(), saveReport] () mutable
                 {
                     input.installType = SelfUpdateManager::detectInstallation().type;
                     auto checks = diagnostics.run(input);
                     std::optional<std::filesystem::path> report;
                     if(saveReport) report = diagnostics.writeReport(input, checks, Diagnostics::reportFolder());
                     queue.post(
                         [this, checks = std::move(checks), report, saveReport]
                         {
                             data.diagnostics.checks = checks;
                             data.diagnostics.busy = false;
                             if(report) data.diagnostics.reportPath = report->string();
                             if(saveReport && !report) data.diagnostics.error = "cannot write the report";
                             publish();
                         });
                 });
    }

    void handle (const Protocol::RunDiagnostics&) { runDiagnostics(false); }
    void handle (const Protocol::SaveReport&) { runDiagnostics(true); }

    // Runs a backup job; `work` returns the result text, the list is read again afterwards
    void backupJobFor (const std::string& channel, std::function<std::string()> work)
    {
        data.backups.busy = true;
        data.backups.result.clear();
        data.backups.resultChannel = channel;
        startJob(backupJob,
                 [this, work = std::move(work)]
                 {
                     std::string result = work ? work() : "";
                     auto entries = profileBackup.list();
                     queue.post(
                         [this, result, entries]
                         {
                             data.backups.busy = false;
                             data.backups.result = result;
                             data.backups.entries = entries;
                             // A restored user.cfg may select another language than the one installed
                             if(result == "restored") checkChannels();
                             publish();
                         });
                 });
    }

    void handle (const Protocol::SetGameSettings& command)
    {
        auto* channel = findChannel(command.channel);
        if(!channel) return;
        channel->settingsError.clear();
        std::string error;
        auto lines = GameSettings::linesOf(command.settings, error);
        if(!lines)
        {
            channel->settingsError = error;
            return;
        }
        if(channel->installPath.empty())
        {
            channel->settingsError = "no game folder";
            return;
        }
        auto keys = GameSettings::apply(channel->installPath / "user.cfg", *lines, channel->managedKeys);
        if(!keys)
        {
            channel->settingsError = "cannot write user.cfg";
            return;
        }
        channel->gameSettings = command.settings;
        channel->managedKeys = *keys;
        channel->settingsWritten = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        channel->settingsChecked = false;
        channel->ignoredSettings.clear();
        applyGraphicsApi(*channel);
    }

    void handle (const Protocol::LoadBackups&)
    {
        if(!data.backups.busy) backupJobFor("", nullptr);
    }

    void handle (const Protocol::CreateBackup& command)
    {
        auto* channel = findChannel(command.channel);
        if(!channel || channel->installPath.empty() || data.backups.busy) return;
        backupJobFor(command.channel, [this, name = command.channel, path = channel->installPath, version = data.selfUpdate.currentVersion]
                     { return profileBackup.create(name, path, version) ? std::string("created") : std::string("nothing to back up"); });
    }

    void handle (const Protocol::RestoreBackup& command)
    {
        auto* channel = findChannel(command.channel);
        if(!channel || channel->installPath.empty() || data.backups.busy) return;
        // The game rewrites these files while it runs and when it quits
        if(gamePlaying())
        {
            data.backups.result = "Star Citizen is running";
            data.backups.resultChannel = command.channel;
            return;
        }
        backupJobFor(command.channel,
                     [this, name = command.channel, backup = command.backup, path = channel->installPath, version = data.selfUpdate.currentVersion]
                     { return profileBackup.restore(name, backup, path, version).value_or("restored"); });
    }

    void handle (const Protocol::SetPrograms& command)
    {
        data.programs.clear();
        for(auto program : command.programs)
        {
            auto first = program.command.find_first_not_of(" \t");
            if(first == std::string::npos) continue;
            program.command = program.command.substr(first, program.command.find_last_not_of(" \t") - first + 1);
            data.programs.push_back(std::move(program));
        }
        data.failedPrograms.clear();
    }

    // Only the code is checked; the window falls back to English for a language it does not ship
    void handle (const Protocol::SetInterfaceLanguage& command)
    {
        static const std::regex code("[a-z]{2,3}(_[A-Z]{2})?");
        if(command.language.empty() || std::regex_match(command.language, code)) data.interfaceLanguage = command.language;
    }

    void handle (const Protocol::FinishSetup&) { data.setupDone = true; }

    void handle (const Protocol::SetTheme& command)
    {
        if(command.theme.empty() || command.theme == "dark" || command.theme == "light") data.theme = command.theme;
    }

    void handle (const Protocol::DismissWhatsNew&)
    {
        if(data.whatsNew.version.empty()) return;
        data.lastSeenVersion = data.whatsNew.version;
        data.whatsNew = {};
    }

   public:
    // The callback is called on the backend thread
    // version: of this launcher, compared with the published releases
    BackEnd (StateCallback stateCallback, std::string version)
        : lg("BackEnd")
        , settingsJsonParser(PATHS::JSON_FILES::SETTINGS_USER)
        , updateManager(data.shuttingDown)
        , itemValues(data.shuttingDown)
        , selfUpdateManager(data.shuttingDown, updateSigningKey())
        , stateCallback(std::move(stateCallback))
    {
        data.selfUpdate.currentVersion = std::move(version);
        // Settings first, so the GUI can show them while the language list is still downloading
        queue.post(
            [this]
            {
                loadSettings();
                watchGame();
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
        for(auto* job : {&languagesJob, &updateJob, &searchJob, &versionCheckJob, &selfUpdateJob, &gameJob, &gameWatchJob, &checkTimerJob,
                         &whatsNewJob, &cacheJob, &diagnosticsJob, &backupJob})
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
                ++data.handledCommands;
                saveSettings();
                publish();
            });
    }
};
