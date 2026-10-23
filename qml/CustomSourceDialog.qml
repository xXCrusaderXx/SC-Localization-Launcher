import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Lets the user enter an own global.ini source for one channel instead of a catalogue language
Popup {
    id: dialog

    property string channelName
    // Protocol::CustomSource of the channel: { url, folder }
    property var source: ({})
    // Localization folders to offer, e.g. from the catalogue
    property var folders: []

    signal saved(string url, string folder)
    signal removed

    // Same rules as the backend: HTTPS only, a plain folder name
    readonly property bool urlValid: /^https:\/\/\S+$/.test(urlField.text.trim())
    readonly property bool folderValid: /^[A-Za-z0-9_()-]+$/.test(folderBox.editText.trim())
    readonly property bool hasSource: (source.url || "") !== ""

    function openFor(channel, current, knownFolders) {
        channelName = channel;
        source = current || {};
        folders = knownFolders;
        urlField.text = source.url || "";
        const folder = source.folder || "german_(germany)";
        folderBox.currentIndex = folders.indexOf(folder);
        folderBox.editText = folder;
        open();
    }

    // Since Qt 6.8 the language picker's popup can be a window of its own that keeps the keyboard after it
    // closed; give the keyboard back to the main window, then to the address field
    onOpened: {
        urlField.Window.window.requestActivate();
        urlField.forceActiveFocus();
    }

    objectName: "customSourceDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 520
    modal: true
    // Lets the address field take the keyboard focus right away
    focus: true
    padding: 24

    background: Rectangle {
        radius: 14
        color: Qt.rgba(0.04, 0.07, 0.13, 0.98)
        border.color: Theme.border
    }

    component Field: Rectangle {
        implicitHeight: 36
        radius: 8
        color: Theme.inputBackground
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

        Text {
            Layout.fillWidth: true
            text: qsTr("Own source for %1").arg(dialog.channelName)
            color: Theme.text
            font.pixelSize: 18
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Link to a global.ini, e.g. a translation you are testing. Only use sources you trust: the file replaces the game texts.")
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
        }

        Text {
            text: qsTr("Address (HTTPS)")
            color: Theme.text
            font.pixelSize: 12
            font.bold: true
        }

        TextField {
            id: urlField

            objectName: "customSourceUrl"
            Layout.fillWidth: true
            placeholderText: "https://…/global.ini"
            placeholderTextColor: Theme.disabled
            color: Theme.text
            font.pixelSize: 12
            selectByMouse: true
            background: Field {
                border.color: urlField.text !== "" && !dialog.urlValid ? Theme.danger : urlField.activeFocus ? Theme.accent : Theme.border
            }
        }

        Text {
            text: qsTr("Language folder in the game")
            color: Theme.text
            font.pixelSize: 12
            font.bold: true
        }

        ComboBox {
            id: folderBox

            Layout.fillWidth: true
            editable: true
            model: dialog.folders
            font.pixelSize: 12
            // The built-in editor keeps editText and the list selection in sync; only its colours are adapted
            palette.text: Theme.text
            palette.base: Theme.inputBackground
            palette.window: Qt.rgba(0.04, 0.07, 0.13, 0.98)
            palette.button: Theme.inputBackground
            palette.buttonText: Theme.text
            palette.highlight: Theme.accent
            palette.highlightedText: "white"
            background: Field {
                border.color: !dialog.folderValid ? Theme.danger : Theme.border
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 10

            DialogButton {
                visible: dialog.hasSource
                text: qsTr("Remove")
                onClicked: {
                    dialog.removed();
                    dialog.close();
                }
            }

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                text: qsTr("Cancel")
                onClicked: dialog.close()
            }

            DialogButton {
                text: qsTr("Save and use")
                highlighted: true
                enabled: dialog.urlValid && dialog.folderValid
                onClicked: {
                    dialog.saved(urlField.text.trim(), folderBox.editText.trim());
                    dialog.close();
                }
            }
        }
    }
}
