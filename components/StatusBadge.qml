import QtQuick
import FlaxHub

Rectangle {
    id: root

    property alias text: label.text
    property color tone: Theme.warning

    implicitWidth: label.implicitWidth + Theme.spaceM * 2
    implicitHeight: label.implicitHeight + Theme.spaceXs * 2
    radius: height / 2
    color: Qt.alpha(tone, 0.15)

    Text {
        id: label

        anchors.centerIn: parent
        color: root.tone
        font.pixelSize: Theme.fontSmall
        font.bold: true
    }
}
