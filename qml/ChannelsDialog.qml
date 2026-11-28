import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Lists the channel folders with their size, creates a test channel as a copy of LIVE and removes a channel
// after a confirmation
Popup {
    id: dialog

    // Protocol::ChannelFolders: { busy, scanned, folders: [{ name, path, exists, bytes, canCreate, canRemove }], copyBytes,
    // reflink, freeBytes, operation, channel, progress, result, resultChannel }
    property var channelFolders: ({})
    // Star Citizen or the RSI Launcher runs; the backend refuses to copy or delete then
    property bool gameOpen: false

    signal createRequested(string channel)
    signal cancelRequested
    signal removeRequested(string channel)
    signal startRequested

    readonly property var folders: channelFolders.folders || []
    readonly property bool busy: channelFolders.busy === true
    readonly property string operation: channelFolders.operation || ""
    readonly property real copyBytes: channelFolders.copyBytes || 0
    readonly property bool reflink: channelFolders.reflink === true
    readonly property real freeBytes: channelFolders.freeBytes === undefined ? -1 : channelFolders.freeBytes
    // ChannelFolderManager::SPACE_MARGIN
    readonly property real spaceMargin: 1024 * 1024 * 1024
    readonly property bool enoughSpace: freeBytes >= 0 && freeBytes >= (reflink ? 0 : copyBytes) + spaceMargin
    readonly property var live: folders.find(f => f.name === "LIVE") || ({})
    // The action waiting for confirmation: { action: "copy" or "remove", channel }
    property var pending: null
    readonly property var pendingFolder: pending ? folders.find(f => f.name === pending.channel) || ({}) : ({})

    function formatSize(bytes) {
        if (bytes >= 1024 * 1024 * 1024)
            return qsTr("%1 GB").arg((bytes / (1024 * 1024 * 1024)).toLocaleString(Qt.locale(), "f", 1));
        if (bytes >= 1024 * 1024)
            return qsTr("%1 MB").arg(Math.round(bytes / (1024 * 1024)));
        return qsTr("%1 KB").arg(Math.max(1, Math.round(bytes / 1024)));
    }

    function resultText(result, channel) {
        switch (result) {
        case "":
            return "";
        case "created":
            return qsTr("%1 was created. Open the RSI Launcher, choose %1 and let it verify the files; it downloads only what differs from LIVE.").arg(channel);
        case "removed":
            return qsTr("%1 was removed.").arg(channel);
        case "cancelled":
            return qsTr("The copy was cancelled; nothing of it was kept.");
        case "gameRunning":
            return qsTr("Close Star Citizen and the RSI Launcher first.");
        case "updateRunning":
            return qsTr("Wait until the translations are updated.");
        case "exists":
            return qsTr("The folder of %1 is not empty; nothing was copied.").arg(channel);
        case "noSpace":
            return qsTr("Not enough free space for the copy.");
        case "live":
            return qsTr("LIVE cannot be removed here; use the RSI Launcher for that.");
        case "notFound":
            return qsTr("The folder of %1 was not found.").arg(channel);
        case "notAllowed":
            return qsTr("Only a channel folder directly inside the StarCitizen folder can be created or removed.");
        default:
            return qsTr("Failed; the log has the details.");
        }
    }

    objectName: "channelsDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 640
    height: Math.min(implicitHeight, parent.height - 32)
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
                text: qsTr("Channels")
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
            color: dialog.gameOpen ? Theme.warning : Theme.muted
            font.pixelSize: 12
            text: {
                if (dialog.gameOpen)
                    return qsTr("Close Star Citizen and the RSI Launcher first.");
                if (dialog.busy && !dialog.channelFolders.scanned)
                    return qsTr("Measuring the channel folders …");
                if (dialog.live.path && !/[\/\\]StarCitizen[\/\\]LIVE[\/\\]?$/.test(dialog.live.path))
                    return qsTr("To create a channel from LIVE, LIVE's folder must be the folder LIVE inside the StarCitizen folder of the RSI Launcher.");
                return qsTr("A new PTU, EPTU or Tech-Preview channel can start as a copy of LIVE, so the RSI Launcher only downloads what differs. A channel you no longer play can be removed.");
            }
        }

        // Scrolls when the window is small, so the confirmation below stays in view
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
                    model: dialog.folders

                    delegate: RowLayout {
                        id: row

                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 10

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: row.modelData.name
                                color: Theme.text
                                font.pixelSize: 12
                                font.bold: true
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: Theme.shownPath(row.modelData.path)
                                elide: Text.ElideMiddle
                                color: Theme.muted
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: row.modelData.exists ? dialog.formatSize(row.modelData.bytes) : qsTr("Not installed")
                            color: row.modelData.exists ? Theme.text : Theme.muted
                            font.pixelSize: 12
                            font.bold: row.modelData.exists
                        }

                        DialogButton {
                            visible: row.modelData.canCreate
                            text: qsTr("Create from LIVE")
                            enabled: !dialog.busy && !dialog.gameOpen
                            onClicked: dialog.pending = {
                                action: "copy",
                                channel: row.modelData.name
                            }
                        }

                        DialogButton {
                            visible: row.modelData.canRemove
                            text: qsTr("Remove …")
                            enabled: !dialog.busy && !dialog.gameOpen
                            onClicked: dialog.pending = {
                                action: "remove",
                                channel: row.modelData.name
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
            visible: dialog.pending !== null || dialog.operation !== "" || (dialog.channelFolders.result || "") !== ""
        }

        // Confirmation of a copy or removal
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: dialog.pending !== null && dialog.operation === ""

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.text
                font.pixelSize: 12
                text: {
                    if (!dialog.pending)
                        return "";
                    const path = Theme.shownPath(dialog.pendingFolder.path || "");
                    if (dialog.pending.action === "remove")
                        return qsTr("Delete %1 (%2) in %3? The game files of this channel are deleted, and the launcher forgets its settings except the language. The RSI Launcher can install it again.").arg(dialog.pending.channel).arg(dialog.formatSize(dialog.pendingFolder.bytes || 0)).arg(path);
                    if (dialog.reflink)
                        return qsTr("LIVE (%1) is copied to %2. This file system shares the data of the copy with LIVE, so it takes almost no extra space at first; the saving can shrink after game patches.").arg(dialog.formatSize(dialog.copyBytes)).arg(path);
                    return qsTr("LIVE (%1) is copied to %2. This file system cannot share the data with LIVE, so the copy needs %1; %3 are free.").arg(dialog.formatSize(dialog.copyBytes)).arg(path).arg(dialog.formatSize(Math.max(0, dialog.freeBytes)));
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: dialog.pending !== null && dialog.pending.action === "copy" && !dialog.enoughSpace
                color: Theme.danger
                font.pixelSize: 12
                text: qsTr("Not enough free space: the copy needs %1 and leaves 1 GB free for the patch.").arg(dialog.formatSize(dialog.reflink ? 0 : dialog.copyBytes))
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
                    text: dialog.pending && dialog.pending.action === "remove" ? qsTr("Delete %1").arg(dialog.formatSize(dialog.pendingFolder.bytes || 0)) : qsTr("Copy LIVE")
                    highlighted: true
                    enabled: !dialog.busy && !dialog.gameOpen && (dialog.pending === null || dialog.pending.action === "remove" || dialog.enoughSpace)
                    onClicked: {
                        if (dialog.pending.action === "remove")
                            dialog.removeRequested(dialog.pending.channel);
                        else
                            dialog.createRequested(dialog.pending.channel);
                        dialog.pending = null;
                    }
                }
            }
        }

        // A copy or removal in progress
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: dialog.operation !== ""

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.text
                font.pixelSize: 12
                text: dialog.operation === "copy" ? qsTr("Copying LIVE to %1 … %2 %").arg(dialog.channelFolders.channel).arg(Math.round((dialog.channelFolders.progress || 0) * 100)) : qsTr("Deleting %1 …").arg(dialog.channelFolders.channel)
            }

            ProgressBar {
                Layout.fillWidth: true
                visible: dialog.operation === "copy"
                value: dialog.channelFolders.progress || 0
            }

            RowLayout {
                Layout.fillWidth: true
                visible: dialog.operation === "copy"

                Item {
                    Layout.fillWidth: true
                }

                DialogButton {
                    text: qsTr("Cancel copy")
                    onClicked: dialog.cancelRequested()
                }
            }
        }

        // The result of the last copy or removal
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: dialog.operation === "" && dialog.pending === null && (dialog.channelFolders.result || "") !== ""

            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 12
                color: dialog.channelFolders.result === "created" || dialog.channelFolders.result === "removed" ? Theme.success : Theme.danger
                text: dialog.resultText(dialog.channelFolders.result || "", dialog.channelFolders.resultChannel || "")
            }

            DialogButton {
                visible: dialog.channelFolders.result === "created"
                text: qsTr("Start RSI Launcher")
                enabled: !dialog.gameOpen
                onClicked: dialog.startRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                text: qsTr("Close")
                onClicked: dialog.close()
            }
        }
    }
}
