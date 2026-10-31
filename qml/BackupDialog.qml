import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Backs up and restores keybindings, profile settings, characters and user.cfg per game channel
Popup {
    id: dialog

    // Protocol::Backups: { busy, entries: [{ channel, name, bytes }], result, resultChannel }
    property var backups: ({})
    // Channels with a game folder
    property var channels: []
    property bool gamePlaying: false

    signal backupRequested(string channel)
    signal restoreRequested(string channel, string backup)

    readonly property bool busy: backups.busy === true
    readonly property var entries: backups.entries || []
    // The backup waiting for confirmation: { channel, name }
    property var pending: null

    // "LIVE-20261024-203000-123.zip" -> date and time in the user's format
    function backupTime(name) {
        const match = /-(\d{4})(\d{2})(\d{2})-(\d{2})(\d{2})(\d{2})/.exec(name);
        if (!match)
            return name;
        const date = new Date(match[1], match[2] - 1, match[3], match[4], match[5], match[6]);
        return date.toLocaleString(Qt.locale(), Locale.ShortFormat);
    }

    function resultText(result) {
        switch (result) {
        case "":
            return "";
        case "created":
            return qsTr("Backup saved.");
        case "restored":
            return qsTr("Backup restored. The files it replaced were backed up first.");
        case "nothing to back up":
            return qsTr("This channel has no keybindings, characters or user.cfg yet.");
        case "Star Citizen is running":
            return qsTr("Close Star Citizen first; the game rewrites these files.");
        default:
            return qsTr("Failed: %1").arg(result);
        }
    }

    objectName: "backupDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 600
    height: Math.min(implicitHeight, 480)
    modal: true
    padding: 24
    onClosed: pending = null

    background: Rectangle {
        radius: 14
        color: Theme.popup
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 12

        Shortcut {
            sequence: "Esc"
            enabled: dialog.opened
            onActivated: dialog.close()
        }

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("Profile backups")
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
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
            text: dialog.channels.length === 0 ? qsTr("Choose a game folder first.") : qsTr("Keybindings, profile settings, saved characters and user.cfg of a channel. The newest 10 backups per channel are kept.")
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: list.implicitHeight
            clip: true

            ColumnLayout {
                id: list

                width: dialog.availableWidth
                spacing: 6

                Repeater {
                    model: dialog.channels

                    delegate: ColumnLayout {
                        id: group

                        required property var modelData
                        readonly property var channelBackups: dialog.entries.filter(e => e.channel === group.modelData)

                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true
                                text: group.modelData
                                color: Theme.text
                                font.pixelSize: 13
                                font.bold: true
                            }

                            DialogButton {
                                text: qsTr("Back up %1").arg(group.modelData)
                                enabled: !dialog.busy
                                onClicked: dialog.backupRequested(group.modelData)
                            }
                        }

                        Text {
                            visible: group.channelBackups.length === 0
                            text: qsTr("No backups yet")
                            color: Theme.muted
                            font.pixelSize: 11
                        }

                        Repeater {
                            model: group.channelBackups

                            delegate: RowLayout {
                                id: row

                                required property var modelData

                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    Layout.fillWidth: true
                                    text: dialog.backupTime(row.modelData.name)
                                    color: Theme.text
                                    font.pixelSize: 12
                                }

                                Text {
                                    text: qsTr("%1 KB").arg(Math.max(1, Math.round(row.modelData.bytes / 1024)))
                                    color: Theme.muted
                                    font.pixelSize: 11
                                }

                                DialogButton {
                                    text: qsTr("Restore")
                                    enabled: !dialog.busy && !dialog.gamePlaying
                                    onClicked: dialog.pending = {
                                        channel: row.modelData.channel,
                                        name: row.modelData.name
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            text: dialog.gamePlaying ? qsTr("Close Star Citizen first; the game rewrites these files.") : dialog.resultText(dialog.backups.result || "")
            color: dialog.gamePlaying || ["", "created", "restored"].indexOf(dialog.backups.result || "") < 0 ? Theme.warning : Theme.success
            font.pixelSize: 11
        }

        // Asks once more: a restore overwrites the keybindings and settings of the channel
        RowLayout {
            Layout.fillWidth: true
            visible: dialog.pending !== null
            spacing: 10

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: dialog.pending ? qsTr("Restore %1 from %2? The current files are backed up first.").arg(dialog.pending.channel).arg(dialog.backupTime(dialog.pending.name)) : ""
                color: Theme.text
                font.pixelSize: 12
            }

            DialogButton {
                text: qsTr("Cancel")
                onClicked: dialog.pending = null
            }

            DialogButton {
                text: qsTr("Restore now")
                highlighted: true
                enabled: !dialog.busy && !dialog.gamePlaying
                onClicked: {
                    dialog.restoreRequested(dialog.pending.channel, dialog.pending.name);
                    dialog.pending = null;
                }
            }
        }
    }
}
