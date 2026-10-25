import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FlaxHub

// Asks before the window closes while an engine is still installing. Quitting cancels the install, so the
// folder it was writing to may be left partly written.
Dialog {
    id: root

    // Emitted when the user chooses to quit. The dialog closes first.
    signal quitAnyway()

    title: qsTr("Install in progress")
    modal: true
    width: 440
    padding: Theme.spaceL
    anchors.centerIn: Overlay.overlay

    // Keep installing has the focus, so pressing Enter without reading the dialog does not quit.
    onOpened: keepButton.forceActiveFocus()

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceM

        Text {
            Layout.fillWidth: true
            text: qsTr("An engine is still installing. Quitting cancels the install, and the folder it was writing to may be left partly written.")
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
            spacing: Theme.spaceS

            Item {
                Layout.fillWidth: true
            }

            HubButton {
                id: keepButton

                text: qsTr("Keep installing")
                onClicked: root.close()
            }

            HubButton {
                text: qsTr("Quit anyway")
                danger: true
                onClicked: {
                    root.close();
                    root.quitAnyway();
                }
            }
        }
    }
}
