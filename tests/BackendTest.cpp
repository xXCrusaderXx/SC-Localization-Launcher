#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>

#include "TestSupport.hpp"
#include "backend/BackEnd.hpp"
#include "logging/logger/custom_logger/custom_logger.h"

using Protocol::Language;
using Protocol::UpdateStatus;

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
        // channels.json is loaded in the background, the second state carries it
        return waitForState([] (const auto& s) { return channel(s, "LIVE").availableLanguages.size() == 3; }).has_value();
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
    void initTestCase ()
    {
        LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>());
        qputenv("SC_CHANNELS_URL", server.url("/channels.json").toUtf8());
    }

    void init ()
    {
        resetUserDir();
        std::lock_guard lock(mutex);
        states.clear();
    }

    void cleanup () { backEnd.reset(); }

    void startsWithDefaultsWithoutSettingsFile ()
    {
        QVERIFY(start());
        auto state = latest();

        QCOMPARE(state.channels.size(), Protocol::CHANNEL_NAMES.size());
        for(size_t i = 0; i < state.channels.size(); ++i)
        {
            QCOMPARE(state.channels[i].name, Protocol::CHANNEL_NAMES[i]);
            QCOMPARE(state.channels[i].installPath, std::string());
            QVERIFY(state.channels[i].language == Language::English);
        }
        QCOMPARE(nlohmann::json(state.options), nlohmann::json(Protocol::Options{}));
        QVERIFY(state.update == UpdateStatus::Idle);
    }

    void migratesLegacySettings ()
    {
        writeSettingsFile(legacySettings("/games/LIVE", "deVoll", true));
        QVERIFY(start());
        auto state = latest();

        QCOMPARE(channel(state, "LIVE").installPath, std::string("/games/LIVE"));
        QVERIFY(channel(state, "LIVE").language == Language::GermanFull);
        QVERIFY(state.options.launchGameAfterUpdate);
        QCOMPARE(state.rsiLauncherPath, std::string("/nonexistent/RSI Launcher"));

        // The next save writes the new format
        size_t from = published();
        backEnd->execute(Protocol::SetOption{"showUpdateProgress", true});
        QVERIFY(waitForState([] (const auto& s) { return s.options.showUpdateProgress; }, from));
        auto file = readSettingsFile();
        QVERIFY(!file.contains("settings"));
        QCOMPARE(file["channels"]["LIVE"]["language"], "deFull");
        QCOMPARE(file["options"]["launchGameAfterUpdate"], true);
    }

    void offersOnlyWhatTheServerProvides ()
    {
        QVERIFY(start());
        auto state = latest();
        using Languages = std::vector<Language>;

        QCOMPARE(channel(state, "LIVE").availableLanguages, (Languages{Language::English, Language::German, Language::GermanFull}));
        QCOMPARE(channel(state, "PTU").availableLanguages, Languages{Language::English});
        QCOMPARE(channel(state, "EPTU").availableLanguages, Languages{Language::English});
        // Listed as TECH-PREV on the server
        QCOMPARE(channel(state, "TECH-PREVIEW").availableLanguages, (Languages{Language::English, Language::German}));
        QVERIFY(std::none_of(state.channels.begin(), state.channels.end(), [] (const auto& c) { return c.name == "TECH-PREV"; }));
    }

    void keepsChoiceTheServerDoesNotOffer ()
    {
        QVERIFY(start());
        size_t from = published();
        backEnd->execute(Protocol::SetLanguage{"PTU", Language::German});
        auto index = waitForState([] (const auto&) { return true; }, from);
        QVERIFY(index);

        QVERIFY(channel(latest(), "PTU").language == Language::English);
        QCOMPARE(readSettingsFile()["channels"]["PTU"]["language"], "de");
    }

    void ignoresInvalidCommands ()
    {
        QVERIFY(start());
        size_t from = published();
        backEnd->execute(Protocol::SetLanguage{"NOPE", Language::German});
        backEnd->execute(Protocol::SetInstallPath{"NOPE", "/games/NOPE"});
        backEnd->execute(Protocol::SetOption{"noSuchOption", true});
        backEnd->execute(Protocol::SetOption{"startWithSystem", true});
        QVERIFY(waitForState([] (const auto& s) { return s.options.startWithSystem; }, from));

        auto state = latest();
        QCOMPARE(state.channels.size(), Protocol::CHANNEL_NAMES.size());
        QVERIFY(!readSettingsFile()["options"].contains("noSuchOption"));
        QVERIFY(!readSettingsFile()["channels"].contains("NOPE"));
    }

    void updateInstallsAndRemovesTranslation ()
    {
        QTemporaryDir game;
        auto gamePath = std::filesystem::path(game.path().toStdString());
        std::ofstream(gamePath / "user.cfg") << "r_DisplayInfo = 2\n";

        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", gamePath.string()});
        backEnd->execute(Protocol::SetLanguage{"LIVE", Language::German});

        // German comes from the fallback URL, the first one answers 404
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        std::ifstream ini(gamePath / "data/Localization/german_(germany)/global.ini");
        QCOMPARE(QByteArray::fromStdString(std::string(std::istreambuf_iterator<char>(ini), {})), TRANSLATION);
        std::ifstream cfg(gamePath / "user.cfg");
        QCOMPARE(std::string(std::istreambuf_iterator<char>(cfg), {}),
                 std::string("r_DisplayInfo = 2\ng_language = german_(germany)\ng_languageAudio = english\n"));

        backEnd->execute(Protocol::SetLanguage{"LIVE", Language::English});
        QCOMPARE(runUpdate(), UpdateStatus::Succeeded);
        QVERIFY(!std::filesystem::exists(gamePath / "data"));
        std::ifstream cfgAfter(gamePath / "user.cfg");
        QCOMPARE(std::string(std::istreambuf_iterator<char>(cfgAfter), {}), std::string("r_DisplayInfo = 2\n"));
    }

    void updateReportsFailure ()
    {
        QTemporaryDir game;
        auto localization = std::filesystem::path(game.path().toStdString()) / "data/Localization/german_(germany)";

        QVERIFY(start());
        backEnd->execute(Protocol::SetInstallPath{"LIVE", game.path().toStdString()});
        backEnd->execute(Protocol::SetLanguage{"LIVE", Language::GermanFull});

        // Both German full URLs answer 404
        QCOMPARE(runUpdate(), UpdateStatus::Failed);
        QVERIFY(!std::filesystem::exists(localization / "global.ini"));
        QVERIFY(!std::filesystem::exists(localization / "global.ini.download"));
    }
};

QTEST_GUILESS_MAIN(BackendTest)
#include "BackendTest.moc"
