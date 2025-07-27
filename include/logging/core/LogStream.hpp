#pragma once

#include <sstream>
#include <string>

#include "LogEx.hpp"
#include "SeverityLevel.h"

namespace LoggerFramework
{

class LogStream
{
  protected:
    LogEx& m_logger;
    std::ostringstream os;
    SeverityLevel level;
    std::string channel;

  private:
    LogStream(const LogStream&);

  public:
    LogStream(LogEx& logger, SeverityLevel level)
        : m_logger(logger)
        , level(level)
    {
        channel = m_logger.start();
    }

    virtual ~LogStream()
    {
        m_logger.write(channel, os.str(), level);
    }

    template <typename T> LogStream& operator<<(const T& value)
    {
        os << value;
        return *this;
    }
};

}; // namespace LoggerFramework
