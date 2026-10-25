import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import FlaxHub

// A folder path that can be typed or picked with Browse. The path is passed on through `chosen` when typing ends or
// Browse picks a folder, not on each keystroke. Browse opens at the folder typed in the field.
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
        Accessible.name: root.label
        onEditingFinished: root.chosen(text)
    }

    // Shows the path again when the owner changes it from outside, such as when a dialog reopens with its saved folder.
    // Typing breaks a plain binding, so this one restores itself on the next change.
    Binding {
        target: field
        property: "text"
        value: root.path
        restoreMode: Binding.RestoreBinding
    }

    HubButton {
        text: qsTrId("main.folderfield.browse")
        onClicked: {
            folderDialog.currentFolder = FileManager.folderUrl(field.text);
            folderDialog.open();
        }
    }

    FolderDialog {
        id: folderDialog

        title: root.dialogTitle
        onAccepted: {
            const folder = FileManager.toLocalPath(selectedFolder);
            if (folder === "")
                return;

            field.text = folder;
            root.chosen(folder);
        }
    }
}
