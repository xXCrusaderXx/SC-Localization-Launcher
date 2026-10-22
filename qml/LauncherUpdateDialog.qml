import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Shows whether a newer launcher version is published on GitHub, with its release notes.
// Installing happens on the release page for now.
Popup {
    id: dialog

    // Protocol::SelfUpdate: { status, currentVersion, latestVersion, notes, url, error }
    property var selfUpdate: ({})

    signal checkRequested

    readonly property string status: selfUpdate.status || "idle"

    // Button in the launcher style; `highlighted` uses the accent gradient
    component DialogButton: Button {
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

    objectName: "launcherUpdateDialog"
    // Centered in the window; Overlay attached properties are not available with Qt 6.4
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 520
    height: Math.min(implicitHeight, 440)
    modal: true
    padding: 24

    background: Rectangle {
        radius: 14
        color: Qt.rgba(0.04, 0.07, 0.13, 0.98)
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 12

        // A modal popup blocks the window's shortcuts, so it needs its own
        Shortcut {
            sequence: "Esc"
            enabled: dialog.opened
            onActivated: dialog.close()
        }

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("Launcher update")
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
            color: dialog.status === "error" ? Theme.danger : Theme.text
            font.pixelSize: 13
            text: {
                switch (dialog.status) {
                case "checking":
                    return qsTr("Checking for a new version …");
                case "available":
                    return qsTr("Version %1 is available. You are using %2.").arg(dialog.selfUpdate.latestVersion).arg(dialog.selfUpdate.currentVersion);
                case "upToDate":
                    return qsTr("You are using the latest version (%1).").arg(dialog.selfUpdate.currentVersion);
                case "error":
                    return dialog.selfUpdate.error === "rate limit" ? qsTr("GitHub is limiting requests right now. Please try again in an hour.") : qsTr("The check failed: %1").arg(dialog.selfUpdate.error);
                default:
                    return qsTr("Version %1").arg(dialog.selfUpdate.currentVersion);
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: notes.implicitHeight
            visible: dialog.status === "available" && (dialog.selfUpdate.notes || "") !== ""
            clip: true

            Text {
                id: notes

                width: dialog.availableWidth
                text: dialog.selfUpdate.notes || ""
                textFormat: Text.MarkdownText
                wrapMode: Text.Wrap
                color: Theme.muted
                linkColor: Theme.accent
                font.pixelSize: 12
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                text: qsTr("Check again")
                enabled: dialog.status !== "checking"
                onClicked: dialog.checkRequested()
            }

            DialogButton {
                visible: dialog.status === "available"
                text: qsTr("Open download page")
                highlighted: true
                onClicked: Qt.openUrlExternally(dialog.selfUpdate.url)
            }
        }
    }
}
