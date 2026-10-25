import QtQuick
import QtQuick.Controls.Basic
import FlaxHub

Button {
    id: root

    property bool primary: false
    // A destructive action, such as deleting an engine. It is filled with the danger color.
    property bool danger: false

    implicitHeight: Theme.controlHeight
    leftPadding: Theme.spaceM
    rightPadding: Theme.spaceM
    hoverEnabled: true

    // Enter presses a focused button, as it does in a native dialog. The Basic style ignores Enter.
    Keys.onReturnPressed: (event) => {
        event.accepted = true;
        root.click();
    }
    Keys.onEnterPressed: (event) => {
        event.accepted = true;
        root.click();
    }

    contentItem: Text {
        text: root.text
        color: root.primary || root.danger ? Theme.accentText : Theme.text
        font.pixelSize: Theme.fontBody
        font.bold: root.primary || root.danger
        opacity: root.enabled ? 1 : Theme.disabledTextOpacity
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Theme.radius
        color: root.danger ? (root.down ? Qt.darker(Theme.danger, 1.15) : Theme.danger)
             : root.primary ? (root.down ? Qt.darker(Theme.accent, 1.15) : Theme.accent)
             : (root.hovered ? Theme.surfaceHover : Theme.surface)
        opacity: root.enabled ? 1 : Theme.disabledFillOpacity
        // The focus ring shows only for keyboard focus. It is white on a filled button, where the accent would not show.
        border.width: root.visualFocus ? Theme.focusRingWidth : 0
        border.color: root.primary || root.danger ? Theme.accentText : Theme.accent
    }
}
