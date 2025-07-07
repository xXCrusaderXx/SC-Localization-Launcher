#pragma once

#include <condition_variable>
#include <memory>
#include <sstream>
#include <string>

#include "core/GlobalLogger.hpp"
#include "core/LoggerBase.hpp"
#include "core/SeverityLevel.h"

namespace LoggerFramework
{

class LogEx
{

  private:
    std::shared_ptr<LoggerBase> logger;
    std::string channel = "Default";

    std::condition_variable m_Signal;
    std::mutex m_Mutex;
    bool m_bOccupied = false;

  public:
    LogEx(const std::string& newChannelName)
        : logger(GlobalLogger::getLogger())
        , channel(newChannelName)
    {
    }

    ~LogEx() = default;

    const std::string& start()
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Signal.wait(lock, [this] { return (this->m_bOccupied == false); });
        m_bOccupied = true;
        return channel;
    }

    void write(const std::string& channel, const std::string& message, const SeverityLevel& level)
    {
        std::unique_lock<std::mutex> lock(m_Mutex);

        logger->write(channel, message, level);

        m_bOccupied = false;
        m_Signal.notify_one();
    }

    std::string getId()
    {
        if(logger)
        {
            std::ostringstream oss;
            oss << static_cast<const void*>(logger.get());
            return oss.str();
        }
        return {};
    }
};

}; // namespace LoggerFramework
