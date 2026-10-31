#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>
#include <QXmlStreamReader>
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
    TestSigningKey releaseKey;

    QQuickItem* findButton (const QString& text) const
    {
        // Popup contents hang below the window's root object, the rest below its content item.
        // Disabled buttons don't count: a click on one is lost, e.g. "Check again" while a check is running
        auto matches = [&text] (QQuickItem* item)
        { return item->inherits("QQuickAbstractButton") && item->isVisible() && item->isEnabled() && item->property("text") == text; };
        for(QObject* root : {static_cast<QObject*>(window->contentItem()), engine->rootObjects().first()})
            for(auto* item : root->findChildren<QQuickItem*>())
                if(matches(item)) return item;
        // Items made by a Repeater are no QObject children; walk the visual tree as well
        std::function<QQuickItem*(QQuickItem*)> walk = [&] (QQuickItem* item) -> QQuickItem*
        {
            if(!item->isVisible()) return nullptr;
            if(matches(item)) return item;
            for(auto* child : item->childItems())
                if(auto* found = walk(child)) return found;
            return nullptr;
        };
        return walk(window->contentItem());
    }

    // Not inside QTRY_*: those evaluate their expression once more after it succeeded, which would click twice
    bool clickButton (const QString& text)
    {
        QQuickItem* button = findButton(text);
        if(!button) return false;
        QPointF center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, {}, center.toPoint());
        return true;
    }

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
        watchOwnTestPrograms();
        qputenv("SC_LANGUAGES_URL", server.url("/languages.json").toUtf8());
        qputenv("SC_RELEASES_URL", server.url("/releases/latest").toUtf8());
        qputenv("SC_UPDATE_SIGNING_KEY", QByteArray::fromStdString(releaseKey.publicKeyFile()));
        QCoreApplication::setApplicationVersion("0.6.0");
        resetUserDir();
        // Starts with the game launch switched on, the first test turns it off before any update runs
        writeSettingsFile(legacySettings(liveDir.path().toStdString(), "eng", true), PATHS::JSON_FILES::SETTINGS_LEGACY);
        // The settings come from a version before release notes were shown, so the window opens with those of 0.6.0
        nlohmann::json releases = nlohmann::json::array({{{"tag_name", "v0.6.0"}, {"body", "- Notes of this version"}}});
        server.setFile("/releases", QByteArray::fromStdString(releases.dump()));

        bridge = std::make_unique<BackendBridge>();
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty("backend", bridge.get());
        bridge->followInterfaceLanguage(engine.get());
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

    // A failed test may leave a modal dialog open, which would swallow the clicks of the next test
    void cleanup ()
    {
        for(const char* name : {"launcherUpdateDialog", "customSourceDialog", "whatsNewDialog", "cacheDialog", "diagnosticsDialog", "backupDialog",
                                "gameSettingsDialog", "programsDialog", "setupAssistant", "statisticsDialog", "dashboardDialog"})
        {
            auto* dialog = engine->rootObjects().first()->findChild<QObject*>(name);
            if(!dialog || !dialog->property("visible").toBool()) continue;
            QMetaObject::invokeMethod(dialog, "close");
            QTRY_VERIFY(!dialog->property("visible").toBool());
        }
        // The settings panel's backdrop would catch the next test's clicks as well
        if(auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel"); panel && panel->property("open").toBool())
        {
            panel->setProperty("open", false);
            QTRY_VERIFY(!panel->property("visible").toBool());
        }
        qunsetenv("APPIMAGE");
    }

    void showsWhatsNewOnceAtStart ()
    {
        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("whatsNewDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(findText("What's new in version 0.6.0"));
        QVERIFY(clickButton("OK"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QTRY_COMPARE(QString::fromStdString(readSettingsFile().value("lastSeenVersion", "")), QString("0.6.0"));
        // The settings file is saved before the state reaches the bridge
        QTRY_VERIFY(bridge->state()["whatsNew"].toMap()["version"].toString().isEmpty());
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
        // The notice of the chosen language replaces the path in the row
        QTRY_VERIFY(findText("Wird vorbereitet"));
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

    void settingsOfferItemValuesAndCreditTheWiki ()
    {
        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        QVERIFY(click("⚙"));
        QTRY_VERIFY(panel->property("open").toBool());
        QTRY_VERIFY(findText("Add size, class and grade to item names"));
        QVERIFY(findText("Add item values to descriptions"));
        // Credited next to the translations, with its licence
        QVERIFY(findText("Item values"));
        QVERIFY(findText("License: <a href=\"https://creativecommons.org/licenses/by-nc-sa/4.0/\">CC BY-NC-SA 4.0</a>"));
        panel->setProperty("open", false);
        QTRY_VERIFY_WITH_TIMEOUT(!panel->property("visible").toBool(), 10000);
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

    void showsWhenTheGameCannotStart ()
    {
        // Everything is up to date after runUpdate(), so the button only starts the game
        // The RSI Launcher folder has no LUG script, and without PATH there is no Lutris either
        QTemporaryDir rsiLauncher;
        bridge->setRsiLauncherPath(QUrl::fromLocalFile(rsiLauncher.path()));
        bridge->setOption("launchGameAfterUpdate", true);
        QTRY_VERIFY(launchGameOption());

        QTRY_VERIFY(findText("Start"));
        QVERIFY(click("Start"));
        QTRY_VERIFY_WITH_TIMEOUT(findText("Cannot start the game: install it with the LUG Helper or Lutris"), 30000);
        bridge->setOption("launchGameAfterUpdate", false);
        QTRY_VERIFY(!launchGameOption());
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

    void installsLauncherUpdateFromTheDialog ()
    {
        QTemporaryDir dir;
        auto appImage = std::filesystem::path(dir.path().toStdString()) / "SC-Localization-Launcher.AppImage";
        std::ofstream(appImage) << "old version";
        qputenv("APPIMAGE", QByteArray::fromStdString(appImage.string()));
        publishAppImageRelease(server, releaseKey, "new version");

        auto* button = engine->rootObjects().first()->findChild<QQuickItem*>("launcherUpdateButton");
        QVERIFY(button);
        QPointF center = button->mapToScene(QPointF(button->width() / 2, button->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, {}, center.toPoint());
        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("launcherUpdateDialog");
        QTRY_VERIFY(dialog->property("opened").toBool());

        // The earlier check knew no package for this installation; checking again finds the signed AppImage
        QTRY_VERIFY(findButton("Check again"));
        QVERIFY(clickButton("Check again"));
        QTRY_VERIFY(findButton("Install update"));
        QVERIFY(clickButton("Install update"));
        QTRY_VERIFY_WITH_TIMEOUT(findButton("Restart now"), 30000);
        QCOMPARE(readFile(appImage), std::string("new version"));

        // Restarting would start the fake AppImage, so the test stops here
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        qunsetenv("APPIMAGE");
    }

    void repairButtonRestoresTheTranslation ()
    {
        // LIVE got German in runUpdate(); something overwrites it
        auto ini = std::filesystem::path(liveDir.path().toStdString()) / "data/Localization/german_(germany)/global.ini";
        QVERIFY(std::filesystem::exists(ini));
        std::ofstream(ini) << "from the game\n";

        // Repeater delegates only hang in the visual tree, so walk that to the LIVE row and its button
        std::function<QQuickItem*(QQuickItem*, const char*)> find = [&] (QQuickItem* item, const char* name) -> QQuickItem*
        {
            if(item->objectName() == name && item->isVisible()) return item;
            for(auto* child : item->childItems())
                if(auto* found = find(child, name)) return found;
            return nullptr;
        };
        QQuickItem* liveRow = nullptr;
        std::function<void(QQuickItem*)> findRow = [&] (QQuickItem* item)
        {
            if(!liveRow && item->property("channel").toMap().value("name") == "LIVE") liveRow = item;
            for(auto* child : item->childItems()) findRow(child);
        };
        findRow(window->contentItem());
        QVERIFY(liveRow);
        auto* repair = find(liveRow, "repairButton");
        QVERIFY(repair);
        QPointF center = repair->mapToScene(QPointF(repair->width() / 2, repair->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, {}, center.toPoint());
        QTRY_COMPARE_WITH_TIMEOUT(QByteArray::fromStdString(readFile(ini)), TRANSLATION, 30000);
        // The file is written before the update reports back; until then the channel rows stay locked
        QTRY_COMPARE(bridge->state()["update"].toString(), QString("succeeded"));
    }

    // The offscreen platform has no system tray, like GNOME without the AppIndicator extension
    void closeToTrayNeedsATray ()
    {
        QObject* root = engine->rootObjects().first();
        QVERIFY(!root->findChild<QObject*>("settingsPanel")->property("trayAvailable").toBool());
        bridge->setOption("closeToTray", true);
        QTRY_VERIFY(bridge->state()["options"].toMap()["closeToTray"].toBool());
        // Closing must still quit, or the window would be gone with no icon to bring it back
        QVERIFY(!root->property("closesToTray").toBool());
        QVERIFY(!findText("Keep running in the system tray"));
        bridge->setOption("closeToTray", false);
        QTRY_VERIFY(!bridge->state()["options"].toMap()["closeToTray"].toBool());
    }

    void trayStatusFollowsTheChannels ()
    {
        QObject* root = engine->rootObjects().first();
        auto expected = [this]
        {
            QString status = "ok";
            for(const auto& c : bridge->state()["channels"].toList())
            {
                QString s = c.toMap()["status"].toString();
                if(c.toMap()["installPath"].toString().isEmpty()) continue;
                if(s == "damaged" || s == "failed") return QString("error");
                if(s == "notInstalled" || s == "updateAvailable") status = "pending";
            }
            return status;
        };
        QTRY_COMPARE(root->property("overallStatus").toString(), expected());
        QVERIFY(QFile::exists(":/tray/" + root->property("overallStatus").toString() + ".png"));
    }

    void clearsCachesFromTheSettings ()
    {
        // LIVE points to liveDir; a DXVK cache there is the only cache this test has
        QString cache = liveDir.filePath("StarCitizen.dxvk-cache");
        QVERIFY(QFile(cache).open(QIODevice::WriteOnly));
        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        // The button sits below the visible part of the panel; its signal is what it would send
        QMetaObject::invokeMethod(panel, "cacheCleanerRequested");

        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("cacheDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(findText("DXVK/VKD3D cache"));
        QTRY_VERIFY(findButton("Delete 1 KB"));
        QVERIFY(clickButton("Delete 1 KB"));
        QTRY_VERIFY(!QFile::exists(cache));
        QTRY_VERIFY(findText("Deleted"));
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        panel->setProperty("open", false);
        // Its backdrop would catch the next test's clicks until it has slid out
        QTRY_VERIFY(!panel->property("visible").toBool());
    }

    void savesAReportFromTheSettings ()
    {
        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        // Like the cache cleaner, the button sits below the visible part of the panel
        QMetaObject::invokeMethod(panel, "diagnosticsRequested");

        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("diagnosticsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(findText("LIVE · Game folder"));
        QTRY_VERIFY(findButton("Save report (ZIP)"));
        QVERIFY(clickButton("Save report (ZIP)"));
        QTRY_VERIFY(!bridge->state()["diagnostics"].toMap()["reportPath"].toString().isEmpty());
        QString report = bridge->state()["diagnostics"].toMap()["reportPath"].toString();
        QVERIFY(QFile::exists(report));
        // Not clicked: it would open the file manager
        QTRY_VERIFY(findButton("Show in folder"));
        QFile::remove(report);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        panel->setProperty("open", false);
        QTRY_VERIFY(!panel->property("visible").toBool());
    }

    void backsUpAndRestoresFromTheSettings ()
    {
        QDir(liveDir.path()).mkpath("user/client/0/Profiles/default");
        QString actionmaps = liveDir.filePath("user/client/0/Profiles/default/actionmaps.xml");
        auto write = [] (const QString& file, const QByteArray& content)
        {
            QFile out(file);
            QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
            out.write(content);
        };
        write(actionmaps, "<ActionMaps version=\"1\"/>");

        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        // The button sits below the visible part of the panel
        QMetaObject::invokeMethod(panel, "backupsRequested");
        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("backupDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(findButton("Back up LIVE"));
        // Rows made by a Repeater are laid out on the next polish; a grab runs it, so the click hits their final place
        window->grabWindow();
        QVERIFY(clickButton("Back up LIVE"));
        QTRY_VERIFY(findText("Backup saved."));

        write(actionmaps, "<ActionMaps version=\"2\"/>");
        QTRY_VERIFY(findButton("Restore"));
        window->grabWindow();
        QVERIFY(clickButton("Restore"));
        // Nothing happens before the confirmation
        QTRY_VERIFY(findButton("Restore now"));
        {
            QFile unchanged(actionmaps);
            QVERIFY(unchanged.open(QIODevice::ReadOnly));
            QCOMPARE(unchanged.readAll(), QByteArray("<ActionMaps version=\"2\"/>"));
        }
        window->grabWindow();
        QVERIFY(clickButton("Restore now"));
        QTRY_VERIFY(findText("Backup restored. The files it replaced were backed up first."));
        QFile restored(actionmaps);
        QVERIFY(restored.open(QIODevice::ReadOnly));
        QCOMPARE(restored.readAll(), QByteArray("<ActionMaps version=\"1\"/>"));

        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        panel->setProperty("open", false);
        QTRY_VERIFY(!panel->property("visible").toBool());
    }

    void savesGameSettingsFromTheSettings ()
    {
        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        // The button sits below the visible part of the panel
        QMetaObject::invokeMethod(panel, "gameSettingsRequested");
        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("gameSettingsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        dialog->setProperty("channelName", "LIVE");
        auto* custom = engine->rootObjects().first()->findChild<QQuickItem*>("gameSettingsCustom");
        QVERIFY(custom);
        custom->setProperty("text", "r_DisplayInfo = 1");
        window->grabWindow();
        QVERIFY(clickButton("Save"));
        QTRY_COMPARE(channel("LIVE")["gameSettings"].toMap()["customLines"].toString(), QString("r_DisplayInfo = 1"));
        QTRY_VERIFY(findText("Takes effect at the next game start; afterwards the launcher shows whether Star Citizen accepted it."));
        QFile cfg(liveDir.filePath("user.cfg"));
        QVERIFY(cfg.open(QIODevice::ReadOnly));
        QVERIFY(cfg.readAll().contains("r_DisplayInfo = 1"));

        // No Wine prefix and no Game.log in the test folder: the API waits for the game
        auto* graphicsApi = engine->rootObjects().first()->findChild<QObject*>("graphicsApiBox");
        QVERIFY(graphicsApi);
        graphicsApi->setProperty("currentIndex", 2);
        QVERIFY(clickButton("Save"));
        QTRY_COMPARE(channel("LIVE")["gameSettings"].toMap()["graphicsApi"].toString(), QString("dx11"));
        QTRY_VERIFY(findText("The graphics API is set after Star Citizen quits, or after the first start of a new game version."));
        graphicsApi->setProperty("currentIndex", 0);

        custom->setProperty("text", "not a setting");
        QVERIFY(clickButton("Save"));
        QTRY_VERIFY(findText("Not saved – this line is no \"key = value\" or sets something the launcher manages itself: not a setting"));

        // Leaves user.cfg as the other tests expect it
        custom->setProperty("text", "");
        QVERIFY(clickButton("Save"));
        QTRY_VERIFY(channel("LIVE")["gameSettings"].toMap()["customLines"].toString().isEmpty());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        panel->setProperty("open", false);
        QTRY_VERIFY(!panel->property("visible").toBool());
    }

    void savesProgramsFromTheSettings ()
    {
        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        // The button sits below the visible part of the panel
        QMetaObject::invokeMethod(panel, "programsRequested");
        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("programsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(findText("No programs yet."));
        QVERIFY(clickButton("Add command"));

        // Made by a Repeater, so only found in the visual tree
        std::function<QQuickItem*(QQuickItem*)> walk = [&] (QQuickItem* item) -> QQuickItem*
        {
            if(item->objectName() == "programCommand0") return item;
            for(auto* child : item->childItems())
                if(auto* found = walk(child)) return found;
            return nullptr;
        };
        QQuickItem* field = nullptr;
        QTRY_VERIFY((field = walk(window->contentItem())) && field->isVisible() && field->width() > 0);
        // As typing does; the offscreen window gives no reliable keyboard focus to a field in a popup
        field->setProperty("text", "true");
        QMetaObject::invokeMethod(field, "textEdited");
        QVERIFY(clickButton("Save"));
        QTRY_COMPARE(bridge->state()["programs"].toList().size(), 1);
        auto program = bridge->state()["programs"].toList().first().toMap();
        QCOMPARE(program["command"].toString(), QString("true"));
        QVERIFY(program["enabled"].toBool());
        QVERIFY(!program["stopWithGame"].toBool());

        // Leaves no program behind for the other tests
        QVERIFY(click("−"));
        QTRY_VERIFY(findText("No programs yet."));
        QVERIFY(clickButton("Save"));
        QTRY_VERIFY(bridge->state()["programs"].toList().isEmpty());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        panel->setProperty("open", false);
        QTRY_VERIFY(!panel->property("visible").toBool());
    }

    void setupAssistantFromTheSettings ()
    {
        QString previous = channel("LIVE")["language"].toString();
        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        QMetaObject::invokeMethod(panel, "setupRequested");
        auto* assistant = engine->rootObjects().first()->findChild<QObject*>("setupAssistant");
        QVERIFY(assistant);
        QTRY_VERIFY(assistant->property("opened").toBool());
        QTRY_VERIFY(!panel->property("visible").toBool());
        QVERIFY(findText("Step 1 of 4"));

        QVERIFY(clickButton("Next"));
        QTRY_VERIFY(findText("Step 2 of 4"));
        // The channels with a game folder from the test settings
        QStringList found;
        for(const auto& c : bridge->state()["channels"].toList())
            if(!c.toMap()["installPath"].toString().isEmpty()) found << c.toMap()["name"].toString();
        QVERIFY(found.contains("LIVE"));
        QTRY_VERIFY(findText("Found: " + found.join(", ")));
        QVERIFY(clickButton("Next"));
        QTRY_VERIFY(findText("Step 3 of 4"));
        auto* translation = engine->rootObjects().first()->findChild<QObject*>("setupTranslationBox");
        QVERIFY(translation);
        QStringList names = translation->property("model").toStringList();
        QVERIFY(names.contains("Deutsch"));
        // As a pick from the list does
        QMetaObject::invokeMethod(translation, "activated", Q_ARG(int, int(names.indexOf("Deutsch"))));
        QTRY_COMPARE(channel("LIVE")["language"].toString(), QString("de"));

        QVERIFY(clickButton("Next"));
        QTRY_VERIFY(findText("Step 4 of 4"));
        QVERIFY(clickButton("Done"));
        QTRY_VERIFY(!assistant->property("visible").toBool());
        QTRY_VERIFY(readSettingsFile()["setupDone"].get<bool>());
        QVERIFY(!bridge->state()["setupNeeded"].toBool());

        // Leaves LIVE as the other tests expect it
        bridge->setLanguage("LIVE", previous);
        QTRY_COMPARE(channel("LIVE")["language"].toString(), previous);
    }

    void showsStatisticsFromTheSettings ()
    {
        // A finished session in the LIVE folder of the test settings
        std::filesystem::path live = liveDir.path().toStdString();
        std::filesystem::create_directories(live / "logbackups");
        std::ofstream(live / "logbackups" / "Game Build(1) 10 Jan 26 (18 00 00).log")
            << "<2026-01-10T18:00:00.000Z> Log started\n"
               "<2026-01-10T18:10:00.000Z> [Notice] <Vehicle Control Flow> CVehicle::Initialize: Local client node [1] granted control token for "
               "'AEGS_Sabre_123' [2] [Team_CGP4][Vehicle]\n"
               "<2026-01-10T20:00:00.000Z> [Notice] <SystemQuit> CSystem::Quit invoked with - cause=30016, exitCode=0\n";

        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        QMetaObject::invokeMethod(panel, "statisticsRequested");
        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("statisticsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(bridge->state()["statistics"].toMap()["loaded"].toBool());
        QTRY_VERIFY(findText("2.0 h") || findText("2,0 h"));
        QVERIFY(findText("Game starts"));

        QVERIFY(clickButton("Save as HTML"));
        QTRY_VERIFY(!bridge->state()["statistics"].toMap()["reportPath"].toString().isEmpty());
        QFile report(bridge->state()["statistics"].toMap()["reportPath"].toString());
        QVERIFY(report.open(QIODevice::ReadOnly));
        QByteArray html = report.readAll();
        QVERIFY(html.contains("AEGS Sabre"));
        QVERIFY(html.startsWith("<!DOCTYPE html>"));
        report.remove();

        std::filesystem::remove_all(live / "logbackups");
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        panel->setProperty("open", false);
        QTRY_VERIFY(!panel->property("visible").toBool());
    }

    void dashboardTilesCanBeChosen ()
    {
        auto* dashboard = engine->rootObjects().first()->findChild<QQuickItem*>("dashboard");
        QVERIFY(dashboard);
        QTRY_VERIFY(dashboard->isVisible());
        // Default tiles; their values come from the statistics loaded at start
        QTRY_VERIFY(bridge->state()["statistics"].toMap()["loaded"].toBool());
        QVERIFY(findText("Play time"));
        QVERIFY(findText("Free space"));

        QVERIFY(click("Customize …"));
        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("dashboardDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        // Adds "Crashes" at the end through its "+" button
        std::function<QQuickItem*(QQuickItem*)> walk = [&] (QQuickItem* item) -> QQuickItem*
        {
            if(item->objectName() == "addTile_crashes") return item;
            for(auto* child : item->childItems())
                if(auto* found = walk(child)) return found;
            return nullptr;
        };
        QQuickItem* add = nullptr;
        QTRY_VERIFY((add = walk(window->contentItem())) && add->width() > 0);
        QMetaObject::invokeMethod(add, "clicked");
        QVERIFY(clickButton("Save"));
        QTRY_COMPARE(bridge->state()["dashboardTiles"].toStringList(), (QStringList{"playTime", "lastPlayed", "topShip", "freeSpace", "crashes"}));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(readSettingsFile()["dashboardTiles"].size(), size_t(5));

        // Back to the default for the other tests
        bridge->setDashboardTiles({"playTime", "lastPlayed", "topShip", "freeSpace"});
        QTRY_COMPARE(bridge->state()["dashboardTiles"].toStringList().size(), 4);
    }

    void switchesTheTheme ()
    {
        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        QTRY_VERIFY(panel->property("visible").toBool());
        auto* box = engine->rootObjects().first()->findChild<QObject*>("themeBox");
        QVERIFY(box);

        // As a pick from the list does: 1 = dark, 2 = light, 0 = like the system
        QMetaObject::invokeMethod(box, "activated", Q_ARG(int, 2));
        QTRY_COMPARE(bridge->state()["theme"].toString(), QString("light"));
        QTRY_VERIFY(!window->property("darkTheme").toBool());
        QCOMPARE(window->color(), QColor("#eef2f7"));
        QMetaObject::invokeMethod(box, "activated", Q_ARG(int, 1));
        QTRY_VERIFY(window->property("darkTheme").toBool());
        QCOMPARE(window->color(), QColor("#0b1220"));

        QMetaObject::invokeMethod(box, "activated", Q_ARG(int, 0));
        QTRY_COMPARE(bridge->state()["theme"].toString(), QString());
        QCOMPARE(window->property("darkTheme").toBool(), BackendBridge::systemDark());
        panel->setProperty("open", false);
        QTRY_VERIFY(!panel->property("visible").toBool());
    }

    void switchesTheInterfaceLanguage ()
    {
        QStringList codes;
        for(const auto& language : BackendBridge::interfaceLanguages()) codes << language.toMap()["code"].toString();
        if(!codes.contains("de")) QSKIP("Built without Qt Linguist, no translations");
        QCOMPARE(codes.first(), QString("en"));

        auto* panel = engine->rootObjects().first()->findChild<QObject*>("settingsPanel");
        panel->setProperty("open", true);
        QTRY_VERIFY(panel->property("visible").toBool());
        QTRY_VERIFY(findText("Start Star Citizen after the update"));
        auto* box = engine->rootObjects().first()->findChild<QObject*>("interfaceLanguageBox");
        QVERIFY(box);
        // As a pick from the list does; index 0 is the system language
        QMetaObject::invokeMethod(box, "activated", Q_ARG(int, int(codes.indexOf("de")) + 1));
        QTRY_COMPARE(bridge->state()["interfaceLanguage"].toString(), QString("de"));
        QTRY_VERIFY(findText("Star Citizen nach dem Update starten"));
        QCOMPARE(readSettingsFile()["interfaceLanguage"].get<std::string>(), std::string("de"));

        // Back to the system language, which is English in the tests (LANGUAGE=en)
        QMetaObject::invokeMethod(box, "activated", Q_ARG(int, 0));
        QTRY_COMPARE(bridge->state()["interfaceLanguage"].toString(), QString());
        QTRY_VERIFY(findText("Start Star Citizen after the update"));
        panel->setProperty("open", false);
        QTRY_VERIFY(!panel->property("visible").toBool());
    }

    // Every shipped interface language loads, is complete and keeps the placeholders of the English text
    void shipsCompleteInterfaceLanguages ()
    {
        QStringList codes;
        for(const auto& language : BackendBridge::interfaceLanguages()) codes << language.toMap()["code"].toString();
        if(!codes.contains("de")) QSKIP("Built without Qt Linguist, no translations");
        QStringList tsFiles = QDir(SC_TRANSLATIONS_DIR).entryList({"sc-localization-launcher_*.ts"});
        QCOMPARE(codes.size(), tsFiles.size());
        static const QRegularExpression placeholder("%(\\d|n)");
        for(const auto& code : codes)
        {
            QTranslator translator;
            QVERIFY2(translator.load("sc-localization-launcher_" + code, ":/i18n"), qPrintable(code));
            // English only translates plural forms, everything else is the source text
            if(code == "en") continue;
            QFile file(QString(SC_TRANSLATIONS_DIR) + "/sc-localization-launcher_" + code + ".ts");
            QVERIFY(file.open(QIODevice::ReadOnly));
            QXmlStreamReader xml(&file);
            QString source;
            while(xml.readNext() != QXmlStreamReader::EndDocument && !xml.hasError())
            {
                if(!xml.isStartElement()) continue;
                if(xml.name() == QLatin1String("source")) source = xml.readElementText();
                if(xml.name() != QLatin1String("translation")) continue;
                QVERIFY2(xml.attributes().value("type").isEmpty(), qPrintable(code + ": " + source));
                QString text = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                QVERIFY2(!text.trimmed().isEmpty(), qPrintable(code + ": " + source));
                for(auto it = placeholder.globalMatch(source); it.hasNext();)
                {
                    QString wanted = it.next().captured();
                    QVERIFY2(text.contains(wanted), qPrintable(code + ": " + wanted + " missing in " + text));
                }
            }
            QVERIFY2(!xml.hasError(), qPrintable(code + ": " + xml.errorString()));
        }
    }

    void ownSourceThroughTheDialog ()
    {
        // The dialog only accepts HTTPS; nothing is downloaded in this test
        QString url = "https://example.org/test/global.ini";

        // Open the LIVE picker (the topmost one) and choose the entry for an own source
        QQuickItem* picker = languagePicker();
        QVERIFY(picker);
        // Locked while an update runs, a click would be lost
        QTRY_VERIFY(picker->isEnabled());
        QPointF center = picker->mapToScene(QPointF(picker->width() / 2, picker->height() / 2));
        QTest::mouseClick(window, Qt::LeftButton, {}, center.toPoint());
        QTRY_VERIFY(findText("Own source …"));
        QVERIFY(click("Own source …"));

        auto* dialog = engine->rootObjects().first()->findChild<QObject*>("customSourceDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QCOMPARE(dialog->property("channelName").toString(), QString("LIVE"));

        // The address field takes the keyboard focus when the dialog opens
        auto* urlField = engine->rootObjects().first()->findChild<QQuickItem*>("customSourceUrl");
        QVERIFY(urlField);
        QTRY_VERIFY(urlField->hasActiveFocus());
        // keyClicks() only exists for widgets
        for(QChar character : url) QTest::keyClick(window, character.toLatin1());
        QTRY_COMPARE(urlField->property("text").toString(), url);
        QTRY_VERIFY(findButton("Save and use"));
        QVERIFY(clickButton("Save and use"));
        QTRY_COMPARE(channel("LIVE")["language"].toString(), QString("custom"));
        QCOMPARE(channel("LIVE")["custom"].toMap()["url"].toString(), url);
        QCOMPARE(channel("LIVE")["custom"].toMap()["folder"].toString(), QString("german_(germany)"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }
};

QTEST_MAIN(GuiTest)
#include "GuiTest.moc"
