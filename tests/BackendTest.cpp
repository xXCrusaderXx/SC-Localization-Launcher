#include <QProcess>
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
using Protocol::GameStart;
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
    // The tests run without PATH so nothing real is started; restored after tests that need bash
    QByteArray testPath = qgetenv("PATH");

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

    // Fake LUG Helper installation: the Wine prefix holds sc-launch.sh with these commands; returns the RSI Launcher folder
    static std::filesystem::path lugPrefix (const QTemporaryDir& prefix, const std::string& commands)
    {
        auto root = std::filesystem::path(prefix.path().toStdString());
        auto rsiLauncher = root / "drive_c/Program Files/Roberts Space Industries/RSI Launcher";
        std::filesystem::create_directories(rsiLauncher);
        std::ofstream(root / "sc-launch.sh") << "#!/usr/bin/env bash\n" << commands;
        return rsiLauncher;
    }

    // Updates LIVE with "Update & start" and returns how the start of the RSI Launcher ended
    std::optional<GameStart> updateAndStartGame (const std::filesystem::path& rsiLauncher)
    {
        QTemporaryDir game;
        if(!start()) return std::nullopt;
        backEnd->execute(Protocol::SetOption{"launchGameAfterUpdate", true});
        backEnd->execute(Protocol::SetRsiLauncherPath{rsiLauncher.string()});
        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        if(!setLanguage("LIVE", "de") || runUpdate() != UpdateStatus::Succeeded) return std::nullopt;
        if(!waitForState([] (const auto& s) { return s.game == GameStart::Starting; })) return std::nullopt;
        auto done = waitForState([] (const auto& s) { return s.game != GameStart::Starting && s.game != GameStart::Idle; });
        if(!done) return std::nullopt;
        std::lock_guard lock(mutex);
        return states[*done].game;
    }

    // Watched instead of "RSI Launcher.exe", see watchOwnTestPrograms()
    static std::string fakeRsiLauncher () { return fakeWineProcess(testRsiLauncherProgram()); }

   private slots:
    void initTestCase ()
    {
        LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>());
        watchOwnTestPrograms();
    }

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

    void cleanup ()
    {
        backEnd.reset();
        qputenv("PATH", testPath);
    }

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

    // os-release values of real distributions
    void picksThePackageForDerivatives ()
    {
        using OS = std::map<std::string, std::string>;
        auto variant = [] (const std::string& type, const OS& os, bool imageBased = false)
        { return SelfUpdateManager::packageVariant(type, os, imageBased); };

        QCOMPARE(variant("deb", OS{{"ID", "ubuntu"}, {"VERSION_ID", "24.04"}, {"UBUNTU_CODENAME", "noble"}}), std::string("ubuntu24.04"));
        // Linux Mint 22, Pop!_OS 24.04
        QCOMPARE(variant("deb", OS{{"ID", "linuxmint"}, {"ID_LIKE", "ubuntu debian"}, {"VERSION_ID", "22"}, {"UBUNTU_CODENAME", "noble"}}),
                 std::string("ubuntu24.04"));
        QCOMPARE(variant("deb", OS{{"ID", "pop"}, {"ID_LIKE", "ubuntu debian"}, {"VERSION_ID", "24.04"}, {"UBUNTU_CODENAME", "noble"}}),
                 std::string("ubuntu24.04"));
        // Ubuntu 25.04 and Mint 21 (Ubuntu 22.04) have other library versions
        QCOMPARE(variant("deb", OS{{"ID", "ubuntu"}, {"VERSION_ID", "25.04"}, {"UBUNTU_CODENAME", "plucky"}}), std::string());
        QCOMPARE(variant("deb", OS{{"ID", "linuxmint"}, {"ID_LIKE", "ubuntu debian"}, {"UBUNTU_CODENAME", "jammy"}}), std::string());

        QCOMPARE(variant("deb", OS{{"ID", "debian"}, {"VERSION_ID", "12"}, {"VERSION_CODENAME", "bookworm"}}), std::string("debian12"));
        // LMDE 6 names its Debian base separately
        QCOMPARE(variant("deb", OS{{"ID", "linuxmint"}, {"ID_LIKE", "debian"}, {"VERSION_CODENAME", "faye"}, {"DEBIAN_CODENAME", "bookworm"}}),
                 std::string("debian12"));
        QCOMPARE(variant("deb", OS{{"ID", "debian"}, {"VERSION_ID", "13"}, {"VERSION_CODENAME", "trixie"}}), std::string());

        QCOMPARE(variant("rpm", OS{{"ID", "fedora"}, {"VERSION_ID", "42"}}), std::string("fedora"));
        QCOMPARE(variant("rpm", OS{{"ID", "nobara"}, {"ID_LIKE", "rhel centos fedora"}}), std::string("fedora"));
        // Fedora Atomic / Bazzite: no dnf install on an image-based system
        QCOMPARE(variant("rpm", OS{{"ID", "fedora"}, {"VERSION_ID", "42"}}, true), std::string());
        QCOMPARE(variant("rpm", OS{{"ID", "opensuse-tumbleweed"}, {"ID_LIKE", "opensuse suse"}}), std::string());

        QCOMPARE(variant("arch", OS{{"ID", "cachyos"}, {"ID_LIKE", "arch"}}), std::string("arch"));
        QCOMPARE(variant("source", OS{{"ID", "ubuntu"}, {"VERSION_ID", "24.04"}}), std::string());
    }

    // Output of `flatpak info` and `flatpak remotes --columns=name,url` with LC_ALL=C
    void readsFlatpakInformation ()
    {
        std::string info =
            "\nSC Localization Launcher - Community translations for Star Citizen on Linux\n\n"
            "          ID: io.github.xXCrusaderXx.SCLocalizationLauncher\n"
            "         Ref: app/io.github.xXCrusaderXx.SCLocalizationLauncher/x86_64/stable\n"
            "     Version: 0.7.0\n"
            "      Origin: sc-localization-launcher\n"
            "Installation: user\n";
        QCOMPARE(SelfUpdateManager::flatpakInfoField(info, "Version"), std::string("0.7.0"));
        QCOMPARE(SelfUpdateManager::flatpakInfoField(info, "Installation"), std::string("user"));
        QCOMPARE(SelfUpdateManager::flatpakInfoField(info, "Origin"), std::string("sc-localization-launcher"));
        QCOMPARE(SelfUpdateManager::flatpakInfoField(info, "Missing"), std::string());

        std::string remotes =
            "flathub\thttps://dl.flathub.org/repo/\n"
            "sc-localization-launcher\thttps://xxcrusaderxx.github.io/SC-Localization-Launcher/repo/\n";
        QCOMPARE(SelfUpdateManager::flatpakRemoteUrl(remotes, "sc-localization-launcher"),
                 std::string("https://xxcrusaderxx.github.io/SC-Localization-Launcher/repo/"));
        QCOMPARE(SelfUpdateManager::flatpakRemoteUrl(remotes, "sclocalizationlauncher-origin"), std::string());
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

        // A process named like the game under Wine
        qputenv("PATH", "/usr/bin:/bin");
        QProcess running;
        running.start("/bin/bash", {"-c", QString::fromStdString(fakeWineProcess(testGameProgram()))});
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

    void md4MatchesTheReference ()
    {
        auto md4 = [] (const std::string& text)
        {
            auto digest = Zsync::detail::Md4::digest(reinterpret_cast<const unsigned char*>(text.data()), text.size());
            return QByteArray(reinterpret_cast<const char*>(digest.data()), 16).toHex().toStdString();
        };
        // RFC 1320, appendix A.5
        QCOMPARE(md4(""), std::string("31d6cfe0d16ae931b73c59d7e0c089c0"));
        QCOMPARE(md4("abc"), std::string("a448017aaf21d8525fc10ae87aa6729d"));
        QCOMPARE(md4("12345678901234567890123456789012345678901234567890123456789012345678901234567890"),
                 std::string("e33b4ddc9c38f2199c3e7b164fcc0536"));
    }

    void rollingChecksumMatchesBlockChecksum ()
    {
        std::string data = randomBytes(4096, 1);
        const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());
        auto rolling = Zsync::detail::Rsum::of(bytes, 1024);
        for(size_t x = 0; x + 1024 < data.size(); ++x)
        {
            rolling.roll(bytes[x], bytes[x + 1024], 10);
            auto direct = Zsync::detail::Rsum::of(bytes + x + 1, 1024);
            QCOMPARE(rolling.a, direct.a);
            QCOMPARE(rolling.b, direct.b);
        }
    }

    void zsyncReusesUnchangedBlocks ()
    {
        // The new version has bytes inserted, changed and appended, so blocks move to other offsets
        std::string oldVersion = randomBytes(64 * 1024, 2);
        std::string newVersion = oldVersion;
        newVersion.insert(10000, randomBytes(100, 3));
        newVersion.replace(40000, 50, randomBytes(50, 4));
        newVersion += randomBytes(700, 5);

        auto control = Zsync::parse(makeZsync(newVersion));
        QVERIFY(control);
        QCOMPARE(control->length, uint64_t(newVersion.size()));
        QTemporaryDir dir;
        auto seed = std::filesystem::path(dir.path().toStdString()) / "old";
        auto target = std::filesystem::path(dir.path().toStdString()) / "new";
        std::ofstream(seed, std::ios::binary) << oldVersion;
        auto missing = Zsync::reuse(*control, seed, target);
        QVERIFY(missing);

        uint64_t missingBytes = 0;
        for(const auto& range : *missing) missingBytes += range.end - range.begin;
        // One block around each change, two for the tail
        QVERIFY2(missingBytes <= 5 * 1024, std::to_string(missingBytes).c_str());

        std::string assembled = readFile(target);
        QCOMPARE(assembled.size(), newVersion.size());
        for(const auto& range : *missing)
            assembled.replace(range.begin, range.end - range.begin, newVersion.substr(range.begin, range.end - range.begin));
        QVERIFY(assembled == newVersion);
    }

    void zsyncMergesRangesWithSmallGaps ()
    {
        std::vector<Zsync::Range> ranges{{0, 10}, {20, 30}, {1000, 1010}, {1015, 1020}, {5000, 5010}};
        auto merged = Zsync::merge(ranges, 3);
        QCOMPARE(merged.size(), size_t(3));
        QCOMPARE(merged[0].end, uint64_t(30));
        QCOMPARE(merged[1].begin, uint64_t(1000));
        QCOMPARE(merged[1].end, uint64_t(1020));
        QCOMPARE(merged[2].begin, uint64_t(5000));
        QCOMPARE(Zsync::merge(ranges, 10).size(), ranges.size());
    }

    void rejectsBrokenZsyncFiles ()
    {
        std::string control = makeZsync(randomBytes(5000, 6));
        QVERIFY(Zsync::parse(control));
        QVERIFY(!Zsync::parse(control.substr(0, control.size() - 1)));
        QVERIFY(!Zsync::parse("Recompress: gzip --best\n" + control));
        std::string badBlockSize = control;
        badBlockSize.replace(badBlockSize.find("Blocksize: 1024"), 15, "Blocksize: 1000");
        QVERIFY(!Zsync::parse(badBlockSize));
    }

    // An installed AppImage and the release that replaces part of it
    static std::pair<std::string, std::string> appImageVersions ()
    {
        std::string oldVersion = randomBytes(256 * 1024, 7);
        std::string newVersion = oldVersion;
        newVersion.replace(100000, 3000, randomBytes(3000, 8));
        return {oldVersion, newVersion};
    }

    void installsAppImageUpdateAsDelta ()
    {
        QTemporaryDir dir;
        auto appImage = fakeAppImage(dir);
        auto [oldVersion, newVersion] = appImageVersions();
        std::ofstream(appImage, std::ios::binary) << oldVersion;
        qputenv("SC_UPDATE_SIGNING_KEY", QByteArray::fromStdString(releaseKey.publicKeyFile()));
        publishAppImageRelease(server, releaseKey, newVersion, {}, {}, makeZsync(newVersion));
        int fullDownloads = server.requestCount("GET /download/package 200");
        int partialDownloads = server.requestCount("GET /download/package 206");
        QVERIFY(start());

        auto update = checkAndInstall();
        QCOMPARE(jsonText(update.status), jsonText(Protocol::SelfUpdateStatus::Installed));
        QVERIFY(readFile(appImage) == newVersion);
        QCOMPARE(server.requestCount("GET /download/package 200"), fullDownloads);
        QCOMPARE(server.requestCount("GET /download/package 206"), partialDownloads + 1);
    }

    void deltaUpdateFallsBackToTheWholePackage ()
    {
        QTemporaryDir dir;
        auto appImage = fakeAppImage(dir);
        auto [oldVersion, newVersion] = appImageVersions();
        qputenv("SC_UPDATE_SIGNING_KEY", QByteArray::fromStdString(releaseKey.publicKeyFile()));
        QVERIFY(start());

        // Block checksums of another file: the assembled file fails the signed checksum
        std::ofstream(appImage, std::ios::binary) << oldVersion;
        publishAppImageRelease(server, releaseKey, newVersion, {}, {}, makeZsync(oldVersion));
        int fullDownloads = server.requestCount("GET /download/package 200");
        auto update = checkAndInstall();
        QCOMPARE(jsonText(update.status), jsonText(Protocol::SelfUpdateStatus::Installed));
        QVERIFY(readFile(appImage) == newVersion);
        QCOMPARE(server.requestCount("GET /download/package 200"), fullDownloads + 1);

        // A server that ignores the range sends the whole file, which must not overwrite the reused blocks
        std::ofstream(appImage, std::ios::binary) << oldVersion;
        publishAppImageRelease(server, releaseKey, newVersion, {}, {}, makeZsync(newVersion));
        server.setRangeRequests(false);
        update = checkAndInstall();
        server.setRangeRequests(true);
        QCOMPARE(jsonText(update.status), jsonText(Protocol::SelfUpdateStatus::Installed));
        QVERIFY(readFile(appImage) == newVersion);
        QCOMPARE(server.requestCount("GET /download/package 200"), fullDownloads + 3);
    }

    // Releases on the test server around the tested version 0.6.0, newest first like GitHub lists them
    void publishReleaseList ()
    {
        nlohmann::json releases =
            nlohmann::json::array({{{"tag_name", "v0.7.0-rc1"}, {"prerelease", true}, {"body", "- Not released yet"}},
                                   {{"tag_name", "v0.6.0"}, {"body", "### Added\n\n- Six\n\n### Verify the download\n\nminisign -Vm SHA256SUMS\n"}},
                                   {{"tag_name", "v0.5.1"}, {"body", "- Five one\r\n"}},
                                   {{"tag_name", "v0.5.0"}, {"body", "- Five"}}});
        server.setFile("/releases", QByteArray::fromStdString(releases.dump()));
    }

    // Sends LoadWhatsNew and returns the release notes it brought, empty if none came
    Protocol::WhatsNew loadWhatsNew ()
    {
        size_t from = published();
        backEnd->execute(Protocol::LoadWhatsNew{});
        auto found = waitForState([] (const auto& s) { return !s.whatsNew.version.empty(); }, from, 5000);
        if(!found) return {};
        std::lock_guard lock(mutex);
        return states[*found].whatsNew;
    }

    void showsReleaseNotesOnceAfterAnUpdate ()
    {
        publishReleaseList();
        writeSettingsFile({{"lastSeenVersion", "0.5.0"}});
        QVERIFY(start());

        auto whatsNew = loadWhatsNew();
        QCOMPARE(whatsNew.version, std::string("0.6.0"));
        QCOMPARE(whatsNew.notes, std::string("## 0.6.0\n\n### Added\n\n- Six\n\n## 0.5.1\n\n- Five one"));

        size_t from = published();
        backEnd->execute(Protocol::DismissWhatsNew{});
        QVERIFY(waitForState([] (const auto& s) { return s.whatsNew.version.empty(); }, from));
        QCOMPARE(readSettingsFile().value("lastSeenVersion", ""), std::string("0.6.0"));

        int requests = server.requestCount("GET /releases 200");
        QVERIFY(restart());
        QVERIFY(loadWhatsNew().version.empty());
        QCOMPARE(server.requestCount("GET /releases 200"), requests);
    }

    void newInstallationShowsNoReleaseNotes ()
    {
        publishReleaseList();
        int requests = server.requestCount("GET /releases 200");
        QVERIFY(start());

        QVERIFY(loadWhatsNew().version.empty());
        QCOMPARE(server.requestCount("GET /releases 200"), requests);
        QCOMPARE(readSettingsFile().value("lastSeenVersion", ""), std::string("0.6.0"));
    }

    void launcherSetUpBeforeReleaseNotesShowsTheCurrentOnes ()
    {
        publishReleaseList();
        writeSettingsFile({{"channels", {{"LIVE", {{"installPath", "/games/LIVE"}}}}}});
        QVERIFY(start());

        auto whatsNew = loadWhatsNew();
        QCOMPARE(whatsNew.version, std::string("0.6.0"));
        QCOMPARE(whatsNew.notes, std::string("## 0.6.0\n\n### Added\n\n- Six"));
    }

    void releaseNotesAreAskedForAgainWhenOffline ()
    {
        qputenv("SC_RELEASES_URL", server.url("/offline/latest").toUtf8());
        writeSettingsFile({{"lastSeenVersion", "0.5.0"}});
        QVERIFY(start());

        QVERIFY(loadWhatsNew().version.empty());
        QTRY_COMPARE(server.requestCount("GET /offline 404"), 1);
        QVERIFY(waitForPreviousCommands());
        QCOMPARE(readSettingsFile().value("lastSeenVersion", ""), std::string("0.5.0"));
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

    void startsTheRsiLauncherWithoutElectronNodeMode ()
    {
        // VS Code and other Electron apps pass this on; the RSI Launcher would then quit with "bad option"
        qputenv("ELECTRON_RUN_AS_NODE", "1");
        qputenv("PATH", "/usr/bin:/bin");
        QTemporaryDir prefix;
        auto root = std::filesystem::path(prefix.path().toStdString());
        auto rsiLauncher =
            lugPrefix(prefix, std::string("echo \"${ELECTRON_RUN_AS_NODE-unset}\" > electron-env\necho $$ > pid\n") + fakeRsiLauncher());

        auto result = updateAndStartGame(rsiLauncher);
        QProcess::execute("kill", {QString::fromStdString(readFile(root / "pid")).trimmed()});
        qunsetenv("ELECTRON_RUN_AS_NODE");
        QCOMPARE(result, std::optional(GameStart::Running));
        QCOMPARE(readFile(root / "electron-env"), std::string("unset\n"));
        // Once it quits, the launcher is ready to start it again
        QVERIFY(waitForState([] (const auto& s) { return s.game == GameStart::Idle; }, published() - 1));
    }

    void startsTheGameWithoutUpdate ()
    {
        qputenv("PATH", "/usr/bin:/bin");
        QTemporaryDir prefix;
        auto root = std::filesystem::path(prefix.path().toStdString());
        auto rsiLauncher = lugPrefix(prefix, std::string("echo $$ > pid\n") + fakeRsiLauncher());

        QVERIFY(start());
        backEnd->execute(Protocol::SetRsiLauncherPath{rsiLauncher.string()});
        backEnd->execute(Protocol::StartGame{});
        auto running = waitForState([] (const auto& s) { return s.game == GameStart::Running; });
        QProcess::execute("kill", {QString::fromStdString(readFile(root / "pid")).trimmed()});
        QVERIFY(running);
        // Nothing was downloaded
        QCOMPARE(latest().update, UpdateStatus::Idle);
    }

    void noticesAnRsiLauncherStartedElsewhere ()
    {
        qputenv("PATH", "/usr/bin:/bin");
        QVERIFY(start());
        QProcess rsiLauncher;
        rsiLauncher.start("/bin/bash", {"-c", QString::fromStdString(fakeRsiLauncher())});
        QVERIFY(rsiLauncher.waitForStarted());
        auto running = waitForState([] (const auto& s) { return s.game == GameStart::Running; });
        rsiLauncher.kill();
        rsiLauncher.waitForFinished();
        QVERIFY(running);
        QVERIFY(waitForState([] (const auto& s) { return s.game == GameStart::Idle; }, *running));
    }

    void doesNotUpdateWhileTheGameRuns ()
    {
        qputenv("PATH", "/usr/bin:/bin");
        QTemporaryDir gameFolder;
        QVERIFY(installGerman(gameFolder));
        QProcess game;
        game.start("/bin/bash", {"-c", QString::fromStdString(fakeWineProcess(testGameProgram()))});
        QVERIFY(game.waitForStarted());
        auto playing = waitForState([] (const auto& s) { return s.game == GameStart::Playing; });

        // Ignored while the game reads the files; the marker shows the command has been handled
        size_t from = published();
        backEnd->execute(Protocol::StartUpdate{});
        QVERIFY(waitForPreviousCommands());
        bool updated = false;
        {
            std::lock_guard lock(mutex);
            for(size_t i = from; i < states.size(); ++i) updated |= states[i].update == UpdateStatus::Running;
        }
        game.kill();
        game.waitForFinished();
        QVERIFY(playing);
        QVERIFY(!updated);
        QVERIFY(waitForState([] (const auto& s) { return s.game == GameStart::Idle; }, *playing));
    }

    // A LUG Helper prefix with the game's caches, the GraphicsSettings beside them, the prefix's Mesa cache, the DXVK and
    // vkd3d caches of LIVE, and a "shaders" symlink that leads out of the prefix. Returns the LIVE folder.
    static std::filesystem::path cachePrefix (const std::filesystem::path& prefix, const std::filesystem::path& outside)
    {
        auto write = [] (const std::filesystem::path& file, size_t bytes)
        {
            std::filesystem::create_directories(file.parent_path());
            std::ofstream(file, std::ios::binary) << std::string(bytes, 'x');
        };
        auto version = prefix / "drive_c/users/steamuser/AppData/Local/Star Citizen/starcitizen_(sc-alpha-4.8.0)_abc_0";
        write(version / "shaders/a.bin", 1000);
        write(version / "vulkanshadercache/b.bin", 2000);
        write(version / "GraphicsSettings/GraphicsSettings.json", 10);
        write(prefix / "mesa_shader_cache/index", 300);
        auto live = prefix / "drive_c/Program Files/Roberts Space Industries/StarCitizen/LIVE";
        write(live / "StarCitizen.dxvk-cache", 40);
        write(live / "Bin64/vkd3d-proton.cache", 50);
        write(live / "Bin64/StarCitizen.exe", 5);
        write(outside / "keep.bin", 70);
        auto other = prefix / "drive_c/users/steamuser/AppData/Local/Star Citizen/starcitizen_(sc-alpha-4.7.0)_abc_0";
        std::filesystem::create_directories(other);
        std::filesystem::create_directory_symlink(outside, other / "shaders");
        return live;
    }

    Protocol::Caches scanCaches ()
    {
        size_t from = published();
        backEnd->execute(Protocol::ScanCaches{});
        auto found = waitForState([] (const auto& s) { return s.caches.scanned && !s.caches.busy; }, from);
        if(!found) return {};
        std::lock_guard lock(mutex);
        return states[*found].caches;
    }

    void clearsOnlyTheShaderCachesItFound ()
    {
        QTemporaryDir prefixDir, outsideDir;
        std::filesystem::path prefix = prefixDir.path().toStdString();
        std::filesystem::path outside = outsideDir.path().toStdString();
        auto live = cachePrefix(prefix, outside);
        auto userCache = PATHS::xdgDir("XDG_CACHE_HOME", ".cache") / "mesa_shader_cache";
        requireTestHome(userCache);
        std::filesystem::create_directories(userCache);
        std::ofstream(userCache / "index") << "shared";
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", live.string()});

        auto caches = scanCaches();
        std::map<std::string, std::pair<std::string, uint64_t>> found;
        for(const auto& entry : caches.entries) found[entry.path] = {entry.kind, entry.bytes};
        auto version = prefix / "drive_c/users/steamuser/AppData/Local/Star Citizen/starcitizen_(sc-alpha-4.8.0)_abc_0";
        std::map<std::string, std::pair<std::string, uint64_t>> expected{{(version / "shaders").string(), {"game", 1000}},
                                                                         {(version / "vulkanshadercache").string(), {"game", 2000}},
                                                                         {(prefix / "mesa_shader_cache").string(), {"prefix", 300}},
                                                                         {(live / "StarCitizen.dxvk-cache").string(), {"dxvk", 40}},
                                                                         {(live / "Bin64/vkd3d-proton.cache").string(), {"dxvk", 50}},
                                                                         {userCache.string(), {"user", 6}}};
        QCOMPARE(found, expected);

        // Everything but the cache shared with other programs, plus paths the scan never offered
        std::vector<std::string> paths;
        for(const auto& [path, entry] : expected)
            if(entry.first != "user") paths.push_back(path);
        paths.push_back((outside / "keep.bin").string());
        paths.push_back((version / "GraphicsSettings").string());
        size_t from = published();
        backEnd->execute(Protocol::ClearCaches{paths});
        auto done =
            waitForState([] (const auto& s)
                         { return !s.caches.busy && !s.caches.entries.empty() && s.caches.entries[0].result != Protocol::CacheResult::None; }, from);
        QVERIFY(done);
        {
            std::lock_guard lock(mutex);
            for(const auto& entry : states[*done].caches.entries)
                QCOMPARE(jsonText(entry.result), jsonText(entry.kind == "user" ? Protocol::CacheResult::None : Protocol::CacheResult::Deleted));
        }
        for(const auto& [path, entry] : expected) QCOMPARE(std::filesystem::exists(path), entry.first == "user");
        QVERIFY(std::filesystem::exists(version / "GraphicsSettings/GraphicsSettings.json"));
        QVERIFY(std::filesystem::exists(outside / "keep.bin"));
        QVERIFY(std::filesystem::exists(live / "Bin64/StarCitizen.exe"));
    }

    void doesNotClearCachesWhileTheGameRuns ()
    {
        qputenv("PATH", "/usr/bin:/bin");
        QTemporaryDir prefixDir, outsideDir;
        auto live = cachePrefix(prefixDir.path().toStdString(), outsideDir.path().toStdString());
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", live.string()});
        auto caches = scanCaches();
        QVERIFY(!caches.entries.empty());

        QProcess game;
        game.start("/bin/bash", {"-c", QString::fromStdString(fakeWineProcess(testGameProgram()))});
        QVERIFY(game.waitForStarted());
        QVERIFY(waitForState([] (const auto& s) { return s.game == GameStart::Playing; }));
        std::string path = caches.entries.front().path;
        size_t from = published();
        backEnd->execute(Protocol::ClearCaches{{path}});
        auto refused = waitForState([] (const auto& s) { return s.caches.entries.front().result == Protocol::CacheResult::GameRunning; }, from);
        game.kill();
        game.waitForFinished();
        QVERIFY(refused);
        QVERIFY(std::filesystem::exists(path));
    }

    void zipChecksumMatchesTheReference () { QCOMPARE(ZipWriter::crc32("123456789"), uint32_t(0xcbf43926)); }

    void anonymizesHomeAndUserName ()
    {
        Diagnostics diagnostics;
        std::string home = PATHS::getUserRootPath().string();
        std::string user = PATHS::getUserRootPath().filename().string();
        std::string text = home + "/Games/sc\nC:\\users\\" + user + "\\AppData\n/mnt/x/drive_c/users/" + user + "/AppData\n" + user + "X stays\n";
        QCOMPARE(diagnostics.anonymize(text),
                 std::string("~/Games/sc\nC:\\users\\<user>\\AppData\n/mnt/x/drive_c/users/<user>/AppData\n") + user + "X stays\n");
    }

    Protocol::Diagnostics runDiagnostics (bool saveReport)
    {
        size_t from = published();
        if(saveReport)
            backEnd->execute(Protocol::SaveReport{});
        else
            backEnd->execute(Protocol::RunDiagnostics{});
        auto done = waitForState([] (const auto& s) { return !s.diagnostics.busy && !s.diagnostics.checks.empty(); }, from);
        if(!done) return {};
        std::lock_guard lock(mutex);
        return states[*done].diagnostics;
    }

    static std::string statusOf (const Protocol::Diagnostics& diagnostics, const std::string& id, const std::string& channel = "")
    {
        for(const auto& check : diagnostics.checks)
            if(check.id == id && check.channel == channel) return nlohmann::json(check.status).get<std::string>();
        return "missing";
    }

    void diagnosticsFindAChangedUserCfg ()
    {
        QTemporaryDir game;
        QVERIFY(installGerman(game));
        auto result = runDiagnostics(false);
        QCOMPARE(statusOf(result, "launcher"), std::string("info"));
        QCOMPARE(statusOf(result, "folder", "LIVE"), std::string("ok"));
        QCOMPARE(statusOf(result, "writable", "LIVE"), std::string("ok"));
        QCOMPARE(statusOf(result, "translation", "LIVE"), std::string("ok"));
        QCOMPARE(statusOf(result, "userCfg", "LIVE"), std::string("ok"));
        // A temporary folder has no drive_c above it
        QCOMPARE(statusOf(result, "winePrefix"), std::string("error"));
        QCOMPARE(statusOf(result, "folder", "PTU"), std::string("missing"));

        auto gamePath = std::filesystem::path(game.path().toStdString());
        std::ofstream(gamePath / "user.cfg") << "r_DisplayInfo = 2\n";
        QCOMPARE(statusOf(runDiagnostics(false), "userCfg", "LIVE"), std::string("error"));
        // Read only: the check repaired nothing
        QCOMPARE(readFile(gamePath / "user.cfg"), std::string("r_DisplayInfo = 2\n"));
    }

    void savesAnAnonymizedReport ()
    {
        QTemporaryDir game;
        QVERIFY(installGerman(game));
        auto result = runDiagnostics(true);
        QVERIFY2(!result.reportPath.empty(), result.error.c_str());
        std::string zip = readFile(result.reportPath);
        QVERIFY(zip.starts_with("PK\x03\x04"));
        for(const char* name : {"report.txt", "checks.json", "settings.json", "logs/"}) QVERIFY2(zip.find(name) != std::string::npos, name);
        // The test home holds the game folder and the logs; none of it may show its real path
        QVERIFY(zip.find(PATHS::getUserRootPath().string()) == std::string::npos);
        // Same check as unzip -t, where Python is there to do it
        if(QFile::exists("/usr/bin/python3"))
            QCOMPARE(QProcess::execute("/usr/bin/python3", {"-c", "import sys, zipfile; sys.exit(zipfile.ZipFile(sys.argv[1]).testzip() is not None)",
                                                            QString::fromStdString(result.reportPath)}),
                     0);
        std::filesystem::remove(result.reportPath);
    }

    // A channel folder as the game leaves it: user.cfg, a profile, an exported mapping, a saved character (binary),
    // plus files that are no part of a profile backup
    static void writeProfile (const std::filesystem::path& game)
    {
        auto write = [&game] (const std::string& file, const std::string& content)
        {
            std::filesystem::create_directories((game / file).parent_path());
            std::ofstream(game / file, std::ios::binary) << content;
        };
        write("user.cfg", "r_DisplayInfo = 2\n");
        write("user/client/0/Profiles/default/actionmaps.xml", "<ActionMaps version=\"1\"/>\n");
        write("user/client/0/Profiles/default/attributes.xml", "<Attributes/>\n");
        write("user/client/0/controls/mappings/hotas.xml", "<ActionMaps/>\n");
        write("user/client/0/CustomCharacters/pilot.chf", std::string("CHF\0\x01\x02", 6));
        write("user/client/0/game.cfg", "sys_spec = -1\n");
        write("Data.p4k", "game data");
    }

    Protocol::Backups backupCommand (const Protocol::Command& command)
    {
        size_t from = published();
        backEnd->execute(command);
        // The job publishes once it is done, with busy off again
        auto done = waitForState([] (const auto& s) { return !s.backups.busy && !s.backups.result.empty(); }, from);
        if(!done) return {};
        std::lock_guard lock(mutex);
        return states[*done].backups;
    }

    void acceptsOnlyProfilePathsInBackups ()
    {
        for(const char* path : {"user.cfg", "user/client/0/Profiles/default/actionmaps.xml", "user/client/0/controls/mappings/a.xml",
                                "user/client/0/CustomCharacters/pilot.chf"})
            QVERIFY2(ProfileBackup::isProfilePath(path), path);
        for(const char* path : {"../user.cfg", "/etc/passwd", "user.cfg.bak", "Data.p4k", "user/client/0/game.cfg", "user/client/0/Other/x.xml",
                                "user/client/0/Profiles/../../../Bin64/StarCitizen.exe", "user/client/../0/Profiles/x", "user/client/0/Profiles\\x"})
            QVERIFY2(!ProfileBackup::isProfilePath(path), path);
    }

    void backsUpAndRestoresAProfile ()
    {
        QTemporaryDir game;
        auto path = std::filesystem::path(game.path().toStdString());
        writeProfile(path);
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", path.string()});

        auto created = backupCommand(Protocol::CreateBackup{"LIVE"});
        QCOMPARE(created.result, std::string("created"));
        QCOMPARE(created.entries.size(), size_t(1));
        std::string backup = created.entries.front().name;
        QVERIFY(backup.starts_with("LIVE-") && backup.ends_with(".zip"));

        // The player changes a keybinding, loses a character and the game changes its own files
        std::ofstream(path / "user/client/0/Profiles/default/actionmaps.xml") << "<ActionMaps version=\"2\"/>\n";
        std::filesystem::remove(path / "user/client/0/CustomCharacters/pilot.chf");
        std::ofstream(path / "Data.p4k") << "patched";
        auto restored = backupCommand(Protocol::RestoreBackup{"LIVE", backup});
        QCOMPARE(restored.result, std::string("restored"));
        QCOMPARE(readFile(path / "user/client/0/Profiles/default/actionmaps.xml"), std::string("<ActionMaps version=\"1\"/>\n"));
        QCOMPARE(readFile(path / "user/client/0/CustomCharacters/pilot.chf"), std::string("CHF\0\x01\x02", 6));
        QCOMPARE(readFile(path / "Data.p4k"), std::string("patched"));
        // What the restore replaced was backed up first
        QCOMPARE(restored.entries.size(), size_t(2));
        QVERIFY(restored.entries.front().name != backup);
    }

    void keepsTheNewestBackups ()
    {
        QTemporaryDir game;
        writeProfile(game.path().toStdString());
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        Protocol::Backups backups;
        std::vector<std::string> names;
        for(size_t i = 0; i < ProfileBackup::KEEP + 2; ++i)
        {
            backups = backupCommand(Protocol::CreateBackup{"LIVE"});
            names.push_back(backups.entries.front().name);
        }
        QCOMPARE(backups.entries.size(), ProfileBackup::KEEP);
        // Newest first, the two oldest are gone
        for(size_t i = 0; i < ProfileBackup::KEEP; ++i) QCOMPARE(backups.entries[i].name, names[names.size() - 1 - i]);
    }

    void refusesBackupsThatWriteElsewhere ()
    {
        QTemporaryDir game;
        auto path = std::filesystem::path(game.path().toStdString());
        writeProfile(path);
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", path.string()});
        auto folder = ProfileBackup().folder() / "LIVE";
        requireTestHome(folder);
        std::filesystem::create_directories(folder);
        {
            ZipWriter zip(folder / "LIVE-20990101-000000-000.zip");
            zip.add("user.cfg", "g_language = evil\n");
            zip.add("user/client/0/Profiles/../../../Bin64/evil.dll", "evil");
            QVERIFY(zip.finish());
        }
        auto result = backupCommand(Protocol::RestoreBackup{"LIVE", "LIVE-20990101-000000-000.zip"});
        QCOMPARE(result.result, std::string("damaged backup"));
        QCOMPARE(readFile(path / "user.cfg"), std::string("r_DisplayInfo = 2\n"));
        QVERIFY(!std::filesystem::exists(path / "Bin64/evil.dll"));
        QCOMPARE(backupCommand(Protocol::RestoreBackup{"LIVE", "../../evil.zip"}).result, std::string("unknown backup"));
    }

    void doesNotRestoreWhileTheGameRuns ()
    {
        qputenv("PATH", "/usr/bin:/bin");
        QTemporaryDir game;
        auto path = std::filesystem::path(game.path().toStdString());
        writeProfile(path);
        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", path.string()});
        std::string backup = backupCommand(Protocol::CreateBackup{"LIVE"}).entries.front().name;
        std::ofstream(path / "user.cfg") << "changed\n";

        QProcess running;
        running.start("/bin/bash", {"-c", QString::fromStdString(fakeWineProcess(testGameProgram()))});
        QVERIFY(running.waitForStarted());
        QVERIFY(waitForState([] (const auto& s) { return s.game == GameStart::Playing; }));
        size_t from = published();
        backEnd->execute(Protocol::RestoreBackup{"LIVE", backup});
        auto refused = waitForState([] (const auto& s) { return s.backups.result == "Star Citizen is running"; }, from);
        running.kill();
        running.waitForFinished();
        QVERIFY(refused);
        QCOMPARE(readFile(path / "user.cfg"), std::string("changed\n"));
    }

    void reportsWhenTheRsiLauncherQuitsAtOnce ()
    {
        qputenv("PATH", "/usr/bin:/bin");
        QTemporaryDir prefix;
        auto rsiLauncher = lugPrefix(prefix, "echo 'RSI Launcher.exe: bad option: --in-process-gpu' > sc-launch.log\nexit 1\n");
        QCOMPARE(updateAndStartGame(rsiLauncher), std::optional(GameStart::Failed));
    }

    void reportsWhenNothingCanStartTheGame ()
    {
        // No sc-launch.sh, and without PATH no Lutris either
        QTemporaryDir folder;
        QCOMPARE(updateAndStartGame(folder.path().toStdString()), std::optional(GameStart::NoStarter));
    }

    // LIVE with German chosen but not installed yet, then the option for automatic updates switched on,
    // which installs right away when the window asked for background checks
    bool chooseGermanWithAutomaticUpdates (const QTemporaryDir& game)
    {
        if(!start()) return false;
        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        if(!setLanguage("LIVE", "de")) return false;
        backEnd->execute(Protocol::SetOption{"installNewTranslations", true});
        return true;
    }

    void updatesInTheBackground ()
    {
        QTemporaryDir game;
        QVERIFY(chooseGermanWithAutomaticUpdates(game));
        // Only what the window sends; nothing is installed before
        QVERIFY(waitForPreviousCommands());
        QCOMPARE(latest().update, UpdateStatus::Idle);

        backEnd->execute(Protocol::StartBackgroundChecks{});
        QVERIFY(waitForState([] (const auto& s) { return s.update == UpdateStatus::Succeeded; }));
        auto ini = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        QCOMPARE(QByteArray::fromStdString(readFile(ini)), TRANSLATION);
    }

    void backgroundUpdateWaitsUntilTheGameIsClosed ()
    {
        qputenv("PATH", "/usr/bin:/bin");
        QProcess starCitizen;
        // Named like the game under Wine
        starCitizen.start("/bin/bash", {"-c", QString::fromStdString(fakeWineProcess(testGameProgram()))});
        QVERIFY(starCitizen.waitForStarted());
        QTest::qWait(200);

        QTemporaryDir game;
        auto ini = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        QVERIFY(chooseGermanWithAutomaticUpdates(game));
        backEnd->execute(Protocol::StartBackgroundChecks{});
        QVERIFY(waitForPreviousCommands());
        QCOMPARE(latest().game, GameStart::Playing);
        QCOMPARE(latest().update, UpdateStatus::Idle);
        QVERIFY(!std::filesystem::exists(ini));

        starCitizen.kill();
        starCitizen.waitForFinished();
        QVERIFY(waitForState([] (const auto& s) { return s.update == UpdateStatus::Succeeded; }));
        QCOMPARE(QByteArray::fromStdString(readFile(ini)), TRANSLATION);
    }

    void checksTheSourcesRegularly ()
    {
        qputenv("SC_CHECK_INTERVAL_SECONDS", "1");
        QTemporaryDir game;
        QVERIFY(installGerman(game));
        backEnd->execute(Protocol::StartBackgroundChecks{});

        // The project publishes a new version while the launcher is open; without the option it is only shown
        server.setFile("/de.ini", TRANSLATION + "new_key=Neu\n");
        bool shown = waitForState([] (const auto& s) { return channel(s, "LIVE").status == ChannelStatus::UpdateAvailable; }).has_value();
        qunsetenv("SC_CHECK_INTERVAL_SECONDS");
        QVERIFY(shown);
        QCOMPARE(latest().update, UpdateStatus::Succeeded);
    }
};

QTEST_GUILESS_MAIN(BackendTest)
#include "BackendTest.moc"
