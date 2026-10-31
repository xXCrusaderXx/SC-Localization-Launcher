import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The computer as the game saw it (Protocol::SystemSnapshot) and how its performance index develops between starts
Popup {
    id: dialog

    // Protocol::Statistics
    property var statistics: ({})
    // The interface language's number and date format
    property var numberLocale: Qt.locale()
    property string operatingSystem: ""
    readonly property var systems: statistics.systems || []
    readonly property var latest: systems.length > 0 ? systems[systems.length - 1] : null
    // The starts a forecast is based on
    readonly property var recent: systems.slice(-10)

    signal saveRequested(string html)
    signal showReportRequested(string path)

    function number(value, decimals) {
        return Number(value).toLocaleString(numberLocale, "f", decimals);
    }

    function dateText(seconds) {
        return new Date(seconds * 1000).toLocaleDateString(numberLocale, Locale.ShortFormat);
    }

    // Mean ± two standard deviations of one index over the recent starts
    function range(key) {
        const values = recent.map(s => s[key]);
        const mean = values.reduce((a, b) => a + b, 0) / values.length;
        const deviation = Math.sqrt(values.reduce((a, b) => a + (b - mean) * (b - mean), 0) / values.length);
        return [mean - 2 * deviation, mean + 2 * deviation];
    }

    function forecastText() {
        if (recent.length < 3)
            return qsTr("A forecast needs at least three game starts.");
        const cpu = range("cpuIndex");
        const gpu = range("gpuIndex");
        return qsTr("Expected at the next start: CPU %1–%2, GPU %3–%4.").arg(number(cpu[0], 0)).arg(number(cpu[1], 0)).arg(number(gpu[0], 0)).arg(number(gpu[1], 0));
    }

    // Whether the newest start differs by more than 5 % from the ones before, and what changed with it
    function changeText() {
        if (systems.length < 4)
            return "";
        const before = systems.slice(-11, -1);
        const notes = [];
        for (const [key, name] of [["cpuIndex", qsTr("CPU")], ["gpuIndex", qsTr("GPU")]]) {
            const mean = before.reduce((a, s) => a + s[key], 0) / before.length;
            const change = (latest[key] - mean) / mean * 100;
            if (change <= -5)
                notes.push(qsTr("%1 %2 % slower than usual").arg(name).arg(number(-change, 0)));
            else if (change >= 5)
                notes.push(qsTr("%1 %2 % faster than usual").arg(name).arg(number(change, 0)));
        }
        if (notes.length === 0)
            return "";
        const previous = systems[systems.length - 2];
        let cause = "";
        if (previous.driverVersion !== latest.driverVersion && latest.driverVersion)
            cause = qsTr("The graphics driver changed from %1 to %2.").arg(previous.driverVersion || "?").arg(latest.driverVersion);
        else if (previous.gameVersion !== latest.gameVersion)
            cause = qsTr("The game version changed from %1 to %2.").arg(previous.gameVersion).arg(latest.gameVersion);
        return qsTr("Last start: %1.").arg(notes.join(", ")) + (cause ? " " + cause : "");
    }

    readonly property var facts: latest ? [
        [qsTr("Processor"), latest.cpu + (latest.threads ? " · " + qsTr("%1 threads").arg(latest.threads) : "")],
        [qsTr("Memory"), qsTr("%1 GB").arg(number(latest.memoryMb / 1024, 0))],
        [qsTr("Graphics card"), latest.gpu + (latest.videoMemoryMb ? " · " + qsTr("%1 GB").arg(number(latest.videoMemoryMb / 1024, 0)) : "")],
        [qsTr("Graphics API"), latest.renderer === "vulkan" ? "Vulkan " + latest.vulkanVersion : "DirectX 11"],
        [qsTr("Driver"), latest.driverVersion || "–"],
        [qsTr("Game version"), latest.gameVersion || "–"],
        [qsTr("Operating system"), operatingSystem]
    ] : []

    function escaped(text) {
        return String(text).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    function html() {
        const rows = facts.map(f => "<tr><th>" + escaped(f[0]) + "</th><td>" + escaped(f[1]) + "</td></tr>").join("");
        const history = systems.slice().reverse().map(s => "<tr><td>" + escaped(dateText(s.time)) + "</td><td>" + escaped(s.gameVersion) + "</td><td>" + escaped(s.driverVersion || "–") + "</td><td>" + number(s.cpuIndex, 1) + "</td><td>" + number(s.gpuIndex, 1) + "</td></tr>").join("");
        return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>" + escaped(qsTr("System information")) + "</title>"
            + "<style>body{font-family:sans-serif;max-width:760px;margin:2em auto;color:#0f172a}th,td{text-align:left;padding:4px 12px}"
            + "table{border-collapse:collapse;margin-bottom:2em}tr:nth-child(even){background:#eef2f7}</style></head><body>"
            + "<h1>" + escaped(qsTr("System information")) + "</h1><table>" + rows + "</table>"
            + "<h2>" + escaped(qsTr("Performance index")) + "</h2><p>" + escaped(forecastText()) + " " + escaped(changeText()) + "</p>"
            + "<p><small>" + escaped(qsTr("The game measures both at every start; higher is faster. CPU and GPU use different scales and can't be compared with each other.")) + "</small></p>"
            + "<table><tr><th>" + escaped(qsTr("Date")) + "</th><th>" + escaped(qsTr("Game version")) + "</th><th>" + escaped(qsTr("Driver")) + "</th><th>CPU</th><th>GPU</th></tr>" + history + "</table>"
            + "<p><small>SC Localization Launcher</small></p></body></html>";
    }

    objectName: "systemInfoDialog"
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
                text: qsTr("System information")
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
                if (dialog.statistics.busy === true || dialog.statistics.loaded !== true)
                    return qsTr("Reading the game's logs …");
                if (!dialog.latest)
                    return qsTr("No game logs found yet. They appear after the first start of Star Citizen.");
                return qsTr("As the game saw your computer at its last start (%1).").arg(dialog.dateText(dialog.latest.time));
            }
            wrapMode: Text.Wrap
            color: Theme.muted
            font.pixelSize: 12
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(infoColumn.implicitHeight, 330)
            visible: dialog.latest !== null
            contentWidth: availableWidth

            ColumnLayout {
                id: infoColumn

                width: parent.width
                spacing: 12

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 6

                    Repeater {
                        model: dialog.facts.length * 2

                        Text {
                            required property int index

                            Layout.fillWidth: index % 2 === 1
                            text: dialog.facts[Math.floor(index / 2)][index % 2]
                            color: index % 2 === 0 ? Theme.muted : Theme.text
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Text {
                    text: qsTr("Performance index")
                    color: Theme.text
                    font.pixelSize: 12
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: dialog.latest ? qsTr("Last start: CPU %1, GPU %2.").arg(dialog.number(dialog.latest.cpuIndex, 1)).arg(dialog.number(dialog.latest.gpuIndex, 1)) + " " + dialog.forecastText() : ""
                    color: Theme.text
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: dialog.latest ? dialog.changeText() : ""
                    color: Theme.warning
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("The game measures both at every start; higher is faster. CPU and GPU use different scales and can't be compared with each other.")
                    color: Theme.muted
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                // The recent starts, newest first
                Repeater {
                    model: dialog.recent.slice().reverse()

                    Text {
                        required property var modelData

                        Layout.fillWidth: true
                        text: dialog.dateText(modelData.time) + "  ·  " + modelData.gameVersion + (modelData.driverVersion ? "  ·  " + modelData.driverVersion : "") + "  ·  CPU " + dialog.number(modelData.cpuIndex, 1) + "  ·  GPU " + dialog.number(modelData.gpuIndex, 1)
                        color: Theme.muted
                        font.pixelSize: 11
                        font.family: "monospace"
                        elide: Text.ElideRight
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
                enabled: dialog.latest !== null && dialog.statistics.busy !== true
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
