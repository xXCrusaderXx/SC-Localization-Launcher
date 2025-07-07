#include "TestDLL.hpp"

#include "logging.h"

using namespace LoggerFramework;

extern "C" TESTDLL_API TestDLL* createPlugin()
{
    return new TestDLL();
}
