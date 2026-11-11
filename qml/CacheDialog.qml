import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Shows what takes the space on the game's drives, lists the shader caches of the game and the graphics drivers and
// deletes the chosen ones
Popup {
    id: dialog

    // Protocol::Caches: { busy, scanned, entries: [{ kind, path, bytes, result }], usage: [{ kind, name, path, bytes }] }
    property var caches: ({})
    // StarCitizen.exe runs; the backend refuses to delete then
    property bool gamePlaying: false

    signal clearRequested(var paths)

    readonly property var entries: caches.entries || []
    // Channels and Wine prefixes first, then the caches still there, then the free space of each drive
    readonly property var usage: {
        const list = (caches.usage || []).filter(u => u.kind !== "free");
        const cacheBytes = entries.filter(e => e.result !== "deleted").reduce((sum, e) => sum + e.bytes, 0);
        if (entries.length > 0)
            list.push({
                kind: "caches",
                name: "",
                path: "",
                bytes: cacheBytes
            });
        return list.concat((caches.usage || []).filter(u => u.kind === "free"));
    }
    readonly property bool busy: caches.busy === true
    // path -> chosen; the caches shared by all programs are left out unless picked
    property var chosen: ({})
    readonly property var chosenPaths: entries.filter(e => chosen[e.path] === true && e.result !== "deleted").map(e => e.path)
    readonly property real chosenBytes: entries.filter(e => chosenPaths.indexOf(e.path) >= 0).reduce((sum, e) => sum + e.bytes, 0)

    function formatSize(bytes) {
        if (bytes >= 1024 * 1024 * 1024)
            return qsTr("%1 GB").arg((bytes / (1024 * 1024 * 1024)).toLocaleString(Qt.locale(), "f", 1));
        if (bytes >= 1024 * 1024)
            return qsTr("%1 MB").arg(Math.round(bytes / (1024 * 1024)));
        return qsTr("%1 KB").arg(Math.max(1, Math.round(bytes / 1024)));
    }

    function kindText(kind) {
        switch (kind) {
        case "game":
            return qsTr("Star Citizen shader cache");
        case "dxvk":
            return qsTr("DXVK/VKD3D cache");
        case "prefix":
            return qsTr("Graphics driver cache in the Wine prefix");
        default:
            return qsTr("Graphics driver cache of all programs");
        }
    }

    function usageText(item) {
        switch (item.kind) {
        case "channel":
            return qsTr("Game files %1").arg(item.name);
        case "prefix":
            return qsTr("Wine prefix without the game files");
        case "caches":
            return qsTr("Shader caches listed below");
        default:
            return qsTr("Free on this drive");
        }
    }

    function resultText(result) {
        switch (result) {
        case "deleted":
            return qsTr("Deleted");
        case "notFound":
            return qsTr("Not found");
        case "failed":
            return qsTr("Could not be deleted");
        case "gameRunning":
            return qsTr("Star Citizen is running");
        default:
            return "";
        }
    }

    function toggle(path, on) {
        const next = Object.assign({}, chosen);
        next[path] = on;
        chosen = next;
    }

    objectName: "cacheDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 640
    height: Math.min(implicitHeight, 600)
    modal: true
    padding: 24

    // A new scan starts with the game's own caches chosen
    onEntriesChanged: {
        const next = {};
        for (const entry of entries)
            next[entry.path] = entry.path in chosen ? chosen[entry.path] : entry.kind !== "user";
        chosen = next;
    }

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
                text: qsTr("Disk space and shader caches")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
            }

            IconButton {
                glyph: "✕"
                onClicked: dialog.close()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            visible: dialog.usage.length > 0

            Repeater {
                model: dialog.usage

                delegate: RowLayout {
                    id: usageRow

                    required property var modelData

                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: dialog.usageText(usageRow.modelData)
                        color: usageRow.modelData.kind === "free" ? Theme.muted : Theme.text
                        font.pixelSize: 12
                    }

                    Text {
                        Layout.fillWidth: true
                        text: Theme.shownPath(usageRow.modelData.path)
                        elide: Text.ElideMiddle
                        color: Theme.muted
                        font.pixelSize: 10
                    }

                    Text {
                        text: dialog.formatSize(usageRow.modelData.bytes)
                        color: usageRow.modelData.kind === "free" ? Theme.muted : Theme.text
                        font.pixelSize: 12
                        font.bold: usageRow.modelData.kind !== "free"
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
            visible: dialog.usage.length > 0
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: dialog.gamePlaying ? Theme.warning : Theme.muted
            font.pixelSize: 12
            text: {
                if (dialog.gamePlaying)
                    return qsTr("Close Star Citizen first; the game uses these caches while it runs.");
                if (dialog.busy && !dialog.caches.scanned)
                    return qsTr("Looking for caches …");
                if (dialog.entries.length === 0)
                    return qsTr("No caches found for the chosen game folders.");
                return qsTr("Helps against stutter or graphics errors after a game or driver update. The caches are rebuilt on the next start, which then takes longer. Graphics settings are kept.");
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: list.implicitHeight
            visible: dialog.entries.length > 0
            clip: true

            ColumnLayout {
                id: list

                width: dialog.availableWidth
                spacing: 6

                Repeater {
                    model: dialog.entries

                    delegate: RowLayout {
                        id: row

                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 10

                        CheckBox {
                            checked: dialog.chosen[row.modelData.path] === true
                            enabled: !dialog.busy && row.modelData.result !== "deleted"
                            onToggled: dialog.toggle(row.modelData.path, checked)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: dialog.kindText(row.modelData.kind)
                                color: Theme.text
                                font.pixelSize: 12
                            }

                            Text {
                                Layout.fillWidth: true
                                text: Theme.shownPath(row.modelData.path)
                                elide: Text.ElideMiddle
                                color: Theme.muted
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: dialog.resultText(row.modelData.result) || dialog.formatSize(row.modelData.bytes)
                            color: row.modelData.result === "deleted" ? Theme.success : row.modelData.result === "" || row.modelData.result === "none" ? Theme.muted : Theme.danger
                            font.pixelSize: 11
                        }
                    }
                }
            }
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
                text: dialog.chosenPaths.length > 0 ? qsTr("Delete %1").arg(dialog.formatSize(dialog.chosenBytes)) : qsTr("Delete")
                highlighted: true
                enabled: !dialog.busy && !dialog.gamePlaying && dialog.chosenPaths.length > 0
                onClicked: dialog.clearRequested(dialog.chosenPaths)
            }
        }
    }
}
