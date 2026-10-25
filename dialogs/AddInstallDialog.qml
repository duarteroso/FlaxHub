pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQml.Models
import FlaxHub

HubDialog {
    id: root

    required property InstallListModel installs
    required property AppSettings prefs

    property string preselectedVersion: ""

    // Emitted once the install has started, so the progress dialog can take this one's place.
    signal installStarted(string version, string path)

    // The version to install. The menu sets it, and the dialog starts with the one it opened for.
    property string selectedVersion: ""

    // Explains an empty version list while GitHub is being asked, or after it failed.
    readonly property string versionStatus: {
        switch (root.installs.versionFetchState) {
        case InstallListModel.Fetching: return qsTrId("main.addinstalldialog.status_loading");
        case InstallListModel.FetchFailed: return qsTrId("main.addinstalldialog.status_fetch_failed");
        default: return "";
        }
    }

    // The destination. It starts at the saved install folder and changes only when Browse picks another folder.
    property string destination: ""
    // The destination without spaces around it. Empty when nothing is chosen.
    readonly property string destinationPath: root.destination.trim()
    // The folder the engine is cloned into, inside the destination. Empty until a destination and a version are set.
    readonly property string installPath: root.selectedVersion !== "" ? root.installs.installPathFor(root.selectedVersion, root.destination) : ""
    // A full path starts at a root, a drive or the home folder. The model refuses any other.
    readonly property bool destinationIsFull: /^([\/\\~]|[A-Za-z]:)/.test(root.destinationPath)
    // The folder must be missing or empty, so the install can go ahead. Without a path nothing can be installed.
    // canInstallTo is not a binding, so the folder is checked by refreshFolder instead.
    property bool folderFree: false
    // Why Install cannot go ahead, or empty.
    readonly property string warning: {
        if (root.selectedVersion === "")
            return "";
        if (!root.folderFree)
            return root.destinationPath === "" ? qsTrId("main.addinstalldialog.destination_empty")
                 : !root.destinationIsFull ? qsTrId("main.addinstalldialog.destination_not_full_path")
                                           : qsTrId("main.addinstalldialog.destination_not_empty");
        return "";
    }

    // Checks the folder again. It runs when the destination or the version changes, when the dialog opens, and when the app
    // is active again, since the folder can be filled or emptied while the dialog is open.
    function refreshFolder() {
        root.folderFree = root.installPath !== "" && root.installs.canInstallTo(root.installPath);
    }

    onInstallPathChanged: root.refreshFolder()

    function openFor(version) {
        preselectedVersion = version;
        destination = prefs.installDirectory;
        open();
    }

    title: qsTrId("main.addinstalldialog.title")
    // The version menu is the first control, so the focus starts there. Enter installs once the choice is valid.
    initialFocus: versionButton
    defaultButton: installButton

    onAboutToShow: {
        root.installs.fetchAvailableVersions();
        root.selectedVersion = root.installs.canInstall(preselectedVersion) ? preselectedVersion : "";
        // The version may not have changed, so the folder is checked here as well as on a change of installPath.
        root.refreshFolder();
    }

    // The folder can be filled or emptied outside the dialog, such as in a file manager, so it is checked again
    // when the app comes back to the front.
    Connections {
        target: Qt.application

        function onStateChanged() {
            if (root.opened && Qt.application.state === Qt.ApplicationActive)
                root.refreshFolder();
        }
    }

    // Only the newest versions are listed, installed or not. Older ones cannot be installed, so they are left out.
    SortFilterProxyModel {
        id: recentVersions

        sourceModel: root.installs
        filters: ValueFilter { roleName: "recent"; value: true }
    }

    // One version in the menu. Choosing it selects that version. Installed, running and failed versions are disabled.
    // A failed one is retried from the Installs view. An installed version shows a sync icon just left of its name, which
    // stays centered.
    Component {
        id: versionItem

        MenuItem {
            id: entry

            required property string version
            required property bool addable
            required property var model
            readonly property bool installed: model.state === InstallListModel.Installed

            text: version
            enabled: addable
            onTriggered: root.selectedVersion = version
            // Menu rows keep the style's left-aligned label, so each row replaces it with this centered one.
            contentItem: Text {
                text: entry.text
                color: entry.enabled ? entry.palette.windowText : entry.palette.placeholderText
                font: entry.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                // Placed from the centered name's own width, so the name itself does not move.
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    x: (parent.width - parent.contentWidth) / 2 - Theme.spaceS - width
                    visible: entry.installed
                    source: Qt.resolvedUrl("../icons/sync_saved_locally.svg")
                    color: Theme.accent
                }
            }
        }
    }

    Text {
        text: qsTrId("main.addinstalldialog.version_label")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
    }

    Button {
        id: versionButton

        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        implicitHeight: Theme.controlHeight
        leftPadding: Theme.spaceM
        rightPadding: Theme.spaceM
        hoverEnabled: true
        Accessible.name: qsTrId("main.addinstalldialog.version_label")
        onClicked: versionMenu.opened ? versionMenu.close() : versionMenu.open()
        // Enter opens the menu, and is accepted here so the dialog's default button never sees it.
        Keys.onReturnPressed: event => { clicked(); event.accepted = true; }
        Keys.onEnterPressed: event => { clicked(); event.accepted = true; }

        contentItem: RowLayout {
            spacing: Theme.spaceS

            Text {
                Layout.fillWidth: true
                text: root.selectedVersion !== "" ? root.selectedVersion : qsTrId("main.addinstalldialog.version_placeholder")
                color: root.selectedVersion !== "" ? Theme.text : Theme.textMuted
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                text: "▾"
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
            }
        }

        background: Rectangle {
            implicitHeight: Theme.controlHeight
            radius: Theme.radius
            color: versionButton.hovered ? Theme.surfaceHover : Theme.surface
            border.width: versionButton.visualFocus ? Theme.focusRingWidth : 0
            border.color: Theme.accent
        }

        Menu {
            id: versionMenu

            // Popups don't reliably follow palette changes, so a menu can keep the theme it was created in.
            // Following the button's palette keeps it in step with the theme.
            palette: parent.palette
            y: versionButton.height
            width: versionButton.width
            // Presses on the button don't close the menu, so onClicked can toggle it.
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

            Instantiator {
                model: recentVersions
                delegate: versionItem

                onObjectAdded: (index, object) => versionMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => versionMenu.removeItem(object)
            }
        }
    }

    Text {
        visible: root.versionStatus !== ""
        Layout.fillWidth: true
        text: root.versionStatus
        color: root.installs.versionFetchState === InstallListModel.FetchFailed ? Theme.danger : Theme.textMuted
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Text {
        text: qsTrId("main.addinstalldialog.location_label")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
    }

    FolderField {
        Layout.fillWidth: true
        label: qsTrId("main.addinstalldialog.location_label")
        path: root.destination
        dialogTitle: qsTrId("main.addinstalldialog.location_dialog_title")
        onChosen: folder => root.destination = folder
    }

    Text {
        Layout.fillWidth: true
        visible: root.warning !== ""
        text: root.warning
        color: Theme.danger
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        text: qsTrId("main.addinstalldialog.location_hint")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        visible: root.installs.busy
        text: qsTrId("main.addinstalldialog.busy_notice")
        color: Theme.textMuted
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    buttons: [
        HubButton {
            text: qsTrId("main.addinstalldialog.cancel")
            onClicked: root.close()
        },

        HubButton {
            id: installButton

            text: qsTrId("main.addinstalldialog.install")
            primary: true
            // Only one install runs at a time, so this waits for a running install to finish.
            enabled: root.selectedVersion !== "" && root.folderFree && !root.installs.busy
            onClicked: {
                // A refusal is reported by the model, and the dialog stays open so the user can choose another folder.
                if (!root.installs.startInstall(root.selectedVersion, root.installPath)) {
                    root.refreshFolder();
                    return;
                }
                root.installStarted(root.selectedVersion, root.installPath);
                root.close();
            }
        }
    ]
}
