#pragma once

#include "SeverityLevel.h"

template <typename T> T& operator<<(T& os, const SeverityLevel& level)
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
    default:
        os << "UNKNOWN";
        break;
    }
    return os;
}
