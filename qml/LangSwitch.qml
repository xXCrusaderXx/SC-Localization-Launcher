import QtQuick

// Segmented control with a sliding highlight, e.g. ENG | DE | DE Voll
Rectangle {
    id: control

    property var options: [] // [{ key, label, enabled }]
    property string currentKey: ""
    property bool interactive: true

    signal picked(string key)

    readonly property int segmentWidth: 66
    readonly property int currentIndex: {
        for (let i = 0; i < options.length; ++i)
            if (options[i].key === currentKey)
                return i;
        return -1;
    }

    implicitWidth: options.length * segmentWidth + 6
    implicitHeight: 32
    radius: height / 2
    color: Theme.inputBackground
    border.color: Theme.border

    Rectangle {
        visible: control.currentIndex >= 0
        x: 3 + control.currentIndex * control.segmentWidth
        y: 3
        width: control.segmentWidth
        height: control.height - 6
        radius: height / 2
        opacity: control.interactive ? 1 : 0.5
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0
                color: Theme.accent
            }
            GradientStop {
                position: 1
                color: Theme.accent2
            }
        }

        Behavior on x {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }
    }

    Row {
        x: 3
        y: 3

        Repeater {
            model: control.options

            delegate: Item {
                id: segment

                required property var modelData
                required property int index
                readonly property bool selected: index === control.currentIndex
                readonly property bool available: control.interactive && modelData.enabled

                width: control.segmentWidth
                height: control.height - 6

                Text {
                    anchors.centerIn: parent
                    text: segment.modelData.label
                    font.pixelSize: 12
                    font.bold: segment.selected
                    color: segment.selected ? "white" : segment.available ? Theme.text : Theme.disabled
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: segment.available && !segment.selected
                    cursorShape: Qt.PointingHandCursor
                    onClicked: control.picked(segment.modelData.key)
                }
            }
        }
    }
}
