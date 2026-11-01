import QtQuick
import QtQuick.Layouts

// One install path in the settings: label, path, open and change actions
Rectangle {
    id: row

    property string label
    property string path: ""
    property bool locked: false
    // False for paths the user cannot change, e.g. the log folder
    property bool editable: true

    signal changeRequested

    Layout.fillWidth: true
    implicitHeight: 50
    radius: 10
    color: Theme.panel
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 8
        spacing: 10

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1

            Text {
                text: row.label
                color: Theme.text
                font.pixelSize: 12
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: row.path !== "" ? Theme.shownPath(row.path) : qsTr("Not set")
                color: row.path !== "" ? Theme.muted : Theme.disabled
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }
        }

        IconButton {
            visible: row.path !== ""
            glyph: "↗"
            onClicked: Qt.openUrlExternally(backend.fromLocalFile(row.path))
        }

        IconButton {
            visible: row.editable
            glyph: "✎"
            onClicked: if (!row.locked)
                row.changeRequested()
        }
    }
}
