import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import FlaxHub

// One step of an install. The marker shows its status. A running step shows a bar under its label,
// indeterminate while the step cannot report progress, and an optional detail line under that.
ColumnLayout {
    id: root

    // "pending", "running", "done" or "failed".
    property string status: "pending"
    property alias text: label.text
    property alias detail: detailText.text
    // 0..1, or -1 when the step cannot report progress.
    property real progress: -1

    readonly property string marker: {
        switch (root.status) {
        case "running": return "●";
        case "done": return "✓";
        case "failed": return "✕";
        default: return "○";
        }
    }

    readonly property color tone: {
        switch (root.status) {
        case "running": return Theme.accent;
        case "done": return Theme.text;
        case "failed": return Theme.danger;
        default: return Theme.textMuted;
        }
    }

    spacing: Theme.spaceXs

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spaceS

        Text {
            id: markerText

            Layout.preferredWidth: 16
            horizontalAlignment: Text.AlignHCenter
            text: root.marker
            color: root.tone
            font.pixelSize: Theme.fontBody
            font.bold: true
        }

        Text {
            id: label

            Layout.fillWidth: true
            color: root.status === "pending" ? Theme.textMuted : Theme.text
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
        }
    }

    ProgressBar {
        // The style fills the bar with palette.dark and draws the track with palette.midlight. Both are set on the bar
        // only, because palette.dark also outlines popups and palette.midlight is the pressed-item color.
        palette.dark: Theme.accent
        palette.midlight: Theme.border
        visible: root.status === "running"
        Layout.fillWidth: true
        Layout.leftMargin: markerText.width + Theme.spaceS
        from: 0
        to: 1
        indeterminate: root.progress < 0
        value: root.progress
    }

    Text {
        id: detailText

        visible: root.status === "running" && text !== ""
        Layout.fillWidth: true
        Layout.leftMargin: markerText.width + Theme.spaceS
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        elide: Text.ElideRight
    }
}
