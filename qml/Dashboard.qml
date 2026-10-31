import QtQuick
import QtQuick.Layouts

// Tiles on the main window with values from the play statistics; which ones and their order come from the settings
ColumnLayout {
    id: dashboard

    // Protocol::Statistics
    property var statistics: ({})
    // State.dashboardTiles
    property var tiles: []
    // Rows that fit into the window; 0 hides the tiles
    property int rows: 2
    // The interface language's number and date format
    property var numberLocale: Qt.locale()
    // Every tile id, in the order the customize dialog offers them; the backend checks them against DASHBOARD_TILES
    readonly property var allTiles: ["playTime", "lastPlayed", "topShip", "freeSpace", "sessions", "crashes", "disconnects", "quantumJumps", "missions", "killsDeaths"]
    readonly property var shownTiles: tiles.slice(0, rows * 4)

    signal customizeRequested
    signal statisticsRequested

    function number(value, decimals) {
        return Number(value).toLocaleString(numberLocale, "f", decimals);
    }

    function hours(seconds) {
        return qsTr("%1 h").arg(number(seconds / 3600, 1));
    }

    function labelOf(id) {
        switch (id) {
        case "playTime":
            return qsTr("Play time");
        case "lastPlayed":
            return qsTr("Last played");
        case "topShip":
            return qsTr("Favourite ship");
        case "freeSpace":
            return qsTr("Free space");
        case "sessions":
            return qsTr("Game starts");
        case "crashes":
            return qsTr("Crashes");
        case "disconnects":
            return qsTr("Disconnects");
        case "quantumJumps":
            return qsTr("Quantum jumps");
        case "missions":
            return qsTr("Missions completed");
        case "killsDeaths":
            return qsTr("Kills / deaths");
        }
        return id;
    }

    function valueOf(id) {
        const s = statistics;
        if (s.loaded !== true)
            return "…";
        switch (id) {
        case "playTime":
            return hours(s.playSeconds || 0);
        case "lastPlayed":
            return s.lastPlayed > 0 ? new Date(s.lastPlayed * 1000).toLocaleDateString(numberLocale, Locale.ShortFormat) : "–";
        case "topShip":
            return (s.ships || []).length > 0 ? s.ships[0].name.replace(/_/g, " ") : "–";
        case "freeSpace":
            return s.diskFreeBytes >= 0 ? qsTr("%1 GB").arg(number(s.diskFreeBytes / 1e9, 0)) : "–";
        case "sessions":
            return number(s.sessions || 0, 0);
        case "crashes":
            return number(s.crashes || 0, 0);
        case "disconnects":
            return number(s.disconnects || 0, 0);
        case "quantumJumps":
            return number(s.quantumJumps || 0, 0);
        case "missions":
            return number(s.missionsCompleted || 0, 0);
        case "killsDeaths":
            return (s.kills || 0) + " / " + (s.deaths || 0);
        }
        return "";
    }

    // A second line under the value, where it says more
    function detailOf(id) {
        const s = statistics;
        if (s.loaded !== true)
            return "";
        if (id === "lastPlayed" && s.lastPlayed > 0)
            return hours(s.lastSessionSeconds || 0);
        return "";
    }

    visible: rows > 0
    spacing: 8

    RowLayout {
        Layout.fillWidth: true

        Text {
            text: qsTr("DASHBOARD")
            color: Theme.muted
            font.pixelSize: 11
            font.bold: true
            font.letterSpacing: 2
        }

        Item {
            Layout.fillWidth: true
        }

        Text {
            text: qsTr("Customize …")
            color: customizeMouse.containsMouse ? Theme.text : Theme.accent
            font.pixelSize: 11
            font.bold: true

            MouseArea {
                id: customizeMouse

                anchors.fill: parent
                anchors.margins: -6
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: dashboard.customizeRequested()
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: dashboard.tiles.length === 0
        text: qsTr("No tiles chosen.")
        color: Theme.muted
        font.pixelSize: 12
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 4
        columnSpacing: 8
        rowSpacing: 8

        Repeater {
            model: dashboard.shownTiles

            Rectangle {
                required property string modelData

                objectName: "dashboardTile_" + modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: 62
                radius: 10
                color: tileMouse.containsMouse ? Theme.panelHover : Theme.panel
                border.color: Theme.border

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 1

                    Text {
                        width: parent.width
                        text: dashboard.valueOf(modelData)
                        color: Theme.text
                        font.pixelSize: 16
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: dashboard.labelOf(modelData) + (dashboard.detailOf(modelData) ? " · " + dashboard.detailOf(modelData) : "")
                        color: Theme.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                // The whole statistics are one click away
                MouseArea {
                    id: tileMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: dashboard.statisticsRequested()
                }
            }
        }
    }
}
