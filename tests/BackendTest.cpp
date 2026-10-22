#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>

#include "TestSupport.hpp"
#include "backend/BackEnd.hpp"
#include "logging/logger/custom_logger/custom_logger.h"

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
            });
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

   private slots:
    void initTestCase () { LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>()); }

    void init ()
    {
        qputenv("SC_LANGUAGES_URL", server.url("/languages.json").toUtf8());
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
        QCOMPARE(nlohmann::json(state.options), nlohmann::json(Protocol::Options{}));
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
        QCOMPARE(file["channels"]["LIVE"]["language"], "de-full");
        QCOMPARE(file["options"]["launchGameAfterUpdate"], true);
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
        for(size_t i = 1; i < state.languages.size(); ++i) QCOMPARE(nlohmann::json(state.languages[i].credit), testCredit());
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
        QCOMPARE(readSettingsFile()["channels"]["PTU"]["language"], "de");

        // Nothing else to update, so the update does not even start, and nothing is removed
        backEnd->execute(Protocol::StartUpdate{});
        QVERIFY(waitForPreviousCommands());
        QVERIFY(latest().update == UpdateStatus::Idle);
        QCOMPARE(readFile(ini), std::string("installed earlier\n"));
    }
};

QTEST_GUILESS_MAIN(BackendTest)
#include "BackendTest.moc"
