import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FlaxHub

HubDialog {
    id: root

    required property ProjectListModel projects
    required property InstallListModel installs
    required property var installedEngines
    required property AppSettings prefs

    // The folder the project folder is created in. Starts at the saved projects folder each time the dialog opens.
    property string location: ""

    // The name the editor gives the project: its .flaxproj file takes this, while the folder keeps the typed name.
    readonly property string projectName: root.projects.sanitizedName(nameField.text.trim())
    readonly property string engineVersion: engineBox.currentText
    readonly property bool canCreate: projectName !== "" && location !== "" && engineVersion !== "" && !projects.creating

    function openFor() {
        nameField.text = "";
        location = prefs.projectsDirectory;
        open();
    }

    title: qsTrId("main.newprojectdialog.title")
    initialFocus: nameField
    defaultButton: createButton
    // The editor keeps running while it creates the project, so the dialog stays open until it finishes.
    closePolicy: projects.creating ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    // The box keeps the text of the engine it showed when its index last changed, and a re-sorted list does not
    // change the index. Resetting it first makes the box read the sorted list, so the default engine is the one shown.
    onAboutToShow: {
        engineBox.currentIndex = -1;
        engineBox.currentIndex = engineBox.count > 0 ? 0 : -1;
    }

    Connections {
        target: root.projects

        function onCreated() {
            root.close();
        }
    }

    Text {
        text: qsTrId("main.newprojectdialog.name_label")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
    }

    HubTextField {
        id: nameField

        Layout.fillWidth: true
        enabled: !root.projects.creating
        placeholderText: qsTrId("main.newprojectdialog.name_placeholder")
        Accessible.name: qsTrId("main.newprojectdialog.name_label")
    }

    Text {
        Layout.fillWidth: true
        text: root.projectName !== ""
              ? qsTrId("main.newprojectdialog.file_name_hint").arg(root.projectName)
              : qsTrId("main.newprojectdialog.name_invalid")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Text {
        text: qsTrId("main.newprojectdialog.location_label")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
    }

    FolderField {
        Layout.fillWidth: true
        enabled: !root.projects.creating
        label: qsTrId("main.newprojectdialog.location_label")
        path: root.location
        dialogTitle: qsTrId("main.newprojectdialog.location_dialog_title")
        onChosen: folder => root.location = folder
    }

    Text {
        Layout.fillWidth: true
        text: qsTrId("main.newprojectdialog.location_hint")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Text {
        text: qsTrId("main.newprojectdialog.engine_label")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
    }

    ComboBox {
        id: engineBox

        Layout.fillWidth: true
        enabled: !root.projects.creating
        model: root.installedEngines
        textRole: "version"
        Accessible.name: qsTrId("main.newprojectdialog.engine_label")
    }

    Text {
        visible: engineBox.count === 0
        Layout.fillWidth: true
        text: qsTrId("main.newprojectdialog.no_engine")
        color: Theme.warning
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Text {
        visible: root.projects.creating
        Layout.fillWidth: true
        text: qsTrId("main.newprojectdialog.creating_hint")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    buttons: [
        HubButton {
            text: qsTrId("main.newprojectdialog.cancel")
            enabled: !root.projects.creating
            onClicked: root.close()
        },

        HubButton {
            id: createButton

            text: root.projects.creating ? qsTrId("main.newprojectdialog.creating") : qsTrId("main.newprojectdialog.create")
            primary: true
            enabled: root.canCreate
            onClicked: root.projects.create(nameField.text, root.location, root.engineVersion)
        }
    ]
}
