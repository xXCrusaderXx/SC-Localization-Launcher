pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#0b1220"
    readonly property color panel: Qt.rgba(0.06, 0.09, 0.16, 0.72)
    readonly property color panelHover: Qt.rgba(0.09, 0.13, 0.22, 0.85)
    readonly property color inputBackground: Qt.rgba(0, 0, 0, 0.35)
    readonly property color border: Qt.rgba(1, 1, 1, 0.09)
    readonly property color hover: Qt.rgba(1, 1, 1, 0.08)

    readonly property color text: "#e2e8f0"
    readonly property color muted: "#94a3b8"
    readonly property color disabled: "#475569"

    readonly property color accent: "#38bdf8"
    readonly property color accent2: "#6366f1"
    readonly property color success: "#22c55e"
    readonly property color warning: "#f59e0b"
    readonly property color danger: "#ef4444"
}
