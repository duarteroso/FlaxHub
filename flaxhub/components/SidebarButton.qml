import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FlaxHub

AbstractButton {
    id: root

    property url iconSource
    property bool current: false
    property bool collapsed: false

    implicitHeight: Theme.controlHeight
    padding: Theme.spaceS
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    // Collapsed, the button shows only its icon, so the tooltip names it. Its text is still its accessible name.
    ToolTip.visible: root.hovered && root.collapsed
    ToolTip.text: root.text

    contentItem: RowLayout {
        // Collapsed, the spacing is zero so the two filling spacers center the icon.
        spacing: root.collapsed ? 0 : Theme.spaceM

        Item {
            Layout.fillWidth: root.collapsed
        }

        Icon {
            source: root.iconSource
            color: root.current ? Theme.accent : Theme.textMuted
        }

        Text {
            visible: !root.collapsed
            text: root.text
            color: root.current ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontBody
            font.bold: root.current
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth: root.collapsed
        }
    }

    background: Rectangle {
        radius: Theme.radius
        color: root.current ? Theme.surface
             : root.hovered ? Theme.surfaceHover
             : "transparent"
        border.width: root.visualFocus ? Theme.focusRingWidth : 0
        border.color: Theme.accent
    }
}
