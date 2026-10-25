import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FlaxHub

// Asks before an engine's folder is deleted, since that cannot be undone. The uninstall itself runs
// in the Installs view, where the row shows its progress.
Dialog {
    id: root

    required property InstallListModel installs

    property string version: ""
    property string path: ""

    function openFor(version, path) {
        root.version = version;
        root.path = path;
        open();
    }

    title: qsTr("Uninstall engine")
    modal: true
    width: 440
    padding: Theme.spaceL
    anchors.centerIn: Overlay.overlay

    // Cancel has the focus, so pressing Enter without reading the dialog does not delete anything.
    onOpened: cancelButton.forceActiveFocus()

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceM

        Text {
            Layout.fillWidth: true
            text: qsTr("Delete FlaxEngine %1? This permanently deletes the folder below and everything in it. This can't be undone.").arg(root.version)
            color: Theme.text
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            text: root.path
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideMiddle
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
                id: cancelButton

                text: qsTr("Cancel")
                onClicked: root.close()
            }

            HubButton {
                text: qsTr("Uninstall")
                danger: true
                onClicked: {
                    root.installs.uninstall(root.version);
                    root.close();
                }
            }
        }
    }
}
