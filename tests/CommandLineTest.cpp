#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <sstream>

#include "TestSupport.hpp"
#include "frontend/CommandLine.hpp"
#include "logging/logging.h"

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
    void initTestCase ()
    {
        LoggerFramework::registerLogger(LoggerFramework::createLogger(false));
        watchOwnTestPrograms();
    }

    void init ()
    {
        qputenv("SC_LANGUAGES_URL", server.url("/languages.json").toUtf8());
        qputenv("SC_RELEASES_URL", server.url("/releases/latest").toUtf8());
        resetUserDir();
    }

    void requestsTheCommandLine_data ()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<bool>("expected");
        const QString program = "sc-localization-launcher";
        QTest::newRow("no arguments") << QStringList{program} << false;
        QTest::newRow("program name is not checked") << QStringList{"--check"} << false;
        QTest::newRow("--check") << QStringList{program, "--check"} << true;
        QTest::newRow("--update") << QStringList{program, "--update"} << true;
        QTest::newRow("--version") << QStringList{program, "--version"} << true;
        QTest::newRow("-v") << QStringList{program, "-v"} << true;
        QTest::newRow("--help") << QStringList{program, "--help"} << true;
        QTest::newRow("-h") << QStringList{program, "-h"} << true;
        QTest::newRow("option after other arguments") << QStringList{program, "--channel", "LIVE", "--update"} << true;
        QTest::newRow("only a channel") << QStringList{program, "--channel", "LIVE"} << false;
        QTest::newRow("unknown option") << QStringList{program, "--unknown"} << false;
        QTest::newRow("longer than an option") << QStringList{program, "--checkx"} << false;
        QTest::newRow("shorter than an option") << QStringList{program, "--chec"} << false;
        QTest::newRow("without dashes") << QStringList{program, "check"} << false;
        QTest::newRow("different case") << QStringList{program, "--Check"} << false;
    }

    void requestsTheCommandLine ()
    {
        QFETCH(QStringList, arguments);
        QFETCH(bool, expected);
        std::vector<QByteArray> storage;
        for(const auto& argument : arguments) storage.push_back(argument.toUtf8());
        std::vector<char*> argv;
        for(auto& s : storage) argv.push_back(s.data());
        argv.push_back(nullptr);
        QCOMPARE(CommandLine::requested(int(arguments.size()), argv.data()), expected);
    }

    void showsTheVersion ()
    {
        QCOMPARE(run({"--version"}), int(CommandLine::Ok));
        QCOMPARE(output, std::string("SC Localization Launcher 0.8.0\n"));
    }

    void showsTheHelp ()
    {
        for(const QString option : {"--help", "-h"})
        {
            QVERIFY2(run({option}) == int(CommandLine::Ok), qPrintable(option));
            QVERIFY2(errors.empty(), qPrintable(option));
            // Qt wraps long option descriptions to the terminal width
            const QString help = QString::fromStdString(output).simplified();
            for(const char* text : {"Installs and updates community translations for Star Citizen.",
                                    "Exit codes: 0 = current or updated, 1 = updates available (--check), 2 = error.", "--check",
                                    "Show which channels need an update, without changing anything.", "--update", "--channel", "--version", "--help"})
                QVERIFY2(help.contains(QString(text)), qPrintable(option + ": " + text));
        }
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

    void reportsAHandInstalledTranslation ()
    {
        // Placed by hand or by another tool: the version is unknown, so it counts as an update
        QTemporaryDir game;
        configure(game, "de");
        auto folder = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)";
        std::filesystem::create_directories(folder);
        std::ofstream(folder / "global.ini") << "test_key=Von Hand\n";

        QCOMPARE(run({"--check"}), int(CommandLine::UpdatesAvailable));
        QCOMPARE(output, std::string("LIVE: Deutsch, update available\n"));
        QCOMPARE(readFile(folder / "global.ini"), std::string("test_key=Von Hand\n"));
    }

    void reportsAndRepairsChangedFiles ()
    {
        QTemporaryDir game;
        configure(game, "de");
        QCOMPARE(run({"--update"}), int(CommandLine::Ok));
        // e.g. a game update overwrote the translation
        auto ini = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        std::ofstream(ini) << "test_key=Overwritten\n";

        QCOMPARE(run({"--check"}), int(CommandLine::UpdatesAvailable));
        QCOMPARE(output, std::string("LIVE: Deutsch, files changed, update to repair\n"));

        QCOMPARE(run({"--update"}), int(CommandLine::Ok));
        QCOMPARE(output, std::string("LIVE: Deutsch, up to date\n"));
        QCOMPARE(QByteArray::fromStdString(readFile(ini)), TRANSLATION);
    }

    void showsTheCodeOfALanguageMissingFromTheCatalogue ()
    {
        // "nocredit" is dropped from the catalogue, the settings still name it
        QTemporaryDir game;
        configure(game, "nocredit");
        QCOMPARE(run({"--check"}), int(CommandLine::UpdatesAvailable));
        QCOMPARE(output, std::string("LIVE: nocredit, not installed\n"));
    }

    void failsWhenNothingCanBeUpdated ()
    {
        // The chosen language is not offered, so no update starts, and the channel is still not current
        QTemporaryDir game;
        configure(game, "nocredit");
        QCOMPARE(run({"--update"}), int(CommandLine::Error));
        QCOMPARE(output, std::string("Nothing to update.\nLIVE: nocredit, not installed\n"));
        QVERIFY(!std::filesystem::exists(std::filesystem::path(game.path().toStdString()) / "data"));
    }

    void failsWhenTheLockCannotBeTakenOver ()
    {
        // The lock is held, but its holder no longer answers on its socket
        QTemporaryDir game;
        configure(game, "de");
        SingleInstance window(instance);
        QVERIFY(window.isPrimary());
        QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if(runtime.isEmpty()) runtime = QDir::tempPath();
        QVERIFY(QFile::remove(runtime + "/" + instance + ".socket"));

        QCOMPARE(run({"--update"}), int(CommandLine::Error));
        QVERIFY(!std::filesystem::exists(std::filesystem::path(game.path().toStdString()) / "data"));
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
        // Named like the game under Wine
        starCitizen.start("/bin/bash", {"-c", QString::fromStdString(fakeWineProcess(testGameProgram()))});
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
