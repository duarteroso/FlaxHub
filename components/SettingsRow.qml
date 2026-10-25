import QtQuick
import QtQuick.Layouts
import FlaxHub

// One line of Settings: the label on the left, then the controls for it. Children are the controls.
// `labelExtras` goes after the label, for something such as a help link.
RowLayout {
    id: root

    // The label text. Each control in the row gives its own accessible name from it.
    required property string label
    property alias labelExtras: labelRow.data
    default property alias controls: controlRow.data

    Layout.fillWidth: true
    spacing: Theme.spaceM

    Row {
        id: labelRow

        Layout.preferredWidth: Theme.labelWidth
        spacing: Theme.spaceXs

        Text {
            text: root.label
            color: Theme.text
            font.pixelSize: Theme.fontBody
            Accessible.role: Accessible.StaticText
            Accessible.name: root.label
        }
    }

    RowLayout {
        id: controlRow

        Layout.fillWidth: true
        spacing: Theme.spaceM
    }
}
