import QtQuick
import QtQuick.Layouts
import FlaxHub

// Buttons placed inside this element are laid out below the title and detail.
ColumnLayout {
    id: root

    property string title
    property string detail

    spacing: Theme.spaceS

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Theme.spaceXl
        text: root.title
        color: Theme.text
        font.pixelSize: Theme.fontTitle
        font.bold: true
        textFormat: Text.PlainText
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: Theme.readableWidth
        text: root.detail
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }
}
