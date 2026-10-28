import QtQuick
import QtQuick.Layouts
import FlaxHub

// One step of an install. The marker shows its status. A running step shows a bar under its label,
// indeterminate while the step cannot report progress, and an optional detail line under that. A failed
// step shows its detail too, in the danger colour and wrapped, since the reason can be longer than a line.
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

            Layout.preferredWidth: Theme.markerWidth
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

    HubProgressBar {
        visible: root.status === "running"
        Layout.fillWidth: true
        Layout.leftMargin: markerText.width + Theme.spaceS
        indeterminate: root.progress < 0
        value: root.progress
        Accessible.name: qsTrId("main.installstep.progress_of").arg(label.text)
    }

    Text {
        id: detailText

        visible: (root.status === "running" || root.status === "failed") && text !== ""
        Layout.fillWidth: true
        Layout.leftMargin: markerText.width + Theme.spaceS
        color: root.status === "failed" ? Theme.danger : Theme.textMuted
        font.pixelSize: Theme.fontSmall
        wrapMode: root.status === "failed" ? Text.Wrap : Text.NoWrap
        elide: root.status === "failed" ? Text.ElideNone : Text.ElideRight
        // The detail is the last lines of program output, which are plain text.
        textFormat: Text.PlainText
    }
}
