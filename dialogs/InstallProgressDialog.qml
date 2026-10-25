import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQml.Models
import FlaxHub

// Shows an install's steps while it runs. It is centred on the same overlay as AddInstallDialog,
// so it takes that dialog's place when the install starts.
Dialog {
    id: root

    required property InstallListModel installs

    property string version: ""
    property string path: ""

    readonly property var stepLabels: [qsTr("Clone repository"), qsTr("Generate project files"), qsTr("Build editor")]

    function openFor(version, path) {
        root.version = version;
        root.path = path;
        open();
    }

    // The status of one step, given the step the install is on.
    function stepStatus(index, installState, currentStep) {
        if (installState === InstallListModel.Installed || index < currentStep)
            return "done";
        if (index > currentStep || installState === InstallListModel.Available)
            return "pending";
        return installState === InstallListModel.Failed ? "failed" : "running";
    }

    // The install this dialog shows. Its state, step and log change as it runs.
    SortFilterProxyModel {
        id: currentInstall

        sourceModel: root.installs
        filters: ValueFilter {
            roleName: "version"
            value: root.version
        }
    }

    // The same install, while one of its steps is still running. Any row means the install is in progress.
    SortFilterProxyModel {
        id: runningInstall

        sourceModel: currentInstall
        filters: AnyOfFilter {
            ValueFilter { roleName: "state"; value: InstallListModel.Cloning }
            ValueFilter { roleName: "state"; value: InstallListModel.Generating }
            ValueFilter { roleName: "state"; value: InstallListModel.Building }
        }
    }

    // A proxy model has no count of its own, so an Instantiator counts its rows.
    Instantiator {
        id: runningRows

        model: runningInstall
        delegate: QtObject {}
    }

    readonly property bool installing: runningRows.count > 0

    title: qsTr("Installing engine")
    modal: true
    width: 440
    padding: Theme.spaceL
    anchors.centerIn: Overlay.overlay

    // Escape and a click outside close the dialog only once the install has stopped running.
    closePolicy: installing ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceM

        Text {
            Layout.fillWidth: true
            text: qsTr("FlaxEngine %1").arg(root.version)
            color: Theme.text
            font.pixelSize: Theme.fontBody
            font.bold: true
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: root.path
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideMiddle
        }

        // The filter leaves at most one row, so this creates at most one block.
        Repeater {
            model: currentInstall

            delegate: ColumnLayout {
                id: install

                readonly property int installState: model.state
                readonly property int currentStep: model.step
                readonly property real stepProgress: model.stepProgress
                readonly property string lastLine: model.log.trim().split("\n").pop()

                Layout.fillWidth: true
                spacing: Theme.spaceS

                Repeater {
                    model: root.stepLabels

                    delegate: InstallStep {
                        required property int index
                        required property string modelData

                        Layout.fillWidth: true
                        text: modelData
                        status: root.stepStatus(index, install.installState, install.currentStep)
                        progress: install.stepProgress
                        detail: status === "running" ? install.lastLine : ""
                    }
                }
            }
        }
    }

    // The dialog's padding only reaches the content, so the footer gets its own side and bottom margin.
    footer: Item {
        implicitHeight: buttons.implicitHeight + Theme.spaceL

        RowLayout {
            id: buttons

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.spaceL
            anchors.rightMargin: Theme.spaceL
            anchors.bottomMargin: Theme.spaceL
            spacing: Theme.spaceS

            Item {
                Layout.fillWidth: true
            }

            HubButton {
                visible: !root.installing
                text: qsTr("Close")
                primary: true
                onClicked: root.close()
            }
        }
    }
}
