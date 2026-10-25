import QtQuick
import QtQuick.Controls.Basic
import FlaxHub

TextField {
    id: root

    implicitHeight: 36
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
