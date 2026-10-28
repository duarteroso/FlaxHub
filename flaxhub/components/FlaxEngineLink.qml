pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import FlaxHub

// Opens the Flax Engine website from the sidebar. Its icon is the Flax Engine logo in its own white, followed by an
// external-link icon to show that the link leaves the app. Collapsed, the sidebar is too narrow for the wordmark and
// that icon, so only the emblem, the circle at the left of the logo, shows.
AbstractButton {
    id: root

    property bool collapsed: false

    text: qsTrId("main.flaxenginelink.website")
    implicitHeight: Theme.controlHeight
    padding: Theme.spaceS
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    // The logo has no label, so the tooltip names the link whether the sidebar is collapsed or not.
    ToolTip.visible: root.hovered || root.visualFocus
    ToolTip.text: root.text
    onClicked: Qt.openUrlExternally("https://flaxengine.com/")

    contentItem: RowLayout {
        spacing: root.collapsed ? 0 : Theme.spaceM

        // The two spacers center the logo and the icon in the button, or the emblem alone when collapsed.
        Item {
            Layout.fillWidth: true
        }

        Item {
            id: logo

            // The shown part of the logo is the emblem, the 367 pixel circle at (208, 18) in the image, when collapsed, or the
            // emblem and the wordmark, which ends at 1270, when expanded. The item is as wide as that part at its height, so
            // the image is scaled to that height, and the margins around it fall outside the item and are clipped.
            readonly property real shownWidth: root.collapsed ? 367 : 1062
            readonly property real iconScale: Math.min(width / shownWidth, height / 367)

            implicitHeight: Theme.iconSize
            implicitWidth: shownWidth * implicitHeight / 367
            clip: true

            Image {
                x: (logo.width - logo.shownWidth * logo.iconScale) / 2 - 208 * logo.iconScale
                y: (logo.height - 367 * logo.iconScale) / 2 - 18 * logo.iconScale
                width: 1500 * logo.iconScale
                height: 400 * logo.iconScale
                source: Qt.resolvedUrl("../icons/Logo_Title.png")
                // Rasterize at device pixels, as Icon does.
                sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
            }
        }

        // Half the size of the sidebar icons, so it reads as a small mark beside the logo.
        Icon {
            visible: !root.collapsed
            implicitWidth: Theme.iconSize * 0.75
            implicitHeight: Theme.iconSize * 0.75
            source: Qt.resolvedUrl("../icons/open_in_new.svg")
            color: Theme.textMuted
        }

        Item {
            Layout.fillWidth: true
        }
    }

    background: Rectangle {
        radius: Theme.radius
        color: root.hovered ? Theme.surfaceHover : "transparent"
        border.width: root.visualFocus ? Theme.focusRingWidth : 0
        border.color: Theme.accent
    }
}
