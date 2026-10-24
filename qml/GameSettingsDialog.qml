import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Frame limits and own lines in a channel's user.cfg; the rest of the file is left as it is
Popup {
    id: dialog

    // Channels with a game folder (Protocol::Channel)
    property var channels: []
    property string channelName: channels.length > 0 ? channels[0].name : ""
    readonly property var channel: channels.find(c => c.name === channelName) || ({})
    readonly property var settings: channel.gameSettings || ({})

    signal saveRequested(string channel, int maxFps, int maxIdleFps, string customLines)

    // Puts the stored values into the fields, e.g. after switching the channel
    function load() {
        maxFpsField.text = settings.maxFps > 0 ? String(settings.maxFps) : "";
        idleFpsField.text = settings.maxIdleFps > 0 ? String(settings.maxIdleFps) : "";
        customField.text = settings.customLines || "";
    }

    function statusText() {
        if (channel.settingsError === "frame limit out of range")
            return qsTr("Frame limits go from 1 to 1000; leave the field empty for the game's default.");
        if (channel.settingsError)
            return qsTr("Not saved – this line is no \"key = value\" or sets something the launcher manages itself: %1").arg(channel.settingsError);
        if ((channel.ignoredSettings || []).length > 0)
            return qsTr("Star Citizen ignored these settings at its last start (not whitelisted): %1").arg(channel.ignoredSettings.join(", "));
        if (channel.settingsChecked)
            return qsTr("Star Citizen accepted these settings at its last start.");
        if (!(settings.maxFps > 0 || settings.maxIdleFps > 0 || (settings.customLines || "") !== ""))
            return "";
        return qsTr("Takes effect at the next game start; afterwards the launcher shows whether Star Citizen accepted it.");
    }

    objectName: "gameSettingsDialog"
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
        color: Qt.rgba(0.04, 0.07, 0.13, 0.98)
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
                text: qsTr("Game settings")
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
            text: qsTr("Written into user.cfg of the channel. Other lines of the file stay as they are.")
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
            palette.window: Qt.rgba(0.04, 0.07, 0.13, 0.98)
            palette.button: Theme.inputBackground
            palette.buttonText: Theme.text
            palette.highlight: Theme.accent
            palette.highlightedText: "white"
            background: Field {}
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Text {
                text: qsTr("Frame limit (FPS)")
                color: Theme.text
                font.pixelSize: 12
            }

            TextField {
                id: maxFpsField

                Layout.preferredWidth: 120
                placeholderText: qsTr("game default")
                placeholderTextColor: Theme.disabled
                validator: IntValidator {
                    bottom: 1
                    top: 1000
                }
                color: Theme.text
                font.pixelSize: 12
                background: Field {}
            }

            Text {
                text: qsTr("Frame limit in menus (FPS)")
                color: Theme.text
                font.pixelSize: 12
            }

            TextField {
                id: idleFpsField

                Layout.preferredWidth: 120
                placeholderText: qsTr("game default")
                placeholderTextColor: Theme.disabled
                validator: IntValidator {
                    bottom: 1
                    top: 1000
                }
                color: Theme.text
                font.pixelSize: 12
                background: Field {}
            }
        }

        Text {
            text: qsTr("Own lines (\"key = value\", one per line)")
            color: Theme.text
            font.pixelSize: 12
            font.bold: true
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 110

            TextArea {
                id: customField

                objectName: "gameSettingsCustom"
                placeholderText: "r_DisplayInfo = 1"
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
            color: dialog.channel.settingsError || (dialog.channel.ignoredSettings || []).length > 0 ? Theme.warning : Theme.muted
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
                onClicked: dialog.saveRequested(dialog.channelName, parseInt(maxFpsField.text) || 0, parseInt(idleFpsField.text) || 0, customField.text)
            }
        }
    }
}
