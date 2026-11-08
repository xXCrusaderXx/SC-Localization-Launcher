#include "logging/logging.h"
#include "backend/utils/platform.hpp"

#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "backend/utils/paths.hpp"

namespace LoggerFramework
{

namespace
{

constexpr std::size_t MAX_LOG_FILES = 10;

std::mutex registeredMutex;
std::shared_ptr<spdlog::logger> registered;

// "debug_2026-10-24 18:21:37.762.log": the names sort by start time, which Diagnostics relies on
std::filesystem::path newLogFileName ()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm = Platform::localTime(time);
    std::ostringstream name;
    name << "debug_" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count() << ".log";
    return name.str();
}

// Full upper case level names (<WARNING>); spdlog only has lower case or single letters
class UpperCaseLevel final : public spdlog::custom_flag_formatter
{
   public:
    void format (const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override
    {
        const std::string_view name = levelName(msg.level);
        dest.append(name.data(), name.data() + name.size());
    }

    std::unique_ptr<custom_flag_formatter> clone () const override { return std::make_unique<UpperCaseLevel>(); }

   private:
    static std::string_view levelName (spdlog::level::level_enum level)
    {
        switch(level)
        {
            case spdlog::level::trace:
                return "TRACE";
            case spdlog::level::debug:
                return "DEBUG";
            case spdlog::level::info:
                return "INFO";
            case spdlog::level::warn:
                return "WARNING";
            case spdlog::level::err:
                return "ERROR";
            case spdlog::level::critical:
                return "CRITICAL";
            default:
                return "UNKNOWN";
        }
    }
};

// Opens a new log file with the first line, then deletes all but the newest log files
class LazyFileSink final : public spdlog::sinks::base_sink<std::mutex>
{
   public:
    explicit LazyFileSink (std::filesystem::path folder)
        : m_Folder(std::move(folder))
    {
    }

   protected:
    void sink_it_ (const spdlog::details::log_msg& msg) override
    {
        if(!m_Tried) open();
        if(!m_File.is_open()) return;
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        m_File.write(formatted.data(), static_cast<std::streamsize>(formatted.size()));
    }

    void flush_ () override
    {
        if(m_File.is_open()) m_File.flush();
    }

   private:
    void open ()
    {
        m_Tried = true;
        const std::filesystem::path file = m_Folder / newLogFileName();
        m_File.open(file, std::ios::out | std::ios::app);
        if(!m_File.is_open()) std::cerr << "Cannot open the log file " << file << "\n";
        deleteOldLogs(file);
    }

    void deleteOldLogs (const std::filesystem::path& current) const
    {
        std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> older;
        std::error_code ec;
        for(std::filesystem::directory_iterator it(m_Folder, ec), end; !ec && it != end; it.increment(ec))
        {
            if(!it->is_regular_file(ec) || it->path().extension() != ".log" || it->path() == current) continue;
            older.emplace_back(it->last_write_time(ec), it->path());
        }
        if(older.size() < MAX_LOG_FILES) return;

        // Newest first; the current file is one of the files kept
        std::sort(older.begin(), older.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
        for(std::size_t i = MAX_LOG_FILES - 1; i < older.size(); ++i)
        {
            if(!std::filesystem::remove(older[i].second, ec) && ec)
                std::cerr << "Cannot delete the old log file " << older[i].second << ": " << ec.message() << "\n";
        }
    }

    std::filesystem::path m_Folder;
    std::ofstream m_File;
    bool m_Tried = false;
};

}  // namespace

std::shared_ptr<spdlog::logger> createLogger (bool console)
{
    const std::filesystem::path folder = PATHS::LOGPATH_USER / "logs";
    try
    {
        std::filesystem::create_directories(folder);
    }
    catch(const std::filesystem::filesystem_error& e)
    {
        throw std::runtime_error("Cannot create the log folder " + folder.string() + ": " + e.what());
    }

    std::vector<spdlog::sink_ptr> sinks{std::make_shared<LazyFileSink>(folder)};
    // Off on the command line, where stdout carries the result for scripts
    if(console) sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    auto logger = std::make_shared<spdlog::logger>("launcher", sinks.begin(), sinks.end());
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<UpperCaseLevel>('*').set_pattern("[%Y-%m-%d %H:%M:%S.%e]<%^%*%$>\t%v");
    logger->set_formatter(std::move(formatter));
    logger->set_level(spdlog::level::trace);
    // Every line reaches the file at once: the diagnostics ZIP reads it while the launcher runs, and a crash loses nothing
    logger->flush_on(spdlog::level::trace);
    return logger;
}

void registerLogger (std::shared_ptr<spdlog::logger> logger)
{
    std::lock_guard lock(registeredMutex);
    registered = std::move(logger);
}

void write (spdlog::level::level_enum level, const std::string& channel, const std::string& message) noexcept
{
    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard lock(registeredMutex);
        logger = registered;
    }
    // Without a registered logger (e.g. a small tool using the backend) the line is dropped
    if(!logger) return;
    try
    {
        logger->log(level, "[{}]\t: {}", channel, message);
    }
    catch(...)
    {
    }
}

}  // namespace LoggerFramework
