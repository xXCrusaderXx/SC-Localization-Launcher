#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

#include "logger/terminal_logger/terminal_logger.h"
#include "logging.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

class TerminalLoggerTest : public ::testing::Test
{
  protected:
    std::streambuf* originalCout;
    std::streambuf* originalCerr;
    std::ostringstream capturedOut;
    std::ostringstream capturedErr;

    void SetUp() override
    {
        LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::TerminalLogger>());

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

inline LoggerFramework::LogStream log_debug(LoggerFramework::LogEx& logger)
{
    return LoggerFramework::LogStream(logger, SeverityLevel::Debug);
}

TEST_F(TerminalLoggerTest, InlineStream)
{
    LoggerFramework::LogEx lg("InlineStream");
    log_debug(lg) << "Debug Message";

    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);

    EXPECT_NE(output.find("<DEBUG>"), std::string::npos);
    EXPECT_NE(output.find("[InlineStream]"), std::string::npos);
    EXPECT_NE(output.find("Debug Message"), std::string::npos);
}

TEST_F(TerminalLoggerTest, LogsAreWrittenToStreams)
{
    LoggerFramework::LogEx lg("LogsAreWrittenToStreams");
    LOG_DEBUG(lg) << "test";

    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);

    EXPECT_NE(output.find("DEBUG"), std::string::npos);
    EXPECT_NE(output.find("test"), std::string::npos);
}

TEST_F(TerminalLoggerTest, SeverityLevelCheck)
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

TEST_F(TerminalLoggerTest, MultipleLoggersAreIndependent)
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

TEST_F(TerminalLoggerTest, HandlesEmptyAndVeryLongMessages)
{
    LoggerFramework::LogEx lgc("EdgeCaseChannel");

    LOG_INFO(lgc) << "";
    LOG_INFO(lgc) << std::string(10000, 'x');

    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);

    EXPECT_NE(output.find(std::string(10000, 'x')), std::string::npos);
}

TEST_F(TerminalLoggerTest, DllWithTerminalLoggerTest)
{
    LoggerFramework::LogEx lg1("DllWithTerminalLoggerTest");
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

TEST_F(TerminalLoggerTest, UniqueChannelName_GeneratesUniqueIds)
{
    std::string name1 = UniqueChannelName::create("TestChannel");
    std::string name2 = UniqueChannelName::create("TestChannel");
    std::string name3 = UniqueChannelName::create("TestChannel");

    EXPECT_EQ(name1, "TestChannel#0");
    EXPECT_EQ(name2, "TestChannel#1");
    EXPECT_EQ(name3, "TestChannel#2");
}

TEST_F(TerminalLoggerTest, UniqueChannelName_DifferentBaseNames)
{
    LoggerFramework::LogEx lg1(UniqueChannelName::create("unique1"));
    LoggerFramework::LogEx lg2(UniqueChannelName::create("unique2"));
    LoggerFramework::LogEx lg3(UniqueChannelName::create("unique3"));
    LOG_INFO(lg1) << "message";
    LOG_INFO(lg2) << "message";
    LOG_INFO(lg3) << "message";
    std::string output = capturedOut.str() + capturedErr.str();
    showCaptured(output);

    UniqueChannelName::reset();

    std::string name1 = UniqueChannelName::create("Alpha");
    std::string name2 = UniqueChannelName::create("Beta");

    EXPECT_EQ(name1, "Alpha#0");
    EXPECT_EQ(name2, "Beta#1"); // globaler Zähler erhöht sich
}
