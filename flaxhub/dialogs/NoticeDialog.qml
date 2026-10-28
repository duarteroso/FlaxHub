import QtQuick
import QtQuick.Layouts
import FlaxHub

// Shows a short message that the user acknowledges with OK. The owner calls show with a title and the text.
HubDialog {
    id: root

    property string message: ""

    function show(dialogTitle, text) {
        title = dialogTitle;
        message = text;
        open();
    }

    defaultButton: okButton

    Text {
        Layout.fillWidth: true
        text: root.message
        color: Theme.text
        font.pixelSize: Theme.fontBody
        wrapMode: Text.Wrap
        // The message can carry an error from the system, such as a path, so it is shown as plain text.
        textFormat: Text.PlainText
    }

    buttons: [
        HubButton {
            id: okButton

            text: qsTrId("main.noticedialog.ok")
            primary: true
            onClicked: root.close()
        }
    ]
}
