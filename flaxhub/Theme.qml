pragma Singleton

import QtQuick

QtObject {
    // Colors follow the Flax editor's default dark style (FlaxEngine, Source/Editor/Options/OptionsModule.cs, CreateDefaultStyle).
    readonly property color windowBg: "#1c1c1c"
    // The editor has no sidebar color, so this stays a step darker than windowBg.
    readonly property color sidebarBg: "#171717"
    readonly property color surface: "#2d2d30"
    readonly property color surfaceHover: "#333337"
    readonly property color border: "#54545c"
    // Text tokens reach WCAG AA (4.5:1) on windowBg, sidebarBg, surface and surfaceHover, and on the tinted StatusBadge over them.
    readonly property color text: "#ffffff"
    readonly property color textMuted: "#a9a9b3"
    // The editor's blue for selection borders and focus. White text on it is 3.15:1, so accentText stays dark. As text on surface it is 4.4:1.
    readonly property color accent: "#1c97ea"
    readonly property color accentText: "#111111"
    readonly property color warning: "#ffa500"
    // The editor's reds are about 3:1 or less as text on these backgrounds, so danger keeps a lighter red that reads as text.
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
