import QtQuick
import QtQuick.Controls.Basic

// Picks the launcher's own interface language: "Automatic (system language)" or a built-in translation.
// It only displays `current` and sends the choice to the backend, which answers with the next state.
ComboBox {
    id: control

    // State.interfaceLanguage; "" = the system language
    property string current: ""
    // "" = the system language, then the built-in translations by code
    readonly property var codes: [""].concat(backend.interfaceLanguages.map(l => l.code))

    implicitWidth: 220
    model: [qsTr("Automatic (system language)")].concat(backend.interfaceLanguages.map(l => l.name))
    currentIndex: Math.max(0, codes.indexOf(current))
    onActivated: index => backend.setInterfaceLanguage(codes[index])
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
