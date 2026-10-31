import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Play statistics from the game's own logs (Protocol::Statistics), with an HTML export
Popup {
    id: dialog

    // Protocol::Statistics
    property var statistics: ({})
    readonly property bool busy: statistics.busy === true
    readonly property bool empty: statistics.loaded === true && (statistics.sessions || 0) === 0
    // Weeks shown in the activity chart, the current one last
    readonly property int weeks: 26
    // Seconds per day, "YYYY-MM-DD" -> seconds
    readonly property var secondsPerDay: {
        const map = {};
        for (const day of statistics.days || [])
            map[day.date] = day.seconds;
        return map;
    }
    readonly property int busiestDay: Math.max(1, ...(statistics.days || []).map(d => d.seconds))

    signal saveRequested(string html)
    signal showReportRequested(string path)

    function hours(seconds) {
        return qsTr("%1 h").arg(Number(seconds / 3600).toLocaleString(Qt.locale(), "f", 1));
    }

    // "AEGS_Sabre" -> "AEGS Sabre"
    function shipName(name) {
        return name.replace(/_/g, " ");
    }

    function regionName(code) {
        if (code.startsWith("eu"))
            return qsTr("Europe");
        if (code.startsWith("us"))
            return qsTr("North America");
        if (code.startsWith("apse"))
            return qsTr("Australia");
        if (code.startsWith("ap"))
            return qsTr("Asia");
        return code;
    }

    function dateText(seconds) {
        return seconds > 0 ? isoDate(new Date(seconds * 1000)) : "";
    }

    // Day `row` (0 = Monday) of week `column` (0 = oldest), counted back from today
    function dayOf(column, row) {
        const today = new Date();
        const monday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - ((today.getDay() + 6) % 7));
        return new Date(monday.getFullYear(), monday.getMonth(), monday.getDate() - 7 * (weeks - 1 - column) + row);
    }

    function isoDate(date) {
        return date.getFullYear() + "-" + String(date.getMonth() + 1).padStart(2, "0") + "-" + String(date.getDate()).padStart(2, "0");
    }

    readonly property var tiles: [
        { "label": qsTr("Play time"), "value": hours(statistics.playSeconds || 0) },
        { "label": qsTr("Game starts"), "value": String(statistics.sessions || 0) },
        { "label": qsTr("Crashes"), "value": String(statistics.crashes || 0) },
        { "label": qsTr("Disconnects"), "value": String(statistics.disconnects || 0) },
        { "label": qsTr("Ships flown"), "value": String((statistics.ships || []).length) },
        { "label": qsTr("Quantum jumps"), "value": String(statistics.quantumJumps || 0) },
        { "label": qsTr("Missions completed"), "value": String(statistics.missionsCompleted || 0) },
        { "label": qsTr("Kills / deaths"), "value": (statistics.kills || 0) + " / " + (statistics.deaths || 0) }
    ]

    function escaped(text) {
        return String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    // A self-contained page in the interface language
    function html() {
        let rows = tiles.map(t => "<tr><th>" + escaped(t.label) + "</th><td>" + escaped(t.value) + "</td></tr>").join("");
        let ships = (statistics.ships || []).map(s => "<tr><td>" + escaped(shipName(s.name)) + "</td><td>" + s.count + "</td></tr>").join("");
        let regions = (statistics.regions || []).map(r => "<tr><td>" + escaped(regionName(r.name)) + " (" + escaped(r.name) + ")</td><td>" + r.count + "</td></tr>").join("");
        let days = (statistics.days || []).slice().reverse().map(d => "<tr><td>" + d.date + "</td><td>" + escaped(hours(d.seconds)) + "</td></tr>").join("");
        return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>" + escaped(qsTr("Star Citizen statistics")) + "</title>"
            + "<style>body{font-family:sans-serif;max-width:720px;margin:2em auto;color:#0f172a}th,td{text-align:left;padding:4px 12px}"
            + "table{border-collapse:collapse;margin-bottom:2em}tr:nth-child(even){background:#eef2f7}</style></head><body>"
            + "<h1>" + escaped(qsTr("Star Citizen statistics")) + "</h1><p>" + escaped(qsTr("%1 to %2, from the game's own logs.").arg(dateText(statistics.firstPlayed)).arg(dateText(statistics.lastPlayed))) + "</p>"
            + "<table>" + rows + "</table>"
            + "<h2>" + escaped(qsTr("Ships")) + "</h2><table><tr><th>" + escaped(qsTr("Ship")) + "</th><th>" + escaped(qsTr("Game starts")) + "</th></tr>" + ships + "</table>"
            + "<h2>" + escaped(qsTr("Server regions")) + "</h2><table>" + regions + "</table>"
            + "<h2>" + escaped(qsTr("Play time per day")) + "</h2><table>" + days + "</table>"
            + "<p><small>SC Localization Launcher</small></p></body></html>";
    }

    objectName: "statisticsDialog"
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 640
    height: Math.min(implicitHeight, parent.height - 40)
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
                text: qsTr("Statistics")
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
            text: {
                if (dialog.busy || dialog.statistics.loaded !== true)
                    return qsTr("Reading the game's logs …");
                if (dialog.empty)
                    return qsTr("No game logs found yet. They appear after the first start of Star Citizen.");
                return qsTr("%1 to %2, from the game's own logs of all channels. Quantum jumps are logged from game version 4.5 on.").arg(dialog.dateText(dialog.statistics.firstPlayed)).arg(dialog.dateText(dialog.statistics.lastPlayed));
            }
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentColumn.implicitHeight, 330)
            visible: dialog.statistics.loaded === true && !dialog.empty
            contentWidth: availableWidth

            ColumnLayout {
                id: contentColumn

                width: parent.width
                spacing: 14

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 10
                    rowSpacing: 10

                    Repeater {
                        model: dialog.tiles

                        Rectangle {
                            required property var modelData

                            Layout.fillWidth: true
                            implicitHeight: 62
                            radius: 10
                            color: Theme.panel
                            border.color: Theme.border

                            Column {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 2

                                Text {
                                    text: modelData.value
                                    color: Theme.text
                                    font.pixelSize: 18
                                    font.bold: true
                                }

                                Text {
                                    text: modelData.label
                                    color: Theme.muted
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }

                Text {
                    text: qsTr("Play time per day, last six months")
                    color: Theme.text
                    font.pixelSize: 12
                    font.bold: true
                }

                // One column per week, Monday at the top; darker = longer
                Row {
                    spacing: 3

                    Repeater {
                        model: dialog.weeks

                        Column {
                            id: week

                            required property int index

                            spacing: 3

                            Repeater {
                                model: 7

                                Rectangle {
                                    required property int index
                                    readonly property date day: dialog.dayOf(week.index, index)
                                    readonly property real seconds: dialog.secondsPerDay[dialog.isoDate(day)] || 0

                                    width: 14
                                    height: 14
                                    radius: 3
                                    visible: day <= new Date()
                                    color: seconds > 0 ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.25 + 0.75 * seconds / dialog.busiestDay) : Theme.inputBackground
                                }
                            }
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 24

                    Text {
                        text: qsTr("Ships")
                        color: Theme.text
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Text {
                        text: qsTr("Server regions")
                        color: Theme.text
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        text: (dialog.statistics.ships || []).length > 0 ? (dialog.statistics.ships || []).slice(0, 5).map(s => qsTr("%1 – %n game start(s)", "", s.count).arg(dialog.shipName(s.name))).join("\n") : qsTr("No ship flown yet")
                        color: Theme.muted
                        font.pixelSize: 12
                        lineHeight: 1.3
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        text: (dialog.statistics.regions || []).map(r => qsTr("%1 – %n game start(s)", "", r.count).arg(dialog.regionName(r.name))).join("\n")
                        color: Theme.muted
                        font.pixelSize: 12
                        lineHeight: 1.3
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: (dialog.statistics.reportPath || "") !== "" || (dialog.statistics.error || "") !== ""
            text: dialog.statistics.error ? qsTr("The report could not be saved.") : qsTr("Saved: %1").arg(dialog.statistics.reportPath || "")
            wrapMode: Text.WrapAnywhere
            color: dialog.statistics.error ? Theme.warning : Theme.success
            font.pixelSize: 11
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            DialogButton {
                visible: (dialog.statistics.reportPath || "") !== ""
                text: qsTr("Show in folder")
                onClicked: dialog.showReportRequested(dialog.statistics.reportPath)
            }

            DialogButton {
                text: qsTr("Save as HTML")
                enabled: dialog.statistics.loaded === true && !dialog.empty && !dialog.busy
                onClicked: dialog.saveRequested(dialog.html())
            }

            DialogButton {
                text: qsTr("Close")
                highlighted: true
                onClicked: dialog.close()
            }
        }
    }
}
