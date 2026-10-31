import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Shows whether a newer launcher version is published on GitHub, with its release notes, and installs it
// when the release has a signed package for this installation.
Popup {
    id: dialog

    // Protocol::SelfUpdate: { status, currentVersion, latestVersion, notes, url, error, installType, canInstall, progress }
    property var selfUpdate: ({})

    signal checkRequested
    signal installRequested
    signal restartRequested

    readonly property string status: selfUpdate.status || "idle"
    readonly property bool busy: status === "checking" || status === "downloading" || status === "installing"
    readonly property bool packageManager: ["deb", "rpm", "arch"].indexOf(selfUpdate.installType) >= 0

    function errorText(error) {
        switch (error) {
        case "rate limit":
            return qsTr("GitHub is limiting requests right now. Please try again in an hour.");
        case "invalid signature":
            return qsTr("The update is not signed correctly and was not installed.");
        case "checksum mismatch":
            return qsTr("The download is damaged and was not installed.");
        case "Star Citizen is running":
            return qsTr("Close Star Citizen first, then try again.");
        case "cancelled":
            return qsTr("The installation was cancelled.");
        default:
            return qsTr("The update failed: %1").arg(error);
        }
    }

    // Why the launcher cannot install this release itself
    function manualHint() {
        switch (selfUpdate.installType) {
        case "flatpak":
            return qsTr("Update the Flatpak through your software centre or with \"flatpak update\".");
        case "source":
            return qsTr("This launcher was built from source; build the new version the same way.");
        default:
            return qsTr("Download the new version from the release page.");
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
                case "downloading":
                    return qsTr("Downloading version %1 …").arg(dialog.selfUpdate.latestVersion);
                case "installing":
                    return dialog.packageManager ? qsTr("Installing … Your system may ask for your password.") : qsTr("Installing …");
                case "installed":
                    return qsTr("Version %1 is installed. Restart the launcher to use it.").arg(dialog.selfUpdate.latestVersion);
                case "error":
                    return dialog.errorText(dialog.selfUpdate.error);
                default:
                    return qsTr("Version %1").arg(dialog.selfUpdate.currentVersion);
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.status === "available" && !dialog.selfUpdate.canInstall
            text: dialog.manualHint()
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
        }

        // Download progress
        Rectangle {
            Layout.fillWidth: true
            visible: dialog.status === "downloading"
            implicitHeight: 6
            radius: 3
            color: Theme.inputBackground

            Rectangle {
                width: parent.width * Math.min(1, dialog.selfUpdate.progress || 0)
                height: parent.height
                radius: parent.radius
                color: Theme.accent
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
                visible: dialog.status !== "installed"
                text: qsTr("Check again")
                enabled: !dialog.busy
                onClicked: dialog.checkRequested()
            }

            DialogButton {
                visible: dialog.status === "available" || dialog.status === "error"
                text: qsTr("Open download page")
                highlighted: dialog.status === "available" && !dialog.selfUpdate.canInstall
                onClicked: Qt.openUrlExternally(dialog.selfUpdate.url || "https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/latest")
            }

            DialogButton {
                visible: dialog.status === "available" && dialog.selfUpdate.canInstall === true
                text: qsTr("Install update")
                highlighted: true
                onClicked: dialog.installRequested()
            }

            DialogButton {
                visible: dialog.status === "installed"
                text: qsTr("Restart now")
                highlighted: true
                onClicked: dialog.restartRequested()
            }
        }
    }
}
