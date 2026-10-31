pragma ComponentBehavior: Bound

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
    required property var installedEngines
    required property AppSettings prefs

    signal installRequested(string version)
    signal newProjectRequested()
    // The owner asks the user before the project's folder is deleted.
    signal removeFolderRequested(string name, string path)

    color: Theme.windowBg

    // Both groups count, so the empty state shows only when no project is listed.
    readonly property int projectCount: pinnedRows.count + otherRows.count

    // The project whose pin button takes the focus once its row reappears in the other group.
    property string pinFocusPath: ""

    // Scrolls the list just far enough to show the item.
    function scrollIntoView(item) {
        const top = item.mapToItem(list.contentItem, 0, 0).y;
        const bottom = top + item.height;
        if (top < list.contentY)
            list.contentY = top;
        else if (bottom > list.contentY + list.height)
            list.contentY = bottom - list.height;
    }

    // Starts the project's editor. A failure is reported through the models' errorOccurred, which the window shows.
    function openProject(path) {
        root.projects.open(path, root.prefs.editorArguments);
    }

    // Searched and sorted once. The two groups below split it by pin, so both follow the search and the sort without
    // ever mixing.
    SortFilterProxyModel {
        id: visibleProjects

        sourceModel: root.projects
        filters: FunctionFilter {
            property string needle: searchField.text.toLowerCase()
            onNeedleChanged: invalidate()
            function filter(name: string): bool {
                return name.toLowerCase().includes(needle);
            }
        }
        sorters: [
            RoleSorter {
                roleName: "lastOpened"
                sortOrder: Qt.DescendingOrder
                enabled: sortBox.currentIndex === 0
            },
            StringSorter {
                roleName: "name"
                caseSensitivity: Qt.CaseInsensitive
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

        title: qsTrId("main.projectsview.add_existing_title")
        onAccepted: {
            const folder = FileManager.toLocalPath(selectedFolder);
            if (folder !== "")
                root.projects.addExisting(folder);
        }
    }

    // One project row. The pinned group and the other projects both show it.
    Component {
        id: projectRow

        HubCard {
            id: row

            // The project's roles. `model` is also declared, because setData goes through it: the nested ComboBox
            // has a `model` property of its own.
            required property var model
            required property string name
            required property string path
            required property string engineVersion
            required property bool engineInstalled
            required property bool pinned

            readonly property string pinLabel: row.pinned ? qsTrId("main.projectsview.unpin") : qsTrId("main.projectsview.pin")

            function setEngineVersion(version) {
                row.model.engineVersion = version;
            }

            // Sized by the layout, not bound to the list's width: the binding and the layout both set the width, and while
            // the window resized the row flipped between the two on alternate frames.
            Layout.fillWidth: true

            Component.onCompleted: {
                if (root.pinFocusPath !== "" && row.path === root.pinFocusPath) {
                    root.pinFocusPath = "";
                    pinButton.forceActiveFocus();
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceM

                ToolButton {
                    id: pinButton

                    hoverEnabled: true
                    contentItem: Icon {
                        source: row.pinned ? Qt.resolvedUrl("../icons/star_filled.svg") : Qt.resolvedUrl("../icons/star.svg")
                        color: row.pinned ? Theme.accent : Theme.text
                    }
                    Accessible.name: row.pinLabel
                    ToolTip.visible: pinButton.hovered
                    ToolTip.text: row.pinLabel
                    onClicked: {
                        // Deferred: pinning moves the row to the other group, which destroys this delegate.
                        const path = row.path;
                        const pinned = !row.pinned;
                        if (pinButton.activeFocus)
                            root.pinFocusPath = path;
                        Qt.callLater(() => root.projects.setPinned(path, pinned));
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spaceXxs

                    Text {
                        Layout.fillWidth: true
                        text: row.name
                        color: Theme.text
                        font.pixelSize: Theme.fontSubtitle
                        font.bold: true
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }

                    Text {
                        Layout.fillWidth: true
                        text: FileManager.displayPath(row.path)
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        elide: Text.ElideMiddle
                        textFormat: Text.PlainText
                    }
                }

                // The picker always shows the project's own version, even when that engine is not installed.
                ComboBox {
                    // The project's own engine is left out, so the list only offers other versions.
                    model: SortFilterProxyModel {
                        sourceModel: root.installedEngines
                        filters: ValueFilter {
                            roleName: "version"
                            value: row.engineVersion
                            inverted: true
                        }
                    }
                    textRole: "version"
                    valueRole: "version"
                    Accessible.name: qsTrId("main.projectsview.engine_version_of").arg(row.name)
                    // The project's own engine is left out of the list, so the index stays -1. The count is read so the index is
                    // set again once the engines are listed, and a pick replaces this binding, so onActivated restores it.
                    currentIndex: count > 0 ? indexOfValue(row.engineVersion) : -1
                    // Nothing to switch to when the project's engine is the only one installed.
                    enabled: count > 0
                    Layout.preferredWidth: Theme.comboWidth

                    contentItem: Text {
                        leftPadding: Theme.spaceS
                        text: row.engineVersion
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    onActivated: (index) => {
                        row.setEngineVersion(textAt(index));
                        currentIndex = Qt.binding(() => indexOfValue(row.engineVersion));
                    }
                }

                StatusBadge {
                    visible: !row.engineInstalled
                    text: qsTrId("main.projectsview.not_installed")
                    tone: Theme.warning
                }

                HubButton {
                    visible: !row.engineInstalled
                    text: qsTrId("main.projectsview.install")
                    onClicked: root.installRequested(row.engineVersion)
                }

                HubButton {
                    text: qsTrId("main.projectsview.open")
                    primary: true
                    enabled: row.engineInstalled
                    onClicked: root.openProject(row.path)
                }

                RowMenuButton {
                    MenuItem {
                        text: FileManager.revealLabel
                        onTriggered: FileManager.reveal(row.path)
                    }

                    MenuItem {
                        text: qsTrId("main.projectsview.remove_from_list")
                        onTriggered: {
                            // Deferred so the delegate that owns this menu is not destroyed mid-handler.
                            const path = row.path;
                            Qt.callLater(() => root.projects.remove(path));
                        }
                    }

                    MenuItem {
                        text: qsTrId("main.projectsview.remove_folder")
                        // Deletes the project's folder, so the item is red, as the engine delete is on the Installs view.
                        palette.windowText: Theme.danger
                        onTriggered: root.removeFolderRequested(row.name, row.path)
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
            title: qsTrId("main.projectsview.title")

            HubButton {
                text: qsTrId("main.projectsview.add_existing")
                onClicked: {
                    addFolderDialog.currentFolder = FileManager.folderUrl(root.prefs.projectsDirectory);
                    addFolderDialog.open();
                }
            }

            HubButton {
                text: qsTrId("main.projectsview.new_project")
                primary: true
                onClicked: root.newProjectRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceS

            HubSearchField {
                id: searchField

                Layout.fillWidth: true
                placeholderText: qsTrId("main.projectsview.search_placeholder")
            }

            ComboBox {
                id: sortBox

                model: [qsTrId("main.projectsview.sort_last_opened"), qsTrId("main.projectsview.sort_name")]
                Accessible.name: qsTrId("main.projectsview.sort_label")
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

            Connections {
                target: list.Window.window

                function onActiveFocusItemChanged() {
                    let item = list.Window.window.activeFocusItem;
                    const focused = item;
                    while (item && item !== list.contentItem)
                        item = item.parent;
                    if (item)
                        root.scrollIntoView(focused);
                }
            }

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
                       ? qsTrId("main.projectsview.empty_search_title").arg(searchField.text)
                       : qsTrId("main.projectsview.empty_title")
                detail: searchField.text.length > 0
                        ? qsTrId("main.projectsview.empty_search_detail")
                        : qsTrId("main.projectsview.empty_detail")
            }
        }
    }
}
