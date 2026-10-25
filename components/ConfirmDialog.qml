import QtQuick
import QtQuick.Layouts
import FlaxHub

// Asks before something that cannot be undone: deleting an engine folder, clearing one, or quitting while work runs.
// Cancel is the default and takes the focus, so Enter never confirms. The owner sets `question` and `confirmText`,
// and listens for `confirmed`, which is emitted after the dialog closes.
HubDialog {
    id: root

    // The version and folder the question is about. openFor sets them.
    property string version: ""
    property string path: ""
    // The question, shown above the path. Owners bind it, since the wording can use `version`.
    property string question: ""
    // The label of the confirming button, which is styled as a danger action.
    property string confirmText: ""
    property string cancelText: qsTrId("main.confirmdialog.cancel")

    // Emitted only when the user confirms, with the version and path that openFor set.
    signal confirmed(string version, string path)

    function openFor(version, path) {
        root.version = version;
        root.path = path;
        root.open();
    }

    defaultButton: cancelButton

    Text {
        Layout.fillWidth: true
        text: root.question
        color: Theme.text
        font.pixelSize: Theme.fontBody
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        visible: root.path !== ""
        text: root.path
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        elide: Text.ElideMiddle
        textFormat: Text.PlainText
    }

    buttons: [
        HubButton {
            id: cancelButton

            text: root.cancelText
            onClicked: root.close()
        },
        HubButton {
            text: root.confirmText
            danger: true
            onClicked: {
                root.close();
                root.confirmed(root.version, root.path);
            }
        }
    ]
}
