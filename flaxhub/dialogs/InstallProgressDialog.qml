pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQml.Models
import FlaxHub

// Shows an install's steps while it runs. It is centred on the same overlay as AddInstallDialog,
// so it takes that dialog's place when the install starts.
HubDialog {
    id: root

    required property InstallListModel installs

    property string version: ""
    property string path: ""

    readonly property var stepLabels: [
        qsTrId("main.installprogressdialog.step_clone"),
        qsTrId("main.installprogressdialog.step_generate"),
        qsTrId("main.installprogressdialog.step_build")
    ]

    function openFor(version, path) {
        root.version = version;
        root.path = path;
        open();
    }

    // The status of one step, given the install's state and the step it is on. The steps before the current one have
    // finished. The current one is running while the install runs it, and failed if it fails. Once the install has
    // passed it, the step is done, whether the checks then pass (Installed) or not (Invalid). The steps after it did not run.
    function stepStatus(index, installState, currentStep) {
        switch (installState) {
        case InstallListModel.Cloning:
        case InstallListModel.Generating:
        case InstallListModel.Building:
            return index < currentStep ? "done" : index === currentStep ? "running" : "pending";
        case InstallListModel.Failed:
            return index < currentStep ? "done" : index === currentStep ? "failed" : "pending";
        case InstallListModel.Installed:
            return "done";
        case InstallListModel.Checking:
        case InstallListModel.Invalid:
            return index <= currentStep ? "done" : "pending";
        default:
            // Available and Removing have no step outcome to show.
            return "pending";
        }
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
        filters: FunctionFilter {
            function filter(state: int): bool {
                return state === InstallListModel.Cloning
                    || state === InstallListModel.Generating
                    || state === InstallListModel.Building;
            }
        }
    }

    // A proxy model has no count of its own, so an Instantiator counts its rows.
    Instantiator {
        id: runningRows

        model: runningInstall
        delegate: QtObject {}
    }

    readonly property bool installing: runningRows.count > 0

    title: qsTrId("main.installprogressdialog.title")
    // Close is the default. It is hidden while the install runs, so Enter does nothing then.
    defaultButton: closeButton

    // Escape and a click outside close the dialog only once the install has stopped running.
    closePolicy: installing ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Text {
        Layout.fillWidth: true
        text: qsTrId("main.installprogressdialog.engine_name").arg(root.version)
        color: Theme.text
        font.pixelSize: Theme.fontBody
        font.bold: true
        elide: Text.ElideRight
    }

    Text {
        Layout.fillWidth: true
        text: FileManager.displayPath(root.path)
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        elide: Text.ElideMiddle
        textFormat: Text.PlainText
    }

    // The filter leaves at most one row, so this creates at most one block.
    Repeater {
        model: currentInstall

        delegate: ColumnLayout {
            id: install

            // The state is read through `model`, because `state` is also a property of every Item.
            required property var model
            required property int step
            required property real stepProgress
            required property string log
            required property string problem

            readonly property int installState: install.model.state

            // The detail line under a step with this status. A running step shows the log's last line. A failed step
            // shows the last two: its failure message, and the line above it, which usually holds the program's error.
            function detailFor(stepStatus) {
                if (stepStatus !== "running" && stepStatus !== "failed")
                    return "";

                const lines = install.log.trim().split("\n");
                return stepStatus === "running" ? lines.pop() : lines.slice(-2).join("\n");
            }

            Layout.fillWidth: true
            spacing: Theme.spaceS

            Repeater {
                model: root.stepLabels

                delegate: InstallStep {
                    required property int index
                    required property string modelData

                    Layout.fillWidth: true
                    text: modelData
                    status: root.stepStatus(index, install.installState, install.step)
                    progress: install.stepProgress
                    detail: root.visible ? install.detailFor(status) : ""
                }
            }

            // The steps ran, but the folder failed the checks afterwards, so the reasons are shown under the steps.
            Text {
                visible: install.installState === InstallListModel.Invalid
                Layout.fillWidth: true
                text: qsTrId("main.installprogressdialog.checks_failed").arg(install.problem)
                color: Theme.warning
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
        }
    }

    buttons: [
        HubButton {
            id: closeButton

            visible: !root.installing
            text: qsTrId("main.installprogressdialog.close")
            primary: true
            onClicked: root.close()
        }
    ]
}
