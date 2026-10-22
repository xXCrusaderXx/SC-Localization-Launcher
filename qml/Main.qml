import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // Latest backend snapshot (Protocol::State), replaced as a whole on every change
    readonly property var appState: backend.state
    readonly property var channels: appState.channels || []

    readonly property bool updateBusy: appState.update === "running"
    readonly property bool updateFailed: appState.update === "failed"
    readonly property bool updateReady: channels.some(c => c.installPath !== "")
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
            model: window.channels.length

            delegate: ChannelRow {
                required property int index

                channel: window.channels[index] || ({})
                languages: window.appState.languages || []
                locked: window.updateBusy
                onLanguagePicked: language => backend.setLanguage(channel.name, language)
                onPathRequested: window.choosePath(channel.name)
            }
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
                        color: window.updateBusy || window.searchBusy ? Theme.accent : window.updateFailed ? Theme.danger : window.updateReady ? Theme.success : Theme.disabled

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
                        text: window.updateBusy ? qsTr("Downloading translations …") : window.searchBusy ? qsTr("Searching for Star Citizen installations …") : window.updateFailed ? qsTr("Update failed, see the log for details") : window.appState.update === "succeeded" ? qsTr("Translations updated") : window.updateReady ? qsTr("Ready") : qsTr("Choose a game folder or use the automatic search")
                        color: Theme.muted
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
