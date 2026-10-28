pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import FlaxHub

Rectangle {
    id: root

    property int currentIndex: 0
    property bool collapsed: false

    readonly property var sections: [
        { label: qsTrId("main.sidebar.projects"), iconSource: Qt.resolvedUrl("../icons/joystick.svg") },
        { label: qsTrId("main.sidebar.installs"), iconSource: Qt.resolvedUrl("../icons/install_desktop.svg") }
    ]
    // Settings sits right after the sections, and its view follows them in the StackLayout.
    readonly property int settingsIndex: sections.length

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
                font.pixelSize: Theme.fontHeading
            }

            Text {
                visible: !root.collapsed
                text: qsTrId("main.sidebar.brand")
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

        SidebarButton {
            Layout.fillWidth: true
            text: qsTrId("main.sidebar.settings")
            iconSource: Qt.resolvedUrl("../icons/settings.svg")
            current: root.currentIndex === root.settingsIndex
            collapsed: root.collapsed
            onClicked: root.currentIndex = root.settingsIndex
        }

        Item {
            Layout.fillHeight: true
        }

        FlaxEngineLink {
            Layout.fillWidth: true
            collapsed: root.collapsed
        }
    }
}
