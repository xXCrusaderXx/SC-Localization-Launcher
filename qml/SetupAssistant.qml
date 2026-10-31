import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Offered on the first start: interface language, game folders, translation and the main options in four steps.
// Every choice goes to the backend at once, like in the settings; Skip and Done only mark the setup as finished.
Popup {
    id: assistant

    property var appState: ({})
    property bool trayAvailable: false
    property int page: 0
    readonly property int lastPage: 3
    // Channels with a game folder (Protocol::Channel)
    readonly property var channels: (appState.channels || []).filter(c => c.installPath !== "")
    readonly property var options: appState.options || {}
    // Languages that at least one found channel offers, in catalogue order
    readonly property var translations: (appState.languages || []).filter(l => channels.some(c => (c.availableLanguages || []).indexOf(l.code) >= 0))
    readonly property string chosenLanguage: channels.length > 0 ? channels[0].language : ""

    // The folder dialog of the main window, for the LIVE channel
    signal folderRequested
    // install: download the chosen translation right away
    signal finished(bool install)

    // Every found channel that offers the language gets it
    function chooseTranslation(code) {
        for (const channel of channels)
            if ((channel.availableLanguages || []).indexOf(code) >= 0)
                backend.setLanguage(channel.name, code);
    }

    objectName: "setupAssistant"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: Math.min(560, parent.width - 40)
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    padding: 28
    onOpened: page = 0

    background: Rectangle {
        radius: 14
        color: Theme.popup
        border.color: Theme.border
    }

    component Title: Text {
        Layout.fillWidth: true
        color: Theme.text
        font.pixelSize: 20
        font.bold: true
        wrapMode: Text.WordWrap
    }

    component Body: Text {
        Layout.fillWidth: true
        color: Theme.muted
        font.pixelSize: 13
        wrapMode: Text.WordWrap
    }

    contentItem: ColumnLayout {
        spacing: 16

        Text {
            text: qsTr("Step %1 of %2").arg(assistant.page + 1).arg(assistant.lastPage + 1)
            color: Theme.accent
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 1
        }

        StackLayout {
            Layout.fillWidth: true
            currentIndex: assistant.page

            ColumnLayout {
                spacing: 14

                Title {
                    text: qsTr("Welcome")
                }

                Body {
                    text: qsTr("This launcher installs community translations of Star Citizen and keeps them up to date. A few steps and you are ready; everything can be changed later in the settings.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Language of the launcher")
                        color: Theme.text
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    InterfaceLanguageBox {
                        current: assistant.appState.interfaceLanguage || ""
                    }
                }
            }

            ColumnLayout {
                spacing: 14

                Title {
                    text: qsTr("Find Star Citizen")
                }

                Body {
                    text: qsTr("The launcher looks for the RSI Launcher and the game versions (LIVE, PTU, …) on your drives, including Lutris and LUG Helper installations.")
                }

                RowLayout {
                    spacing: 10

                    DialogButton {
                        text: qsTr("Search automatically")
                        highlighted: assistant.channels.length === 0
                        enabled: assistant.appState.searching !== true
                        onClicked: backend.autoSearch()
                    }

                    DialogButton {
                        text: qsTr("Choose folder …")
                        enabled: assistant.appState.searching !== true
                        onClicked: assistant.folderRequested()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: {
                        if (assistant.appState.searching === true)
                            return qsTr("Searching …");
                        if (assistant.channels.length > 0)
                            return qsTr("Found: %1").arg(assistant.channels.map(c => c.name).join(", "));
                        return qsTr("Nothing found yet. Choose the LIVE folder of the game, e.g. …/StarCitizen/LIVE.");
                    }
                    color: assistant.channels.length > 0 ? Theme.success : Theme.muted
                    font.pixelSize: 12
                    font.bold: assistant.channels.length > 0
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                spacing: 14

                Title {
                    text: qsTr("Choose a translation")
                }

                Body {
                    text: qsTr("Which language should Star Citizen use? It is set for every game version found.")
                }

                ComboBox {
                    objectName: "setupTranslationBox"
                    Layout.preferredWidth: 260
                    visible: assistant.translations.length > 0
                    model: assistant.translations.map(l => l.name)
                    currentIndex: assistant.translations.findIndex(l => l.code === assistant.chosenLanguage)
                    onActivated: index => assistant.chooseTranslation(assistant.translations[index].code)
                    font.pixelSize: 12
                    palette.text: Theme.text
                    palette.base: Theme.inputBackground
                    palette.window: Theme.popup
                    palette.button: Theme.inputBackground
                    palette.buttonText: Theme.text
                    palette.highlight: Theme.accent
                    palette.highlightedText: "white"
                    background: Rectangle {
                        implicitHeight: 32
                        radius: 8
                        color: Theme.inputBackground
                        border.color: Theme.border
                    }
                }

                Body {
                    visible: assistant.translations.length === 0
                    text: assistant.channels.length === 0 ? qsTr("No game folder yet; go back one step.") : qsTr("Loading the list of translations …")
                }
            }

            ColumnLayout {
                spacing: 14

                Title {
                    text: qsTr("Almost done")
                }

                Toggle {
                    Layout.fillWidth: true
                    text: qsTr("Update translations automatically")
                    description: qsTr("Checks every hour and installs new versions; waits while Star Citizen runs")
                    checked: assistant.options.installNewTranslations === true
                    onSwitched: on => backend.setOption("installNewTranslations", on)
                }

                Toggle {
                    Layout.fillWidth: true
                    visible: assistant.trayAvailable
                    text: qsTr("Keep running in the system tray")
                    description: qsTr("Closing the window keeps the launcher in the tray, so it goes on checking")
                    checked: assistant.options.closeToTray === true
                    onSwitched: on => backend.setOption("closeToTray", on)
                }

                Body {
                    text: qsTr("\"Install now\" downloads the translation; Star Citizen itself is not started.")
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 10

            DialogButton {
                text: qsTr("Skip")
                onClicked: assistant.finished(false)
            }

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                visible: assistant.page > 0
                text: qsTr("Back")
                onClicked: assistant.page -= 1
            }

            DialogButton {
                visible: assistant.page < assistant.lastPage
                text: qsTr("Next")
                highlighted: true
                // A translation needs a game folder
                enabled: assistant.page !== 1 || assistant.channels.length > 0
                onClicked: assistant.page += 1
            }

            DialogButton {
                visible: assistant.page === assistant.lastPage
                text: qsTr("Done")
                onClicked: assistant.finished(false)
            }

            DialogButton {
                visible: assistant.page === assistant.lastPage
                text: qsTr("Install now")
                highlighted: true
                enabled: assistant.channels.length > 0 && assistant.appState.update !== "running"
                onClicked: assistant.finished(true)
            }
        }
    }
}
