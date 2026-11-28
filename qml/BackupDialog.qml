import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Backs up and restores keybindings, profile settings, characters and user.cfg per game channel, together with the
// launcher's settings; before a restore it shows which files differ from the current ones
Popup {
    id: dialog

    // Protocol::Backups: { busy, entries: [{ channel, name, bytes }], result, resultChannel,
    //                      comparison: { channel, name, busy, files: [{ path, change }], unchanged, hasSettings, programs, error } }
    property var backups: ({})
    // Channels with a game folder
    property var channels: []
    property bool gamePlaying: false

    signal backupRequested(string channel)
    signal compareRequested(string channel, string backup)
    signal restoreRequested(string channel, string backup, bool settings)

    readonly property bool busy: backups.busy === true
    readonly property var entries: backups.entries || []
    // The backup waiting for confirmation: { channel, name }
    property var pending: null
    // Whether the confirmed restore also writes back the launcher's settings
    property bool restoreSettings: false
    readonly property var comparison: backups.comparison || ({})
    // The comparison of the pending backup is there
    readonly property bool compared: pending !== null && comparison.channel === pending.channel && comparison.name === pending.name && comparison.busy !== true
    readonly property var changedFiles: compared ? (comparison.files || []) : []
    readonly property int shownFiles: 8
    // Commands the launcher's settings of the backup would newly start with the game
    readonly property var newPrograms: compared ? (comparison.programs || []) : []

    function confirmRestore(channel, name) {
        pending = {
            channel: channel,
            name: name
        };
        restoreSettings = false;
        compareRequested(channel, name);
    }

    function fileName(path) {
        return path === "launcher/settings.json" ? qsTr("Launcher settings (channels, options, programs)") : path;
    }

    function changeText(change) {
        return change === "missing" ? qsTr("missing now") : qsTr("changed");
    }

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
    height: Math.min(implicitHeight, parent.height - 40)
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
            text: dialog.channels.length === 0 ? qsTr("Choose a game folder first.") : qsTr("Keybindings, profile settings, saved characters and user.cfg of a channel, together with the launcher's settings. The newest 10 backups per channel are kept.")
        }

        ScrollView {
            // The confirmation takes the room of the list
            visible: dialog.pending === null
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
                                    onClicked: dialog.confirmRestore(row.modelData.channel, row.modelData.name)
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

        // Asks once more and lists what a restore overwrites; files not in the backup stay as they are
        ColumnLayout {
            Layout.fillWidth: true
            visible: dialog.pending !== null
            spacing: 6

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: dialog.pending ? qsTr("Restore %1 from %2? The current files are backed up first.").arg(dialog.pending.channel).arg(dialog.backupTime(dialog.pending.name)) : ""
                color: Theme.text
                font.pixelSize: 12
            }

            Text {
                Layout.fillWidth: true
                visible: dialog.pending !== null && !dialog.compared
                text: qsTr("Comparing the backup with the current files …")
                color: Theme.muted
                font.pixelSize: 11
            }

            Text {
                Layout.fillWidth: true
                visible: dialog.compared && (dialog.comparison.error || "") !== ""
                wrapMode: Text.Wrap
                text: dialog.resultText(dialog.comparison.error || "")
                color: Theme.warning
                font.pixelSize: 11
            }

            Text {
                Layout.fillWidth: true
                visible: dialog.compared && (dialog.comparison.error || "") === ""
                wrapMode: Text.Wrap
                text: dialog.changedFiles.length === 0 ? qsTr("All files in this backup are the same as the current ones.") : qsTr("%n file(s) differ from the current ones:", "", dialog.changedFiles.length)
                color: Theme.text
                font.pixelSize: 11
            }

            Repeater {
                model: dialog.changedFiles.slice(0, dialog.shownFiles)

                delegate: RowLayout {
                    id: fileRow

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    spacing: 10

                    Text {
                        Layout.fillWidth: true
                        text: dialog.fileName(fileRow.modelData.path)
                        elide: Text.ElideMiddle
                        color: Theme.text
                        font.pixelSize: 11
                    }

                    Text {
                        text: dialog.changeText(fileRow.modelData.change)
                        color: fileRow.modelData.change === "missing" ? Theme.warning : Theme.muted
                        font.pixelSize: 11
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                visible: dialog.changedFiles.length > dialog.shownFiles
                text: qsTr("and %n more", "", dialog.changedFiles.length - dialog.shownFiles)
                color: Theme.muted
                font.pixelSize: 11
            }

            Text {
                Layout.fillWidth: true
                visible: dialog.changedFiles.length > 0 && (dialog.comparison.unchanged || 0) > 0
                text: qsTr("%n file(s) are the same as now.", "", dialog.comparison.unchanged || 0)
                color: Theme.muted
                font.pixelSize: 11
            }

            // Programs from a backup run with the game; the backup may come from someone else
            Text {
                Layout.fillWidth: true
                visible: dialog.compared && dialog.comparison.hasSettings === true && dialog.newPrograms.length > 0
                wrapMode: Text.Wrap
                text: qsTr("These programs will start with the game:")
                color: Theme.warning
                font.pixelSize: 11
                font.bold: true
            }

            Repeater {
                model: dialog.compared && dialog.comparison.hasSettings === true ? dialog.newPrograms : []

                delegate: Text {
                    required property var modelData

                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    text: modelData
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    color: Theme.warning
                    font.pixelSize: 11
                    font.family: "monospace"
                }
            }

            Toggle {
                Layout.fillWidth: true
                visible: dialog.compared && dialog.comparison.hasSettings === true
                text: qsTr("Also restore the launcher's settings")
                description: qsTr("Game folders, languages, options and programs. Installed translations and the game settings of other channels stay as they are.")
                checked: dialog.restoreSettings
                onSwitched: on => dialog.restoreSettings = on
            }

            Text {
                Layout.fillWidth: true
                visible: dialog.compared && (dialog.comparison.error || "") === "" && dialog.comparison.hasSettings !== true
                wrapMode: Text.Wrap
                text: qsTr("This backup holds no launcher settings.")
                color: Theme.muted
                font.pixelSize: 11
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item {
                    Layout.fillWidth: true
                }

                DialogButton {
                    text: qsTr("Cancel")
                    onClicked: dialog.pending = null
                }

                DialogButton {
                    text: qsTr("Restore now")
                    highlighted: true
                    enabled: !dialog.busy && !dialog.gamePlaying && dialog.compared && (dialog.comparison.error || "") === ""
                    onClicked: {
                        dialog.restoreRequested(dialog.pending.channel, dialog.pending.name, dialog.restoreSettings && dialog.comparison.hasSettings === true);
                        dialog.pending = null;
                    }
                }
            }
        }
    }
}
