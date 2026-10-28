pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import QtQuick.Window
import FlaxHub

// Shows a single-color SVG in `color`. The SVG must be filled #ffffff: MultiEffect colorization
// tints by luminance, so white maps to `color` and the shape's alpha (antialiasing) is kept.
// Don't mask a Rectangle with the SVG instead: the mask thresholds the alpha, so edges go bold and aliased.
Item {
    id: root

    property url source
    property color color: Theme.text

    implicitWidth: Theme.iconSize
    implicitHeight: Theme.iconSize

    Image {
        anchors.fill: parent
        source: root.source
        // Rasterize at device pixels. Without this the SVG is drawn at its 24px size and stretched.
        sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
        layer.enabled: true
        layer.smooth: true
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: root.color
        }
    }
}
