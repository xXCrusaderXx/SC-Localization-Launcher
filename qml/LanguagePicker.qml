import QtQuick
import QtQuick.Controls.Basic

// Drop-down with the languages of one channel. It only displays `currentCode` and reports a choice
// via picked(), the new value comes back with the next backend state.
ComboBox {
    id: control

    property var languages: [] // [{ code, name, credit, notice, available, action }]; an action entry opens something instead of choosing
    property string currentCode: ""

    signal picked(string code)

    readonly property int currentLanguageIndex: {
        for (let i = 0; i < languages.length; ++i)
            if (languages[i].code === currentCode)
                return i;
        return -1;
    }

    model: languages
    textRole: "name"
    currentIndex: currentLanguageIndex
    // A new model resets currentIndex and breaks the binding above
    onCountChanged: currentIndex = Qt.binding(() => control.currentLanguageIndex)
    implicitWidth: 200
    implicitHeight: 32
    leftPadding: 14
    rightPadding: 30

    // Selecting only reports the choice; the binding above shows what the backend confirms
    onActivated: index => {
        const code = languages[index].code;
        currentIndex = Qt.binding(() => control.currentLanguageIndex);
        if (code !== currentCode)
            picked(code);
    }

    background: Rectangle {
        radius: height / 2
        color: control.hovered && control.enabled ? Theme.hover : Theme.inputBackground
        border.color: control.visualFocus ? Theme.accent : Theme.border
        opacity: control.enabled ? 1 : 0.5
    }

    contentItem: Text {
        // Not displayText: ComboBox resets currentIndex whenever the model changes
        text: control.currentLanguageIndex >= 0 ? control.languages[control.currentLanguageIndex].name : control.currentCode
        textFormat: Text.PlainText
        color: Theme.text
        font.pixelSize: 12
        font.bold: true
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 12
        anchors.verticalCenter: parent.verticalCenter
        text: "▾"
        color: Theme.muted
        font.pixelSize: 12
    }

    delegate: ItemDelegate {
        id: entry

        required property var modelData
        required property int index

        width: control.popup.availableWidth
        enabled: modelData.available
        highlighted: control.highlightedIndex === index

        contentItem: Column {
            spacing: 1

            Text {
                width: parent.width
                text: entry.modelData.available ? entry.modelData.name : qsTr("%1 (not available)").arg(entry.modelData.name)
                textFormat: Text.PlainText
                color: entry.index === control.currentLanguageIndex || entry.modelData.action ? Theme.accent : entry.enabled ? Theme.text : Theme.disabled
                font.pixelSize: 12
                font.bold: entry.index === control.currentLanguageIndex
                elide: Text.ElideRight
            }

            // Credit of the translation, required by the licenses of the community projects
            Text {
                width: parent.width
                visible: ((entry.modelData.credit || {}).project || "") !== ""
                text: entry.modelData.credit.project + (entry.modelData.credit.license ? " · " + entry.modelData.credit.license : "")
                textFormat: Text.PlainText
                color: Theme.muted
                font.pixelSize: 10
                elide: Text.ElideRight
            }

            // Notice of the translation project for this channel, e.g. "in preparation"
            Text {
                width: parent.width
                visible: (entry.modelData.notice || "") !== ""
                text: entry.modelData.notice || ""
                textFormat: Text.PlainText
                color: Theme.warning
                font.pixelSize: 10
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            color: entry.highlighted ? Theme.hover : "transparent"
            radius: 6
        }
    }

    popup: Popup {
        y: control.height + 4
        // Wider than the button, so credit and notice lines fit
        width: Math.max(control.width, 300)
        // As tall as the entries need, but never past the bottom of the window
        implicitHeight: Math.min(contentItem.implicitHeight + 8, control.Window.height - control.mapToItem(null, 0, control.height).y - 12)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }

        background: Rectangle {
            radius: 10
            color: Theme.popup
            border.color: Theme.border
        }
    }
}
