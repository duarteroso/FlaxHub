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
    // Text tokens reach WCAG AA (4.5:1) on windowBg, sidebarBg, surface and surfaceHover, and on the tinted StatusBadge over them.
    readonly property color text: dark ? "#e7e7e7" : "#1f1f1f"
    readonly property color textMuted: dark ? "#ababab" : "#565656"
    // The light accent is dark enough for white text on it. Its StatusBadge tint (Qt.alpha 0.09) is what keeps its text at 4.5:1.
    readonly property color accent: dark ? "#4ba3ff" : "#135bab"
    readonly property color accentText: dark ? "#111111" : "#ffffff"
    readonly property color warning: dark ? "#f0b429" : "#784d00"
    readonly property color danger: dark ? "#ef6b6b" : "#bf2727"

    readonly property int spaceXxs: 2
    readonly property int spaceXs: 4
    readonly property int spaceS: 8
    readonly property int spaceM: 12
    readonly property int spaceL: 16
    readonly property int spaceXl: 24

    readonly property int radius: 8
    readonly property int radiusSmall: 4

    readonly property int fontSmall: 12
    readonly property int fontBody: 14
    readonly property int fontSubtitle: 16
    readonly property int fontTitle: 20
    readonly property int fontHeading: 24

    // Sizes of controls and of the layouts they sit in.
    readonly property int controlHeight: 36
    readonly property int dialogWidth: 440
    readonly property int labelWidth: 200
    readonly property int comboWidth: 120
    readonly property int markerWidth: 16
    readonly property int iconSize: 20
    readonly property int focusRingWidth: 2
    // The widest a line of text runs in an empty state, so it stays readable.
    readonly property int readableWidth: 420

    // Opacity of disabled content (text and icons) and of a disabled button's fill.
    readonly property real disabledTextOpacity: 0.4
    readonly property real disabledFillOpacity: 0.5

    readonly property int sidebarWidth: 200
    readonly property int sidebarCollapsedWidth: 56
    // The window width under which the sidebar shows only icons.
    readonly property int sidebarCollapseBreakpoint: 760
}
