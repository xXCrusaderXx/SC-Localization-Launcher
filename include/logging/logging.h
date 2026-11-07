#pragma once

#include <spdlog/common.h>

#include <memory>
#include <sstream>
#include <string>

namespace spdlog
{
class logger;
}

namespace LoggerFramework
{

// Logs to stdout (optional) and to a file in PATHS::LOGPATH_USER / "logs". The file is created with the first line,
// so a start that only hands over to the running launcher leaves none; then only the newest 10 log files are kept.
// Throws std::runtime_error if the log folder cannot be created.
std::shared_ptr<spdlog::logger> createLogger (bool console = true);

// The logger every LogEx writes to; nullptr stops logging
void registerLogger (std::shared_ptr<spdlog::logger> logger);

void write (spdlog::level::level_enum level, const std::string& channel, const std::string& message) noexcept;

// A named channel. The logger is looked up per line, so a channel created before registerLogger() still logs.
class LogEx
{
   public:
    explicit LogEx (std::string channel)
        : m_Channel(std::move(channel))
    {
    }

    const std::string& channel () const { return m_Channel; }

   private:
    std::string m_Channel;
};

// Collects one line and writes it when the statement ends
class LogStream
{
   public:
    LogStream (const LogEx& channel, spdlog::level::level_enum level)
        : m_Channel(channel)
        , m_Level(level)
    {
    }

    LogStream (const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    ~LogStream () { write(m_Level, m_Channel.channel(), m_Text.str()); }

    template <typename T>
    LogStream& operator<<(const T& value)
    {
        m_Text << value;
        return *this;
    }

   private:
    const LogEx& m_Channel;
    spdlog::level::level_enum m_Level;
    std::ostringstream m_Text;
};

}  // namespace LoggerFramework

#define LOG_DEBUG(x) LoggerFramework::LogStream(x, spdlog::level::debug)
#define LOG_INFO(x) LoggerFramework::LogStream(x, spdlog::level::info)
#define LOG_WARNING(x) LoggerFramework::LogStream(x, spdlog::level::warn)
#define LOG_ERROR(x) LoggerFramework::LogStream(x, spdlog::level::err)
#define LOG_CRITICAL(x) LoggerFramework::LogStream(x, spdlog::level::critical)
