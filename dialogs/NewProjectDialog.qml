import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQml.Models
import FlaxHub

Dialog {
    id: root

    required property ProjectListModel projects
    required property InstallListModel installs
    required property var prefs

    // The folder the project folder is created in. Starts at the saved projects folder each time the dialog opens.
    property string location: ""
    property string errorMessage: ""

    // The name the editor gives the project: its .flaxproj file takes this, while the folder keeps the typed name.
    readonly property string projectName: root.projects.sanitizedName(nameField.text.trim())
    readonly property string engineVersion: engineBox.currentText
    readonly property bool canCreate: projectName !== "" && location !== "" && engineVersion !== "" && !projects.creating

    function openFor() {
        nameField.text = "";
        location = prefs.projectsDirectory;
        errorMessage = "";
        open();
    }

    title: qsTr("New project")
    modal: true
    width: 440
    padding: Theme.spaceL
    anchors.centerIn: Overlay.overlay
    // The editor keeps running while it creates the project, so the dialog stays open until it finishes.
    closePolicy: projects.creating ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    // Only installed engines can make a project. The default engine sorts first, so it is the one picked.
    SortFilterProxyModel {
        id: installedEngines

        sourceModel: root.installs
        filters: ValueFilter {
            roleName: "state"
            value: InstallListModel.Installed
        }
        sorters: [
            RoleSorter {
                roleName: "isDefault"
                sortOrder: Qt.DescendingOrder
            }
        ]
    }

    onAboutToShow: engineBox.currentIndex = engineBox.count > 0 ? 0 : -1

    Connections {
        target: root.projects

        function onCreated() {
            root.close();
        }

        function onCreateFailed(message) {
            root.errorMessage = message;
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceM

        Text {
            text: qsTr("Project name")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
        }

        TextField {
            id: nameField

            Layout.fillWidth: true
            enabled: !root.projects.creating
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.accentText
            leftPadding: Theme.spaceM
            rightPadding: Theme.spaceM
            placeholderText: qsTr("My game")

            background: Rectangle {
                implicitHeight: 36
                radius: Theme.radius
                color: Theme.surface
                border.width: 1
                border.color: nameField.activeFocus ? Theme.accent : Theme.border
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.projectName !== ""
                  ? qsTr("The project file is %1.flaxproj").arg(root.projectName)
                  : qsTr("Use at least one letter or digit.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }

        Text {
            text: qsTr("Location")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
        }

        FolderField {
            Layout.fillWidth: true
            enabled: !root.projects.creating
            path: root.location
            dialogTitle: qsTr("Choose project location")
            onChosen: folder => root.location = folder
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("The project folder is created inside this location.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }

        Text {
            text: qsTr("Engine version")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
        }

        ComboBox {
            id: engineBox

            Layout.fillWidth: true
            enabled: !root.projects.creating
            model: installedEngines
            textRole: "version"
        }

        Text {
            visible: engineBox.count === 0
            Layout.fillWidth: true
            text: qsTr("No engine is installed. Install one in the Installs view first.")
            color: Theme.warning
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }

        Text {
            visible: root.projects.creating
            Layout.fillWidth: true
            text: qsTr("The editor is creating the project and closes when it is done.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }

        Text {
            visible: root.errorMessage !== ""
            Layout.fillWidth: true
            text: root.errorMessage
            color: Theme.danger
            font.pixelSize: Theme.fontSmall
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
                text: qsTr("Cancel")
                enabled: !root.projects.creating
                onClicked: root.close()
            }

            HubButton {
                text: root.projects.creating ? qsTr("Creating…") : qsTr("Create")
                primary: true
                enabled: root.canCreate
                onClicked: {
                    root.errorMessage = "";
                    root.projects.create(nameField.text, root.location, root.engineVersion);
                }
            }
        }
    }
}
