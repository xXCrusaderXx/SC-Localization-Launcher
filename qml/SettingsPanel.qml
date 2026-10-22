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
                text: "Einstellungen"
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
                    text: "VERHALTEN"
                }

                ColumnLayout {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 14

                    Toggle {
                        text: "Star Citizen nach dem Update starten"
                        description: "Startet über das LUG-Skript oder Lutris"
                        checked: panel.options.launchGameAfterUpdate === true
                        onSwitched: on => backend.setOption("launchGameAfterUpdate", on)
                    }

                    Toggle {
                        text: "Übersetzung beim Start aktualisieren"
                        comingSoon: true
                        checked: panel.options.updateAtStart === true
                    }

                    Toggle {
                        text: "Neue Übersetzungen automatisch installieren"
                        comingSoon: true
                        checked: panel.options.installNewTranslations === true
                    }

                    Toggle {
                        text: "Nach Updates in den Tray minimieren"
                        comingSoon: true
                        checked: panel.options.minimizeAfterUpdate === true
                    }

                    Toggle {
                        text: "Mit dem System starten"
                        comingSoon: true
                        checked: panel.options.startWithSystem === true
                    }

                    Toggle {
                        text: "Update-Fortschritt anzeigen"
                        comingSoon: true
                        checked: panel.options.showUpdateProgress === true
                    }
                }

                RowLayout {
                    Layout.rightMargin: 24
                    Layout.topMargin: 12

                    SectionTitle {
                        text: "INSTALLATIONEN"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        Layout.topMargin: 8
                        text: panel.searchBusy ? "Suche läuft …" : "Automatisch suchen"
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
                    Layout.bottomMargin: 24
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
            }
        }
    }
}
