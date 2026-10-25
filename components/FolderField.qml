import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import FlaxHub

// A folder path that is shown, not typed. A click on it copies the path. Browse picks another folder and passes it on
// through `chosen`, and Browse opens at the folder shown.
RowLayout {
    id: root

    required property string path
    required property string dialogTitle
    // The accessible name of the field. The owner passes the text of the label beside it.
    required property string label

    signal chosen(string folder)

    spacing: Theme.spaceM

    HubTextField {
        id: field

        Layout.fillWidth: true
        readOnly: true
        text: FileManager.displayPath(root.path)
        Accessible.name: root.label
        ToolTip.visible: copyArea.containsMouse
        ToolTip.text: copiedTimer.running ? qsTrId("main.folderfield.copied") : qsTrId("main.folderfield.copy_hint")

        // The field takes no typing, so a click anywhere on it copies the path. The mouse area covers the field, so a
        // click does not place a caret. A keyboard user can still reach the field and select the text to copy it.
        MouseArea {
            id: copyArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                FileManager.copyPath(root.path);
                copiedTimer.restart();
            }
        }
    }

    // How long the tooltip says the path was copied.
    Timer {
        id: copiedTimer

        interval: 1500
    }

    HubButton {
        text: qsTrId("main.folderfield.browse")
        onClicked: {
            folderDialog.currentFolder = FileManager.folderUrl(root.path);
            folderDialog.open();
        }
    }

    FolderDialog {
        id: folderDialog

        title: root.dialogTitle
        onAccepted: {
            const folder = FileManager.toLocalPath(selectedFolder);
            if (folder !== "")
                root.chosen(folder);
        }
    }
}
