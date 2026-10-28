import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import FlaxHub

// The shell of the hub's dialogs: the width, the padding, a body column and a footer of buttons on the right.
// Children go in the body, in order. `buttons` holds the footer buttons. Enter presses `defaultButton` while the
// focus is on a field or on the body; a focused HubButton presses itself. `initialFocus` takes the focus when the
// dialog opens, and defaults to `defaultButton`.
Dialog {
    id: root

    // The button Enter presses. Enter does nothing while it is null, disabled or hidden.
    property T.AbstractButton defaultButton: null
    // The item that takes the focus when the dialog opens. Null, or a control that is hidden or disabled, leaves it on the body.
    property Item initialFocus: defaultButton
    // The footer buttons. They follow a spacer, so they sit on the right in the order they are declared.
    property alias buttons: buttonRow.data
    // The body. Its children are stacked in a column as wide as the dialog.
    default property alias content: column.data

    modal: true
    width: Theme.dialogWidth
    padding: Theme.spaceL
    anchors.centerIn: Overlay.overlay

    onOpened: {
        // A hidden or disabled control can't take the focus, so the body takes it instead and Enter still reaches it.
        // Items other than controls have no `enabled`, so only an explicit false counts as disabled.
        if (root.initialFocus && root.initialFocus.visible && root.initialFocus.enabled !== false)
            root.initialFocus.forceActiveFocus();
        else
            body.forceActiveFocus();
    }

    // Keys can't be attached to a Dialog, so the body is a focus scope that passes Enter to the default button.
    // Enter is always accepted here, so the dialog itself never sees it.
    function pressDefault(event) {
        event.accepted = true;

        const button = root.defaultButton;
        if (button && button.enabled && button.visible)
            button.click();
    }

    contentItem: FocusScope {
        id: body

        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight

        Keys.onReturnPressed: (event) => root.pressDefault(event)
        Keys.onEnterPressed: (event) => root.pressDefault(event)

        ColumnLayout {
            id: column

            width: root.availableWidth
            spacing: Theme.spaceM
        }
    }

    // The dialog's padding only reaches the content, so the footer gets its own side and bottom margin.
    footer: Item {
        implicitHeight: buttonRow.implicitHeight + Theme.spaceL

        RowLayout {
            id: buttonRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.spaceL
            anchors.rightMargin: Theme.spaceL
            anchors.bottomMargin: Theme.spaceL
            spacing: Theme.spaceS

            Item {
                Layout.fillWidth: true
            }
        }
    }
}
