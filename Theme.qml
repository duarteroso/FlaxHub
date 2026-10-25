pragma Singleton

import QtQuick

QtObject {
    // "system", "light" or "dark". Main.qml binds this to the saved preference.
    property string preference: "system"

    readonly property bool dark: preference === "dark"
        || (preference === "system" && Application.styleHints.colorScheme === Qt.Dark)

    readonly property color windowBg: dark ? "#1f1f1f" : "#e7e7e7"
    readonly property color sidebarBg: dark ? "#171717" : "#e0e0e0"
    readonly property color surface: dark ? "#262626" : "#f4f4f4"
    readonly property color surfaceHover: dark ? "#2d2d2d" : "#ebebeb"
    readonly property color border: dark ? "#363636" : "#cfcfcf"
    readonly property color text: dark ? "#e7e7e7" : "#1f1f1f"
    readonly property color textMuted: dark ? "#9b9b9b" : "#6b6b6b"
    readonly property color accent: dark ? "#4ba3ff" : "#4ba3ff" //"#1a6fd1"
    readonly property color accentText: dark ? "#111111" : "#ffffff"
    readonly property color warning: dark ? "#f0b429" : "#a86b00"
    readonly property color danger: dark ? "#ef6b6b" : "#c62828"

    readonly property int spaceXs: 4
    readonly property int spaceS: 8
    readonly property int spaceM: 12
    readonly property int spaceL: 16
    readonly property int spaceXl: 24

    readonly property int radius: 8
    readonly property int radiusSmall: 4

    readonly property int fontSmall: 12
    readonly property int fontBody: 14
    readonly property int fontTitle: 20
    readonly property int fontHeading: 24

    readonly property int sidebarWidth: 200
    readonly property int sidebarCollapsedWidth: 56
}
