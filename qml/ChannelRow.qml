import QtQuick
import QtQuick.Layouts

// One game channel (LIVE, PTU, ...) with its install path and translation choice
Rectangle {
    id: row

    // Protocol::Channel: { name, installPath, language, availableLanguages }
    property var channel: ({})
    // Protocol::State.languages: [{ code, name, credit }]
    property var languages: []
    property bool locked: false

    signal languagePicked(string language)
    signal pathRequested

    readonly property bool pathSet: (channel.installPath || "") !== ""
    readonly property var available: channel.availableLanguages || []
    readonly property bool choiceAvailable: available.indexOf(channel.language) >= 0
    readonly property string choiceName: {
        for (const language of languages)
            if (language.code === channel.language)
                return language.name;
        return channel.language || "";
    }
    // Every known language; the ones this channel does not offer right now are shown but cannot be picked
    readonly property var options: languages.map(l => ({
                code: l.code,
                name: l.name,
                credit: l.credit,
                available: available.indexOf(l.code) >= 0
            }))

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
                text: !row.pathSet ? qsTr("No install path set") : row.choiceAvailable ? row.channel.installPath : qsTr("%1 is currently not available for this channel").arg(row.choiceName)
                color: row.pathSet && !row.choiceAvailable ? Theme.warning : Theme.muted
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }
        }

        LanguagePicker {
            visible: row.pathSet
            enabled: !row.locked
            languages: row.options
            currentCode: row.channel.language || ""
            onPicked: code => row.languagePicked(code)
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
                text: qsTr("Choose folder")
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
