#include <iostream>
#include <string>

#include "core/SeverityLevel.h"
#include "logger/terminal_logger/terminal_logger.h"
#include "logging.h"

namespace LoggerFramework
{

TerminalLogger::TerminalLogger()
{
    if(init())
    {
    }
}

bool TerminalLogger::init()
{
    return true;
}

void TerminalLogger::write(const std::string& m_Channel, const std::string& message, const SeverityLevel& m_Severity)
{
    std::cout << "[" << getFormattedTimestamp() << "]" << "<" << m_Severity << ">\t" << "[" << m_Channel << "]" << "\t: " << message << std::endl;
}

} // namespace LoggerFramework
