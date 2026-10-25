import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FlaxHub

// Shows a short message that the user acknowledges with OK. The owner calls show with a title and the text.
Dialog {
    id: root

    property string message: ""

    function show(dialogTitle, text) {
        title = dialogTitle;
        message = text;
        open();
    }

    modal: true
    width: 440
    padding: Theme.spaceL
    anchors.centerIn: Overlay.overlay

    // OK has the focus, so pressing Enter closes the dialog.
    onOpened: okButton.forceActiveFocus()

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceM

        Text {
            Layout.fillWidth: true
            text: root.message
            color: Theme.text
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }
    }

    // The dialog's padding only reaches the content, so the footer gets its own side and bottom margin.
    footer: Item {
        implicitHeight: buttons.implicitHeight + Theme.spaceL

        RowLayout {
            id: buttons

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.spaceL
            anchors.rightMargin: Theme.spaceL
            anchors.bottomMargin: Theme.spaceL

            Item {
                Layout.fillWidth: true
            }

            HubButton {
                id: okButton

                text: qsTr("OK")
                primary: true
                onClicked: root.close()
            }
        }
    }
}
