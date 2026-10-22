import QtQuick
import QtQuick.Controls.Basic

// Switch in the launcher style, optionally with a description and a "coming soon" badge.
// It only displays `checked` and reports clicks via switched(), the new value comes back with the next backend state.
Switch {
    id: control

    property string description: ""
    property bool comingSoon: false

    signal switched(bool on)

    checkable: false
    onClicked: switched(!checked)
    enabled: !comingSoon
    padding: 0
    opacity: enabled ? 1 : 0.55

    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 22
        y: (control.height - height) / 2
        radius: 11
        color: control.checked ? Theme.accent : Theme.inputBackground
        border.color: Theme.border

        Behavior on color {
            ColorAnimation {
                duration: 150
            }
        }

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: 16
            height: 16
            radius: 8
            color: "white"

            Behavior on x {
                NumberAnimation {
                    duration: 150
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    contentItem: Item {
        implicitWidth: textColumn.implicitWidth + control.indicator.width + 12
        implicitHeight: Math.max(textColumn.implicitHeight, control.indicator.implicitHeight)

        Column {
            id: textColumn

            x: control.indicator.width + 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Row {
                spacing: 8

                Text {
                    text: control.text
                    color: Theme.text
                    font.pixelSize: 13
                }

                Rectangle {
                    visible: control.comingSoon
                    anchors.verticalCenter: parent.verticalCenter
                    width: badge.implicitWidth + 12
                    height: 16
                    radius: 8
                    color: "transparent"
                    border.color: Theme.muted

                    Text {
                        id: badge
                        anchors.centerIn: parent
                        text: "BALD"
                        color: Theme.muted
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 1
                    }
                }
            }

            Text {
                visible: control.description !== ""
                text: control.description
                color: Theme.muted
                font.pixelSize: 11
            }
        }
    }
}
