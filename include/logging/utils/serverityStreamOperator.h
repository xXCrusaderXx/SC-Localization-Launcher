#pragma once

#include "logging/core/SeverityLevel.h"

template <typename T>
T& operator<<(T& os, const SeverityLevel& level)
{
    switch(level)
    {
        case SeverityLevel::Debug:
            os << "DEBUG";
            break;
        case SeverityLevel::Info:
            os << "INFO";
            break;
        case SeverityLevel::Warning:
            os << "WARNING";
            break;
        case SeverityLevel::Error:
            os << "ERROR";
            break;
        case SeverityLevel::Critical:
            os << "CRITICAL";
            break;
        case SeverityLevel::Process:
            os << "PROCESS";
            break;
        case SeverityLevel::Test:
            os << "TEST";
            break;
        case SeverityLevel::System:
            os << "SYSTEM";
            break;
        default:
            os << "UNKNOWN";
            break;
    }
    return os;
}
