import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Chooses the dashboard's tiles and their order; sent as a whole on save
Popup {
    id: dialog

    // State.dashboardTiles
    property var tiles: []
    // Every tile id, from Dashboard.allTiles
    property var allTiles: []
    // Tile id -> label, from Dashboard.labelOf
    property var labelOf: id => id
    // The edited copy
    property var chosen: []
    readonly property int maxTiles: 8

    signal saveRequested(var tiles)

    function move(index, by) {
        let list = chosen.slice();
        const [tile] = list.splice(index, 1);
        list.splice(index + by, 0, tile);
        chosen = list;
    }

    objectName: "dashboardDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 460
    height: Math.min(implicitHeight, parent.height - 40)
    modal: true
    focus: true
    padding: 24
    onOpened: chosen = tiles.slice()

    background: Rectangle {
        radius: 14
        color: Theme.popup
        border.color: Theme.border
    }

    component TileRow: Rectangle {
        default property alias content: rowLayout.data

        Layout.fillWidth: true
        implicitHeight: 34
        radius: 8
        color: Theme.panel
        border.color: Theme.border

        RowLayout {
            id: rowLayout

            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 4
            spacing: 2
        }
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
                text: qsTr("Customize the dashboard")
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
            text: qsTr("Up to %1 tiles, shown in this order. The main window shows as many rows as fit.").arg(dialog.maxTiles)
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(listColumn.implicitHeight, 300)
            contentWidth: availableWidth

            ColumnLayout {
                id: listColumn

                width: parent.width
                spacing: 6

                Repeater {
                    model: dialog.chosen

                    TileRow {
                        required property string modelData
                        required property int index

                        Text {
                            Layout.fillWidth: true
                            text: dialog.labelOf(modelData)
                            color: Theme.text
                            font.pixelSize: 12
                        }

                        IconButton {
                            glyph: "↑"
                            opacity: index > 0 ? 1 : 0.3
                            onClicked: if (index > 0) dialog.move(index, -1)
                        }

                        IconButton {
                            glyph: "↓"
                            opacity: index < dialog.chosen.length - 1 ? 1 : 0.3
                            onClicked: if (index < dialog.chosen.length - 1) dialog.move(index, 1)
                        }

                        IconButton {
                            glyph: "✕"
                            onClicked: dialog.chosen = dialog.chosen.filter((_, i) => i !== index)
                        }
                    }
                }

                Text {
                    Layout.topMargin: 6
                    visible: dialog.allTiles.some(t => dialog.chosen.indexOf(t) < 0)
                    text: qsTr("More tiles")
                    color: Theme.muted
                    font.pixelSize: 11
                    font.bold: true
                }

                Repeater {
                    model: dialog.allTiles.filter(t => dialog.chosen.indexOf(t) < 0)

                    TileRow {
                        required property string modelData

                        opacity: dialog.chosen.length < dialog.maxTiles ? 1 : 0.5

                        Text {
                            Layout.fillWidth: true
                            text: dialog.labelOf(modelData)
                            color: Theme.muted
                            font.pixelSize: 12
                        }

                        IconButton {
                            objectName: "addTile_" + modelData
                            glyph: "+"
                            onClicked: if (dialog.chosen.length < dialog.maxTiles) dialog.chosen = dialog.chosen.concat([modelData])
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
                text: qsTr("Save")
                highlighted: true
                onClicked: {
                    dialog.saveRequested(dialog.chosen);
                    dialog.close();
                }
            }
        }
    }
}
