import QtQuick
import QtQuick.Controls.Basic
import FlaxHub

Button {
    id: root

    property bool primary: false
    // A destructive action, such as deleting an engine. It is filled with the danger color.
    property bool danger: false

    implicitHeight: 36
    leftPadding: Theme.spaceM
    rightPadding: Theme.spaceM
    hoverEnabled: true

    contentItem: Text {
        text: root.text
        color: root.primary || root.danger ? Theme.accentText : Theme.text
        font.pixelSize: Theme.fontBody
        font.bold: root.primary || root.danger
        opacity: root.enabled ? 1 : 0.4
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Theme.radius
        color: root.danger ? (root.down ? Qt.darker(Theme.danger, 1.15) : Theme.danger)
             : root.primary ? (root.down ? Qt.darker(Theme.accent, 1.15) : Theme.accent)
             : (root.hovered ? Theme.surfaceHover : Theme.surface)
        opacity: root.enabled ? 1 : 0.5
    }
}
