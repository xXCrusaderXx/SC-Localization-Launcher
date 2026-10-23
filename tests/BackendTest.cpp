#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>

#include "TestSupport.hpp"
#include "UpdateSigningKey.hpp"
#include "backend/BackEnd.hpp"
#include "logging/logger/custom_logger/custom_logger.h"

using Protocol::ChannelStatus;
using Protocol::UpdateStatus;
using Codes = std::vector<std::string>;

// Drives BackEnd through its commands and checks the published states and the files it writes
class BackendTest : public QObject
{
    Q_OBJECT

   private:
    TestServer server{fixtureFiles()};
    std::unique_ptr<BackEnd> backEnd;
    std::mutex mutex;
    std::vector<Protocol::State> states;

    static Protocol::Channel channel (const Protocol::State& state, const std::string& name)
    {
        auto it = std::find_if(state.channels.begin(), state.channels.end(), [&] (const auto& c) { return c.name == name; });
        return it != state.channels.end() ? *it : Protocol::Channel{};
    }

    static Codes languageCodes (const Protocol::State& state)
    {
        Codes codes;
        for(const auto& language : state.languages) codes.push_back(language.code);
        return codes;
    }

    size_t published ()
    {
        std::lock_guard lock(mutex);
        return states.size();
    }

    Protocol::State latest ()
    {
        std::lock_guard lock(mutex);
        return states.back();
    }

    // Index of the first state from `from` on that matches
    template <typename Predicate>
    std::optional<size_t> waitForState (Predicate predicate, size_t from = 0, int timeoutMs = 30000)
    {
        std::optional<size_t> found;
        bool matched = QTest::qWaitFor(
            [&]
            {
                std::lock_guard lock(mutex);
                for(size_t i = from; i < states.size() && !found; ++i)
                    if(predicate(states[i])) found = i;
                return found.has_value();
            },
            timeoutMs);
        return matched ? found : std::nullopt;
    }

    bool start ()
    {
        backEnd = std::make_unique<BackEnd>(
            [this] (const Protocol::State& state)
            {
                std::lock_guard lock(mutex);
                states.push_back(state);
            },
            "0.6.0");
        // The language list is loaded in the background, a later state carries it
        return waitForState([] (const auto& s) { return s.languages.size() > 1; }).has_value();
    }

    // Sets a language and waits until the state shows it
    bool setLanguage (const std::string& name, const std::string& code)
    {
        size_t from = published();
        backEnd->execute(Protocol::SetLanguage{name, code});
        return waitForState([&] (const auto& s) { return channel(s, name).language == code; }, from).has_value();
    }

    // Commands run in order, so once this marker shows up, all commands sent before it are done
    bool waitForPreviousCommands ()
    {
        bool value = !latest().options.showUpdateProgress;
        size_t from = published();
        backEnd->execute(Protocol::SetOption{"showUpdateProgress", value});
        return waitForState([&] (const auto& s) { return s.options.showUpdateProgress == value; }, from).has_value();
    }

    // Like closing and opening the launcher again
    bool restart ()
    {
        backEnd.reset();
        {
            std::lock_guard lock(mutex);
            states.clear();
        }
        return start();
    }

    // Installs German into a fresh game folder on LIVE
    bool installGerman (const QTemporaryDir& game)
    {
        if(!start()) return false;
        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        return setLanguage("LIVE", "de") && runUpdate() == UpdateStatus::Succeeded && channel(latest(), "LIVE").status == ChannelStatus::Installed;
    }

    // Runs one update and returns its final status
    std::optional<UpdateStatus> runUpdate ()
    {
        size_t from = published();
        backEnd->execute(Protocol::StartUpdate{});
        auto running = waitForState([] (const auto& s) { return s.update == UpdateStatus::Running; }, from);
        if(!running) return std::nullopt;
        auto done = waitForState([] (const auto& s) { return s.update != UpdateStatus::Running; }, *running + 1);
        if(!done) return std::nullopt;
        std::lock_guard lock(mutex);
        return states[*done].update;
    }

    TestSigningKey releaseKey;
    // Checks for the launcher update and installs it if it can; returns the final self-update state
    Protocol::SelfUpdate checkAndInstall ()
    {
        using Protocol::SelfUpdateStatus;
        size_t from = published();
        backEnd->execute(Protocol::CheckForUpdate{});
        auto checked = waitForState(
            [] (const auto& s) { return s.selfUpdate.status != SelfUpdateStatus::Checking && s.selfUpdate.status != SelfUpdateStatus::Idle; }, from);
        if(!checked) return {};
        Protocol::SelfUpdate update;
        {
            std::lock_guard lock(mutex);
            update = states[*checked].selfUpdate;
        }
        if(!update.canInstall) return update;

        from = published();
        backEnd->execute(Protocol::InstallUpdate{});
        auto done =
            waitForState([] (const auto& s)
                         { return s.selfUpdate.status == SelfUpdateStatus::Installed || s.selfUpdate.status == SelfUpdateStatus::Error; }, from);
        std::lock_guard lock(mutex);
        return done ? states[*done].selfUpdate : Protocol::SelfUpdate{};
    }

    // An AppImage in a temporary folder that the launcher runs from
    std::filesystem::path fakeAppImage (const QTemporaryDir& dir)
    {
        auto path = std::filesystem::path(dir.path().toStdString()) / "SC-Localization-Launcher.AppImage";
        std::ofstream(path) << "old version";
        qputenv("APPIMAGE", QByteArray::fromStdString(path.string()));
        return path;
    }

   private slots:
    void initTestCase () { LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>()); }

    void init ()
    {
        qputenv("SC_LANGUAGES_URL", server.url("/languages.json").toUtf8());
        qputenv("SC_RELEASES_URL", server.url("/releases/latest").toUtf8());
        server.setFile("/de.ini", TRANSLATION);
        server.setFile("/releases/latest", fixtureFiles().value("/releases/latest"));
        // No signing key and no AppImage unless a test sets them: the launcher then never installs anything
        qputenv("SC_UPDATE_SIGNING_KEY", "");
        qunsetenv("APPIMAGE");
        resetUserDir();
        std::lock_guard lock(mutex);
        states.clear();
    }

    void cleanup () { backEnd.reset(); }

    void startsWithDefaultsWithoutSettingsFile ()
    {
        QVERIFY(start());
        auto state = latest();

        for(size_t i = 0; i < Protocol::CHANNEL_NAMES.size(); ++i)
        {
            QCOMPARE(state.channels[i].name, Protocol::CHANNEL_NAMES[i]);
            QCOMPARE(state.channels[i].installPath, std::string());
            QCOMPARE(state.channels[i].language, Protocol::ENGLISH);
        }
        QCOMPARE(jsonText(state.options), jsonText(Protocol::Options{}));
        QVERIFY(state.update == UpdateStatus::Idle);
    }

    void migratesSettingsOfTheFormerLauncher ()
    {
        writeSettingsFile(legacySettings("/games/LIVE", "deVoll", true), PATHS::JSON_FILES::SETTINGS_LEGACY);
        QVERIFY(start());
        auto state = latest();

        QCOMPARE(channel(state, "LIVE").installPath, std::string("/games/LIVE"));
        QCOMPARE(channel(state, "LIVE").language, std::string("de-full"));
        QVERIFY(state.options.launchGameAfterUpdate);
        QCOMPARE(state.rsiLauncherPath, std::string("/nonexistent/RSI Launcher"));

        // Written to the new location in the new format right away; the old file stays untouched
        auto file = readSettingsFile();
        QVERIFY(!file.contains("settings"));
        QCOMPARE(jsonText(file["channels"]["LIVE"]["language"]), jsonText("de-full"));
        QCOMPARE(jsonText(file["options"]["launchGameAfterUpdate"]), jsonText(true));
        QVERIFY(std::filesystem::exists(PATHS::JSON_FILES::SETTINGS_LEGACY));
    }

    void offersOnlyWhatTheSourcesProvide ()
    {
        QVERIFY(start());
        auto state = latest();

        // Invalid entries (unsafe folder, plain HTTP, duplicate code, no credit) are dropped
        QCOMPARE(languageCodes(state), (Codes{"en", "de", "de-full", "fr"}));
        // Every translation carries its credit, English as the original text has none
        QCOMPARE(state.languages[0].credit.project, std::string());
        for(size_t i = 1; i < state.languages.size(); ++i) QCOMPARE(jsonText(state.languages[i].credit), jsonText(testCredit()));
        QCOMPARE(channel(state, "LIVE").availableLanguages, (Codes{"en", "de", "de-full", "fr"}));
        QCOMPARE(channel(state, "PTU").availableLanguages, Codes{"en"});
        QCOMPARE(channel(state, "EPTU").availableLanguages, (Codes{"en", "fr"}));
        QCOMPARE(channel(state, "TECH-PREVIEW").availableLanguages, (Codes{"en", "de"}));
        QVERIFY(std::none_of(state.channels.begin(), state.channels.end(), [] (const auto& c) { return c.name == "TECH-PREV"; }));
    }

    void usesBuiltInListWithoutNetwork ()
    {
        qputenv("SC_LANGUAGES_URL", server.url("/does-not-exist.json").toUtf8());
        QVERIFY(start());

        auto bundled = nlohmann::json::parse(BUNDLED_LANGUAGES_JSON);
        Codes expected{"en"};
        for(const auto& language : bundled["languages"]) expected.push_back(language["code"]);
        QCOMPARE(languageCodes(latest()), expected);
    }

    void rejectsInvalidCommands ()
    {
        QVERIFY(start());
        backEnd->execute(Protocol::SetLanguage{"NOPE", "de"});
        backEnd->execute(Protocol::SetLanguage{"LIVE", "xx"});
        backEnd->execute(Protocol::SetInstallPath{"NOPE", "/games/NOPE"});
        backEnd->execute(Protocol::SetOption{"noSuchOption", true});
        QVERIFY(waitForPreviousCommands());
        auto state = latest();

        QCOMPARE(channel(state, "LIVE").language, Protocol::ENGLISH);
        QVERIFY(std::none_of(state.channels.begin(), state.channels.end(), [] (const auto& c) { return c.name == "NOPE"; }));
        QVERIFY(!readSettingsFile()["options"].contains("noSuchOption"));
    }

    void switchesBetweenLanguages ()
    {
        QTemporaryDir game;
        auto gamePath = std::filesystem::path(game.path().toStdString());
        std::ofstream(gamePath / "user.cfg") << "r_DisplayInfo = 2\n";

        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", gamePath.string()});
        QVERIFY(setLanguage("LIVE", "de"));

        // German comes from the fallback URL, the first one answers 404
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QCOMPARE(QByteArray::fromStdString(readFile(gamePath / "data/Localization/german_(germany)/global.ini")), TRANSLATION);
        QCOMPARE(readFile(gamePath / "user.cfg"), std::string("r_DisplayInfo = 2\ng_language = german_(germany)\ng_languageAudio = english\n"));

        QVERIFY(setLanguage("LIVE", "fr"));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QCOMPARE(QByteArray::fromStdString(readFile(gamePath / "data/Localization/french_(france)/global.ini")), TRANSLATION_FR);
        QVERIFY(!std::filesystem::exists(gamePath / "data/Localization/german_(germany)"));
        QCOMPARE(readFile(gamePath / "user.cfg"), std::string("r_DisplayInfo = 2\ng_language = french_(france)\ng_languageAudio = english\n"));

        QVERIFY(setLanguage("LIVE", Protocol::ENGLISH));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QVERIFY(!std::filesystem::exists(gamePath / "data"));
        QCOMPARE(readFile(gamePath / "user.cfg"), std::string("r_DisplayInfo = 2\n"));
    }

    void failedDownloadKeepsInstalledTranslation ()
    {
        QTemporaryDir game;
        auto gamePath = std::filesystem::path(game.path().toStdString());
        auto ini = gamePath / "data/Localization/german_(germany)/global.ini";

        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", gamePath.string()});
        QVERIFY(setLanguage("LIVE", "de"));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);

        // Both German full URLs answer 404
        QVERIFY(setLanguage("LIVE", "de-full"));
        QCOMPARE(runUpdate(), UpdateStatus::Failed);
        QCOMPARE(QByteArray::fromStdString(readFile(ini)), TRANSLATION);
        QVERIFY(!std::filesystem::exists(ini.string() + ".download"));
        QVERIFY(readFile(gamePath / "user.cfg").find("g_language = german_(germany)") != std::string::npos);
    }

    void skipsChoiceNoSourceOffers ()
    {
        QTemporaryDir game;
        auto gamePath = std::filesystem::path(game.path().toStdString());
        auto ini = gamePath / "data/Localization/german_(germany)/global.ini";
        std::filesystem::create_directories(ini.parent_path());
        std::ofstream(ini) << "installed earlier\n";

        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"PTU", gamePath.string()});
        // The choice is kept and shown, even though PTU has no German source right now
        QVERIFY(setLanguage("PTU", "de"));
        QVERIFY(channel(latest(), "PTU").availableLanguages == Codes{"en"});
        QCOMPARE(jsonText(readSettingsFile()["channels"]["PTU"]["language"]), jsonText("de"));

        // Nothing else to update, so the update does not even start, and nothing is removed
        backEnd->execute(Protocol::StartUpdate{});
        QVERIFY(waitForPreviousCommands());
        QVERIFY(latest().update == UpdateStatus::Idle);
        QCOMPARE(readFile(ini), std::string("installed earlier\n"));
    }

    void statusFollowsInstallation ()
    {
        QTemporaryDir game;
        QVERIFY(start());
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::NoPath);

        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        QVERIFY(setLanguage("LIVE", "de"));
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::NotInstalled);

        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::Installed);
        auto installed = readSettingsFile()["channels"]["LIVE"]["installed"];
        QCOMPARE(jsonText(installed["language"]), jsonText("de"));
        QVERIFY(!installed["etag"].get<std::string>().empty());

        // Back to English: the German files are still there until the next update removes them
        QVERIFY(setLanguage("LIVE", Protocol::ENGLISH));
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::UpdateAvailable);
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::Installed);
    }

    void unchangedTranslationIsNotDownloadedAgain ()
    {
        QTemporaryDir game;
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        QVERIFY(setLanguage("LIVE", "de"));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        int downloads = server.requestCount("GET /de.ini 200");
        int notModified = server.requestCount("GET /de.ini 304");

        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QCOMPARE(server.requestCount("GET /de.ini 200"), downloads);
        QCOMPARE(server.requestCount("GET /de.ini 304"), notModified + 1);
        QCOMPARE(
            QByteArray::fromStdString(readFile(std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini")),
            TRANSLATION);
    }

    void newerVersionIsFoundAtStart ()
    {
        QTemporaryDir game;
        auto ini = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        QVERIFY(setLanguage("LIVE", "de"));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);

        // The translation project publishes a new version while the launcher is closed
        const QByteArray newVersion = "test_key=Neuer Testtext\n";
        server.setFile("/de.ini", newVersion);
        backEnd.reset();
        {
            std::lock_guard lock(mutex);
            states.clear();
        }
        QVERIFY(start());
        QVERIFY(waitForState([] (const auto& s) { return channel(s, "LIVE").status == ChannelStatus::UpdateAvailable; }));

        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QCOMPARE(QByteArray::fromStdString(readFile(ini)), newVersion);
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::Installed);
    }

    void translationOfTheFormerLauncherNeedsAnUpdate ()
    {
        // Installed by the former SC Deutsch Launcher: the files are there, but their version is unknown
        QTemporaryDir game;
        auto ini = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        std::filesystem::create_directories(ini.parent_path());
        std::ofstream(ini) << "old version\n";
        writeSettingsFile(legacySettings(game.path().toStdString(), "de", false), PATHS::JSON_FILES::SETTINGS_LEGACY);

        QVERIFY(start());
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::UpdateAvailable);
    }

    void comparesLauncherVersions ()
    {
        QVERIFY(SelfUpdateManager::isNewer("v0.10.0", "0.9.9"));
        QVERIFY(SelfUpdateManager::isNewer("1.0.0", "0.99.99"));
        QVERIFY(!SelfUpdateManager::isNewer("v0.6.0", "0.6.0"));
        QVERIFY(!SelfUpdateManager::isNewer("0.5.9", "0.6.0"));
        QVERIFY(!SelfUpdateManager::isNewer("nightly", "0.6.0"));
    }

    void findsNewerLauncherRelease ()
    {
        QVERIFY(start());
        QVERIFY(latest().selfUpdate.status == Protocol::SelfUpdateStatus::Idle);
        QCOMPARE(latest().selfUpdate.currentVersion, std::string("0.6.0"));

        backEnd->execute(Protocol::CheckForUpdate{});
        QVERIFY(waitForState([] (const auto& s) { return s.selfUpdate.status == Protocol::SelfUpdateStatus::Available; }));
        auto update = latest().selfUpdate;
        QCOMPARE(update.latestVersion, std::string("9.9.9"));
        QCOMPARE(update.notes, std::string("- New feature"));
        // Not the example.org link from the answer: only GitHub pages are opened
        QCOMPARE(update.url, std::string("https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/latest"));
    }

    void launcherWithoutNewerReleaseIsUpToDate ()
    {
        nlohmann::json release = {{"tag_name", "v0.6.0"}, {"body", ""}, {"html_url", "https://github.com/x/y/releases/tag/v0.6.0"}};
        server.setFile("/releases/latest", QByteArray::fromStdString(release.dump()));
        QVERIFY(start());
        backEnd->execute(Protocol::CheckForUpdate{});
        QVERIFY(waitForState([] (const auto& s) { return s.selfUpdate.status == Protocol::SelfUpdateStatus::UpToDate; }));

        // No release published at all
        qputenv("SC_RELEASES_URL", server.url("/no-release").toUtf8());
        backEnd->execute(Protocol::CheckForUpdate{});
        QVERIFY(waitForState([] (const auto& s) { return s.selfUpdate.status == Protocol::SelfUpdateStatus::Checking; }));
        QVERIFY(waitForState([] (const auto& s) { return s.selfUpdate.status == Protocol::SelfUpdateStatus::UpToDate; }, published() - 1));
        server.setFile("/releases/latest", fixtureFiles().value("/releases/latest"));
    }

    void installsSignedAppImageUpdate ()
    {
        QTemporaryDir dir;
        auto appImage = fakeAppImage(dir);
        qputenv("SC_UPDATE_SIGNING_KEY", QByteArray::fromStdString(releaseKey.publicKeyFile()));
        publishAppImageRelease(server, releaseKey, "new version");
        QVERIFY(start());

        auto update = checkAndInstall();
        QCOMPARE(update.installType, std::string("appimage"));
        QCOMPARE(jsonText(update.status), jsonText(Protocol::SelfUpdateStatus::Installed));
        QCOMPARE(readFile(appImage), std::string("new version"));
        QVERIFY((std::filesystem::status(appImage).permissions() & std::filesystem::perms::owner_exec) != std::filesystem::perms::none);
        QVERIFY(!std::filesystem::exists(appImage.string() + ".new"));
    }

    void rejectsUpdateSignedWithAnotherKey ()
    {
        QTemporaryDir dir;
        auto appImage = fakeAppImage(dir);
        qputenv("SC_UPDATE_SIGNING_KEY", QByteArray::fromStdString(releaseKey.publicKeyFile()));
        TestSigningKey otherKey;
        std::string sums = sha256Hex("evil version") + "  " + APPIMAGE_ASSET + "\n";
        publishAppImageRelease(server, releaseKey, "evil version", sums, otherKey.sign(sums, "sc-localization-launcher v9.9.9"));
        QVERIFY(start());

        auto update = checkAndInstall();
        QCOMPARE(jsonText(update.status), jsonText(Protocol::SelfUpdateStatus::Error));
        QCOMPARE(update.error, std::string("invalid signature"));
        QCOMPARE(readFile(appImage), std::string("old version"));
    }

    void rejectsSignatureOfAnotherRelease ()
    {
        QTemporaryDir dir;
        auto appImage = fakeAppImage(dir);
        qputenv("SC_UPDATE_SIGNING_KEY", QByteArray::fromStdString(releaseKey.publicKeyFile()));
        // A genuine signature, but made for an older release whose checksum list was reused
        std::string sums = sha256Hex("new version") + "  " + APPIMAGE_ASSET + "\n";
        publishAppImageRelease(server, releaseKey, "new version", sums, releaseKey.sign(sums, "sc-localization-launcher v9.9.8"));
        QVERIFY(start());

        auto update = checkAndInstall();
        QCOMPARE(update.error, std::string("invalid signature"));
        QCOMPARE(readFile(appImage), std::string("old version"));
    }

    void rejectsTamperedPackage ()
    {
        QTemporaryDir dir;
        auto appImage = fakeAppImage(dir);
        qputenv("SC_UPDATE_SIGNING_KEY", QByteArray::fromStdString(releaseKey.publicKeyFile()));
        // Signed checksums of the real package, but the server delivers something else
        std::string sums = sha256Hex("new version") + "  " + APPIMAGE_ASSET + "\n";
        publishAppImageRelease(server, releaseKey, "tampered version", sums);
        QVERIFY(start());

        auto update = checkAndInstall();
        QCOMPARE(update.error, std::string("checksum mismatch"));
        QCOMPARE(readFile(appImage), std::string("old version"));
        QVERIFY(!std::filesystem::exists(PATHS::CACHE_USER / "launcher-update"));
    }

    void doesNotInstallWhileTheGameRuns ()
    {
        QTemporaryDir dir;
        auto appImage = fakeAppImage(dir);
        qputenv("SC_UPDATE_SIGNING_KEY", QByteArray::fromStdString(releaseKey.publicKeyFile()));
        publishAppImageRelease(server, releaseKey, "new version");

        // A process named like the game: a copy of sleep
        auto game = std::filesystem::path(dir.path().toStdString()) / "StarCitizen.exe";
        std::filesystem::copy_file(QStandardPaths::findExecutable("sleep", {"/usr/bin", "/bin"}).toStdString(), game);
        QProcess running;
        running.start(QString::fromStdString(game.string()), {"30"});
        QVERIFY(running.waitForStarted());

        QVERIFY(start());
        auto update = checkAndInstall();
        running.kill();
        running.waitForFinished();
        QCOMPARE(update.error, std::string("Star Citizen is running"));
        QCOMPARE(readFile(appImage), std::string("old version"));
    }

    void offersNoInstallWithoutSigningKey ()
    {
        QTemporaryDir dir;
        fakeAppImage(dir);
        publishAppImageRelease(server, releaseKey, "new version");
        QVERIFY(start());

        auto update = checkAndInstall();
        QCOMPARE(jsonText(update.status), jsonText(Protocol::SelfUpdateStatus::Available));
        QVERIFY(!update.canInstall);
    }

    void projectSigningKeyIsUsable ()
    {
        // packaging/minisign.pub is embedded at build time; a broken file would silently disable self-updates
        auto key = Minisign::parsePublicKey(UPDATE_SIGNING_KEY);
        QVERIFY2(key, "packaging/minisign.pub is missing or not a minisign public key");
    }

    void changedTranslationFileIsDamaged ()
    {
        QTemporaryDir game;
        QVERIFY(installGerman(game));
        // A game update puts its own global.ini in place while the launcher is closed
        std::ofstream(std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini") << "from the game\n";

        QVERIFY(restart());
        QVERIFY(waitForState([] (const auto& s) { return channel(s, "LIVE").status == ChannelStatus::Damaged; }));
    }

    void missingUserCfgLineIsDamaged ()
    {
        QTemporaryDir game;
        QVERIFY(installGerman(game));
        std::ofstream(std::filesystem::path(game.path().toStdString()) / "user.cfg") << "r_DisplayInfo = 2\n";

        QVERIFY(restart());
        QVERIFY(waitForState([] (const auto& s) { return channel(s, "LIVE").status == ChannelStatus::Damaged; }));
    }

    void updateRepairsDamagedChannel ()
    {
        QTemporaryDir game;
        auto ini = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        QVERIFY(installGerman(game));
        std::ofstream(ini) << "from the game\n";
        QVERIFY(restart());
        QVERIFY(waitForState([] (const auto& s) { return channel(s, "LIVE").status == ChannelStatus::Damaged; }));

        // The source has not changed, but a damaged channel must not get "not modified"
        int downloads = server.requestCount("GET /de.ini 200");
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QCOMPARE(server.requestCount("GET /de.ini 200"), downloads + 1);
        QCOMPARE(QByteArray::fromStdString(readFile(ini)), TRANSLATION);
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::Installed);
    }

    void repairDownloadsAgain ()
    {
        QTemporaryDir game;
        auto ini = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        QVERIFY(installGerman(game));
        // Changed while the launcher runs, so the check at start has not seen it
        std::ofstream(ini) << "broken\n";

        int downloads = server.requestCount("GET /de.ini 200");
        size_t from = published();
        backEnd->execute(Protocol::RepairChannel{"LIVE"});
        auto running = waitForState([] (const auto& s) { return s.update == UpdateStatus::Running; }, from);
        QVERIFY(running);
        QVERIFY(waitForState([] (const auto& s) { return s.update == UpdateStatus::Succeeded; }, *running + 1));
        QCOMPARE(server.requestCount("GET /de.ini 200"), downloads + 1);
        QCOMPARE(QByteArray::fromStdString(readFile(ini)), TRANSLATION);
    }

    void failedUpdateMarksTheChannel ()
    {
        QTemporaryDir game;
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        // Both German full URLs answer 404
        QVERIFY(setLanguage("LIVE", "de-full"));
        QCOMPARE(runUpdate(), UpdateStatus::Failed);
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::Failed);

        QVERIFY(setLanguage("LIVE", "de"));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::Installed);
    }

    void noticesPerChannelAndLanguage ()
    {
        QVERIFY(start());
        auto live = channel(latest(), "LIVE");
        auto ptu = channel(latest(), "PTU");

        QCOMPARE(live.notices.at("de"), std::string("Wird vorbereitet"));
        QCOMPARE(ptu.notices.at("de"), std::string("Allgemeiner Hinweis"));
        // Shown as plain text: the markup stays visible as characters, the line break becomes a space
        QCOMPARE(live.notices.at("fr"), std::string("<b>Bêta</b> zweite Zeile"));
        QCOMPARE(live.notices.at("de-full"), std::string(120, 'x') + "…");
        QVERIFY(!live.notices.contains("en"));
    }

    // Waits until the LIVE row shows this own source
    bool setCustomSource (const std::string& url, const std::string& folder)
    {
        size_t from = published();
        backEnd->execute(Protocol::SetCustomSource{"LIVE", url, folder});
        return waitForState([&] (const auto& s) { return channel(s, "LIVE").custom.url == url; }, from).has_value();
    }

    void customSourceIsInstalledLikeALanguage ()
    {
        QTemporaryDir game;
        auto gamePath = std::filesystem::path(game.path().toStdString());
        server.setFile("/custom.ini", "custom_key=Eigene Quelle\n");
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", gamePath.string()});
        QVERIFY(setCustomSource(server.url("/custom.ini").toStdString(), "french_(france)"));
        auto live = channel(latest(), "LIVE");
        QVERIFY(std::find(live.availableLanguages.begin(), live.availableLanguages.end(), Protocol::CUSTOM) != live.availableLanguages.end());
        // Only this channel offers it
        auto ptu = channel(latest(), "PTU").availableLanguages;
        QVERIFY(std::find(ptu.begin(), ptu.end(), Protocol::CUSTOM) == ptu.end());

        QVERIFY(setLanguage("LIVE", Protocol::CUSTOM));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QCOMPARE(readFile(gamePath / "data/Localization/french_(france)/global.ini"), std::string("custom_key=Eigene Quelle\n"));
        QVERIFY(readFile(gamePath / "user.cfg").find("g_language = french_(france)") != std::string::npos);
        QVERIFY(channel(latest(), "LIVE").status == ChannelStatus::Installed);
        QCOMPARE(jsonText(readSettingsFile()["channels"]["LIVE"]["custom"]["folder"]), jsonText("french_(france)"));

        // Back to English removes the files of the own source
        QVERIFY(setLanguage("LIVE", Protocol::ENGLISH));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QVERIFY(!std::filesystem::exists(gamePath / "data"));
    }

    void changedCustomFolderRemovesTheOldOne ()
    {
        QTemporaryDir game;
        auto gamePath = std::filesystem::path(game.path().toStdString());
        server.setFile("/custom.ini", "custom_key=Eigene Quelle\n");
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", gamePath.string()});
        QVERIFY(setCustomSource(server.url("/custom.ini").toStdString(), "french_(france)"));
        QVERIFY(setLanguage("LIVE", Protocol::CUSTOM));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);

        size_t from = published();
        backEnd->execute(Protocol::SetCustomSource{"LIVE", server.url("/custom.ini").toStdString(), "spanish_(spain)"});
        QVERIFY(waitForState([] (const auto& s) { return channel(s, "LIVE").custom.folder == "spanish_(spain)"; }, from));
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QVERIFY(std::filesystem::exists(gamePath / "data/Localization/spanish_(spain)/global.ini"));
        QVERIFY(!std::filesystem::exists(gamePath / "data/Localization/french_(france)"));
    }

    void rejectsUnsafeCustomSources ()
    {
        QVERIFY(start());
        backEnd->execute(Protocol::SetCustomSource{"LIVE", "http://example.com/global.ini", "german_(germany)"});
        backEnd->execute(Protocol::SetCustomSource{"LIVE", server.url("/custom.ini").toStdString(), "../../outside"});
        backEnd->execute(Protocol::SetCustomSource{"LIVE", server.url("/custom.ini").toStdString(), ""});
        // Without a valid source "custom" cannot be chosen either
        backEnd->execute(Protocol::SetLanguage{"LIVE", Protocol::CUSTOM});
        QVERIFY(waitForPreviousCommands());
        QCOMPARE(channel(latest(), "LIVE").custom.url, std::string());
        QCOMPARE(channel(latest(), "LIVE").language, Protocol::ENGLISH);
    }

    void removingTheChosenSourceFallsBackToEnglish ()
    {
        QVERIFY(start());
        QVERIFY(setCustomSource(server.url("/custom.ini").toStdString(), "german_(germany)"));
        QVERIFY(setLanguage("LIVE", Protocol::CUSTOM));

        size_t from = published();
        backEnd->execute(Protocol::SetCustomSource{"LIVE", "", ""});
        QVERIFY(waitForState([] (const auto& s) { return channel(s, "LIVE").custom.url.empty(); }, from));
        QCOMPARE(channel(latest(), "LIVE").language, Protocol::ENGLISH);
    }
};

QTEST_GUILESS_MAIN(BackendTest)
#include "BackendTest.moc"
