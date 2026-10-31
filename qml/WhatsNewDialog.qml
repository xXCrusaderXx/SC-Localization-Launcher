import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Shows the release notes once after the launcher was updated
Popup {
    id: dialog

    // Protocol::WhatsNew: { version, notes }
    property var whatsNew: ({})

    objectName: "whatsNewDialog"
    // Centered in the window; Overlay attached properties are not available with Qt 6.4
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 560
    height: Math.min(implicitHeight, 460)
    modal: true
    padding: 24

    background: Rectangle {
        radius: 14
        color: Theme.popup
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
                text: qsTr("What's new in version %1").arg(dialog.whatsNew.version || "")
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
            }

            IconButton {
                glyph: "✕"
                onClicked: dialog.close()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: notes.implicitHeight
            clip: true

            Text {
                id: notes

                width: dialog.availableWidth
                text: dialog.whatsNew.notes || ""
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

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                text: qsTr("OK")
                highlighted: true
                onClicked: dialog.close()
            }
        }
    }
}
