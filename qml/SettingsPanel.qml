import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Settings slide in from the right over the main window
Item {
    id: panel

    // Protocol::State
    property var appState: ({})
    property bool locked: false
    property bool open: false

    signal pathChangeRequested(string target)

    readonly property var options: appState.options || {}
    readonly property var channels: appState.channels || []
    readonly property bool searchBusy: appState.searching === true
    // Languages with a credit, i.e. every translation (English is the original game text)
    readonly property var translations: (appState.languages || []).filter(l => l.credit && l.credit.project)

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
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 14

                    Toggle {
                        text: qsTr("Start Star Citizen after the update")
                        description: qsTr("Starts through the LUG script or Lutris")
                        checked: panel.options.launchGameAfterUpdate === true
                        onSwitched: on => backend.setOption("launchGameAfterUpdate", on)
                    }
                }

                RowLayout {
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
                                    color: Theme.text
                                    font.pixelSize: 12
                                    font.bold: true
                                }

                                Text {
                                    width: parent.width
                                    text: qsTr("%1 by %2").arg("<a href=\"%1\">%2</a>".arg(modelData.credit.url).arg(modelData.credit.project)).arg(modelData.credit.authors)
                                    textFormat: Text.StyledText
                                    linkColor: Theme.accent
                                    color: Theme.muted
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    onLinkActivated: link => Qt.openUrlExternally(link)
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.credit.licenseUrl ? qsTr("License: %1").arg("<a href=\"%1\">%2</a>".arg(modelData.credit.licenseUrl).arg(modelData.credit.license)) : qsTr("License: %1").arg(modelData.credit.license)
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
                    text: qsTr("HELP")
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
