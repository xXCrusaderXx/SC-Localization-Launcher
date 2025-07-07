#pragma once

#if defined(_WIN32)
#ifdef TESTDLL_EXPORTS
#define TESTDLL_API __declspec(dllexport)
#else
#define TESTDLL_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define TESTDLL_API __attribute__((visibility("default")))
#else
#define TESTDLL_API
#endif

#include "logging.h"
#include <iostream>

class TESTDLL_API TestDLL
{
  private:
    LoggerFramework::LogEx lg;

  public:
    TestDLL()
        : lg("TESTDLL-INSTANZE")
    {
        LOG_INFO(lg) << "LOG ID: " << lg.getId();
    }

    ~TestDLL() = default;
};
