#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <functional>
#include <memory>

#include "TestSupport.hpp"
#include "frontend/BackendBridge.hpp"
#include "logging/logger/custom_logger/custom_logger.h"

// Clicks through the real QML GUI on top of the real backend, like a user would.
// The test functions run in order and share one window.
class GuiTest : public QObject
{
    Q_OBJECT

   private:
    TestServer server{fixtureFiles()};
    QTemporaryDir liveDir;
    QTemporaryDir ptuDir;
    std::unique_ptr<BackendBridge> bridge;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow* window = nullptr;

    // Visible Text item showing exactly this text
    QQuickItem* findText (const QString& text) const
    {
        std::function<QQuickItem*(QQuickItem*)> walk = [&] (QQuickItem* item) -> QQuickItem*
        {
            if(!item->isVisible()) return nullptr;
            if(QString(item->metaObject()->className()).startsWith("QQuickText") && item->property("text").toString() == text) return item;
            for(auto* child : item->childItems())
                if(auto* found = walk(child)) return found;
            return nullptr;
        };
        return walk(window->contentItem());
    }

    bool click (const QString& text)
    {
        QQuickItem* item = findText(text);
        if(!item) return false;
        QPointF center = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, {}, center.toPoint());
        return true;
    }

    QVariantMap channel (const QString& name) const
    {
        for(const auto& c : bridge->state()["channels"].toList())
            if(c.toMap()["name"] == name) return c.toMap();
        return {};
    }

    QQuickItem* languagePicker () const
    {
        std::function<QQuickItem*(QQuickItem*)> walk = [&] (QQuickItem* item) -> QQuickItem*
        {
            if(!item->isVisible()) return nullptr;
            if(item->inherits("QQuickComboBox")) return item;
            for(auto* child : item->childItems())
                if(auto* found = walk(child)) return found;
            return nullptr;
        };
        return walk(window->contentItem());
    }

    bool launchGameOption () const { return bridge->state()["options"].toMap()["launchGameAfterUpdate"].toBool(); }

    QQuickItem* launchToggle () const
    {
        QQuickItem* item = findText("Start Star Citizen afterwards");
        while(item && !item->inherits("QQuickSwitch")) item = item->parentItem();
        return item;
    }

   private slots:
    void initTestCase ()
    {
        LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>());
        qputenv("SC_LANGUAGES_URL", server.url("/languages.json").toUtf8());
        resetUserDir();
        // Starts with the game launch switched on, the first test turns it off before any update runs
        writeSettingsFile(legacySettings(liveDir.path().toStdString(), "eng", true), PATHS::JSON_FILES::SETTINGS_LEGACY);

        bridge = std::make_unique<BackendBridge>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty("backend", bridge.get());
        engine->load(QUrl("qrc:/qt/qml/SCLauncher/Main.qml"));
        QVERIFY(!engine->rootObjects().isEmpty());
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_COMPARE(channel("LIVE")["availableLanguages"].toList().size(), 4);
    }

    void cleanupTestCase ()
    {
        // QML first, it still references the bridge
        engine.reset();
        bridge.reset();
    }

    void toggleShowsBackendState ()
    {
        QVERIFY(launchGameOption());
        QVERIFY(launchToggle() && launchToggle()->property("checked").toBool());

        // Off, on, off: the switch must follow the backend state every time
        for(bool expected : {false, true, false})
        {
            QVERIFY(click("Start Star Citizen afterwards"));
            QTRY_COMPARE(launchGameOption(), expected);
            QTRY_COMPARE(launchToggle()->property("checked").toBool(), expected);
        }
        QCOMPARE(readSettingsFile()["options"]["launchGameAfterUpdate"], false);
    }

    void pickLanguage ()
    {
        // LIVE is the only row with a path, so its picker is the only one on screen
        QQuickItem* picker = languagePicker();
        QVERIFY(picker);
        QVERIFY(findText("English"));

        QVERIFY(click("English"));
        QTRY_VERIFY(picker->property("popup").value<QObject*>()->property("opened").toBool());
        // The list creates its entries with the next frame; each translation shows its credit
        QTRY_VERIFY(findText("Deutsch"));
        QVERIFY(findText("Test-INI · CC BY-NC-SA 4.0"));
        QVERIFY(click("Deutsch"));
        QTRY_COMPARE(channel("LIVE")["language"].toString(), QString("de"));
        QCOMPARE(readSettingsFile()["channels"]["LIVE"]["language"], "de");

        // The picker shows the confirmed choice, and opening it again highlights it
        QTRY_VERIFY(!picker->property("popup").value<QObject*>()->property("visible").toBool());
        QTRY_VERIFY(findText("Deutsch"));
        QVERIFY(click("Deutsch"));
        QTRY_VERIFY(picker->property("popup").value<QObject*>()->property("opened").toBool());
        QCOMPARE(picker->property("highlightedIndex").toInt(), 1);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!picker->property("popup").value<QObject*>()->property("visible").toBool());
    }

    void choosePathWithDialog ()
    {
        // The first "Choose folder" belongs to PTU
        QVERIFY(click("Choose folder"));
        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("folderDialog");
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->property("target").toString(), QString("PTU"));

        dialog->setProperty("selectedFolder", QUrl::fromLocalFile(ptuDir.path()));
        QMetaObject::invokeMethod(dialog, "accept");
        QTRY_COMPARE(channel("PTU")["installPath"].toString(), ptuDir.path());
        QCOMPARE(readSettingsFile()["channels"]["PTU"]["installPath"], ptuDir.path().toStdString());
    }

    void runUpdate ()
    {
        QVERIFY(!launchGameOption());

        // The local download is too fast to catch "running" by polling, so record every state
        QStringList statuses;
        auto connection = connect(bridge.get(), &BackendBridge::stateChanged, this, [&] { statuses << bridge->state()["update"].toString(); });

        QVERIFY(click("Update translations"));
        QTRY_VERIFY_WITH_TIMEOUT(statuses.contains("succeeded"), 30000);
        disconnect(connection);
        QCOMPARE(statuses, (QStringList{"running", "succeeded"}));
        QTRY_VERIFY(findText("Translations updated"));
        QVERIFY(QFile::exists(liveDir.filePath("data/Localization/german_(germany)/global.ini")));
    }
};

QTEST_MAIN(GuiTest)
#include "GuiTest.moc"
