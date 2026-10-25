import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQml.Models
import FlaxHub

Rectangle {
    id: root

    required property ProjectListModel projects
    required property InstallListModel installs
    required property var prefs

    signal installRequested(string version)
    signal newProjectRequested()

    color: Theme.windowBg

    // Both groups count, so the empty state shows only when no project is listed.
    readonly property int projectCount: pinnedRows.count + otherRows.count

    function escapeRegExp(text) {
        return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    }

    // Only installed engines can be picked for a project.
    SortFilterProxyModel {
        id: installedEngines

        sourceModel: root.installs
        filters: ValueFilter {
            roleName: "state"
            value: InstallListModel.Installed
        }
    }

    // Searched and sorted once. The two groups below split it by pin, so both follow the search and the sort without
    // ever mixing.
    SortFilterProxyModel {
        id: visibleProjects

        sourceModel: root.projects
        filters: RegExpFilter {
            roleName: "name"
            regExp: new RegExp(root.escapeRegExp(searchField.text), "i")
        }
        sorters: [
            RoleSorter {
                roleName: "lastOpened"
                sortOrder: Qt.DescendingOrder
                enabled: sortBox.currentIndex === 0
            },
            RoleSorter {
                roleName: "name"
                enabled: sortBox.currentIndex === 1
            }
        ]
    }

    SortFilterProxyModel {
        id: pinnedProjects

        sourceModel: visibleProjects
        filters: ValueFilter {
            roleName: "pinned"
            value: true
        }
    }

    SortFilterProxyModel {
        id: otherProjects

        sourceModel: visibleProjects
        filters: ValueFilter {
            roleName: "pinned"
            value: false
        }
    }

    FolderDialog {
        id: addFolderDialog

        title: qsTr("Add existing project")
        onAccepted: {
            const folder = FileManager.toLocalPath(selectedFolder);
            if (folder !== "")
                root.projects.addExisting(folder);
        }
    }

    NoticeDialog {
        id: addNoticeDialog
    }

    Connections {
        target: root.projects

        function onAddFailed(message) {
            addNoticeDialog.show(qsTr("Could not add project"), message);
        }
    }

    // One project row. The pinned group and the other projects both show it.
    Component {
        id: projectRow

        Rectangle {
            id: row

            // Copied out of `model` so nested controls can use them: ComboBox has its own `model` property.
            readonly property string projectPath: model.path
            readonly property string engineVersion: model.engineVersion
            readonly property bool engineInstalled: model.engineInstalled

            function setEngineVersion(version) {
                model.engineVersion = version;
            }

            width: list.width
            implicitHeight: content.implicitHeight + Theme.spaceM * 2
            radius: Theme.radius
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            RowLayout {
                id: content

                anchors.fill: parent
                anchors.margins: Theme.spaceM
                spacing: Theme.spaceM

                ToolButton {
                    contentItem: Icon {
                        source: model.pinned ? Qt.resolvedUrl("../icons/star_filled.svg") : Qt.resolvedUrl("../icons/star.svg")
                        color: model.pinned ? Theme.accent : Theme.text
                    }
                    onClicked: {
                        // Deferred: pinning moves the row to the other group, which destroys this delegate.
                        const path = row.projectPath;
                        const pinned = !model.pinned;
                        Qt.callLater(() => root.projects.setPinned(path, pinned));
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: model.name
                        color: Theme.text
                        font.pixelSize: Theme.fontBody + 2
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: row.projectPath
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideMiddle
                    }
                }

                // The picker always shows the project's own version, even when that engine is not installed.
                ComboBox {
                    id: picker

                    model: installedEngines
                    textRole: "version"
                    currentIndex: -1
                    Layout.preferredWidth: 120

                    contentItem: Text {
                        leftPadding: Theme.spaceS
                        text: row.engineVersion
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    onActivated: (index) => row.setEngineVersion(textAt(index))
                }

                StatusBadge {
                    visible: !row.engineInstalled
                    text: qsTr("Not installed")
                    tone: Theme.warning
                }

                HubButton {
                    visible: !row.engineInstalled
                    text: qsTr("Install…")
                    onClicked: root.installRequested(row.engineVersion)
                }

                HubButton {
                    text: qsTr("Open")
                    primary: true
                    enabled: row.engineInstalled
                    onClicked: root.projects.open(row.projectPath)
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
                            text: FileManager.revealLabel
                            onTriggered: FileManager.reveal(row.projectPath)
                        }

                        MenuItem {
                            text: qsTr("Remove from list")
                            onTriggered: {
                                // Deferred so the delegate that owns this menu is not destroyed mid-handler.
                                const path = row.projectPath;
                                Qt.callLater(() => root.projects.remove(path));
                            }
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceL
        spacing: Theme.spaceM

        ViewHeader {
            Layout.fillWidth: true
            title: qsTr("Projects")

            HubButton {
                text: qsTr("Add existing")
                onClicked: {
                    addFolderDialog.currentFolder = FileManager.folderUrl(root.prefs.projectsDirectory);
                    addFolderDialog.open();
                }
            }

            HubButton {
                text: qsTr("New project")
                primary: true
                onClicked: root.newProjectRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceS

            SearchField {
                id: searchField

                Layout.fillWidth: true
                placeholderText: qsTr("Search projects")
            }

            ComboBox {
                id: sortBox

                model: [qsTr("Last opened"), qsTr("Name")]
            }
        }

        // The pinned projects come first and scroll with the rest, above a line that sets them apart.
        Flickable {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.projectCount > 0
            clip: true
            contentWidth: width
            contentHeight: groups.height

            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: groups

                width: list.width
                height: implicitHeight
                spacing: Theme.spaceS

                Repeater {
                    id: pinnedRows

                    model: pinnedProjects
                    delegate: projectRow
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.border
                    visible: pinnedRows.count > 0 && otherRows.count > 0
                }

                Repeater {
                    id: otherRows

                    model: otherProjects
                    delegate: projectRow
                }
            }
        }

        // Wrapped in an Item: fillHeight does not stretch the nested ColumnLayout, so the
        // column centred everything and the search row moved with the empty state's height.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.projectCount === 0

            EmptyState {
                anchors.centerIn: parent
                title: searchField.text.length > 0
                       ? qsTr("No projects match \"%1\"").arg(searchField.text)
                       : qsTr("No projects yet")
                detail: searchField.text.length > 0
                        ? qsTr("Try a different search.")
                        : qsTr("Create a new project or add one from disk.")
            }
        }
    }
}
