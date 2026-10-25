import QtQuick
import QtQuick.Layouts
import FlaxHub

// A row card in the Projects and Installs lists. Its children are stacked in a column inside the margins,
// and the card is as tall as that column.
Rectangle {
    default property alias content: column.data

    implicitHeight: column.implicitHeight + Theme.spaceM * 2
    radius: Theme.radius
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: Theme.spaceM
        spacing: Theme.spaceS
    }
}
