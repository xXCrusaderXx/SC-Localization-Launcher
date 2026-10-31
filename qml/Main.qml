import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.platform as Platform

ApplicationWindow {
    id: window

    // Latest backend snapshot (Protocol::State), replaced as a whole on every change
    readonly property var appState: backend.state
    readonly property var channels: appState.channels || []
    // The main page only lists channels with an install folder; all of them stay in the settings
    readonly property var visibleChannels: channels.filter(c => c.installPath !== "")

    // Folders offered for an own source: the catalogue's, then the other languages Star Citizen knows
    readonly property var localizationFolders: {
        const folders = [];
        for (const language of appState.languages || [])
            if (language.folder && folders.indexOf(language.folder) < 0)
                folders.push(language.folder);
        for (const folder of ["german_(germany)", "french_(france)", "spanish_(spain)", "italian_(italy)", "polish_(poland)", "portuguese_(brazil)"])
            if (folders.indexOf(folder) < 0)
                folders.push(folder);
        return folders;
    }

    readonly property bool updateBusy: appState.update === "running"
    readonly property bool updateFailed: appState.update === "failed"
    // The button stays busy until the RSI Launcher runs or failed
    readonly property bool gameStarting: appState.game === "starting"
    // The RSI Launcher or the game is open, started here or elsewhere
    readonly property bool gameRunning: appState.game === "running" || appState.game === "playing"
    // StarCitizen.exe runs; translations can't be updated meanwhile
    readonly property bool gamePlaying: appState.game === "playing"
    readonly property bool gameFailed: appState.game === "failed" || appState.game === "noStarter"
    readonly property bool updateReady: channels.some(c => c.installPath !== "")
    // Channels whose translation is missing, outdated or still has to be removed
    readonly property int pendingUpdates: channels.filter(c => ["notInstalled", "updateAvailable", "damaged", "failed"].indexOf(c.status) >= 0).length
    readonly property bool launchAfterUpdate: appState.options.launchGameAfterUpdate === true
    readonly property bool searchBusy: appState.searching === true
    // Status light of the tray icon: "idle" (no game folder), "error", "pending" or "ok"
    readonly property string overallStatus: {
        if (!updateReady)
            return "idle";
        if (updateFailed || channels.some(c => c.status === "damaged" || c.status === "failed"))
            return "error";
        return pendingUpdates > 0 ? "pending" : "ok";
    }
    // Without a tray (e.g. GNOME without the AppIndicator extension) closing always quits, so the window can't get lost
    readonly property bool closesToTray: tray.available && appState.options.closeToTray === true
    // Release notes of an update that was not shown yet
    readonly property bool hasWhatsNew: ((appState.whatsNew || {}).notes || "") !== ""

    // The setting "Appearance", or the desktop's colour scheme
    Binding {
        target: Theme
        property: "dark"
        value: window.appState.theme === "dark" || (window.appState.theme !== "light" && backend.systemDark)
    }

    // Read by the GUI test
    readonly property bool darkTheme: Theme.dark

    // First start: nothing set up yet
    readonly property bool setupNeeded: appState.setupNeeded === true

    onSetupNeededChanged: {
        if (setupNeeded)
            setupAssistant.open();
    }
    onHasWhatsNewChanged: {
        if (hasWhatsNew)
            whatsNewDialog.open();
    }
    Component.onCompleted: backend.loadWhatsNew()
    onClosing: close => {
        if (closesToTray) {
            close.accepted = false;
            window.hide();
        }
    }

    // target is a channel name or "RSI" for the RSI Launcher folder
    function choosePath(target) {
        folderDialog.target = target;
        folderDialog.open();
    }

    function showWindow() {
        window.showNormal();
        window.raise();
        window.requestActivate();
    }

    width: 880
    height: 540
    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "SC Localization Launcher"
    color: Theme.background

    Image {
        anchors.fill: parent
        source: "qrc:/background2_main.png"
        fillMode: Image.PreserveAspectCrop
        horizontalAlignment: Image.AlignRight
    }

    // Darkens the left side so the panels stay readable, the astronaut stays visible on the right
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.0
                color: Theme.veil(0.96)
            }
            GradientStop {
                position: 0.6
                color: Theme.veil(0.82)
            }
            GradientStop {
                position: 1.0
                color: Theme.veil(0.1)
            }
        }
    }

    Item {
        id: header

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: 72

        // Frameless window: drag it by the header
        DragHandler {
            target: null
            onActiveChanged: if (active)
                window.startSystemMove()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 12
            spacing: 12

            // The logo is white; in the light theme it sits on a dark badge
            Rectangle {
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                radius: 21
                color: Theme.dark ? "transparent" : "#0b1220"

                Image {
                    anchors.centerIn: parent
                    source: "qrc:/logo.png"
                    sourceSize: Qt.size(38, 38)
                    width: 38
                    height: 38
                }
            }

            ColumnLayout {
                spacing: 0

                Text {
                    text: "SC Localization Launcher"
                    color: Theme.text
                    font.pixelSize: 21
                    font.bold: true
                }

                Text {
                    text: qsTr("COMMUNITY TRANSLATIONS FOR LINUX")
                    color: Theme.accent
                    font.pixelSize: 10
                    font.letterSpacing: 2
                }
            }

            Item {
                Layout.fillWidth: true
            }

            // Launcher updates; the dot shows that a newer version is published
            IconButton {
                objectName: "launcherUpdateButton"
                glyph: "⇩"

                onClicked: {
                    // Only ask GitHub again when nothing is known or in progress yet
                    if (["idle", "upToDate", "error"].indexOf(window.appState.selfUpdate.status) >= 0)
                        backend.checkForUpdate();
                    launcherUpdateDialog.open();
                }

                Rectangle {
                    visible: ["available", "downloading", "installing", "installed"].indexOf(window.appState.selfUpdate.status) >= 0
                    width: 8
                    height: 8
                    radius: 4
                    color: Theme.accent
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 3
                }
            }

            IconButton {
                glyph: "⚙"
                onClicked: settingsPanel.open = true
            }

            IconButton {
                glyph: "—"
                onClicked: window.showMinimized()
            }

            IconButton {
                glyph: "✕"
                hoverColor: Theme.danger
                onClicked: window.close()
            }
        }
    }

    ColumnLayout {
        anchors {
            left: parent.left
            top: header.bottom
            bottom: footer.top
            leftMargin: 22
            topMargin: 4
        }
        width: 560
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("GAME CHANNELS")
                color: Theme.muted
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 2
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: window.searchBusy ? qsTr("Searching …") : qsTr("Search automatically")
                color: searchMouse.containsMouse ? Theme.text : Theme.accent
                font.pixelSize: 11
                font.bold: true

                MouseArea {
                    id: searchMouse
                    anchors.fill: parent
                    anchors.margins: -6
                    hoverEnabled: true
                    enabled: !window.searchBusy && !window.updateBusy
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backend.autoSearch()
                }
            }
        }

        // Model is the count, not the array: the rows stay alive across snapshots, so their animations keep running
        Repeater {
            model: window.visibleChannels.length

            delegate: ChannelRow {
                required property int index

                channel: window.visibleChannels[index] || ({})
                languages: window.appState.languages || []
                // The running game reads the translation files
                locked: window.updateBusy || window.gamePlaying
                onLanguagePicked: language => backend.setLanguage(channel.name, language)
                onPathRequested: window.choosePath(channel.name)
                onRepairRequested: backend.repairChannel(channel.name)
                onCustomSourceRequested: customSourceDialog.openFor(channel.name, channel.custom, window.localizationFolders)
            }
        }

        // First start or nothing found yet
        Rectangle {
            visible: window.visibleChannels.length === 0
            Layout.fillWidth: true
            implicitHeight: emptyColumn.implicitHeight + 40
            radius: 12
            color: Theme.panel
            border.color: Theme.border

            ColumnLayout {
                id: emptyColumn

                anchors.fill: parent
                anchors.margins: 20
                spacing: 14

                Text {
                    Layout.fillWidth: true
                    text: window.searchBusy ? qsTr("Searching for Star Citizen …") : qsTr("No Star Citizen installation found yet")
                    color: Theme.text
                    font.pixelSize: 15
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Search your drives automatically, or add the game folder of a channel in the settings.")
                    color: Theme.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    spacing: 10

                    component PillButton: Rectangle {
                        id: pill

                        property alias text: pillLabel.text
                        property bool primary: false
                        property bool active: true

                        signal clicked

                        implicitWidth: pillLabel.implicitWidth + 28
                        implicitHeight: 32
                        radius: 16
                        opacity: active ? 1 : 0.5
                        color: primary ? Theme.accent : pillMouse.containsMouse ? Theme.hover : "transparent"
                        border.color: Theme.accent

                        Text {
                            id: pillLabel

                            anchors.centerIn: parent
                            color: pill.primary ? Theme.background : Theme.accent
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            id: pillMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: pill.active
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pill.clicked()
                        }
                    }

                    PillButton {
                        primary: true
                        active: !window.searchBusy
                        text: window.searchBusy ? qsTr("Searching …") : qsTr("Search automatically")
                        onClicked: backend.autoSearch()
                    }

                    PillButton {
                        text: qsTr("Open settings")
                        onClicked: settingsPanel.open = true
                    }
                }
            }
        }

        // Keeps the rows at the top when fewer channels are shown
        Item {
            Layout.fillHeight: true
        }
    }

    Item {
        id: footer

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 92

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 26
            spacing: 16

            ColumnLayout {
                spacing: 6

                Toggle {
                    text: qsTr("Start Star Citizen afterwards")
                    checked: window.launchAfterUpdate
                    onSwitched: on => backend.setOption("launchGameAfterUpdate", on)
                }

                RowLayout {
                    spacing: 8

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: window.updateBusy || window.gameStarting || window.searchBusy ? Theme.accent : window.updateFailed || window.gameFailed ? Theme.danger : window.pendingUpdates > 0 ? Theme.warning : window.updateReady ? Theme.success : Theme.disabled

                        SequentialAnimation on opacity {
                            running: window.updateBusy || window.gameStarting || window.searchBusy
                            loops: Animation.Infinite
                            alwaysRunToEnd: true
                            NumberAnimation {
                                to: 0.2
                                duration: 500
                            }
                            NumberAnimation {
                                to: 1
                                duration: 500
                            }
                        }
                    }

                    Text {
                        text: {
                            if (window.updateBusy)
                                return qsTr("Downloading translations …");
                            if (window.gameStarting)
                                return qsTr("Starting the RSI Launcher …");
                            if (window.searchBusy)
                                return qsTr("Searching for Star Citizen installations …");
                            if (window.updateFailed)
                                return qsTr("Update failed. <a href=\"log\">Open the log</a>");
                            if (window.appState.game === "failed")
                                return qsTr("The RSI Launcher did not start. <a href=\"log\">Open the log</a>");
                            if (window.appState.game === "noStarter")
                                return qsTr("Cannot start the game: install it with the LUG Helper or Lutris");
                            if (window.pendingUpdates > 0)
                                return qsTr("%n channel(s) need an update", "", window.pendingUpdates);
                            if (window.gamePlaying)
                                return window.pendingUpdates > 0 && window.appState.options.installNewTranslations === true ? qsTr("Star Citizen is running – the update follows when it closes") : qsTr("Star Citizen is running");
                            if (window.gameRunning)
                                return qsTr("RSI Launcher is running");
                            if (window.appState.update === "succeeded")
                                return qsTr("Translations updated");
                            return window.updateReady ? qsTr("Ready") : qsTr("Choose a game folder or use the automatic search");
                        }
                        color: Theme.muted
                        textFormat: Text.StyledText
                        linkColor: Theme.accent
                        onLinkActivated: backend.openLogFolder()
                        font.pixelSize: 11
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Rectangle {
                id: updateButton

                readonly property bool busy: window.updateBusy || window.gameStarting
                // With the game start switched on, the button only updates when something is outdated, otherwise it just starts
                readonly property bool startsOnly: window.launchAfterUpdate && window.pendingUpdates === 0
                // Starting again while the RSI Launcher is open would do nothing; updating translations still works
                // until the game itself runs
                readonly property bool clickable: window.updateReady && !busy && !window.gamePlaying && !(window.launchAfterUpdate && window.gameRunning)

                Layout.preferredWidth: 270
                Layout.preferredHeight: 52
                radius: 26
                clip: true
                opacity: clickable || busy || window.gameRunning ? 1 : 0.45
                scale: updateMouse.pressed ? 0.97 : updateMouse.containsMouse && clickable ? 1.03 : 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: Theme.accent
                    }
                    GradientStop {
                        position: 1
                        color: Theme.accent2
                    }
                }

                Behavior on scale {
                    NumberAnimation {
                        duration: 120
                    }
                }

                // Shimmer while the update or the game start is running
                Rectangle {
                    visible: updateButton.busy
                    width: parent.width * 0.35
                    height: parent.height
                    radius: parent.radius
                    color: Qt.rgba(1, 1, 1, 0.25)

                    NumberAnimation on x {
                        running: updateButton.busy
                        loops: Animation.Infinite
                        from: -updateButton.width * 0.35
                        to: updateButton.width
                        duration: 1100
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: {
                        if (window.updateBusy)
                            return qsTr("Updating …");
                        if (window.gameStarting)
                            return qsTr("Starting game …");
                        if (!window.updateReady)
                            return qsTr("No game folder set");
                        if (window.gamePlaying)
                            return qsTr("Game running");
                        if (!window.launchAfterUpdate)
                            return qsTr("Update translations");
                        if (window.gameRunning)
                            return qsTr("Running");
                        return updateButton.startsOnly ? qsTr("Start") : qsTr("Update & start");
                    }
                    color: "white"
                    font.pixelSize: 15
                    font.bold: true
                }

                MouseArea {
                    id: updateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: updateButton.clickable
                    cursorShape: Qt.PointingHandCursor
                    onClicked: updateButton.startsOnly ? backend.startGame() : backend.startUpdate()
                }
            }
        }
    }

    // Thin outline, the frameless window would otherwise blend into dark desktops
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: Theme.border
    }

    SettingsPanel {
        id: settingsPanel
        objectName: "settingsPanel"

        anchors.fill: parent
        appState: window.appState
        trayAvailable: tray.available
        locked: window.updateBusy
        onPathChangeRequested: target => window.choosePath(target)
        onCacheCleanerRequested: {
            cacheDialog.open();
            backend.scanCaches();
        }
        onDiagnosticsRequested: {
            diagnosticsDialog.open();
            backend.runDiagnostics();
        }
        onBackupsRequested: {
            backupDialog.open();
            backend.loadBackups();
        }
        onGameSettingsRequested: gameSettingsDialog.open()
        onProgramsRequested: programsDialog.open()
        onStatisticsRequested: {
            statisticsDialog.open();
            backend.loadStatistics();
        }
        onSetupRequested: {
            settingsPanel.open = false;
            setupAssistant.open();
        }
    }

    StatisticsDialog {
        id: statisticsDialog

        statistics: window.appState.statistics || ({})
        onSaveRequested: html => backend.saveStatisticsReport(html)
        onShowReportRequested: path => backend.openReportFolder(path)
    }

    SetupAssistant {
        id: setupAssistant

        appState: window.appState
        trayAvailable: tray.available
        onFolderRequested: window.choosePath("LIVE")
        onFinished: install => {
            backend.finishSetup();
            if (install)
                backend.updateTranslations();
            close();
        }
    }

    ProgramsDialog {
        id: programsDialog

        programs: window.appState.programs || []
        failedPrograms: window.appState.failedPrograms || []
        onSaveRequested: programs => backend.setPrograms(programs)
    }

    GameSettingsDialog {
        id: gameSettingsDialog

        channels: window.visibleChannels
        onSaveRequested: (channel, maxFps, maxIdleFps, customLines, graphicsApi) => backend.setGameSettings(channel, maxFps, maxIdleFps, customLines, graphicsApi)
    }

    BackupDialog {
        id: backupDialog

        backups: window.appState.backups || ({})
        channels: window.visibleChannels.map(c => c.name)
        gamePlaying: window.gamePlaying
        onBackupRequested: channel => backend.createBackup(channel)
        onRestoreRequested: (channel, backup) => backend.restoreBackup(channel, backup)
    }

    DiagnosticsDialog {
        id: diagnosticsDialog

        diagnostics: window.appState.diagnostics || ({})
        onSaveRequested: backend.saveReport()
        onShowReportRequested: path => backend.openReportFolder(path)
    }

    CacheDialog {
        id: cacheDialog

        caches: window.appState.caches || ({})
        gamePlaying: window.gamePlaying
        onClearRequested: paths => backend.clearCaches(paths)
    }

    CustomSourceDialog {
        id: customSourceDialog

        onSaved: (url, folder) => {
            backend.setCustomSource(channelName, url, folder);
            backend.setLanguage(channelName, "custom");
        }
        onRemoved: backend.setCustomSource(channelName, "", "")
    }

    LauncherUpdateDialog {
        id: launcherUpdateDialog

        selfUpdate: window.appState.selfUpdate || ({})
        onCheckRequested: backend.checkForUpdate()
        onInstallRequested: backend.installUpdate()
        onRestartRequested: backend.restartLauncher()
    }

    WhatsNewDialog {
        id: whatsNewDialog

        whatsNew: window.appState.whatsNew || ({})
        onClosed: backend.dismissWhatsNew()
    }

    Platform.SystemTrayIcon {
        id: tray

        visible: available
        icon.source: "qrc:/tray/" + window.overallStatus + ".png"
        tooltip: {
            let status;
            if (window.updateBusy)
                status = qsTr("Downloading translations …");
            else if (window.overallStatus === "idle")
                status = qsTr("No game folder chosen");
            else if (window.overallStatus === "error")
                status = qsTr("A translation is damaged or its update failed");
            else if (window.overallStatus === "pending")
                status = qsTr("%n channel(s) need an update", "", window.pendingUpdates);
            else
                status = qsTr("All translations are up to date");
            return "SC Localization Launcher\n" + status;
        }
        onActivated: reason => {
            if (reason === Platform.SystemTrayIcon.Trigger)
                window.visible ? window.hide() : window.showWindow();
        }

        menu: Platform.Menu {
            Platform.MenuItem {
                text: qsTr("Open")
                onTriggered: window.showWindow()
            }

            Platform.MenuItem {
                text: qsTr("Update translations")
                // Like the main button: nothing while the game reads the files or another update runs
                enabled: window.pendingUpdates > 0 && !window.updateBusy && !window.gameStarting && !window.gamePlaying
                onTriggered: backend.updateTranslations()
            }

            Platform.MenuSeparator {}

            Platform.MenuItem {
                text: qsTr("Quit")
                onTriggered: Qt.quit()
            }
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: settingsPanel.open = false
    }

    FolderDialog {
        id: folderDialog

        property string target

        // Found by the GUI test
        objectName: "folderDialog"
        title: target === "RSI" ? qsTr("RSI Launcher folder") : qsTr("Install folder for %1").arg(target)
        onAccepted: target === "RSI" ? backend.setRsiLauncherPath(selectedFolder) : backend.setInstallPath(target, selectedFolder)
    }
}
