#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <streambuf>

#include "logger/boost_logger/boost_logger.h"
#include "logging.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class BoostLoggerTest : public ::testing::Test
{
  protected:
    std::streambuf* originalCout;
    std::streambuf* originalCerr;
    std::ostringstream capturedOut;
    std::ostringstream capturedErr;

    void SetUp() override
    {
        LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::BoostLogger>("logs"));

        originalCout = std::cout.rdbuf();
        originalCerr = std::cerr.rdbuf();
        std::cout.rdbuf(capturedOut.rdbuf());
        std::cerr.rdbuf(capturedErr.rdbuf());
    }

    void TearDown() override
    {
        std::cout.rdbuf(originalCout);
        std::cerr.rdbuf(originalCerr);
    }

    void showCaptured(std::string& output)
    {
        std::cout.rdbuf(originalCout);
        std::cerr.rdbuf(originalCerr);

        std::cout << "[DEBUG OUTPUT] Captured log:\n" << output << std::endl;

        std::cout.rdbuf(capturedOut.rdbuf());
        std::cerr.rdbuf(capturedErr.rdbuf());
    }
};

TEST_F(BoostLoggerTest, LogsAreWrittenToStreams)
{
    LoggerFramework::LogEx lg("LogsAreWrittenToStreams");

    LOG_DEBUG(lg) << "Test";

    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);

    EXPECT_NE(output.find("<DEBUG>"), std::string::npos);
    EXPECT_NE(output.find("[LogsAreWrittenToStreams]"), std::string::npos);
    EXPECT_NE(output.find("Test"), std::string::npos);
}

TEST_F(BoostLoggerTest, SeverityLevelCheck)
{
    LoggerFramework::LogEx lg("SeverityLevelCheck");
    LOG_INFO(lg) << "test";
    LOG_DEBUG(lg) << "test";
    LOG_ERROR(lg) << "test";
    LOG_WARNING(lg) << "test";
    LOG_PROCESS(lg) << "test";
    LOG_SYSTEM(lg) << "test";
    LOG_TEST(lg) << "test";
    LOG_CRITICAL(lg) << "test";

    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);

    EXPECT_NE(output.find("INFO"), std::string::npos);
    EXPECT_NE(output.find("DEBUG"), std::string::npos);
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("WARNING"), std::string::npos);
    EXPECT_NE(output.find("PROCESS"), std::string::npos);
    EXPECT_NE(output.find("SYSTEM"), std::string::npos);
    EXPECT_NE(output.find("TEST"), std::string::npos);
    EXPECT_NE(output.find("CRITICAL"), std::string::npos);
}

TEST_F(BoostLoggerTest, MultipleLoggersAreIndependent)
{
    LoggerFramework::LogEx lg1("Channel1");
    LoggerFramework::LogEx lg2("Channel2");

    LOG_INFO(lg1) << "Message from logger 1";
    LOG_INFO(lg2) << "Message from logger 2";

    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);

    EXPECT_NE(output.find("Message from logger 1"), std::string::npos);
    EXPECT_NE(output.find("Message from logger 2"), std::string::npos);
}

TEST_F(BoostLoggerTest, HandlesEmptyAndVeryLongMessages)
{
    LoggerFramework::LogEx lgc("EdgeCaseChannel");

    LOG_INFO(lgc) << "";
    LOG_INFO(lgc) << std::string(10000, 'x');

    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);

    EXPECT_NE(output.find(std::string(10000, 'x')), std::string::npos);
}

TEST_F(BoostLoggerTest, DllWithBoostLoggerTest)
{
    LoggerFramework::LogEx lg1("DllWithBoostLoggerTest");
    LOG_INFO(lg1) << "vor DLL LOG ID: " << lg1.getId();

    typedef void (*PluginFunc)();

#if defined(_WIN32)
    std::string dllPath = std::filesystem::current_path() / "libTestDLL.dll";
    HMODULE handle = LoadLibraryA(dllPath.c_str());
    if(!handle)
    {
        std::cerr << "Failed to load plugin: " << GetLastError() << std::endl;
        return;
    }

    PluginFunc run = (PluginFunc) GetProcAddress(handle, "createPlugin");
    if(!run)
    {
        std::cerr << "Failed to find symbol: " << GetLastError() << std::endl;
        FreeLibrary(handle);
        return;
    }

    run();
    FreeLibrary(handle);

#else
    std::string dllPath = std::filesystem::current_path() / "libTestDLL.so";
    void* handle = dlopen(dllPath.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if(!handle)
    {
        std::cerr << "Failed to load plugin: " << dlerror() << std::endl;
        return;
    }

    dlerror();
    PluginFunc run = (PluginFunc) dlsym(handle, "createPlugin");
    const char* error = dlerror();
    if(error != nullptr)
    {
        std::cerr << "Failed to find symbol: " << error << std::endl;
        return;
    }

    run();

    dlclose(handle);
#endif

    LOG_INFO(lg1) << "nach DLL LOG ID: " << lg1.getId();

    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);
}
