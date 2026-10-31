pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQml.Models
import FlaxHub

Rectangle {
    id: root

    required property InstallListModel installs
    required property AppSettings prefs

    signal addVersionRequested()
    // `invalid` is true for an Invalid entry, which is removed from the list and whose folder is deleted only when allowed.
    signal uninstallRequested(string version, string path, bool invalid)
    // Emitted once an install or a Fix has a step running, so the progress dialog can show it.
    signal installStarted(string version, string path)

    // The install that Locate folder… is for. The dialog is shared by every row, so it remembers the version.
    property string relocatingVersion: ""

    color: Theme.windowBg

    function stateLabel(state) {
        switch (state) {
        case InstallListModel.Cloning: return qsTrId("main.installsview.state_cloning");
        case InstallListModel.Generating: return qsTrId("main.installsview.state_generating");
        case InstallListModel.Building: return qsTrId("main.installsview.state_building");
        case InstallListModel.Removing: return qsTrId("main.installsview.state_removing");
        case InstallListModel.Checking: return qsTrId("main.installsview.state_checking");
        case InstallListModel.Installed: return qsTrId("main.installsview.state_installed");
        case InstallListModel.Failed: return qsTrId("main.installsview.state_failed");
        case InstallListModel.Invalid: return qsTrId("main.installsview.state_invalid");
        default: return qsTrId("main.installsview.state_not_installed");
        }
    }

    function stateColor(state) {
        switch (state) {
        case InstallListModel.Cloning:
        case InstallListModel.Generating:
        case InstallListModel.Building:
        case InstallListModel.Removing:
        case InstallListModel.Checking: return Theme.accent;
        case InstallListModel.Failed: return Theme.danger;
        case InstallListModel.Invalid: return Theme.warning;
        default: return Theme.textMuted;
        }
    }

    // The last two non-empty lines of an install's log. A failed install's log ends with its reason, and the line above
    // it usually holds the program's error.
    function failureDetail(log) {
        return log.split("\n").filter(line => line.trim() !== "").slice(-2).join("\n");
    }

    // Installed engines, plus installs that are running, being checked or removed, have failed, or are no longer valid.
    SortFilterProxyModel {
        id: installedVersions

        sourceModel: root.installs
        filters: FunctionFilter {
            function filter(state: int): bool {
                return state === InstallListModel.Installed
                    || state === InstallListModel.Cloning
                    || state === InstallListModel.Generating
                    || state === InstallListModel.Building
                    || state === InstallListModel.Checking
                    || state === InstallListModel.Removing
                    || state === InstallListModel.Failed
                    || state === InstallListModel.Invalid;
            }
        }
    }

    FolderDialog {
        id: folderDialog

        title: qsTrId("main.installsview.locate_title")
        // A folder that is refused is reported by the model's errorOccurred.
        onAccepted: {
            const folder = FileManager.toLocalPath(selectedFolder);
            if (folder !== "")
                root.installs.relocate(root.relocatingVersion, folder);
        }
    }

    // A Fix that has started a step is passed on as installStarted, so the window opens the progress dialog.
    Connections {
        target: root.installs

        function onFixStarted(version, path) {
            root.installStarted(version, path);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceL
        spacing: Theme.spaceM

        ViewHeader {
            Layout.fillWidth: true
            title: qsTrId("main.installsview.title")

            HubButton {
                text: qsTrId("main.installsview.add_version")
                primary: true
                onClicked: root.addVersionRequested()
            }
        }

        // Install, Retry and Fix are disabled while an install runs, so this says why they are off.
        Text {
            visible: root.installs.busy
            Layout.fillWidth: true
            text: qsTrId("main.installsview.busy_notice")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.Wrap
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: count > 0
            clip: true
            spacing: Theme.spaceS
            model: installedVersions

            ScrollBar.vertical: ScrollBar {}

            delegate: HubCard {
                id: row

                // The install's state is read through `model`, because `state` is also a property of every Item.
                required property var model
                required property int index
                required property string version
                required property string channel
                required property bool isDefault
                required property string path
                required property string problem
                required property string log
                required property real progress
                required property real stepProgress
                required property bool relocatable
                required property bool fixable
                required property bool installable

                readonly property int installState: row.model.state
                readonly property bool isInstalling: installState === InstallListModel.Cloning
                                                  || installState === InstallListModel.Generating
                                                  || installState === InstallListModel.Building
                // A retry is offered only for the newest versions, which is what the model's installable role says.
                readonly property bool canInstall: row.installable
                readonly property bool canUninstall: installState === InstallListModel.Installed
                                                  || installState === InstallListModel.Failed
                                                  || installState === InstallListModel.Invalid
                readonly property bool canLocate: row.relocatable
                readonly property bool canFix: row.fixable

                width: list.width

                // A button in this row can take the focus, so the row scrolls itself into view.
                Connections {
                    target: row.Window.window

                    function onActiveFocusItemChanged() {
                        for (let item = row.Window.window.activeFocusItem; item; item = item.parent) {
                            if (item === row) {
                                list.positionViewAtIndex(row.index, ListView.Contain);
                                return;
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spaceS

                    Text {
                        text: qsTrId("main.installsview.engine_name").arg(row.version)
                        color: Theme.text
                        font.pixelSize: Theme.fontSubtitle
                        font.bold: true
                    }

                    StatusBadge {
                        text: row.channel
                        tone: Theme.textMuted
                    }

                    StatusBadge {
                        visible: row.isDefault
                        text: qsTrId("main.installsview.default_badge")
                        tone: Theme.accent
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        text: root.stateLabel(row.installState)
                        color: root.stateColor(row.installState)
                        font.pixelSize: Theme.fontSmall
                        font.bold: true
                    }

                    HubButton {
                        visible: row.canInstall
                        // Only one install runs at a time, so a Retry or Install waits for any running install to finish.
                        enabled: !root.installs.busy
                        text: row.installState === InstallListModel.Failed ? qsTrId("main.installsview.retry") : qsTrId("main.installsview.install")
                        primary: true
                        onClicked: {
                            // Without a path the model uses its default folder. A refusal is reported by the model.
                            const folder = row.path !== "" ? row.path : root.installs.installPathFor(row.version, root.prefs.installDirectory);
                            if (root.installs.startInstall(row.version, folder))
                                root.installStarted(row.version, folder);
                        }
                    }

                    HubButton {
                        visible: row.canFix
                        // Only one install runs at a time, so a Fix waits for any running install to finish.
                        enabled: !root.installs.busy
                        text: qsTrId("main.installsview.fix")
                        primary: true
                        // Fix deletes the files of a folder that is not a complete checkout, so the model asks the user first.
                        onClicked: root.installs.fix(row.version, false)
                    }

                    HubButton {
                        visible: row.canLocate
                        text: qsTrId("main.installsview.locate")
                        primary: true
                        onClicked: {
                            root.relocatingVersion = row.version;
                            folderDialog.currentFolder = FileManager.folderUrl(row.path);
                            folderDialog.open();
                        }
                    }

                    RowMenuButton {
                        MenuItem {
                            text: qsTrId("main.installsview.set_default")
                            enabled: row.installState === InstallListModel.Installed && !row.isDefault
                            onTriggered: root.installs.setDefault(row.version)
                        }

                        MenuItem {
                            text: FileManager.revealLabel
                            enabled: row.path !== ""
                            onTriggered: FileManager.reveal(row.path)
                        }

                        MenuItem {
                            text: row.installState === InstallListModel.Invalid ? qsTrId("main.installsview.remove_from_list") : qsTrId("main.installsview.uninstall")
                            enabled: row.canUninstall
                            // Uninstall and Remove from list both delete the engine's folder, so the item is red. Disabled, it stays grey.
                            palette.windowText: Theme.danger
                            palette.disabled.windowText: Theme.textMuted
                            onTriggered: root.uninstallRequested(row.version, row.path, row.installState === InstallListModel.Invalid)
                        }
                    }
                }

                HubProgressBar {
                    visible: row.isInstalling || row.installState === InstallListModel.Removing
                    Layout.fillWidth: true
                    indeterminate: row.stepProgress < 0
                    value: row.progress
                    Accessible.name: qsTrId("main.installsview.progress_label").arg(row.version)
                }

                Text {
                    visible: row.path !== ""
                    Layout.fillWidth: true
                    text: FileManager.displayPath(row.path)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideMiddle
                    textFormat: Text.PlainText
                }

                Text {
                    visible: row.installState === InstallListModel.Invalid
                    Layout.fillWidth: true
                    text: row.problem
                    color: Theme.warning
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }

                // A failed install's reason is in its log, which the progress dialog shows only while it is open.
                Text {
                    visible: row.installState === InstallListModel.Failed
                    Layout.fillWidth: true
                    text: visible ? root.failureDetail(row.log) : ""
                    color: Theme.danger
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }

        // Wrapped in an Item: fillHeight does not stretch the nested ColumnLayout, so the
        // column centred everything and the header moved with the empty state's height.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: list.count === 0

            EmptyState {
                anchors.centerIn: parent
                title: qsTrId("main.installsview.empty_title")
                detail: qsTrId("main.installsview.empty_detail")
            }
        }
    }
}
