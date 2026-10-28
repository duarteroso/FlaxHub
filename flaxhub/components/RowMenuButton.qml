import QtQuick
import QtQuick.Controls.Basic

// The "more" button at the end of a row. Its children are the items of the menu it opens.
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

        // Popups don't reliably follow palette changes, so a menu can keep the theme it was created in.
        // Following the button's palette keeps it in step with the theme.
        palette: root.palette
    }
}
