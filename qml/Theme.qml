pragma Singleton
import QtQuick

// Colours of the launcher, and how it shows paths. Main.qml sets `dark` from the setting "Appearance" or the system,
// and the streamer mode with the home folder and user name it hides.
QtObject {
    property bool dark: true
    property bool streamerMode: false
    property string homeFolder: ""
    property string userName: ""

    // A path as the window shows it: in streamer mode without the home folder and the user name
    function shownPath(path) {
        if (!streamerMode || !path)
            return path;
        let shown = homeFolder !== "" && path.startsWith(homeFolder) ? "~" + path.slice(homeFolder.length) : path;
        // Wine prefixes have a user folder of their own
        shown = shown.replace(/\/users\/[^\/]+/gi, "/users/…");
        // Short names like "a" would hide parts of every word
        return userName.length > 2 ? shown.split(userName).join("…") : shown;
    }

    readonly property color background: dark ? "#0b1220" : "#eef2f7"
    readonly property color panel: dark ? Qt.rgba(0.06, 0.09, 0.16, 0.72) : Qt.rgba(1, 1, 1, 0.82)
    readonly property color panelHover: dark ? Qt.rgba(0.09, 0.13, 0.22, 0.85) : Qt.rgba(1, 1, 1, 0.95)
    readonly property color inputBackground: dark ? Qt.rgba(0, 0, 0, 0.35) : Qt.rgba(0.06, 0.09, 0.16, 0.06)
    readonly property color border: dark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0.06, 0.09, 0.16, 0.14)
    readonly property color hover: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0.06, 0.09, 0.16, 0.07)
    // Dialogs, drop-down lists and the settings sheet
    readonly property color popup: dark ? Qt.rgba(0.04, 0.07, 0.13, 0.98) : Qt.rgba(0.97, 0.98, 0.99, 0.98)
    // Behind the settings sheet
    readonly property color scrim: dark ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(0.06, 0.09, 0.16, 0.3)

    readonly property color text: dark ? "#e2e8f0" : "#0f172a"
    readonly property color muted: dark ? "#94a3b8" : "#475569"
    readonly property color disabled: dark ? "#475569" : "#94a3b8"

    readonly property color accent: dark ? "#38bdf8" : "#0284c7"
    readonly property color accent2: dark ? "#6366f1" : "#4f46e5"
    readonly property color success: dark ? "#22c55e" : "#15803d"
    readonly property color warning: dark ? "#f59e0b" : "#b45309"
    readonly property color danger: dark ? "#ef4444" : "#dc2626"

    // Veil over the background picture: strong on the left where the panels are, faint on the right
    function veil(alpha) {
        // Light: the picture is dark space, so even its right edge needs a clear veil
        return dark ? Qt.rgba(0.04, 0.07, 0.13, alpha) : Qt.rgba(0.93, 0.95, 0.97, Math.min(1, 0.45 + alpha * 0.6));
    }
}
