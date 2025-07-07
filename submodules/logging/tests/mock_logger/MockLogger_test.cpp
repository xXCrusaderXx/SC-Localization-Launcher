#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

#include "logger/mock_logger/mock_logger.h"
#include "logging.h"

class MockLoggerTest : public ::testing::Test
{
  protected:
    std::streambuf* originalCout;
    std::streambuf* originalCerr;
    std::ostringstream capturedOut;
    std::ostringstream capturedErr;

    void SetUp() override
    {
        LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::MockLogger>());

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
};

TEST_F(MockLoggerTest, noOutputCheck)
{

    LoggerFramework::LogEx lg("noOutputCheck");
    LOG_DEBUG(lg) << "Debug Message";

    std::string output = capturedOut.str() + capturedErr.str();

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(output.find("<DEBUG>"), std::string::npos);
    EXPECT_EQ(output.find("[noOutputCheck]"), std::string::npos);
    EXPECT_EQ(output.find("Debug Message"), std::string::npos);
}
