import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // Latest backend snapshot (Protocol::State), replaced as a whole on every change
    readonly property var appState: backend.state
    readonly property var channels: appState.channels || []
    // The main page only lists channels with an install folder; all of them stay in the settings
    readonly property var visibleChannels: channels.filter(c => c.installPath !== "")

    readonly property bool updateBusy: appState.update === "running"
    readonly property bool updateFailed: appState.update === "failed"
    readonly property bool updateReady: channels.some(c => c.installPath !== "")
    // Channels whose translation is missing, outdated or still has to be removed
    readonly property int pendingUpdates: channels.filter(c => ["notInstalled", "updateAvailable", "damaged", "failed"].indexOf(c.status) >= 0).length
    readonly property bool launchAfterUpdate: appState.options.launchGameAfterUpdate === true
    readonly property bool searchBusy: appState.searching === true

    // target is a channel name or "RSI" for the RSI Launcher folder
    function choosePath(target) {
        folderDialog.target = target;
        folderDialog.open();
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
                color: Qt.rgba(0.04, 0.07, 0.13, 0.96)
            }
            GradientStop {
                position: 0.6
                color: Qt.rgba(0.04, 0.07, 0.13, 0.82)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(0.04, 0.07, 0.13, 0.1)
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

            Image {
                source: "qrc:/logo.png"
                sourceSize: Qt.size(38, 38)
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
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
                locked: window.updateBusy
                onLanguagePicked: language => backend.setLanguage(channel.name, language)
                onPathRequested: window.choosePath(channel.name)
                onRepairRequested: backend.repairChannel(channel.name)
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
                        color: window.updateBusy || window.searchBusy ? Theme.accent : window.updateFailed ? Theme.danger : window.pendingUpdates > 0 ? Theme.warning : window.updateReady ? Theme.success : Theme.disabled

                        SequentialAnimation on opacity {
                            running: window.updateBusy || window.searchBusy
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
                        text: window.updateBusy ? qsTr("Downloading translations …") : window.searchBusy ? qsTr("Searching for Star Citizen installations …") : window.updateFailed ? qsTr("Update failed. <a href=\"log\">Open the log</a>") : window.pendingUpdates > 0 ? qsTr("%n channel(s) need an update", "", window.pendingUpdates) : window.appState.update === "succeeded" ? qsTr("Translations updated") : window.updateReady ? qsTr("Ready") : qsTr("Choose a game folder or use the automatic search")
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

                readonly property bool clickable: window.updateReady && !window.updateBusy

                Layout.preferredWidth: 270
                Layout.preferredHeight: 52
                radius: 26
                clip: true
                opacity: clickable || window.updateBusy ? 1 : 0.45
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

                // Shimmer while the update is running
                Rectangle {
                    visible: window.updateBusy
                    width: parent.width * 0.35
                    height: parent.height
                    radius: parent.radius
                    color: Qt.rgba(1, 1, 1, 0.25)

                    NumberAnimation on x {
                        running: window.updateBusy
                        loops: Animation.Infinite
                        from: -updateButton.width * 0.35
                        to: updateButton.width
                        duration: 1100
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: window.updateBusy ? qsTr("Updating …") : !window.updateReady ? qsTr("No game folder set") : window.launchAfterUpdate ? qsTr("Update & start") : qsTr("Update translations")
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
                    onClicked: backend.startUpdate()
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
        locked: window.updateBusy
        onPathChangeRequested: target => window.choosePath(target)
    }

    LauncherUpdateDialog {
        id: launcherUpdateDialog

        selfUpdate: window.appState.selfUpdate || ({})
        onCheckRequested: backend.checkForUpdate()
        onInstallRequested: backend.installUpdate()
        onRestartRequested: backend.restartLauncher()
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
