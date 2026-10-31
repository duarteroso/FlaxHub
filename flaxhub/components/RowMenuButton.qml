import QtQuick
import QtQuick.Controls.Basic

// The "more" button at the end of a row. Its children are the items of the menu it opens.
// The menu keeps the space of a hidden item, so an item that can be hidden sets `height: visible ? implicitHeight : 0`.
ToolButton {
    id: root

    default property alias items: menu.contentData

    text: "⋮"
    hoverEnabled: true
    Accessible.name: qsTrId("main.rowmenubutton.more_actions")
    ToolTip.visible: root.hovered
    ToolTip.text: qsTrId("main.rowmenubutton.more_actions")
    onClicked: menu.popup(root, 0, root.height)

    Menu {
        id: menu

        // The menu takes the button's palette, so it uses the window's colors.
        palette: root.palette
    }
}
