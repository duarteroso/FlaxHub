import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import FlaxHub

// A folder path that can be typed or picked with Browse. The owner saves the value it gets from chosen.
RowLayout {
    id: root

    required property string path
    required property string dialogTitle

    signal chosen(string folder)

    spacing: Theme.spaceM

    TextField {
        Layout.fillWidth: true
        text: root.path
        onEditingFinished: root.chosen(text)
    }

    HubButton {
        text: qsTr("Browse…")
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
