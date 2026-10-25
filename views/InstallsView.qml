import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQml.Models
import FlaxHub

Rectangle {
    id: root

    required property InstallListModel installs
    required property var prefs

    signal addVersionRequested()
    signal uninstallRequested(string version, string path)
    // Emitted once a Fix has a step running, so the progress dialog can show it.
    signal fixStarted(string version, string path)

    // The install that Locate folder… is for. The dialog is shared by every row, so it remembers the version.
    property string relocatingVersion: ""

    color: Theme.windowBg

    function stateLabel(state) {
        switch (state) {
        case InstallListModel.Cloning: return qsTr("Cloning…");
        case InstallListModel.Generating: return qsTr("Generating…");
        case InstallListModel.Building: return qsTr("Building…");
        case InstallListModel.Removing: return qsTr("Removing…");
        case InstallListModel.Installed: return qsTr("Installed");
        case InstallListModel.Failed: return qsTr("Failed");
        case InstallListModel.Invalid: return qsTr("Invalid");
        default: return qsTr("Not installed");
        }
    }

    function stateColor(state) {
        switch (state) {
        case InstallListModel.Cloning:
        case InstallListModel.Generating:
        case InstallListModel.Building:
        case InstallListModel.Removing: return Theme.accent;
        case InstallListModel.Failed: return Theme.danger;
        case InstallListModel.Invalid: return Theme.warning;
        default: return Theme.textMuted;
        }
    }

    // Installed engines, plus installs that are running, being removed, have failed, or are no longer valid.
    SortFilterProxyModel {
        id: installedVersions

        sourceModel: root.installs
        filters: AnyOfFilter {
            ValueFilter { roleName: "state"; value: InstallListModel.Installed }
            ValueFilter { roleName: "state"; value: InstallListModel.Cloning }
            ValueFilter { roleName: "state"; value: InstallListModel.Generating }
            ValueFilter { roleName: "state"; value: InstallListModel.Building }
            ValueFilter { roleName: "state"; value: InstallListModel.Removing }
            ValueFilter { roleName: "state"; value: InstallListModel.Failed }
            ValueFilter { roleName: "state"; value: InstallListModel.Invalid }
        }
    }

    FolderDialog {
        id: folderDialog

        title: qsTr("Locate engine folder")
        onAccepted: {
            const folder = FileManager.toLocalPath(selectedFolder);
            if (folder !== "")
                root.installs.relocate(root.relocatingVersion, folder);
        }
    }

    // Runs the Fix for an Invalid install. The progress dialog opens only when a step started, since a fix that finds
    // the install passing is Installed already.
    function startFix(version, path, clearConfirmed) {
        if (root.installs.fix(version, clearConfirmed))
            root.fixStarted(version, path);
    }

    ClearFolderDialog {
        id: clearFolderDialog

        onConfirmed: (version, path) => root.startFix(version, path, true)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceL
        spacing: Theme.spaceM

        ViewHeader {
            Layout.fillWidth: true
            title: qsTr("Installs")

            HubButton {
                text: qsTr("Add version")
                primary: true
                onClicked: root.addVersionRequested()
            }
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

            delegate: Rectangle {
                id: row

                readonly property string version: model.version
                readonly property int installState: model.state
                readonly property bool isInstalling: installState === InstallListModel.Cloning
                                                  || installState === InstallListModel.Generating
                                                  || installState === InstallListModel.Building
                readonly property bool canInstall: installState === InstallListModel.Available
                                                || installState === InstallListModel.Failed
                readonly property bool canUninstall: installState === InstallListModel.Installed
                                                  || installState === InstallListModel.Failed
                                                  || installState === InstallListModel.Invalid
                readonly property bool canLocate: model.relocatable
                readonly property bool canFix: model.fixable

                width: list.width
                implicitHeight: rowContent.implicitHeight + Theme.spaceM * 2
                radius: Theme.radius
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: rowContent

                    anchors.fill: parent
                    anchors.margins: Theme.spaceM
                    spacing: Theme.spaceS

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceS

                        Text {
                            text: qsTr("FlaxEngine %1").arg(row.version)
                            color: Theme.text
                            font.pixelSize: Theme.fontBody + 2
                            font.bold: true
                        }

                        StatusBadge {
                            text: model.channel
                            tone: Theme.textMuted
                        }

                        StatusBadge {
                            visible: model.isDefault
                            text: qsTr("Default")
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
                            text: row.installState === InstallListModel.Failed ? qsTr("Retry") : qsTr("Install")
                            primary: true
                            onClicked: root.installs.startInstall(row.version, model.path !== ""
                                       ? model.path
                                       : root.prefs.installDirectory + "/" + row.version)
                        }

                        HubButton {
                            visible: row.canFix
                            // Only one install runs at a time, so a Fix waits for any running install to finish.
                            enabled: !root.installs.busy
                            text: qsTr("Fix")
                            primary: true
                            onClicked: {
                                // Fix deletes the files of a folder that is not a complete checkout, so the user confirms first.
                                if (root.installs.fixClearsFolder(row.version))
                                    clearFolderDialog.openFor(row.version, model.path);
                                else
                                    root.startFix(row.version, model.path, false);
                            }
                        }

                        HubButton {
                            visible: row.canLocate
                            text: qsTr("Locate folder…")
                            primary: true
                            onClicked: {
                                root.relocatingVersion = row.version;
                                folderDialog.currentFolder = FileManager.folderUrl(model.path);
                                folderDialog.open();
                            }
                        }

                        ToolButton {
                            text: "⋮"
                            onClicked: rowMenu.popup()

                            Menu {
                                id: rowMenu

                                // Popups don't reliably follow palette changes, so a menu can keep the theme it was created in.
                                // Following the button's palette keeps it in step with the theme.
                                palette: parent.palette

                                MenuItem {
                                    text: qsTr("Set as default")
                                    enabled: row.installState === InstallListModel.Installed && !model.isDefault
                                    onTriggered: root.installs.setDefault(row.version)
                                }

                                MenuItem {
                                    text: FileManager.revealLabel
                                    enabled: model.path !== ""
                                    onTriggered: FileManager.reveal(model.path)
                                }

                                MenuItem {
                                    text: row.installState === InstallListModel.Invalid ? qsTr("Remove from list") : qsTr("Uninstall")
                                    enabled: row.canUninstall
                                    onTriggered: {
                                        // Only an Invalid entry is removed without the dialog, since its folder is not deleted.
                                        if (row.installState !== InstallListModel.Invalid) {
                                            root.uninstallRequested(row.version, model.path);
                                            return;
                                        }

                                        // Deferred: removing an invalid entry moves this row to another tab and destroys it.
                                        const version = row.version;
                                        Qt.callLater(() => root.installs.uninstall(version));
                                    }
                                }
                            }
                        }
                    }

                    ProgressBar {
                        // The style fills the bar with palette.dark and draws the track with palette.midlight. Both are set on the bar
                        // only, because palette.dark also outlines popups and palette.midlight is the pressed-item color.
                        palette.dark: Theme.accent
                        palette.midlight: Theme.border
                        visible: row.isInstalling || row.installState === InstallListModel.Removing
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        indeterminate: model.stepProgress < 0
                        value: model.progress
                    }

                    Text {
                        visible: model.path !== ""
                        Layout.fillWidth: true
                        text: model.path
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideMiddle
                    }

                    Text {
                        visible: row.installState === InstallListModel.Invalid
                        Layout.fillWidth: true
                        text: model.problem
                        color: Theme.warning
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
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
                title: qsTr("No engines installed")
                detail: qsTr("Install a FlaxEngine version to open projects with it.")
            }
        }
    }
}
