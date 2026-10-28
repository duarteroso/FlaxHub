import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FlaxHub

Rectangle {
    id: root

    required property AppSettings prefs

    color: Theme.windowBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceL
        spacing: Theme.spaceM

        ViewHeader {
            Layout.fillWidth: true
            title: qsTrId("main.settingsview.title")
        }

        SectionLabel {
            text: qsTrId("main.settingsview.locations")
        }

        SettingsRow {
            id: installsRow

            label: qsTrId("main.settingsview.install_folder")

            FolderField {
                Layout.fillWidth: true
                label: installsRow.label
                path: root.prefs.installDirectory
                dialogTitle: qsTrId("main.settingsview.install_folder_dialog_title")
                onChosen: folder => root.prefs.installDirectory = folder
            }
        }

        SettingsRow {
            id: projectsRow

            label: qsTrId("main.settingsview.projects_folder")

            FolderField {
                Layout.fillWidth: true
                label: projectsRow.label
                path: root.prefs.projectsDirectory
                dialogTitle: qsTrId("main.settingsview.projects_folder_dialog_title")
                onChosen: folder => root.prefs.projectsDirectory = folder
            }
        }

        SectionLabel {
            text: qsTrId("main.settingsview.editor")
        }

        SettingsRow {
            id: argumentsRow

            label: qsTrId("main.settingsview.launch_arguments")

            // A link to the editor's command-line documentation. It is a button, so the keyboard can reach it.
            labelExtras: AbstractButton {
                id: docsLink

                padding: 0
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.name: qsTrId("main.settingsview.command_line_docs")
                ToolTip.visible: docsLink.hovered || docsLink.visualFocus
                ToolTip.text: qsTrId("main.settingsview.command_line_docs")
                onClicked: Qt.openUrlExternally("https://docs.flaxengine.com/manual/editor/advanced/command-line-access.html")

                // Three quarters of the usual icon size, so it reads as a small mark beside the label.
                contentItem: Icon {
                    implicitWidth: Theme.iconSize * 0.75
                    implicitHeight: Theme.iconSize * 0.75
                    source: Qt.resolvedUrl("../icons/open_in_new.svg")
                    color: docsLink.hovered || docsLink.visualFocus ? Theme.accent : Theme.textMuted
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: "transparent"
                    border.width: docsLink.visualFocus ? Theme.focusRingWidth : 0
                    border.color: Theme.accent
                }
            }

            HubTextField {
                Layout.fillWidth: true
                Accessible.name: argumentsRow.label
                text: root.prefs.editorArguments
                placeholderText: qsTrId("main.settingsview.launch_arguments_placeholder")
                onTextEdited: root.prefs.editorArguments = text
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
