#include <filesystem>
#include <iostream>
#include <string>

#include "logging/core/SeverityLevel.h"
#include "logging/logger/custom_logger/custom_logger.h"
#include "logging/logging.h"

namespace LoggerFramework
{

CustomLogger::CustomLogger ()
{
    if(init())
    {
    }
}

CustomLogger::~CustomLogger ()
{
    if(m_LogFile.is_open())
    {
        m_LogFile.close();
    }
}

bool CustomLogger::init ()
{
    m_LogFile.open(std::filesystem::current_path().string() + "/debug_" + getFormattedTimestamp() + ".log", std::ios::out | std::ios::app);
    return m_LogFile.is_open();
    // return true;
}

void CustomLogger::write (const std::string& m_Channel, const std::string& message, const SeverityLevel& m_Severity)
{
    std::ostringstream oss;
    oss << m_Severity;
    std::string severityStr = oss.str();

    std::string logEntry = "[" + getFormattedTimestamp() + "]" + "<" + severityStr + ">\t[" + m_Channel + "]\t: " + message;
    std::cout << logEntry << std::endl;
    // std::cout << "[" << getFormattedTimestamp() << "]" << "<" << m_Severity << ">\t" << "[" << m_Channel << "]" << "\t: " << message << std::endl;

    std::cout << logEntry << std::endl;
    if(m_LogFile.is_open())
    {
        m_LogFile << logEntry << std::endl;
    }
}

}  // namespace LoggerFramework
