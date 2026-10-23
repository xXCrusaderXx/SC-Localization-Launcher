import QtQuick
import QtQuick.Layouts

// One game channel (LIVE, PTU, ...) with its install path and translation choice
Rectangle {
    id: row

    // Protocol::Channel: { name, installPath, language, availableLanguages, status, notices }
    property var channel: ({})
    // Protocol::State.languages: [{ code, name, credit }]
    property var languages: []
    property bool locked: false

    signal languagePicked(string language)
    signal pathRequested
    signal repairRequested

    readonly property bool pathSet: (channel.installPath || "") !== ""
    readonly property var available: channel.availableLanguages || []
    readonly property bool choiceAvailable: available.indexOf(channel.language) >= 0
    // Red: the installed files were changed or the last update failed
    readonly property bool broken: channel.status === "damaged" || channel.status === "failed"
    // Notice of the translation project for the chosen language on this channel
    readonly property string notice: (channel.notices || {})[channel.language] || ""
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
                notice: (channel.notices || {})[l.code] || "",
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
            color: row.broken ? Theme.danger : row.channel.status === "installed" ? Theme.success : row.pathSet ? Theme.warning : Theme.disabled
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
                text: {
                    if (!row.pathSet)
                        return qsTr("No install path set");
                    if (!row.choiceAvailable)
                        return qsTr("%1 is currently not available for this channel").arg(row.choiceName);
                    if (row.channel.status === "damaged")
                        return qsTr("Files were changed – please repair");
                    if (row.channel.status === "failed")
                        return qsTr("The last update failed");
                    if (row.notice !== "")
                        return row.notice;
                    if (row.channel.status === "notInstalled")
                        return qsTr("%1 is not installed yet").arg(row.choiceName);
                    if (row.channel.status === "updateAvailable")
                        return row.channel.language === "en" ? qsTr("The translation is removed with the next update") : qsTr("Update available for %1").arg(row.choiceName);
                    return row.channel.installPath;
                }
                color: row.broken ? Theme.danger : row.pathSet && (!row.choiceAvailable || row.notice !== "" || row.channel.status !== "installed") ? Theme.warning : Theme.muted
                // Notices and language names come from the internet
                textFormat: Text.PlainText
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

        // Downloads the translation again and rewrites user.cfg, e.g. after a game update
        IconButton {
            objectName: "repairButton"
            visible: row.pathSet
            glyph: "↻"
            Accessible.name: qsTr("Repair")
            onClicked: if (!row.locked)
                row.repairRequested()
        }

        IconButton {
            visible: row.pathSet
            glyph: "✎"
            onClicked: if (!row.locked)
                row.pathRequested()
        }
    }
}
