#pragma once

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include "core/LoggerBase.hpp"
#include "core/SeverityLevel.h"
#include "utils/serverityStreamOperator.h"
#include "utils/timeStamp.h"

namespace LoggerFramework
{

class TerminalLogger : public LoggerBase
{
  private:
    TerminalLogger(const TerminalLogger&) = delete;
    TerminalLogger& operator=(const TerminalLogger&) = delete;
    bool init() override;

  public:
    TerminalLogger();
    ~TerminalLogger() override = default;

    void write(const std::string& m_Channel, const std::string& message, const SeverityLevel& m_Severity) override;
};

} // namespace LoggerFramework
