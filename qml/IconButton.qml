import QtQuick

// Small round button showing a text glyph
Rectangle {
    id: control

    property string glyph
    property color hoverColor: Theme.hover

    signal clicked

    width: 30
    height: 30
    radius: 8
    color: mouse.containsMouse ? hoverColor : "transparent"

    Behavior on color {
        ColorAnimation {
            duration: 120
        }
    }

    Text {
        anchors.centerIn: parent
        text: control.glyph
        color: Theme.text
        font.pixelSize: 15
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }
}
