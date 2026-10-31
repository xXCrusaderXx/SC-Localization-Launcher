import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

// Programs started together with the game (Protocol::Program); edited here and sent as a whole on save
Popup {
    id: dialog

    // State.programs and State.failedPrograms
    property var programs: []
    property var failedPrograms: []
    // The edited copy; replaced as a whole when a row is added or removed
    property var rows: []

    signal saveRequested(var programs)

    function load() {
        rows = programs.map(p => ({
                    "command": p.command,
                    "enabled": p.enabled,
                    "stopWithGame": p.stopWithGame
                }));
    }

    function addRow(command) {
        rows = rows.concat([{
                    "command": command,
                    "enabled": true,
                    "stopWithGame": false
                }]);
    }

    function setRow(index, field, value) {
        let changed = rows.slice();
        changed[index] = Object.assign({}, changed[index], {
            [field]: value
        });
        rows = changed;
    }

    objectName: "programsDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 620
    height: Math.min(implicitHeight, 520)
    modal: true
    // Lets the command fields take the keyboard focus
    focus: true
    padding: 24
    onOpened: load()

    component Field: Rectangle {
        implicitHeight: 36
        radius: 8
        color: Theme.inputBackground
        border.color: Theme.border
    }

    background: Rectangle {
        radius: 14
        color: Theme.popup
        border.color: Theme.border
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
                text: qsTr("Programs with the game")
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
            text: qsTr("Started when Star Citizen starts, also when it was started without this launcher. The launcher has to be open, e.g. in the system tray. Quotes work as in a shell; for variables or pipes, start a script or sh -c '…'.")
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(rowColumn.implicitHeight, 240)
            visible: dialog.rows.length > 0
            contentWidth: availableWidth

            ColumnLayout {
                id: rowColumn

                width: parent.width
                spacing: 8

                Repeater {
                    model: dialog.rows

                    ColumnLayout {
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TextField {
                                Layout.fillWidth: true
                                objectName: "programCommand" + index
                                text: modelData.command
                                placeholderText: "/usr/bin/opentrack"
                                placeholderTextColor: Theme.disabled
                                color: Theme.text
                                font.family: "monospace"
                                font.pixelSize: 12
                                selectByMouse: true
                                background: Field {}
                                // Kept in the row without replacing rows, so the field keeps its focus while typing
                                onTextEdited: dialog.rows[index].command = text
                            }

                            IconButton {
                                glyph: "−"
                                onClicked: dialog.rows = dialog.rows.filter((_, i) => i !== index)
                            }
                        }

                        RowLayout {
                            spacing: 20

                            Toggle {
                                text: qsTr("Active")
                                checked: modelData.enabled
                                onSwitched: on => dialog.setRow(index, "enabled", on)
                            }

                            Toggle {
                                text: qsTr("End it when the game quits")
                                checked: modelData.stopWithGame
                                onSwitched: on => dialog.setRow(index, "stopWithGame", on)
                            }
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.rows.length === 0
            text: qsTr("No programs yet.")
            color: Theme.muted
            font.pixelSize: 12
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.failedPrograms.length > 0
            text: qsTr("Could not be started at the last game start: %1").arg(dialog.failedPrograms.join(", "))
            wrapMode: Text.Wrap
            color: Theme.warning
            font.pixelSize: 11
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            DialogButton {
                text: qsTr("Choose program …")
                onClicked: programFile.open()
            }

            DialogButton {
                text: qsTr("Add command")
                onClicked: dialog.addRow("")
            }

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
                onClicked: dialog.saveRequested(dialog.rows)
            }
        }
    }

    FileDialog {
        id: programFile

        title: qsTr("Program to start with the game")
        // Quoted, so a path with spaces stays one word
        onAccepted: dialog.addRow("\"" + decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, "")) + "\"")
    }
}
