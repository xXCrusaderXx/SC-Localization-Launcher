import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// What the project of a channel's translation published on GitHub: its releases, or the commits that changed the file
Popup {
    id: dialog

    // Protocol::TranslationChanges: { channel, language, repository, busy, loaded, kind, entries: [{ title, text, time, url, pending }], error }
    property var changes: ({})
    // Protocol::State.languages, for the name of the language
    property var languages: []
    property var dateLocale: Qt.locale()
    // The channel the list was opened for; until its answer arrives the dialog shows that it is loading
    property string channelName

    readonly property bool current: changes.channel === channelName && !(changes.busy === true)
    readonly property var entries: current ? (changes.entries || []) : []
    readonly property string languageName: {
        for (const language of languages)
            if (language.code === changes.language)
                return language.name;
        return changes.language || "";
    }

    function errorText(error) {
        switch (error) {
        case "rate limit":
            return qsTr("GitHub answers only a few requests per hour without an account. Please try again later.");
        case "network error":
            return qsTr("GitHub cannot be reached. Please try again later.");
        default:
            return qsTr("GitHub did not answer as expected.");
        }
    }

    objectName: "translationChangesDialog"
    // Centered in the window; Overlay attached properties are not available with Qt 6.4
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 560
    height: Math.min(implicitHeight, 520)
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

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("What changed in %1").arg(dialog.languageName)
                    color: Theme.text
                    font.pixelSize: 18
                    font.bold: true
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: dialog.current && dialog.changes.kind !== undefined && dialog.changes.kind !== ""
                    text: dialog.changes.kind === "commits" ? qsTr("Changes to the translation file in %1 on GitHub").arg(dialog.changes.repository || "") : qsTr("Releases of %1 on GitHub").arg(dialog.changes.repository || "")
                    color: Theme.muted
                    font.pixelSize: 11
                    textFormat: Text.PlainText
                    elide: Text.ElideMiddle
                }
            }

            IconButton {
                glyph: "✕"
                onClicked: dialog.close()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: {
                if (!dialog.current)
                    return qsTr("Asking GitHub …");
                if (dialog.changes.error)
                    return dialog.errorText(dialog.changes.error);
                if (dialog.entries.length === 0)
                    return qsTr("The translation project has not published any releases or changes on GitHub yet.");
                return "";
            }
            color: dialog.current && dialog.changes.error ? Theme.warning : Theme.muted
            wrapMode: Text.Wrap
            font.pixelSize: 12
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: list.implicitHeight
            visible: dialog.entries.length > 0
            clip: true

            ColumnLayout {
                id: list

                width: dialog.availableWidth
                spacing: 14

                Repeater {
                    model: dialog.entries

                    delegate: ColumnLayout {
                        id: entry

                        required property var modelData

                        Layout.fillWidth: true
                        spacing: 3

                        // Release names and commit messages come from the internet: never rich text
                        Text {
                            Layout.fillWidth: true
                            text: entry.modelData.title || ""
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: Theme.text
                            font.pixelSize: 13
                            font.bold: true
                        }

                        RowLayout {
                            spacing: 8

                            Text {
                                visible: entry.modelData.time > 0
                                text: new Date(entry.modelData.time * 1000).toLocaleDateString(dialog.dateLocale, Locale.ShortFormat)
                                color: Theme.muted
                                font.pixelSize: 11
                            }

                            Text {
                                visible: entry.modelData.pending === true
                                text: qsTr("Newer than your installed copy")
                                color: Theme.warning
                                font.pixelSize: 11
                                font.bold: true
                            }

                            Text {
                                visible: (entry.modelData.url || "") !== ""
                                text: qsTr("Open on GitHub")
                                color: Theme.accent
                                font.pixelSize: 11

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally(entry.modelData.url)
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: text !== ""
                            text: entry.modelData.text || ""
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            color: Theme.muted
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("As the translation project published it.")
                color: Theme.muted
                font.pixelSize: 10
                wrapMode: Text.Wrap
            }

            DialogButton {
                text: qsTr("OK")
                highlighted: true
                onClicked: dialog.close()
            }
        }
    }
}
