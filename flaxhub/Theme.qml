pragma Singleton

import QtQuick

QtObject {
    readonly property color windowBg: "#1f1f1f"
    readonly property color sidebarBg: "#171717"
    readonly property color surface: "#262626"
    readonly property color surfaceHover: "#2d2d2d"
    readonly property color border: "#363636"
    // Text tokens reach WCAG AA (4.5:1) on windowBg, sidebarBg, surface and surfaceHover, and on the tinted StatusBadge over them.
    readonly property color text: "#e7e7e7"
    readonly property color textMuted: "#ababab"
    readonly property color accent: "#4ba3ff"
    readonly property color accentText: "#111111"
    readonly property color warning: "#f0b429"
    readonly property color danger: "#ef6b6b"

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
