import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The player's own texts for single entries of a channel's global.ini, written again after every translation update
Popup {
    id: dialog

    // Channels with a game folder (Protocol::Channel)
    property var channels: []
    property string channelName: channels.length > 0 ? channels[0].name : ""
    readonly property var channel: channels.find(c => c.name === channelName) || ({})

    signal saveRequested(string channel, string lines)

    // Puts the stored lines into the field, e.g. after switching the channel
    function load() {
        linesField.text = channel.textOverrides || "";
    }

    function statusText() {
        if (channel.overridesError === "too long")
            return qsTr("Not saved – at most 64 KB of own texts per channel.");
        if (channel.overridesError)
            return qsTr("Not saved – this line is no \"key=value\" or repeats a key: %1").arg(channel.overridesError);
        if ((channel.textOverrides || "") === "")
            return "";
        if (channel.language === "en")
            return qsTr("Used once this channel has a translation; English has no file to change.");
        if (channel.overridesPending)
            return qsTr("Written into the translation with its next update.");
        if ((channel.appendedOverrides || []).length > 0)
            return qsTr("Not in the translation, added at its end: %1. If the game does not show them, check the spelling.").arg(channel.appendedOverrides.join(", "));
        return qsTr("In the installed translation; written again after every update.");
    }

    objectName: "textOverridesDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 560
    height: Math.min(implicitHeight, 500)
    modal: true
    padding: 24
    onOpened: load()
    onChannelNameChanged: load()

    component Field: Rectangle {
        implicitHeight: 36
        radius: 8
        color: Theme.inputBackground
        border.color: Theme.border
    }

    background: Rectangle {
        radius: 14
        color: Theme.popup
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 10

        Shortcut {
            sequence: "Esc"
            enabled: dialog.opened
            onActivated: dialog.close()
        }

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("Own texts")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
            }

            IconButton {
                glyph: "✕"
                onClicked: dialog.close()
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Replaces single texts of the channel's translation with your own, e.g. a shorter name. One \"key=value\" per line, with the key as in global.ini; lines starting with ; are comments. They stay on this computer.")
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
        }

        ComboBox {
            id: channelBox

            Layout.fillWidth: true
            visible: dialog.channels.length > 1
            model: dialog.channels.map(c => c.name)
            currentIndex: model.indexOf(dialog.channelName)
            onActivated: index => dialog.channelName = model[index]
            font.pixelSize: 12
            palette.text: Theme.text
            palette.base: Theme.inputBackground
            palette.window: Theme.popup
            palette.button: Theme.inputBackground
            palette.buttonText: Theme.text
            palette.highlight: Theme.accent
            palette.highlightedText: "white"
            background: Field {}
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 180

            TextArea {
                id: linesField

                objectName: "textOverridesLines"
                placeholderText: "key=value"
                placeholderTextColor: Theme.disabled
                color: Theme.text
                font.family: "monospace"
                font.pixelSize: 12
                wrapMode: TextEdit.NoWrap
                selectByMouse: true
                background: Field {}
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: dialog.statusText()
            wrapMode: Text.Wrap
            color: dialog.channel.overridesError ? Theme.warning : Theme.muted
            font.pixelSize: 11
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                text: qsTr("Close")
                onClicked: dialog.close()
            }

            DialogButton {
                text: qsTr("Save")
                highlighted: true
                enabled: dialog.channelName !== ""
                onClicked: dialog.saveRequested(dialog.channelName, linesField.text)
            }
        }
    }
}
