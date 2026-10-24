#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <sstream>

#include "TestSupport.hpp"
#include "frontend/CommandLine.hpp"
#include "logging/logger/custom_logger/custom_logger.h"

// Runs the headless mode like a script would and checks its output, exit code and the files it writes
class CommandLineTest : public QObject
{
    Q_OBJECT

   private:
    TestServer server{fixtureFiles()};
    // Unique per run, so parallel test runs never share a lock
    const QString instance = QString("sc-localization-launcher-cli-test-%1").arg(QCoreApplication::applicationPid());
    std::string output;
    std::string errors;

    int run (const QStringList& options)
    {
        std::ostringstream out;
        std::ostringstream err;
        int code = CommandLine(instance, "0.8.0", out, err).run(QStringList{"sc-localization-launcher"} + options);
        output = out.str();
        errors = err.str();
        return code;
    }

    // LIVE with a game folder and this language; launchGame: the window's "start afterwards" option
    static void configure (const QTemporaryDir& game, const std::string& language, bool launchGame = false, const std::string& rsiLauncher = "")
    {
        writeSettingsFile({{"channels", {{"LIVE", {{"installPath", game.path().toStdString()}, {"language", language}}}}},
                           {"options", {{"launchGameAfterUpdate", launchGame}}},
                           {"rsiLauncherPath", rsiLauncher}});
    }

   private slots:
    void initTestCase () { LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>(false)); }

    void init ()
    {
        qputenv("SC_LANGUAGES_URL", server.url("/languages.json").toUtf8());
        qputenv("SC_RELEASES_URL", server.url("/releases/latest").toUtf8());
        resetUserDir();
    }

    void showsTheVersion ()
    {
        QCOMPARE(run({"--version"}), int(CommandLine::Ok));
        QCOMPARE(output, std::string("SC Localization Launcher 0.8.0\n"));
    }

    void needsExactlyOneAction ()
    {
        QCOMPARE(run({}), int(CommandLine::Error));
        QCOMPARE(run({"--check", "--update"}), int(CommandLine::Error));
        QCOMPARE(run({"--unknown"}), int(CommandLine::Error));
    }

    void failsWithoutGameFolder ()
    {
        QCOMPARE(run({"--check"}), int(CommandLine::Error));
        QVERIFY(errors.find("No channel has a game folder") != std::string::npos);
    }

    void rejectsUnknownChannels ()
    {
        QTemporaryDir game;
        configure(game, "de");
        QCOMPARE(run({"--check", "--channel", "NOPE"}), int(CommandLine::Error));
        QCOMPARE(errors, std::string("Unknown channel: NOPE\n"));
        // Known, but without a game folder
        QCOMPARE(run({"--check", "--channel", "PTU"}), int(CommandLine::Error));
    }

    void checksAndUpdates ()
    {
        QTemporaryDir game;
        configure(game, "de");

        QCOMPARE(run({"--check"}), int(CommandLine::UpdatesAvailable));
        QCOMPARE(output, std::string("LIVE: Deutsch, not installed\n"));
        QVERIFY(!std::filesystem::exists(std::filesystem::path(game.path().toStdString()) / "data"));

        QCOMPARE(run({"--update", "--channel", "LIVE"}), int(CommandLine::Ok));
        QCOMPARE(output, std::string("LIVE: Deutsch, up to date\n"));
        auto ini = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        QCOMPARE(QByteArray::fromStdString(readFile(ini)), TRANSLATION);

        // The next run knows the installed version
        QCOMPARE(run({"--check"}), int(CommandLine::Ok));
        QCOMPARE(output, std::string("LIVE: Deutsch, up to date\n"));
    }

    void reportsAFailedUpdate ()
    {
        // Both German (full) URLs answer 404
        QTemporaryDir game;
        configure(game, "de-full");
        QCOMPARE(run({"--update"}), int(CommandLine::Error));
        QVERIFY(errors.find("The update failed") != std::string::npos);
        QCOMPARE(output, std::string("LIVE: Deutsch (vollständig), last update failed\n"));
    }

    void neverStartsTheGame ()
    {
        // "Start afterwards" is on and a LUG script is there; a script must still not start the game
        QTemporaryDir game;
        QTemporaryDir prefix;
        auto root = std::filesystem::path(prefix.path().toStdString());
        auto rsiLauncher = root / "drive_c/Program Files/Roberts Space Industries/RSI Launcher";
        std::filesystem::create_directories(rsiLauncher);
        std::ofstream(root / "sc-launch.sh") << "touch started\n";
        qputenv("PATH", "/usr/bin:/bin");
        configure(game, "de", true, rsiLauncher.string());

        int code = run({"--update"});
        QTest::qWait(1000);
        qputenv("PATH", "/nonexistent");
        QCOMPARE(code, int(CommandLine::Ok));
        QVERIFY(!std::filesystem::exists(root / "started"));
    }

    void doesNotRunBesideTheWindow ()
    {
        QTemporaryDir game;
        configure(game, "de");
        SingleInstance window(instance);
        QVERIFY(window.isPrimary());
        QSignalSpy activated(&window, &SingleInstance::activationRequested);

        QCOMPARE(run({"--update"}), int(CommandLine::Error));
        QVERIFY(errors.find("is open") != std::string::npos);
        // Only asked whether it runs; its window stays where it is
        QTest::qWait(200);
        QCOMPARE(activated.count(), 0);
        QVERIFY(!std::filesystem::exists(std::filesystem::path(game.path().toStdString()) / "data"));
    }

    void doesNotUpdateWhileTheGameRuns ()
    {
        QTemporaryDir game;
        configure(game, "de");
        qputenv("PATH", "/usr/bin:/bin");
        QProcess starCitizen;
        // Named like the game under Wine (SC_GAME_PROGRAM in tests/CMakeLists.txt)
        starCitizen.start("/bin/bash", {"-c", "exec -a 'C:\\Games\\StarCitizen\\LIVE\\Bin64\\StarCitizen Test.exe' sleep 30"});
        QVERIFY(starCitizen.waitForStarted());
        QTest::qWait(200);

        int code = run({"--update"});
        starCitizen.kill();
        starCitizen.waitForFinished();
        qputenv("PATH", "/nonexistent");
        QCOMPARE(code, int(CommandLine::Error));
        QVERIFY(errors.find("Star Citizen is running") != std::string::npos);
        QVERIFY(!std::filesystem::exists(std::filesystem::path(game.path().toStdString()) / "data"));
    }
};

QTEST_GUILESS_MAIN(CommandLineTest)
#include "CommandLineTest.moc"
