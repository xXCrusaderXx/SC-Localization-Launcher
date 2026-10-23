#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
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
        qputenv("SC_RELEASES_URL", server.url("/releases/latest").toUtf8());
        QCoreApplication::setApplicationVersion("0.6.0");
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
        QCOMPARE(jsonText(readSettingsFile()["options"]["launchGameAfterUpdate"]), jsonText(false));
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
        QCOMPARE(jsonText(readSettingsFile()["channels"]["LIVE"]["language"]), jsonText("de"));

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
        // Channels without a folder are only listed in the settings, not on the main page
        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        QVERIFY(panel);
        QVERIFY(!findText("PTU"));

        QVERIFY(click("⚙"));
        QTRY_VERIFY(panel->property("open").toBool());

        // The ✎ next to the "PTU" label in the settings' install path list
        QQuickItem* label = findText("PTU");
        QVERIFY(label);
        QQuickItem* row = label;
        while(row && !row->property("changeRequested").isValid() && row->property("label").toString() != "PTU") row = row->parentItem();
        QVERIFY(row);
        std::function<QQuickItem*(QQuickItem*)> findEdit = [&] (QQuickItem* item) -> QQuickItem*
        {
            if(QString(item->metaObject()->className()).startsWith("QQuickText") && item->property("text").toString() == "✎") return item;
            for(auto* child : item->childItems())
                if(auto* found = findEdit(child)) return found;
            return nullptr;
        };
        QQuickItem* edit = findEdit(row);
        QVERIFY(edit);

        // The path list may start below the visible part of the panel: scroll the row into view
        QQuickItem* flickable = row;
        while(flickable && !flickable->inherits("QQuickFlickable")) flickable = flickable->parentItem();
        QVERIFY(flickable);
        QQuickItem* content = flickable->property("contentItem").value<QQuickItem*>();
        qreal rowY = row->mapToItem(content, QPointF(0, 0)).y();
        qreal maxY = std::max(0.0, flickable->property("contentHeight").toReal() - flickable->height());
        flickable->setProperty("contentY", std::clamp(rowY - flickable->height() / 2, 0.0, maxY));

        // The panel slides in; click only once the button stays put inside the window (slow CI machines need longer)
        QPointF last(-1, -1);
        auto settled = [&]
        {
            QPointF pos = edit->mapToScene(QPointF(edit->width() / 2, edit->height() / 2));
            bool inside = pos.x() >= 0 && pos.x() < window->width() && pos.y() >= 0 && pos.y() < window->height();
            bool stable = pos == last && inside;
            last = pos;
            return stable;
        };
        QTRY_VERIFY_WITH_TIMEOUT(settled(), 10000);
        QTest::mouseClick(window, Qt::LeftButton, {}, last.toPoint());

        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("folderDialog");
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->property("target").toString(), QString("PTU"));
        // Fails here (instead of crashing in accept()) when the dialog implementation is missing,
        // e.g. on Debian/Ubuntu without qml6-module-qt-labs-folderlistmodel
        QTRY_VERIFY(dialog->property("visible").toBool());

        dialog->setProperty("selectedFolder", QUrl::fromLocalFile(ptuDir.path()));
        QMetaObject::invokeMethod(dialog, "accept");
        QTRY_COMPARE(channel("PTU")["installPath"].toString(), ptuDir.path());
        QCOMPARE(jsonText(readSettingsFile()["channels"]["PTU"]["installPath"]), jsonText(ptuDir.path().toStdString()));

        // With a folder, PTU shows up on the main page
        // (the panel is gone, so the text can only come from the main page)
        panel->setProperty("open", false);
        QTRY_VERIFY_WITH_TIMEOUT(!panel->property("visible").toBool(), 10000);
        QTRY_VERIFY(findText("PTU"));
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

    void launcherUpdateButton ()
    {
        auto* button = engine->rootObjects().first()->findChild<QQuickItem*>("launcherUpdateButton");
        QVERIFY(button);
        QPointF center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, {}, center.toPoint());

        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("launcherUpdateDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(findText("Version 9.9.9 is available. You are using 0.6.0."));
        // Buttons draw their label without a Text item, so look for the button itself
        auto buttons = window->contentItem()->findChildren<QQuickItem*>();
        QVERIFY(
            std::any_of(buttons.begin(), buttons.end(), [] (QQuickItem* item)
                        { return item->inherits("QQuickAbstractButton") && item->isVisible() && item->property("text") == "Open download page"; }));
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }
};

QTEST_MAIN(GuiTest)
#include "GuiTest.moc"
