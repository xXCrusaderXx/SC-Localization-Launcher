import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Takes over what was set up on LIVE into another channel: the language, the launcher's own user.cfg lines and the
// keybindings. Offered once when a new test channel appears, and from the channel's row at any time.
Popup {
    id: dialog

    // The channel that takes over LIVE's settings
    property string channelName: ""
    // Protocol::State.channels and .languages
    property var channels: []
    property var languages: []
    // Protocol::LiveSettings: { busy, channel, result, files }
    property var liveSettings: ({})
    property bool gamePlaying: false
    // A backup or an update runs; the backend refuses the copy meanwhile
    property bool busy: false

    signal copyRequested(string channel, bool language, bool gameSettings, bool profile)
    signal dismissRequested(string channel)

    readonly property var live: channels.find(c => c.name === "LIVE") || ({})
    readonly property var target: channels.find(c => c.name === channelName) || ({})
    readonly property string liveLanguage: live.language || "en"
    readonly property string liveLanguageName: {
        if (liveLanguage === "custom")
            return qsTr("Own source");
        const language = languages.find(l => l.code === liveLanguage);
        return language ? language.name : liveLanguage;
    }
    // Like the backend: an own source belongs to LIVE alone
    readonly property bool languageAvailable: liveLanguage === "en" || (liveLanguage !== "custom" && (target.availableLanguages || []).indexOf(liveLanguage) >= 0)
    readonly property var liveGameSettings: live.gameSettings || ({})
    readonly property bool liveHasGameSettings: (liveGameSettings.maxFps || 0) > 0 || (liveGameSettings.maxIdleFps || 0) > 0 || (liveGameSettings.customLines || "") !== "" || (liveGameSettings.graphicsApi || "") !== ""
    property bool takeLanguage: true
    property bool takeGameSettings: true
    property bool takeProfile: true
    // Sent in this opening of the dialog; until then a close declines the offer
    property bool requested: false
    readonly property bool done: requested && liveSettings.channel === channelName && liveSettings.busy !== true && (liveSettings.result || "") !== ""

    function openFor(channel) {
        channelName = channel;
        takeLanguage = true;
        takeGameSettings = true;
        takeProfile = true;
        requested = false;
        open();
    }

    function resultText(result) {
        switch (result) {
        case "copied":
            return qsTr("Taken over from LIVE. The previous files of %1 were backed up first.").arg(channelName);
        case "Star Citizen is running":
            return qsTr("Close Star Citizen first; the game rewrites these files.");
        case "no game folder":
            return qsTr("LIVE and %1 each need their own game folder.").arg(channelName);
        default:
            return qsTr("Failed: %1").arg(result);
        }
    }

    objectName: "liveSettingsDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 560
    height: Math.min(implicitHeight, parent.height - 40)
    modal: true
    padding: 24
    onClosed: {
        if (!requested && channelName !== "")
            dismissRequested(channelName);
    }

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
                text: qsTr("Take over settings from LIVE")
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
            text: qsTr("%1 can start with what you set up on LIVE. The current files of %1 are backed up first.").arg(dialog.channelName)
        }

        Toggle {
            Layout.fillWidth: true
            enabled: dialog.languageAvailable && !dialog.requested
            text: qsTr("Language: %1").arg(dialog.liveLanguageName)
            description: dialog.languageAvailable ? qsTr("Installed right away") : qsTr("%1 is currently not available for this channel").arg(dialog.liveLanguageName)
            checked: dialog.takeLanguage && dialog.languageAvailable
            onSwitched: on => dialog.takeLanguage = on
        }

        Toggle {
            Layout.fillWidth: true
            enabled: dialog.liveHasGameSettings && !dialog.requested
            text: qsTr("Own user.cfg lines")
            description: dialog.liveHasGameSettings ? qsTr("Frame limits, graphics API and your own lines from the game settings. The language lines stay those of %1.").arg(dialog.channelName) : qsTr("LIVE has no own lines yet.")
            checked: dialog.takeGameSettings && dialog.liveHasGameSettings
            onSwitched: on => dialog.takeGameSettings = on
        }

        Toggle {
            Layout.fillWidth: true
            enabled: !dialog.requested
            text: qsTr("Keybindings and profile")
            description: qsTr("Keybindings, profile settings, exported control mappings and saved characters")
            checked: dialog.takeProfile
            onSwitched: on => dialog.takeProfile = on
        }

        Text {
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.Wrap
            text: dialog.gamePlaying && !dialog.done ? qsTr("Close Star Citizen first; the game rewrites these files.") : dialog.done ? dialog.resultText(dialog.liveSettings.result) : dialog.requested ? qsTr("Taking over …") : ""
            color: dialog.done && dialog.liveSettings.result === "copied" ? Theme.success : dialog.requested && !dialog.done ? Theme.muted : Theme.warning
            font.pixelSize: 11
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.done && dialog.liveSettings.result === "copied"
            text: qsTr("%n keybinding and profile file(s) copied.", "", dialog.liveSettings.files || 0)
            color: Theme.muted
            font.pixelSize: 11
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                visible: !dialog.requested
                text: qsTr("No, thanks")
                onClicked: dialog.close()
            }

            DialogButton {
                visible: !dialog.requested
                text: qsTr("Take over")
                highlighted: true
                enabled: !dialog.busy && !dialog.gamePlaying && ((dialog.takeLanguage && dialog.languageAvailable) || (dialog.takeGameSettings && dialog.liveHasGameSettings) || dialog.takeProfile)
                onClicked: {
                    dialog.requested = true;
                    dialog.copyRequested(dialog.channelName, dialog.takeLanguage && dialog.languageAvailable, dialog.takeGameSettings && dialog.liveHasGameSettings, dialog.takeProfile);
                }
            }

            DialogButton {
                visible: dialog.requested
                enabled: dialog.done
                text: qsTr("Close")
                onClicked: dialog.close()
            }
        }
    }
}
