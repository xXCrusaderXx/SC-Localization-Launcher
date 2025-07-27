#pragma once

#include <string>

#include "SeverityLevel.h"

namespace LoggerFramework
{

class LoggerBase
{
  public:
    virtual ~LoggerBase() = default;
    virtual void write(const std::string& channel, const std::string& message, const SeverityLevel& level) = 0;
    virtual bool init() = 0;
};

}; // namespace LoggerFramework
