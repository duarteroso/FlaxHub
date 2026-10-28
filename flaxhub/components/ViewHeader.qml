import QtQuick
import QtQuick.Layouts
import FlaxHub

RowLayout {
    id: root

    property string title

    spacing: Theme.spaceS

    Text {
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontHeading
        font.bold: true
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
}
