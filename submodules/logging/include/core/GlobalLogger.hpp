#if defined(_WIN32)
#ifdef LOGGER_EXPORTS
#define LOGGER_API __declspec(dllexport)
#else
#define LOGGER_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define LOGGER_API __attribute__((visibility("default")))
#else
#define LOGGER_API
#endif

#pragma once

#include "core/LoggerBase.hpp"
#include <memory>

namespace LoggerFramework
{

class LOGGER_API GlobalLogger
{
  public:
    static void registerLogger(std::shared_ptr<LoggerBase> logger)
    {
        getInstance().m_logger = std::move(logger);
    }

    static std::shared_ptr<LoggerBase> getLogger()
    {
        return getInstance().m_logger;
    }

  private:
    GlobalLogger() = default;
    static GlobalLogger& getInstance()
    {
        static GlobalLogger instance;
        return instance;
    }

    std::shared_ptr<LoggerBase> m_logger;
};

}; // namespace LoggerFramework
