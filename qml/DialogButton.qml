import QtQuick
import QtQuick.Controls.Basic

// Button in the launcher style for dialogs; `highlighted` uses the accent gradient
Button {
    id: button

    implicitHeight: 34
    leftPadding: 16
    rightPadding: 16
    opacity: enabled ? 1 : 0.5

    contentItem: Text {
        text: button.text
        color: button.highlighted ? "white" : Theme.text
        font.pixelSize: 12
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: height / 2
        color: button.highlighted ? Theme.accent : button.hovered ? Theme.hover : "transparent"
        border.color: button.highlighted ? "transparent" : Theme.border
        gradient: button.highlighted ? accentGradient : null

        Gradient {
            id: accentGradient
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
    }
}
