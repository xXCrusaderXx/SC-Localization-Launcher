import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Shows the read-only checks of the installation and saves them as a ZIP report for a bug report
Popup {
    id: dialog

    // Protocol::Diagnostics: { busy, checks: [{ id, channel, status, detail }], reportPath, error }
    property var diagnostics: ({})

    signal saveRequested
    signal showReportRequested(string path)

    readonly property var checks: diagnostics.checks || []
    readonly property bool busy: diagnostics.busy === true

    function checkText(check) {
        const ok = check.status === "ok";
        switch (check.id) {
        case "system":
            return qsTr("System");
        case "launcher":
            return qsTr("Launcher version");
        case "catalogue":
            return ok ? qsTr("Language list loaded") : qsTr("Language list not loaded – offline?");
        case "rsiLauncher":
            if (check.status === "warning")
                return qsTr("No RSI Launcher folder set");
            return ok ? qsTr("RSI Launcher folder") : qsTr("RSI Launcher folder is missing");
        case "winePrefix":
            return ok ? qsTr("Wine prefix") : qsTr("No Wine prefix found above the game folders");
        case "starter":
            if (check.status === "info")
                return qsTr("Game start: looked up outside the Flatpak when starting");
            return ok ? qsTr("Game start") : qsTr("Neither the LUG Helper's sc-launch.sh nor Lutris found");
        case "folder":
            return ok ? qsTr("Game folder") : qsTr("Game folder is missing");
        case "writable":
            return ok ? qsTr("Game folder is writable") : qsTr("No write permission for the game folder");
        case "translation":
            if (check.status === "info")
                return qsTr("No translation chosen (English)");
            if (check.status === "warning")
                return qsTr("Translation installed, but not current");
            return ok ? qsTr("Translation installed") : qsTr("Translation file is missing or empty");
        case "userCfg":
            return ok ? qsTr("user.cfg selects the translation") : qsTr("user.cfg does not select the translation");
        case "diskSpace":
            return ok ? qsTr("Free disk space") : qsTr("Little free disk space");
        default:
            return check.id;
        }
    }

    function statusColor(status) {
        switch (status) {
        case "ok":
            return Theme.success;
        case "warning":
            return Theme.warning;
        case "error":
            return Theme.danger;
        default:
            return Theme.disabled;
        }
    }

    objectName: "diagnosticsDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 640
    height: Math.min(implicitHeight, 500)
    modal: true
    padding: 24

    background: Rectangle {
        radius: 14
        color: Theme.popup
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 12

        Shortcut {
            sequence: "Esc"
            enabled: dialog.opened
            onActivated: dialog.close()
        }

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("Check the installation")
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
            color: Theme.muted
            font.pixelSize: 12
            text: dialog.busy && dialog.checks.length === 0 ? qsTr("Checking …") : qsTr("Only reads, changes nothing. The report contains these results, your settings and the launcher's logs, with your home folder and user name replaced. Game.log is not included.")
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: list.implicitHeight
            clip: true

            ColumnLayout {
                id: list

                width: dialog.availableWidth
                spacing: 6

                Repeater {
                    model: dialog.checks

                    delegate: RowLayout {
                        id: row

                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        // A new channel starts a group
                        Layout.topMargin: row.modelData.channel !== "" && (row.index === 0 || dialog.checks[row.index - 1].channel !== row.modelData.channel) ? 8 : 0
                        spacing: 10

                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 4
                            width: 8
                            height: 8
                            radius: 4
                            color: dialog.statusColor(row.modelData.status)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: (row.modelData.channel ? row.modelData.channel + " · " : "") + dialog.checkText(row.modelData)
                                wrapMode: Text.Wrap
                                color: Theme.text
                                font.pixelSize: 12
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: row.modelData.detail !== ""
                                text: row.modelData.detail
                                elide: Text.ElideMiddle
                                color: Theme.muted
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: (dialog.diagnostics.reportPath || "") !== "" || (dialog.diagnostics.error || "") !== ""
            text: dialog.diagnostics.error ? qsTr("The report could not be saved.") : qsTr("Saved: %1").arg(Theme.shownPath(dialog.diagnostics.reportPath || ""))
            color: dialog.diagnostics.error ? Theme.danger : Theme.success
            elide: Text.ElideMiddle
            font.pixelSize: 11
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                visible: (dialog.diagnostics.reportPath || "") !== ""
                text: qsTr("Show in folder")
                onClicked: dialog.showReportRequested(dialog.diagnostics.reportPath)
            }

            DialogButton {
                text: qsTr("Save report (ZIP)")
                highlighted: true
                enabled: !dialog.busy
                onClicked: dialog.saveRequested()
            }
        }
    }
}
