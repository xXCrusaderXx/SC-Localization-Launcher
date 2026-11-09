#include <unistd.h>

#include <QTest>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <spdlog/logger.h>

#include "backend/utils/paths.hpp"
#include "logging/logging.h"

// Owns the log folder below its own XDG_STATE_HOME, which ctest sets for this executable only
class LoggerTest : public QObject
{
    Q_OBJECT

   private:
    const std::filesystem::path stateDir = PATHS::LOGPATH_USER;
    const std::filesystem::path logsDir = PATHS::LOGPATH_USER / "logs";

    std::vector<std::filesystem::path> logFiles () const
    {
        std::vector<std::filesystem::path> files;
        std::error_code ec;
        for(std::filesystem::directory_iterator it(logsDir, ec), end; !ec && it != end; it.increment(ec))
        {
            if(it->is_regular_file() && it->path().extension() == ".log") files.push_back(it->path());
        }
        return files;
    }

    static void touch (const std::filesystem::path& file, int daysOld)
    {
        std::ofstream(file) << "old";
        std::filesystem::last_write_time(file, std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * daysOld));
    }

    // old_00.log is the oldest, old_11.log the newest
    void createOldLogs () const
    {
        std::filesystem::create_directories(logsDir);
        for(int i = 0; i < 12; ++i)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "old_%02d.log", i);
            touch(logsDir / name, 12 - i);
        }
    }

    static QString readFile (const std::filesystem::path& file)
    {
        std::ifstream in(file);
        std::stringstream content;
        content << in.rdbuf();
        return QString::fromStdString(content.str());
    }

   private slots:
    void initTestCase ()
    {
        // The cases delete the log folder, so they only run inside the test HOME that ctest sets
        const QByteArray testHome = qgetenv("SC_TEST_HOME");
        if(testHome.isEmpty() || stateDir.string().rfind(testHome.toStdString(), 0) != 0) QSKIP("Runs only through ctest with its own HOME");
    }

    void cleanup ()
    {
        std::error_code ec;
        std::filesystem::permissions(logsDir, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, ec);
        std::filesystem::remove_all(stateDir, ec);
    }

    void createsTheMissingLogFolderWithoutAFile ()
    {
        std::filesystem::remove_all(stateDir);
        QVERIFY(LoggerFramework::createLogger(false));
        QVERIFY(std::filesystem::is_directory(logsDir));
        QVERIFY(logFiles().empty());
    }

    void throwsWhenTheLogFolderCannotBeCreated ()
    {
        std::filesystem::remove_all(stateDir);
        std::filesystem::create_directories(stateDir.parent_path());
        std::ofstream(stateDir) << "not a folder";

        bool thrown = false;
        try
        {
            LoggerFramework::createLogger(false);
        }
        catch(const std::runtime_error& e)
        {
            thrown = true;
            QVERIFY2(std::string(e.what()).find(logsDir.string()) != std::string::npos, e.what());
        }
        QVERIFY(thrown);
    }

    void firstLineCreatesOneFileAndKeepsTheNewestLogs ()
    {
        createOldLogs();
        touch(logsDir / "notes.txt", 100);
        std::filesystem::create_directories(logsDir / "dir.log");
        std::filesystem::last_write_time(logsDir / "dir.log", std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * 100));

        LoggerFramework::LogEx lg("LoggerTest");
        LoggerFramework::registerLogger(LoggerFramework::createLogger(false));
        QCOMPARE(logFiles().size(), size_t(12));
        LOG_INFO(lg) << "hello " << 42;
        LOG_ERROR(lg) << "second line";
        LoggerFramework::registerLogger(nullptr);

        QCOMPARE(logFiles().size(), size_t(10));
        for(int i = 0; i < 12; ++i)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "old_%02d.log", i);
            QVERIFY2(std::filesystem::exists(logsDir / name) == (i >= 3), name);
        }
        QVERIFY(std::filesystem::is_regular_file(logsDir / "notes.txt"));
        QVERIFY(std::filesystem::is_directory(logsDir / "dir.log"));

        std::vector<std::filesystem::path> debugLogs;
        for(const auto& file : logFiles())
        {
            if(file.filename().string().rfind("debug_", 0) == 0) debugLogs.push_back(file);
        }
        QCOMPARE(debugLogs.size(), size_t(1));
        // A name Windows accepts, e.g. debug_2026-11-09_10-54-26.929.log
        const std::string name = debugLogs.front().filename().string();
        QVERIFY2(name.find_first_of(": ") == std::string::npos, name.c_str());
        const QString content = readFile(debugLogs.front());
        QVERIFY2(content.contains("<INFO>\t[LoggerTest]\t: hello 42\n"), qPrintable(content));
        QVERIFY2(content.contains("<ERROR>\t[LoggerTest]\t: second line\n"), qPrintable(content));
        QCOMPARE(content.count('\n'), 2);
    }

    void dropsLinesWithoutALoggerAndLogsOnceOneIsRegistered ()
    {
        std::filesystem::remove_all(stateDir);
        // Created before the logger, like a channel in an object built before main() registers it
        LoggerFramework::LogEx lg("Early");
        LOG_INFO(lg) << "dropped";

        auto logger = LoggerFramework::createLogger(false);
        LoggerFramework::registerLogger(logger);
        LOG_WARNING(lg) << "written";
        LoggerFramework::registerLogger(nullptr);
        LOG_INFO(lg) << "dropped too";
        logger.reset();

        const auto files = logFiles();
        QCOMPARE(files.size(), size_t(1));
        const QString content = readFile(files.front());
        QVERIFY2(content.contains("<WARNING>\t[Early]\t: written\n"), qPrintable(content));
        QVERIFY(!content.contains("dropped"));
    }

    void keepsLogsWhenTheyCannotBeDeleted ()
    {
        if(geteuid() == 0) QSKIP("root can delete files in a read-only folder");
        createOldLogs();
        std::filesystem::permissions(logsDir, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::replace);

        LoggerFramework::LogEx lg("LoggerTest");
        LoggerFramework::registerLogger(LoggerFramework::createLogger(false));
        LOG_INFO(lg) << "hello";
        LOG_INFO(lg) << "again";
        LoggerFramework::registerLogger(nullptr);

        QCOMPARE(logFiles().size(), size_t(12));
        QVERIFY(std::filesystem::exists(logsDir / "old_00.log"));
    }
};

QTEST_GUILESS_MAIN(LoggerTest)
#include "LoggerTest.moc"
