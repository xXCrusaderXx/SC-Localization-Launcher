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
    if(!std::filesystem::exists(logPath))
    {
        try
        {
            std::filesystem::create_directories(logPath);
        }
        catch(const std::filesystem::filesystem_error& e)
        {
            throw std::runtime_error(std::string("Fehler beim erstellen bei: " + logPath.string() + "") + e.what());
        }
    }
    cleanOffOldLogs();
    init();
}

CustomLogger::~CustomLogger ()
{
    if(m_LogFile.is_open())
    {
        m_LogFile.close();
    }
}

void CustomLogger::cleanOffOldLogs ()
{
    std::vector<std::filesystem::directory_entry> logFiles;

    // Alle Dateien im Log-Verzeichnis sammeln
    for(const auto& entry : std::filesystem::directory_iterator(logPath))
    {
        if(entry.is_regular_file() && entry.path().extension() == ".log")
        {
            logFiles.push_back(entry);
        }
    }
    if(logFiles.size() < maxFiles)
    {
        return;
    }

    std::sort(logFiles.begin(), logFiles.end(),
              [] (const auto& a, const auto& b) { return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b); });

    // Zu viele? Dann die ältesten löschen
    std::size_t numToDelete = logFiles.size() - maxFiles;
    for(std::size_t i = 0; i < numToDelete; ++i)
    {
        try
        {
            std::filesystem::remove(logFiles[i]);
            std::cout << "Deleted: " << logFiles[i].path() << "\n";
        }
        catch(const std::exception& e)
        {
            std::cerr << "Failed to delete " << logFiles[i].path() << ": " << e.what() << "\n";
        }
    }
}

bool CustomLogger::init ()
{
    m_LogFile.open(logPath.string() + "/debug_" + getFormattedTimestamp() + ".log", std::ios::out | std::ios::app);
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
    // std::cout << "[" << getFormattedTimestamp() << "]" << "<" << m_Severity << ">\t" << "[" << m_Channel << "]" << "\t: " << message <<
    // std::endl;

    if(m_LogFile.is_open())
    {
        m_LogFile << logEntry << std::endl;
    }
}

}  // namespace LoggerFramework
