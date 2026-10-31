#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "logging/logging.h"
#include "protocol/State.hpp"

// Play statistics from the game's own logs: Game.log of the last start and logbackups/*.log of the earlier ones,
// one file per game start. Only counts leave this class; the player's name and IDs are read to tell own kills
// from others, but never stored.
class GameStatistics
{
   public:
    struct Session
    {
        std::int64_t start = 0;
        std::int64_t end = 0;
        // The game handed a crash report to its crash collector; a game that was closed or killed without its own
        // quit is no crash
        bool crashed = false;
        // Error popups of the kind "Disconnection", e.g. the 30000 errors
        int disconnects = 0;
        std::set<std::string> ships;
        std::set<std::string> regions;
        int quantumJumps = 0;
        int missionsCompleted = 0;
        int missionsFailed = 0;
        int kills = 0;
        int deaths = 0;
    };

   private:
    LoggerFramework::LogEx lg;

    // "<2026-06-18T19:45:48.190Z> …" -> seconds since the epoch, 0 if the line has no timestamp
    static std::int64_t timeOf (const std::string& line)
    {
        std::tm tm{};
        if(line.size() < 21 || line[0] != '<' ||
           std::sscanf(line.c_str(), "<%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6)
            return 0;
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        return timegm(&tm);
    }

    // "AEGS_Sabre_7465010201" -> "AEGS_Sabre"; entity names end with an instance number
    static std::string withoutInstance (const std::string& name)
    {
        static const std::regex instance("_\\d+$");
        return std::regex_replace(name, instance, "");
    }

   public:
    GameStatistics ()
        : lg("GameStatistics")
    {
    }

    static Session readSession (const std::filesystem::path& log)
    {
        static const std::regex character(R"(<AccountLoginCharacterStatus_Character> Character: .* - name (\S+) - state STATE_CURRENT)");
        static const std::regex ship(R"(granted control token for '([^']+)')");
        static const std::regex shard(R"(shard\[pub_([a-z]+\d+))");
        static const std::regex mission(R"(mission_state MISSION_STATE_([A-Z_]+))");
        static const std::regex death(R"(CActor::Kill: '([^']+)' \[\d+\] in zone '[^']*' killed by '([^']+)')");

        Session session;
        std::string player;
        std::vector<std::pair<std::string, std::string>> victimsAndKillers;
        std::ifstream in(log);
        std::string line;
        std::smatch match;
        while(std::getline(in, line))
        {
            // The crash handler writes its lines without a timestamp
            if(line.find("crash info collector") != std::string::npos || line.find("\\Crashes\\game.log") != std::string::npos)
                session.crashed = true;
            if(line.empty() || line[0] != '<') continue;
            if(auto time = timeOf(line))
            {
                if(!session.start) session.start = time;
                session.end = time;
            }
            if(line.find("<Error Popup Closed>") != std::string::npos && line.find("[Disconnection]") != std::string::npos)
                ++session.disconnects;
            else if(line.find("<AccountLoginCharacterStatus_Character>") != std::string::npos && std::regex_search(line, match, character))
                player = match[1];
            else if(line.find("<Vehicle Control Flow>") != std::string::npos && std::regex_search(line, match, ship))
                session.ships.insert(withoutInstance(match[1]));
            else if(line.find("<Join PU>") != std::string::npos && std::regex_search(line, match, shard))
                // "euw1b" is a zone of the region "euw1"
                session.regions.insert(match[1]);
            else if(line.find("<Quantum Drive Arrived - Arrived at Final Destination>") != std::string::npos)
                ++session.quantumJumps;
            else if(line.find("<MissionEnded>") != std::string::npos && std::regex_search(line, match, mission))
            {
                if(match[1] == "COMPLETED") ++session.missionsCompleted;
                if(match[1] == "FAILED" || match[1] == "ABANDONED") ++session.missionsFailed;
            }
            else if(line.find("<Actor Death>") != std::string::npos && std::regex_search(line, match, death))
                victimsAndKillers.emplace_back(match[1], match[2]);
        }
        // The character line may come after a death, so kills are counted once the file is read
        for(const auto& [victim, killer] : victimsAndKillers)
        {
            if(player.empty()) break;
            if(victim == player)
                ++session.deaths;
            else if(killer == player)
                ++session.kills;
        }
        return session;
    }

    // The logs of the given channel folders, oldest first; `skipCurrent` leaves out each Game.log, e.g. while
    // the game runs and its session is not over yet
    static std::vector<std::filesystem::path> logsOf (const std::vector<std::filesystem::path>& folders, bool skipCurrent)
    {
        std::vector<std::filesystem::path> logs;
        for(const auto& folder : folders)
        {
            std::error_code ec;
            for(std::filesystem::directory_iterator it(folder / "logbackups", ec), end; !ec && it != end; it.increment(ec))
                if(it->path().extension() == ".log") logs.push_back(it->path());
            if(!skipCurrent && std::filesystem::exists(folder / "Game.log", ec)) logs.push_back(folder / "Game.log");
        }
        return logs;
    }

    Protocol::Statistics collect (const std::vector<std::filesystem::path>& logs, const std::atomic<bool>& stop)
    {
        Protocol::Statistics stats;
        std::map<std::string, int> ships;
        std::map<std::string, int> regions;
        std::map<std::string, std::int64_t> days;
        std::int64_t first = 0;
        std::int64_t last = 0;
        for(const auto& log : logs)
        {
            if(stop) break;
            auto session = readSession(log);
            if(!session.start) continue;
            // A log of more than a day is a game left running, not play time
            auto seconds = std::clamp<std::int64_t>(session.end - session.start, 0, 24 * 3600);
            ++stats.sessions;
            stats.playSeconds += seconds;
            stats.crashes += session.crashed ? 1 : 0;
            stats.disconnects += session.disconnects;
            stats.quantumJumps += session.quantumJumps;
            stats.missionsCompleted += session.missionsCompleted;
            stats.missionsFailed += session.missionsFailed;
            stats.kills += session.kills;
            stats.deaths += session.deaths;
            for(const auto& ship : session.ships) ++ships[ship];
            for(const auto& region : session.regions) ++regions[region];
            char date[11];
            std::time_t start = session.start;
            std::strftime(date, sizeof(date), "%Y-%m-%d", std::gmtime(&start));
            days[date] += seconds;
            first = first ? std::min(first, session.start) : session.start;
            last = std::max(last, session.end);
        }
        auto byCount = [] (const std::map<std::string, int>& counts)
        {
            std::vector<Protocol::NamedCount> list;
            for(const auto& [name, count] : counts) list.push_back({name, count});
            std::stable_sort(list.begin(), list.end(), [] (const auto& a, const auto& b) { return a.count > b.count; });
            return list;
        };
        stats.ships = byCount(ships);
        stats.regions = byCount(regions);
        for(const auto& [date, seconds] : days) stats.days.push_back({date, seconds});
        stats.firstPlayed = first;
        stats.lastPlayed = last;
        LOG_DEBUG(lg) << "collect() - " << logs.size() << " logs, " << stats.sessions << " sessions";
        return stats;
    }
};
