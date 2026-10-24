import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Settings slide in from the right over the main window
Item {
    id: panel

    // Protocol::State
    property var appState: ({})
    property bool locked: false
    // A system tray exists; "close to tray" is offered only then
    property bool trayAvailable: false
    property bool open: false

    signal pathChangeRequested(string target)
    signal cacheCleanerRequested
    signal diagnosticsRequested

    readonly property var options: appState.options || {}
    readonly property var channels: appState.channels || []
    readonly property bool searchBusy: appState.searching === true
    // Languages with a credit, i.e. every translation (English is the original game text)
    readonly property var translations: (appState.languages || []).filter(l => l.credit && l.credit.project)

    // Credits come from languages.json on the internet; inside StyledText they must not be read as markup
    function escapeHtml(text) {
        return String(text || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
    }

    visible: open || sheet.x < width

    // Dimmed backdrop, closes the panel on click
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.5)
        opacity: panel.open ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: 200
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: panel.open = false
        }
    }

    Rectangle {
        id: sheet

        width: 520
        height: parent.height
        x: panel.open ? panel.width - width : panel.width
        color: Qt.rgba(0.04, 0.07, 0.13, 0.97)
        border.color: Theme.border

        Behavior on x {
            NumberAnimation {
                duration: 240
                easing.type: Easing.OutCubic
            }
        }

        // Swallow clicks so they do not reach the backdrop
        MouseArea {
            anchors.fill: parent
        }

        RowLayout {
            id: sheetHeader

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                leftMargin: 24
                rightMargin: 12
                topMargin: 16
            }

            Text {
                text: qsTr("Settings")
                color: Theme.text
                font.pixelSize: 20
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            IconButton {
                glyph: "✕"
                onClicked: panel.open = false
            }
        }

        ScrollView {
            anchors {
                left: parent.left
                right: parent.right
                top: sheetHeader.bottom
                bottom: parent.bottom
                topMargin: 12
            }
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 10

                component SectionTitle: Text {
                    Layout.leftMargin: 24
                    Layout.topMargin: 8
                    color: Theme.muted
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 2
                }

                SectionTitle {
                    text: qsTr("BEHAVIOUR")
                }

                ColumnLayout {
                    // Bound to the panel width, so long texts wrap and long paths are elided instead of pushing the buttons out
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 14

                    Toggle {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Start Star Citizen after the update")
                        description: qsTr("Starts through the LUG script or Lutris")
                        checked: panel.options.launchGameAfterUpdate === true
                        onSwitched: on => backend.setOption("launchGameAfterUpdate", on)
                    }

                    Toggle {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Update translations automatically")
                        description: qsTr("Checks every hour and installs new versions; waits while Star Citizen runs")
                        checked: panel.options.installNewTranslations === true
                        onSwitched: on => backend.setOption("installNewTranslations", on)
                    }

                    Toggle {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: qsTr("Check for launcher updates on start")
                        description: qsTr("Asks GitHub for a newer version of this launcher")
                        checked: panel.options.checkLauncherUpdatesAtStart === true
                        onSwitched: on => backend.setOption("checkLauncherUpdatesAtStart", on)
                    }

                    Toggle {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        visible: panel.trayAvailable
                        text: qsTr("Keep running in the system tray")
                        description: qsTr("Closing the window keeps the launcher in the tray, so it goes on checking")
                        checked: panel.options.closeToTray === true
                        onSwitched: on => backend.setOption("closeToTray", on)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: 24
                    Layout.topMargin: 12

                    SectionTitle {
                        text: qsTr("INSTALLATIONS")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        Layout.topMargin: 8
                        text: panel.searchBusy ? qsTr("Searching …") : qsTr("Search automatically")
                        color: searchMouse.containsMouse ? Theme.text : Theme.accent
                        font.pixelSize: 11
                        font.bold: true

                        MouseArea {
                            id: searchMouse
                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true
                            enabled: !panel.searchBusy && !panel.locked
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.autoSearch()
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 8

                    PathRow {
                        label: "RSI Launcher"
                        path: panel.appState.rsiLauncherPath || ""
                        locked: panel.locked
                        onChangeRequested: panel.pathChangeRequested("RSI")
                    }

                    Repeater {
                        model: panel.channels.length

                        delegate: PathRow {
                            required property int index
                            readonly property var channel: panel.channels[index] || ({})

                            label: channel.name || ""
                            path: channel.installPath || ""
                            locked: panel.locked
                            onChangeRequested: panel.pathChangeRequested(channel.name)
                        }
                    }
                }

                SectionTitle {
                    Layout.topMargin: 12
                    text: qsTr("TRANSLATIONS")
                }

                // Credit and license of every offered translation, as the community projects' licenses require
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 8

                    Repeater {
                        model: panel.translations

                        delegate: Rectangle {
                            required property var modelData

                            Layout.fillWidth: true
                            implicitHeight: creditColumn.implicitHeight + 20
                            radius: 10
                            color: Theme.panel
                            border.color: Theme.border

                            Column {
                                id: creditColumn

                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                    leftMargin: 14
                                    rightMargin: 14
                                }
                                spacing: 2

                                Text {
                                    width: parent.width
                                    text: modelData.name
                                    textFormat: Text.PlainText
                                    color: Theme.text
                                    font.pixelSize: 12
                                    font.bold: true
                                }

                                Text {
                                    width: parent.width
                                    text: qsTr("%1 by %2").arg("<a href=\"%1\">%2</a>".arg(panel.escapeHtml(modelData.credit.url)).arg(panel.escapeHtml(modelData.credit.project))).arg(panel.escapeHtml(modelData.credit.authors))
                                    textFormat: Text.StyledText
                                    linkColor: Theme.accent
                                    color: Theme.muted
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    onLinkActivated: link => Qt.openUrlExternally(link)
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.credit.licenseUrl ? qsTr("License: %1").arg("<a href=\"%1\">%2</a>".arg(panel.escapeHtml(modelData.credit.licenseUrl)).arg(panel.escapeHtml(modelData.credit.license))) : qsTr("License: %1").arg(panel.escapeHtml(modelData.credit.license))
                                    textFormat: Text.StyledText
                                    linkColor: Theme.accent
                                    color: Theme.muted
                                    font.pixelSize: 11
                                    onLinkActivated: link => Qt.openUrlExternally(link)
                                }
                            }
                        }
                    }
                }

                SectionTitle {
                    Layout.topMargin: 12
                    text: qsTr("TOOLS")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Shader caches of the game and the graphics driver; clearing them helps against stutter after updates")
                        wrapMode: Text.Wrap
                        color: Theme.muted
                        font.pixelSize: 11
                    }

                    DialogButton {
                        text: qsTr("Clear caches …")
                        enabled: !panel.locked
                        onClicked: panel.cacheCleanerRequested()
                    }
                }

                SectionTitle {
                    Layout.topMargin: 12
                    text: qsTr("HELP")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Checks folders, rights and translation files and saves a report for a bug report")
                        wrapMode: Text.Wrap
                        color: Theme.muted
                        font.pixelSize: 11
                    }

                    DialogButton {
                        text: qsTr("Check installation …")
                        onClicked: panel.diagnosticsRequested()
                    }
                }

                // Log files are what a bug report needs; the path is shown so it can be mentioned
                PathRow {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.bottomMargin: 24
                    label: qsTr("Log files")
                    path: backend.logFolder
                    editable: false
                }
            }
        }
    }
}
