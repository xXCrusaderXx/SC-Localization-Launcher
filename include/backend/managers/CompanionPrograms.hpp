#pragma once

#include <QProcess>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "backend/managers/UpdateManager.hpp"
#include "backend/utils/platform.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

// Starts the user's programs when Star Citizen or, with startWithLauncher, already the RSI Launcher starts, and ends
// those with stopWithGame when the game quits. Only touched by the backend thread.
class CompanionPrograms
{
   private:
    struct Started
    {
        int64_t pid = 0;
        // A different value means the id now belongs to another process
        uint64_t startTime = 0;
        std::string command;
    };

    LoggerFramework::LogEx lg;
    std::vector<Started> toStop;
    // Commands of the startWithLauncher programs started since the RSI Launcher or the game appeared and not ended
    // since; the others, also those that failed, start again with the next game start
    std::set<std::string> startedWithLauncher;

    static uint64_t startTimeOf (int64_t pid) { return Platform::processStartTime(pid); }

   public:
    // Splits like a shell, without expanding anything: blanks separate words, '…' keeps its text as it is, and a
    // backslash keeps the next character (inside "…" only before " and \). Nothing for an unclosed quote.
    static std::optional<QStringList> splitWords (const std::string& command)
    {
        QStringList words;
        std::string word;
        bool inWord = false;
        char quote = 0;
        for(size_t i = 0; i < command.size(); ++i)
        {
            char c = command[i];
            if(quote == '\'')
            {
                if(c == '\'')
                    quote = 0;
                else
                    word += c;
            }
            else if(quote == '"')
            {
                if(c == '"')
                    quote = 0;
                else if(c == '\\' && i + 1 < command.size() && (command[i + 1] == '"' || command[i + 1] == '\\'))
                    word += command[++i];
                else
                    word += c;
            }
            else if(c == ' ' || c == '\t')
            {
                if(inWord) words << QString::fromStdString(word);
                word.clear();
                inWord = false;
            }
            else
            {
                inWord = true;
                if(c == '\'' || c == '"')
                    quote = c;
                else if(c == '\\' && i + 1 < command.size())
                    word += command[++i];
                else
                    word += c;
            }
        }
        if(quote) return std::nullopt;
        if(inWord) words << QString::fromStdString(word);
        return words;
    }

    CompanionPrograms ()
        : lg("CompanionPrograms")
    {
    }

    // Called when the RSI Launcher or the game appears (gameStarted false) and when the game starts (true); starts
    // the startWithLauncher programs not started yet and, with gameStarted, the others. Returns the commands that
    // could not be started.
    std::vector<std::string> start (const std::vector<Protocol::Program>& programs, bool gameStarted)
    {
        std::vector<std::string> failed;
        for(const auto& program : programs)
        {
            if(!program.enabled) continue;
            if(program.startWithLauncher ? startedWithLauncher.contains(program.command) : !gameStarted) continue;
            auto words = splitWords(program.command);
            std::optional<qint64> pid;
            if(words && !words->isEmpty())
            {
                QString executable = words->takeFirst();
                pid = UpdateManager::startOnHost(executable, *words);
            }
            if(!pid)
            {
                LOG_ERROR(lg) << "start() - cannot start " << program.command;
                failed.push_back(program.command);
                continue;
            }
            LOG_INFO(lg) << "start() - " << program.command << " (pid " << *pid << ")";
            if(program.startWithLauncher) startedWithLauncher.insert(program.command);
            if(program.stopWithGame) toStop.push_back({*pid, startTimeOf(*pid), program.command});
        }
        return failed;
    }

    // Ends what start() started with stopWithGame and is still running; called when the game quits and when the RSI
    // Launcher quits without it
    void stop ()
    {
        for(const auto& started : toStop)
        {
            startedWithLauncher.erase(started.command);
            if(started.startTime == 0 || startTimeOf(started.pid) != started.startTime) continue;
            LOG_INFO(lg) << "stop() - pid " << started.pid;
            Platform::endProcess(started.pid);
        }
        toStop.clear();
    }

    // Neither the RSI Launcher nor the game runs any more, so the next one to appear starts all programs again
    void reset () { startedWithLauncher.clear(); }
};
