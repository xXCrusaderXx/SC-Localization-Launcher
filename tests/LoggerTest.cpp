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

#include "logging/logger/custom_logger/custom_logger.h"

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

    void writesTheNameOfEverySeverityLevel_data ()
    {
        // An int column: the logger's operator<< template for SeverityLevel also matches QTestData and makes the row ambiguous
        QTest::addColumn<int>("level");
        QTest::addColumn<QString>("name");
        QTest::newRow("Debug") << int(SeverityLevel::Debug) << "DEBUG";
        QTest::newRow("Info") << int(SeverityLevel::Info) << "INFO";
        QTest::newRow("Warning") << int(SeverityLevel::Warning) << "WARNING";
        QTest::newRow("Error") << int(SeverityLevel::Error) << "ERROR";
        QTest::newRow("Critical") << int(SeverityLevel::Critical) << "CRITICAL";
        QTest::newRow("Process") << int(SeverityLevel::Process) << "PROCESS";
        QTest::newRow("Test") << int(SeverityLevel::Test) << "TEST";
        QTest::newRow("System") << int(SeverityLevel::System) << "SYSTEM";
    }

    void writesTheNameOfEverySeverityLevel ()
    {
        QFETCH(int, level);
        QFETCH(QString, name);
        std::ostringstream os;
        os << static_cast<SeverityLevel>(level);
        QCOMPARE(QString::fromStdString(os.str()), name);
    }

    void writesUnknownForAValueOutsideTheEnum ()
    {
        std::ostringstream os;
        os << static_cast<SeverityLevel>(99);
        QCOMPARE(QString::fromStdString(os.str()), QString("UNKNOWN"));
    }

    void returnsTheStreamForChaining ()
    {
        std::ostringstream os;
        os << SeverityLevel::Error << "|" << SeverityLevel::Debug;
        QCOMPARE(QString::fromStdString(os.str()), QString("ERROR|DEBUG"));
        QVERIFY(&(os << SeverityLevel::Info) == &os);
    }

    void createsTheMissingLogFolderWithoutAFile ()
    {
        std::filesystem::remove_all(stateDir);
        {
            LoggerFramework::CustomLogger logger(false);
        }
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
            LoggerFramework::CustomLogger logger(false);
        }
        catch(const std::runtime_error& e)
        {
            thrown = true;
            QVERIFY2(std::string(e.what()).find(logsDir.string()) != std::string::npos, e.what());
        }
        QVERIFY(thrown);
    }

    void firstWriteDeletesTheOldestLogs ()
    {
        createOldLogs();
        touch(logsDir / "notes.txt", 100);
        std::filesystem::create_directories(logsDir / "dir.log");
        std::filesystem::last_write_time(logsDir / "dir.log", std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * 100));

        {
            LoggerFramework::CustomLogger logger(false);
            logger.write("LoggerTest", "hello", SeverityLevel::Info);
        }

        QVERIFY(!std::filesystem::exists(logsDir / "old_00.log"));
        QVERIFY(!std::filesystem::exists(logsDir / "old_01.log"));
        for(int i = 3; i < 12; ++i)
        {
            char name[16];
            std::snprintf(name, sizeof(name), "old_%02d.log", i);
            QVERIFY2(std::filesystem::exists(logsDir / name), name);
        }
        QVERIFY(std::filesystem::is_regular_file(logsDir / "notes.txt"));
        QVERIFY(std::filesystem::is_directory(logsDir / "dir.log"));

        std::vector<std::filesystem::path> debugLogs;
        for(const auto& file : logFiles())
        {
            if(file.filename().string().rfind("debug_", 0) == 0) debugLogs.push_back(file);
        }
        QCOMPARE(debugLogs.size(), size_t(1));
        QVERIFY(readFile(debugLogs.front()).contains("<INFO>\t[LoggerTest]\t: hello"));
    }

    void keepsLogsWhenTheyCannotBeDeleted ()
    {
        if(geteuid() == 0) QSKIP("root can delete files in a read-only folder");
        createOldLogs();
        std::filesystem::permissions(logsDir, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::replace);

        {
            LoggerFramework::CustomLogger logger(false);
            logger.write("LoggerTest", "hello", SeverityLevel::Info);
        }

        QVERIFY(std::filesystem::exists(logsDir / "old_00.log"));
        QVERIFY(std::filesystem::exists(logsDir / "old_01.log"));
    }
};

QTEST_GUILESS_MAIN(LoggerTest)
#include "LoggerTest.moc"
