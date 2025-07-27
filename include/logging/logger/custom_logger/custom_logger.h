#pragma once

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include "logging/core/LoggerBase.hpp"
#include "logging/core/SeverityLevel.h"
#include "logging/utils/serverityStreamOperator.h"
#include "logging/utils/timeStamp.h"

namespace LoggerFramework
{

class CustomLogger : public LoggerBase
{
   private:
    CustomLogger (const CustomLogger&) = delete;
    CustomLogger& operator=(const CustomLogger&) = delete;
    bool init () override;

    std::ofstream m_LogFile;

   public:
    CustomLogger ();
    ~CustomLogger () override;

    void write (const std::string& m_Channel, const std::string& message, const SeverityLevel& m_Severity) override;
};

}  // namespace LoggerFramework
