import QtQuick
import QtQuick.Layouts
import FlaxHub

Rectangle {
    id: root

    property int currentIndex: 0
    property bool collapsed: false

    readonly property var sections: [
        { label: qsTr("Projects"), iconSource: Qt.resolvedUrl("../icons/joystick.svg") },
        { label: qsTr("Installs"), iconSource: Qt.resolvedUrl("../icons/install_desktop.svg") },
        { label: qsTr("Settings"), iconSource: Qt.resolvedUrl("../icons/settings.svg") }
    ]

    Layout.preferredWidth: collapsed ? Theme.sidebarCollapsedWidth : Theme.sidebarWidth
    color: Theme.sidebarBg

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceS
        spacing: Theme.spaceXs

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spaceXs
            Layout.bottomMargin: Theme.spaceM
            spacing: Theme.spaceS

            Item {
                Layout.fillWidth: root.collapsed
            }

            Text {
                text: "⬡"
                color: Theme.accent
                font.pixelSize: 24
            }

            Text {
                visible: !root.collapsed
                text: qsTr("FlaxHub")
                color: Theme.text
                font.pixelSize: Theme.fontTitle
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Item {
                Layout.fillWidth: root.collapsed
            }
        }

        Repeater {
            model: root.sections

            delegate: SidebarButton {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                text: modelData.label
                iconSource: modelData.iconSource
                current: root.currentIndex === index
                collapsed: root.collapsed
                onClicked: root.currentIndex = index
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
