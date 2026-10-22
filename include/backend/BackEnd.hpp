#pragma once

#include <algorithm>
#include <functional>
#include <thread>

#include "backend/DirectoryFinder.hpp"
#include "backend/utils/TaskQueue.hpp"
#include "backend/utils/paths.hpp"
#include "database/DataBase.hpp"
#include "jsonParser/ChannelsJsonParser.hpp"
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
    ChannelsJsonParser channelsJsonParser;
    UpdateManager updateManager;
    DirectoryFinder directoryFinder;
    StateCallback stateCallback;

    std::thread channelInfoJob;
    std::thread updateJob;
    std::thread searchJob;

    // Declared last: its thread uses all members above
    TaskQueue queue;

    // Only called on the backend thread. A job that is not running anymore is joined right away,
    // it never waits for the backend thread.
    void startJob (std::thread& job, std::function<void()> function)
    {
        if(job.joinable()) job.join();
        job = std::thread(std::move(function));
    }

    static std::vector<Protocol::Language> availableLanguages (const Database::WebInfo& info)
    {
        std::vector<Protocol::Language> languages{Protocol::Language::English};
        if(info.active != 0 && !info.server1.empty()) languages.push_back(Protocol::Language::German);
        if(info.active != 0 && !info.server3.empty()) languages.push_back(Protocol::Language::GermanFull);
        return languages;
    }

    // A choice the server does not offer (anymore) is treated as English, without forgetting the choice
    static Protocol::Language effectiveLanguage (const Database::ChannelData& channel)
    {
        auto languages = availableLanguages(channel.info);
        bool available = std::find(languages.begin(), languages.end(), channel.language) != languages.end();
        return available ? channel.language : Protocol::Language::English;
    }

    Database::ChannelData* findChannel (const std::string& name)
    {
        auto it = data.channels.find(name);
        if(it != data.channels.end()) return &it->second;
        LOG_ERROR(lg) << "unknown channel: " << name;
        return nullptr;
    }

    Protocol::State snapshot () const
    {
        Protocol::State state;

        // Known channels in display order, then whatever else the server lists
        std::vector<std::string> names = Protocol::CHANNEL_NAMES;
        for(const auto& [name, channel] : data.channels)
            if(std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);

        for(const auto& name : names)
        {
            const auto& channel = data.channels.at(name);
            state.channels.push_back({name, channel.installPath.string(), effectiveLanguage(channel), availableLanguages(channel.info)});
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
        }
        data.rsiLauncherPath = file.rsiLauncherPath;
        data.options = file.options;
    }

    void saveSettings ()
    {
        SettingsFile file;
        for(const auto& [name, channel] : data.channels) file.channels[name] = {channel.installPath.string(), channel.language};
        file.rsiLauncherPath = data.rsiLauncherPath.string();
        file.options = data.options;
        settingsJsonParser.save(file);
    }

    void loadChannelInfo ()
    {
        startJob(channelInfoJob,
                 [this]
                 {
                     auto channels = channelsJsonParser.getChannels();
                     if(!channels)
                     {
                         LOG_ERROR(lg) << "no channel information, translations cannot be downloaded";
                         return;
                     }
                     queue.post(
                         [this, channels = std::move(*channels)]
                         {
                             for(const auto& [name, info] : channels) data.channels[name].info = info;
                             publish();
                         });
                 });
    }

    // --- Command handlers, all on the backend thread ---

    void handle (const Protocol::SetLanguage& command)
    {
        if(auto* channel = findChannel(command.channel)) channel->language = command.language;
    }

    void handle (const Protocol::SetInstallPath& command)
    {
        if(auto* channel = findChannel(command.channel)) channel->installPath = command.path;
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
            if(!channel.installPath.empty()) jobs.push_back({name, channel.installPath, effectiveLanguage(channel), channel.info});

        if(jobs.empty())
        {
            LOG_DEBUG(lg) << "startUpdate - no install path set";
            return;
        }

        data.update = Protocol::UpdateStatus::Running;
        bool launchGame = data.options.launchGameAfterUpdate;
        startJob(updateJob,
                 [this, jobs, launchGame, rsiLauncherPath = data.rsiLauncherPath]
                 {
                     bool success = updateManager.update(jobs);
                     // Also after a partial failure, the other channels are still playable
                     if(launchGame && !data.shuttingDown) updateManager.startGame(rsiLauncherPath);
                     queue.post(
                         [this, success]
                         {
                             data.update = success ? Protocol::UpdateStatus::Succeeded : Protocol::UpdateStatus::Failed;
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
        // Settings first, so the GUI can show them while channels.json is still downloading
        queue.post(
            [this]
            {
                loadSettings();
                publish();
                loadChannelInfo();
            });
        LOG_DEBUG(lg) << "instanziated";
    }

    ~BackEnd ()
    {
        data.shuttingDown = true;
        // Runs the commands still waiting, then no one starts new jobs
        queue.stop();
        for(auto* job : {&channelInfoJob, &updateJob, &searchJob})
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
