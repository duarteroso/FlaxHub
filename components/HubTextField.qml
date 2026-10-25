import QtQuick
import QtQuick.Controls.Basic
import FlaxHub

// A single-line field in the hub's style: a surface with a border that turns to the accent while it has focus.
TextField {
    id: root

    implicitHeight: Theme.controlHeight
    leftPadding: Theme.spaceM
    rightPadding: Theme.spaceM
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText

    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.width: 1
        border.color: root.activeFocus ? Theme.accent : Theme.border
    }
}
