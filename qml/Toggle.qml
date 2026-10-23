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
        // From the unwrapped texts, not from textColumn: its width follows this item's width
        implicitWidth: textColumn.x + Math.max(label.implicitWidth + (badgeBox.visible ? badgeBox.width + 8 : 0), descriptionText.visible ? descriptionText.implicitWidth : 0)
        implicitHeight: Math.max(textColumn.implicitHeight, control.indicator.implicitHeight)

        // Texts wrap when the switch gets less than its implicit width (e.g. in a layout, or with wide fonts)
        Column {
            id: textColumn

            x: control.indicator.width + 12
            width: parent.width - x
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Row {
                width: parent.width
                spacing: 8

                Text {
                    id: label

                    width: Math.min(implicitWidth, parent.width - (badgeBox.visible ? badgeBox.width + parent.spacing : 0))
                    text: control.text
                    color: Theme.text
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    id: badgeBox

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
                        text: qsTr("SOON")
                        color: Theme.muted
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 1
                    }
                }
            }

            Text {
                id: descriptionText

                width: parent.width
                visible: control.description !== ""
                text: control.description
                wrapMode: Text.WordWrap
                color: Theme.muted
                font.pixelSize: 11
            }
        }
    }
}
