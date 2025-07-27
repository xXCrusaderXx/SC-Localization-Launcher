#pragma once

#include "SeverityLevel.h"

#define LOG_STREAM(logger, level) LoggerFramework::LogStream(logger, level)

#define LOG_DEBUG(x) LOG_STREAM(x, SeverityLevel::Debug)
#define LOG_TEST(x) LOG_STREAM(x, SeverityLevel::Test)
#define LOG_INFO(x) LOG_STREAM(x, SeverityLevel::Info)
#define LOG_PROCESS(x) LOG_STREAM(x, SeverityLevel::Process)
#define LOG_WARNING(x) LOG_STREAM(x, SeverityLevel::Warning)
#define LOG_ERROR(x) LOG_STREAM(x, SeverityLevel::Error)
#define LOG_SYSTEM(x) LOG_STREAM(x, SeverityLevel::System)
#define LOG_CRITICAL(x) LOG_STREAM(x, SeverityLevel::Critical)
