#pragma once

#include <QCommandLineParser>
#include <QStringList>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <ostream>

#include "backend/BackEnd.hpp"
#include "frontend/SingleInstance.hpp"

// Headless mode for scripts and systemd timers: checks or updates the translations without a window.
// Exit codes: 0 = everything current or updated, 1 = updates available (--check only), 2 = error.
class CommandLine
{
   public:
    enum ExitCode
    {
        Ok = 0,
        UpdatesAvailable = 1,
        Error = 2
    };

    // Whether these arguments ask for the command line instead of the window; checked before any Qt application exists
    static bool requested (int argc, char* argv[])
    {
        for(int i = 1; i < argc; ++i)
            for(const char* option : {"--check", "--update", "--version", "-v", "--help", "-h"})
                if(std::strcmp(argv[i], option) == 0) return true;
        return false;
    }

    // instanceName: the single-instance lock shared with the window, so both never write at the same time
    CommandLine (QString instanceName, std::string version, std::ostream& out, std::ostream& err)
        : lg("CommandLine")
        , instanceName(std::move(instanceName))
        , version(std::move(version))
        , out(out)
        , err(err)
    {
    }

    int run (const QStringList& arguments)
    {
        QCommandLineParser parser;
        parser.setApplicationDescription(
            "Installs and updates community translations for Star Citizen.\n"
            "Without options the window opens.\n\n"
            "Exit codes: 0 = current or updated, 1 = updates available (--check), 2 = error.");
        QCommandLineOption check("check", "Show which channels need an update, without changing anything.");
        QCommandLineOption update("update", "Update the translations of all channels with a game folder.");
        QCommandLineOption channel("channel", "Only this channel, e.g. LIVE. Can be given more than once.", "name");
        QCommandLineOption versionOption({"v", "version"}, "Show the version.");
        QCommandLineOption help({"h", "help"}, "Show this help.");
        parser.addOptions({check, update, channel, versionOption, help});

        if(!parser.parse(arguments))
        {
            err << parser.errorText().toStdString() << "\n";
            return Error;
        }
        if(parser.isSet(help))
        {
            out << parser.helpText().toStdString();
            return Ok;
        }
        if(parser.isSet(versionOption))
        {
            out << "SC Localization Launcher " << version << "\n";
            return Ok;
        }
        if(parser.isSet(check) == parser.isSet(update))
        {
            err << "Use either --check or --update.\n";
            return Error;
        }

        SingleInstance instance(instanceName);
        if(!instance.isPrimary())
        {
            if(instance.primaryAnswers())
            {
                err << "The SC Localization Launcher is open. Close it first, or use its window.\n";
                return Error;
            }
            // Left over from a launcher that is gone
            if(!instance.takeOver()) return Error;
        }

        BackEnd backEnd(
            [this] (const Protocol::State& state)
            {
                std::lock_guard lock(mutex);
                latest = state;
                changed.notify_all();
            },
            version);

        // Until the sources have answered, a channel could look current or unavailable
        auto ready = waitFor([] (const Protocol::State& s) { return !s.checking; }, std::chrono::minutes(2));
        if(!ready)
        {
            err << "The translation sources did not answer in time.\n";
            return Error;
        }

        std::vector<std::string> names;
        for(const auto& name : parser.values(channel))
        {
            if(!findChannel(*ready, name.toStdString()))
            {
                err << "Unknown channel: " << name.toStdString() << "\n";
                return Error;
            }
            names.push_back(name.toStdString());
        }
        if(names.empty())
            for(const auto& c : ready->channels)
                if(!c.installPath.empty()) names.push_back(c.name);
        for(const auto& name : names)
            if(findChannel(*ready, name)->installPath.empty())
            {
                err << name << " has no game folder. Choose one in the launcher window first.\n";
                return Error;
            }
        if(names.empty())
        {
            err << "No channel has a game folder. Choose one in the launcher window first.\n";
            return Error;
        }

        return parser.isSet(check) ? report(*ready, names) : runUpdate(backEnd, *ready, names);
    }

   private:
    LoggerFramework::LogEx lg;
    QString instanceName;
    std::string version;
    std::ostream& out;
    std::ostream& err;

    std::mutex mutex;
    std::condition_variable changed;
    std::optional<Protocol::State> latest;

    template <typename Predicate>
    std::optional<Protocol::State> waitFor (Predicate predicate, std::chrono::seconds timeout)
    {
        std::unique_lock lock(mutex);
        if(!changed.wait_for(lock, timeout, [&] { return latest && predicate(*latest); })) return std::nullopt;
        return latest;
    }

    static const Protocol::Channel* findChannel (const Protocol::State& state, const std::string& name)
    {
        auto it = std::find_if(state.channels.begin(), state.channels.end(), [&] (const auto& c) { return c.name == name; });
        return it != state.channels.end() ? &*it : nullptr;
    }

    static std::string languageName (const Protocol::State& state, const Protocol::Channel& channel)
    {
        if(channel.language == Protocol::CUSTOM) return "own source";
        for(const auto& language : state.languages)
            if(language.code == channel.language) return language.name;
        return channel.language;
    }

    static bool needsUpdate (Protocol::ChannelStatus status)
    {
        using Protocol::ChannelStatus;
        return status == ChannelStatus::NotInstalled || status == ChannelStatus::UpdateAvailable || status == ChannelStatus::Damaged ||
               status == ChannelStatus::Failed;
    }

    static const char* statusText (Protocol::ChannelStatus status)
    {
        switch(status)
        {
            case Protocol::ChannelStatus::NoPath:
                return "no game folder";
            case Protocol::ChannelStatus::NotInstalled:
                return "not installed";
            case Protocol::ChannelStatus::Installed:
                return "up to date";
            case Protocol::ChannelStatus::UpdateAvailable:
                return "update available";
            case Protocol::ChannelStatus::Damaged:
                return "files changed, update to repair";
            case Protocol::ChannelStatus::Failed:
                return "last update failed";
        }
        return "unknown";
    }

    // One line per channel, e.g. "LIVE: Deutsch, update available"
    int report (const Protocol::State& state, const std::vector<std::string>& names)
    {
        bool pending = false;
        for(const auto& name : names)
        {
            const auto* channel = findChannel(state, name);
            out << name << ": " << languageName(state, *channel) << ", " << statusText(channel->status) << "\n";
            pending |= needsUpdate(channel->status);
        }
        return pending ? UpdatesAvailable : Ok;
    }

    int runUpdate (BackEnd& backEnd, const Protocol::State& ready, const std::vector<std::string>& names)
    {
        if(ready.game == Protocol::GameStart::Playing)
        {
            err << "Star Citizen is running. Translations are updated after it has been closed.\n";
            return Error;
        }

        LOG_INFO(lg) << "update from the command line";
        backEnd.execute(Protocol::StartUpdate{.channels = names, .allowGameStart = false});
        // This is the only command sent, so the first state that counts it shows whether the update started
        auto handled = waitFor([] (const Protocol::State& s) { return s.handledCommands >= 1; }, std::chrono::seconds(30));
        if(!handled) return Error;
        if(handled->update != Protocol::UpdateStatus::Running)
        {
            if(handled->game == Protocol::GameStart::Playing)
            {
                err << "Star Citizen is running. Translations are updated after it has been closed.\n";
                return Error;
            }
            out << "Nothing to update.\n";
            return report(*handled, names) == Ok ? Ok : Error;
        }

        auto done = waitFor([] (const Protocol::State& s) { return s.update != Protocol::UpdateStatus::Running; }, std::chrono::minutes(30));
        if(!done)
        {
            err << "The update did not finish in time.\n";
            return Error;
        }
        report(*done, names);
        if(done->update != Protocol::UpdateStatus::Succeeded)
        {
            err << "The update failed. Details are in the log folder: " << (PATHS::LOGPATH_USER / "logs").string() << "\n";
            return Error;
        }
        return Ok;
    }
};
