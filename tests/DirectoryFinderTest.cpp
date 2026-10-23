#include <QTemporaryDir>
#include <QTest>
#include <filesystem>
#include <fstream>
#include <memory>

#include "backend/DirectoryFinder.hpp"
#include "logging/logger/custom_logger/custom_logger.h"

// Only writes into temporary directories; HOME points to one of them for the duration of a test
class DirectoryFinderTest : public QObject
{
    Q_OBJECT

   private:
    std::unique_ptr<QTemporaryDir> home;
    QByteArray originalHome = qgetenv("HOME");
    std::atomic<bool> shuttingDown{false};

    // A wine prefix with the RSI Launcher and the given channel folders
    static std::filesystem::path createInstallation (const std::filesystem::path& prefix, const std::vector<std::string>& channels)
    {
        auto rsi = prefix / "drive_c/Program Files/Roberts Space Industries";
        std::filesystem::create_directories(rsi / "RSI Launcher");
        std::ofstream(rsi / "RSI Launcher/RSI Launcher.exe") << "";
        for(const auto& channel : channels) std::filesystem::create_directories(rsi / "StarCitizen" / channel);
        return rsi;
    }

   private slots:
    void initTestCase () { LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>()); }

    void init ()
    {
        home = std::make_unique<QTemporaryDir>();
        qputenv("HOME", home->path().toUtf8());
    }

    void cleanup ()
    {
        qputenv("HOME", originalHome);
        home.reset();
    }

    void findsTheLugHelperDefaultPrefix ()
    {
        auto rsi = createInstallation(std::filesystem::path(home->path().toStdString()) / "Games/star-citizen", {"LIVE", "PTU"});

        auto result = DirectoryFinder({}).findRsiInstallation(shuttingDown);
        QVERIFY(result);
        QCOMPARE(result->rsiLauncherPath, rsi / "RSI Launcher");
        QCOMPARE(result->channelPaths.size(), size_t(2));
        QCOMPARE(result->channelPaths.at("LIVE"), rsi / "StarCitizen/LIVE");
        QCOMPARE(result->channelPaths.at("PTU"), rsi / "StarCitizen/PTU");
    }

    void findsAPrefixFromTheLutrisConfig ()
    {
        QTemporaryDir games;
        auto prefix = std::filesystem::path(games.path().toStdString()) / "my star citizen";
        auto rsi = createInstallation(prefix, {"LIVE"});

        auto config = std::filesystem::path(home->path().toStdString()) / ".local/share/lutris/games";
        std::filesystem::create_directories(config);
        std::ofstream(config / "star-citizen-123.yml")
            << "game:\n  exe: whatever\n  prefix: $GAMEDIR\nwine:\n  prefix: '" << prefix.string() << "'\n";

        auto result = DirectoryFinder({}).findRsiInstallation(shuttingDown);
        QVERIFY(result);
        QCOMPARE(result->rsiLauncherPath, rsi / "RSI Launcher");
        QCOMPARE(result->channelPaths.at("LIVE"), rsi / "StarCitizen/LIVE");
    }

    void findsAnInstallationOnAMountedDisk ()
    {
        QTemporaryDir mnt;
        auto rsi = createInstallation(std::filesystem::path(mnt.path().toStdString()) / "Games_1T/star-citizen", {"LIVE"});

        auto result = DirectoryFinder({mnt.path().toStdString()}).findRsiInstallation(shuttingDown);
        QVERIFY(result);
        QCOMPARE(result->rsiLauncherPath, rsi / "RSI Launcher");
    }
};

QTEST_GUILESS_MAIN(DirectoryFinderTest)
#include "DirectoryFinderTest.moc"
