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

    bool launchGameOption () const { return bridge->state()["options"].toMap()["launchGameAfterUpdate"].toBool(); }

    QQuickItem* launchToggle () const
    {
        QQuickItem* item = findText("Star Citizen danach starten");
        while(item && !item->inherits("QQuickSwitch")) item = item->parentItem();
        return item;
    }

   private slots:
    void initTestCase ()
    {
        LoggerFramework::GlobalLogger::registerLogger(std::make_shared<LoggerFramework::CustomLogger>());
        qputenv("SC_CHANNELS_URL", server.url("/channels.json").toUtf8());
        resetUserDir();
        // Starts with the game launch switched on, the first test turns it off before any update runs
        writeSettingsFile(legacySettings(liveDir.path().toStdString(), "eng", true));

        bridge = std::make_unique<BackendBridge>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty("backend", bridge.get());
        engine->load(QUrl("qrc:/qt/qml/SCLauncher/Main.qml"));
        QVERIFY(!engine->rootObjects().isEmpty());
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_COMPARE(channel("LIVE")["availableLanguages"].toList().size(), 3);
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
            QVERIFY(click("Star Citizen danach starten"));
            QTRY_COMPARE(launchGameOption(), expected);
            QTRY_COMPARE(launchToggle()->property("checked").toBool(), expected);
        }
        QCOMPARE(readSettingsFile()["options"]["launchGameAfterUpdate"], false);
    }

    void pickLanguage ()
    {
        // The first "DE" on screen belongs to LIVE, the only row with a path
        QVERIFY(click("DE"));
        QTRY_COMPARE(channel("LIVE")["language"].toString(), QString("de"));
        QCOMPARE(readSettingsFile()["channels"]["LIVE"]["language"], "de");
    }

    void choosePathWithDialog ()
    {
        // The first "Pfad wählen" belongs to PTU
        QVERIFY(click("Pfad wählen"));
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
        connect(bridge.get(), &BackendBridge::stateChanged, this, [&] { statuses << bridge->state()["update"].toString(); });

        QVERIFY(click("Übersetzung aktualisieren"));
        QTRY_VERIFY_WITH_TIMEOUT(statuses.contains("succeeded"), 30000);
        QCOMPARE(statuses, (QStringList{"running", "succeeded"}));
        QTRY_VERIFY(findText("Übersetzungen aktualisiert"));
        QVERIFY(QFile::exists(liveDir.filePath("data/Localization/german_(germany)/global.ini")));
    }
};

QTEST_MAIN(GuiTest)
#include "GuiTest.moc"
