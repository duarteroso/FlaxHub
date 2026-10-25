import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQml.Models
import FlaxHub

Dialog {
    id: root

    required property InstallListModel installs
    required property var prefs

    property string preselectedVersion: ""

    // Emitted once the install has started, so the progress dialog can take this one's place.
    signal installStarted(string version, string path)

    // The version to install. The menu sets it, and the dialog starts with the one it opened for.
    property string selectedVersion: ""

    // Explains an empty version list while GitHub is being asked, or after it failed.
    readonly property string versionStatus: {
        switch (root.installs.versionFetchState) {
        case InstallListModel.Fetching: return qsTr("Loading versions from GitHub…");
        case InstallListModel.FetchFailed: return qsTr("Could not load versions from GitHub. Reopen this dialog to try again.");
        default: return "";
        }
    }

    function openFor(version) {
        preselectedVersion = version;
        destination.text = prefs.installDirectory;
        open();
    }

    title: qsTr("Add engine version")
    modal: true
    width: 440
    padding: Theme.spaceL
    anchors.centerIn: Overlay.overlay

    onAboutToShow: {
        root.installs.fetchAvailableVersions();
        root.selectedVersion = root.installs.canInstall(preselectedVersion) ? preselectedVersion : "";
    }

    // Every version is listed, installed or not, so the list stays the same as installs change.
    // The newest are listed first. The rest go under Legacy.
    SortFilterProxyModel {
        id: recentVersions

        sourceModel: root.installs
        filters: ValueFilter { roleName: "recent"; value: true }
    }

    SortFilterProxyModel {
        id: legacyVersions

        sourceModel: root.installs
        filters: ValueFilter { roleName: "recent"; value: false }
    }

    // Menu rows keep the style's left-aligned label, so each row swaps in this centered one.
    Component {
        id: menuLabel

        Text {
            text: parent.text
            color: parent.enabled ? parent.palette.windowText : parent.palette.placeholderText
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // One version in the menu. Choosing it selects that version. Installed and running versions are disabled.
    Component {
        id: versionItem

        MenuItem {
            id: entry

            required property string version
            required property bool installable

            text: version
            enabled: installable
            onTriggered: root.selectedVersion = version
            Component.onCompleted: contentItem = menuLabel.createObject(entry)
        }
    }

    FolderDialog {
        id: folderDialog

        title: qsTr("Choose install location")
        onAccepted: destination.text = selectedFolder.toString().replace(/^file:\/\//, "")
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.spaceM

        Text {
            text: qsTr("Engine version")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
        }

        Button {
            id: versionButton

            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            implicitHeight: 36
            leftPadding: Theme.spaceM
            rightPadding: Theme.spaceM
            hoverEnabled: true
            onClicked: versionMenu.opened ? versionMenu.close() : versionMenu.open()

            contentItem: RowLayout {
                spacing: Theme.spaceS

                Text {
                    Layout.fillWidth: true
                    text: root.selectedVersion !== "" ? root.selectedVersion : qsTr("Choose a version")
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
                implicitHeight: 36
                radius: Theme.radius
                color: versionButton.hovered ? Theme.surfaceHover : Theme.surface
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

                Menu {
                    id: legacyMenu

                    title: qsTr("Legacy")
                    visible: false

                    Instantiator {
                        id: legacyItems

                        model: legacyVersions
                        delegate: versionItem

                        onObjectAdded: (index, object) => legacyMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => legacyMenu.removeItem(object)
                    }
                }

                // Qt builds the Legacy row itself, so its label is swapped here.
                Component.onCompleted: {
                    for (let i = 0; i < count; ++i) {
                        const row = itemAt(i);
                        if (row.subMenu === legacyMenu)
                            row.contentItem = menuLabel.createObject(row);
                    }
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
            text: qsTr("Install location")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceS

            TextField {
                id: destination

                Layout.fillWidth: true
                color: Theme.text
                selectionColor: Theme.accent
                selectedTextColor: Theme.accentText
                leftPadding: Theme.spaceM
                rightPadding: Theme.spaceM

                background: Rectangle {
                    implicitHeight: 36
                    radius: Theme.radius
                    color: Theme.surface
                    border.width: 1
                    border.color: destination.activeFocus ? Theme.accent : Theme.border
                }
            }

            HubButton {
                text: qsTr("Browse…")
                onClicked: folderDialog.open()
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("The engine is built in its own folder inside this location.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
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
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            HubButton {
                text: qsTr("Install")
                primary: true
                enabled: root.selectedVersion !== ""
                onClicked: {
                    const path = destination.text + "/" + root.selectedVersion;
                    root.installs.startInstall(root.selectedVersion, path);
                    root.installStarted(root.selectedVersion, path);
                    root.close();
                }
            }
        }
    }
}
