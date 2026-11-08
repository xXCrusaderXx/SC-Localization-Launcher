#pragma once

#include <QProcess>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "backend/managers/UpdateManager.hpp"
#include "backend/utils/platform.hpp"
#include "logging/logging.h"
#include "protocol/State.hpp"

// Starts the user's programs when Star Citizen starts and ends those with stopWithGame when it quits.
// Only touched by the backend thread.
class CompanionPrograms
{
   private:
    struct Started
    {
        int64_t pid = 0;
        // A different value means the id now belongs to another process
        uint64_t startTime = 0;
    };

    LoggerFramework::LogEx lg;
    std::vector<Started> toStop;

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

    // Returns the commands that could not be started
    std::vector<std::string> start (const std::vector<Protocol::Program>& programs)
    {
        std::vector<std::string> failed;
        for(const auto& program : programs)
        {
            if(!program.enabled) continue;
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
            if(program.stopWithGame) toStop.push_back({*pid, startTimeOf(*pid)});
        }
        return failed;
    }

    // Ends what start() started with stopWithGame and is still running
    void stop ()
    {
        for(const auto& started : toStop)
        {
            if(started.startTime == 0 || startTimeOf(started.pid) != started.startTime) continue;
            LOG_INFO(lg) << "stop() - pid " << started.pid;
            Platform::endProcess(started.pid);
        }
        toStop.clear();
    }
};
