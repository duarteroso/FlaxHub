import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FlaxHub

Rectangle {
    id: root

    required property var prefs

    color: Theme.windowBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceL
        spacing: Theme.spaceM

        ViewHeader {
            Layout.fillWidth: true
            title: qsTr("Settings")
        }

        Text {
            Layout.topMargin: Theme.spaceS
            text: qsTr("Appearance")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceM

            Text {
                Layout.preferredWidth: 200
                text: qsTr("Theme")
                color: Theme.text
                font.pixelSize: Theme.fontBody
            }

            RadioButton {
                text: qsTr("System")
                checked: root.prefs.theme === "system"
                onClicked: root.prefs.theme = "system"
            }

            RadioButton {
                text: qsTr("Light")
                checked: root.prefs.theme === "light"
                onClicked: root.prefs.theme = "light"
            }

            RadioButton {
                text: qsTr("Dark")
                checked: root.prefs.theme === "dark"
                onClicked: root.prefs.theme = "dark"
            }

            Item {
                Layout.fillWidth: true
            }
        }

        Text {
            Layout.topMargin: Theme.spaceS
            text: qsTr("Locations")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceM

            Text {
                Layout.preferredWidth: 200
                text: qsTr("Engine installs folder")
                color: Theme.text
                font.pixelSize: Theme.fontBody
            }

            FolderField {
                Layout.fillWidth: true
                path: root.prefs.installDirectory
                dialogTitle: qsTr("Choose engine installs folder")
                onChosen: folder => root.prefs.installDirectory = folder
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceM

            Text {
                Layout.preferredWidth: 200
                text: qsTr("Projects folder")
                color: Theme.text
                font.pixelSize: Theme.fontBody
            }

            FolderField {
                Layout.fillWidth: true
                path: root.prefs.projectsDirectory
                dialogTitle: qsTr("Choose projects folder")
                onChosen: folder => root.prefs.projectsDirectory = folder
            }
        }

        Text {
            Layout.topMargin: Theme.spaceS
            text: qsTr("Editor")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceM

            Row {
                Layout.preferredWidth: 200
                spacing: Theme.spaceXs

                Text {
                    text: qsTr("Extra launch arguments")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                }

                Text {
                    text: "↗"
                    color: docsMouse.containsMouse ? Theme.accent : Theme.textMuted
                    font.pixelSize: Theme.fontBody
                    ToolTip.visible: docsMouse.containsMouse
                    ToolTip.text: qsTr("Command-line access documentation")

                    MouseArea {
                        id: docsMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://docs.flaxengine.com/manual/editor/advanced/command-line-access.html")
                    }
                }
            }

            TextField {
                Layout.fillWidth: true
                text: root.prefs.editorArguments
                placeholderText: qsTr("For example: -debug")
                onEditingFinished: root.prefs.editorArguments = text
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
