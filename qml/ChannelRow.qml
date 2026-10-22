import QtQuick
import QtQuick.Layouts

// One game channel (LIVE, PTU, ...) with its install path and translation choice
Rectangle {
    id: row

    // Protocol::Channel: { name, installPath, language, availableLanguages }
    property var channel: ({})
    property bool locked: false

    signal languagePicked(string language)
    signal pathRequested

    readonly property bool pathSet: (channel.installPath || "") !== ""
    readonly property var available: channel.availableLanguages || []
    readonly property bool germanAvailable: available.indexOf("de") >= 0 || available.indexOf("deFull") >= 0

    Layout.fillWidth: true
    implicitHeight: 56
    radius: 12
    color: hover.hovered ? Theme.panelHover : Theme.panel
    border.color: Theme.border

    Behavior on color {
        ColorAnimation {
            duration: 150
        }
    }

    HoverHandler {
        id: hover
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 10
        spacing: 12

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: row.pathSet ? Theme.success : Theme.disabled
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Text {
                text: row.channel.name || ""
                color: Theme.text
                font.pixelSize: 14
                font.bold: true
                font.letterSpacing: 0.5
            }

            Text {
                Layout.fillWidth: true
                text: !row.pathSet ? "Kein Installationspfad gesetzt" : row.germanAvailable ? row.channel.installPath : "Aktuell keine Übersetzung für diesen Kanal"
                color: Theme.muted
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }
        }

        LangSwitch {
            visible: row.pathSet
            interactive: !row.locked
            currentKey: row.channel.language || ""
            options: [
                {
                    key: "eng",
                    label: "ENG",
                    enabled: row.available.indexOf("eng") >= 0
                },
                {
                    key: "de",
                    label: "DE",
                    enabled: row.available.indexOf("de") >= 0
                },
                {
                    key: "deFull",
                    label: "DE Voll",
                    enabled: row.available.indexOf("deFull") >= 0
                }
            ]
            onPicked: key => row.languagePicked(key)
        }

        // Shown as a pill when no path is set, as a small edit button otherwise
        Rectangle {
            visible: !row.pathSet
            implicitWidth: choosePathLabel.implicitWidth + 24
            implicitHeight: 30
            radius: 15
            color: choosePathMouse.containsMouse ? Theme.hover : "transparent"
            border.color: Theme.accent

            Text {
                id: choosePathLabel
                anchors.centerIn: parent
                text: "Pfad wählen"
                color: Theme.accent
                font.pixelSize: 12
                font.bold: true
            }

            MouseArea {
                id: choosePathMouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: !row.locked
                cursorShape: Qt.PointingHandCursor
                onClicked: row.pathRequested()
            }
        }

        IconButton {
            visible: row.pathSet
            glyph: "✎"
            onClicked: if (!row.locked)
                row.pathRequested()
        }
    }
}
