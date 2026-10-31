import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQml.Models
import FlaxHub


ApplicationWindow {
    id: window
    width: 1100
    height: 700
    minimumWidth: 1100
    minimumHeight: 700
    maximumWidth: 1100
    maximumHeight: 700
    visible: true
    title: qsTrId("main.window.title")
    color: Theme.windowBg

    // Set once the user has chosen to quit during an install, so the close is not asked again.
    property bool quitConfirmed: false

    // Quitting cancels a running install, stops a project being created and cuts a folder delete or check short, so ask
    // first. Keep working leaves the window open.
    onClosing: (close) => {
        if ((installModel.busy || installModel.removing() || projectModel.creating || projectModel.removing) && !quitConfirmed) {
            close.accepted = false;
            quitDialog.open();
        }
    }

    // The menu bar only emits signals. Quit and Close Window call close(), so onClosing still asks while work runs.
    // The sidebar index drives the StackLayout, so setting it switches the view and highlights the sidebar too.
    menuBar: AppMenuBar {
        projectCreating: projectModel.creating

        onAboutRequested: noticeDialog.show(qsTrId("main.menu.about_title"), qsTrId("main.menu.about_version").arg(Qt.application.version))
        onSettingsRequested: sidebar.currentIndex = sidebar.settingsIndex
        onQuitRequested: window.close()
        onNewProjectRequested: newProjectDialog.openFor()
        onAddExistingProjectRequested: {
            // Add existing lives on the Projects view, so the view is shown before its folder picker opens.
            sidebar.currentIndex = 0;
            projectsView.chooseExistingProject();
        }
        onAddInstallRequested: addInstallDialog.openFor("")
        // The sidebar's first two sections are Projects and Installs.
        onProjectsRequested: sidebar.currentIndex = 0
        onInstallsRequested: sidebar.currentIndex = 1
    }

    // Basic controls (ComboBox, Menu, TextField) take their colors from the palette.
    palette.window: Theme.surface
    palette.windowText: Theme.text
    // Setting the active colors also sets the disabled ones, so disabled items need their own grey.
    palette.disabled.windowText: Theme.textMuted
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceHover
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    // Menus and lists fill the highlighted item with palette.light, so it needs a fill that keeps palette.text readable.
    palette.light: Theme.surfaceHover
    // Popup and menu outlines and the ComboBox arrow.
    palette.dark: Theme.textMuted
    // A pressed menu or list item fills with palette.midlight. The progress bars set their own track color.
    palette.midlight: Theme.surfaceHover
    palette.placeholderText: Theme.textMuted
    palette.mid: Theme.border

    InstallListModel {
        id: installModel

        // Once per run. The Add engine dialog lists the result.
        Component.onCompleted: fetchAvailableVersions()
    }

    ProjectListModel {
        id: projectModel
        installs: installModel
    }

    // Only installed engines can be picked for a project, newest first. The install model keeps its rows newest first,
    // so the filter alone gives that order.
    SortFilterProxyModel {
        id: installedEngineModel

        sourceModel: installModel
        filters: ValueFilter {
            roleName: "state"
            value: InstallListModel.Installed
        }
    }

    // Both models report a failed action here, and the notice shows its reason.
    Connections {
        target: installModel

        function onErrorOccurred(title, message) {
            noticeDialog.show(title, message);
        }
    }

    Connections {
        target: projectModel

        function onErrorOccurred(title, message) {
            noticeDialog.show(title, message);
        }
    }

    Connections {
        target: AppSettings

        function onErrorOccurred(title, message) {
            noticeDialog.show(title, message);
        }
    }

    Connections {
        target: FileManager

        function onErrorOccurred(title, message) {
            noticeDialog.show(title, message);
        }
    }

    Connections {
        target: installModel

        function onClearConfirmationRequired(version, path) {
            clearFolderDialog.openFor(version, path);
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
            Layout.fillHeight: true
            collapsed: window.width < Theme.sidebarCollapseBreakpoint
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sidebar.currentIndex

            ProjectsView {
                id: projectsView

                projects: projectModel
                installs: installModel
                installedEngines: installedEngineModel
                prefs: AppSettings
                onInstallRequested: (version) => addInstallDialog.openFor(version)
                onNewProjectRequested: newProjectDialog.openFor()
                onRemoveFolderRequested: (name, path) => {
                    removeFolderDialog.projectName = name;
                    removeFolderDialog.openFor("", path);
                }
            }

            InstallsView {
                installs: installModel
                prefs: AppSettings
                onAddVersionRequested: addInstallDialog.openFor("")
                onUninstallRequested: (version, path, invalid) => {
                    uninstallDialog.invalidEntry = invalid;
                    uninstallDialog.openFor(version, path);
                }
                onInstallStarted: (version, path) => installProgressDialog.openFor(version, path)
            }

            SettingsView {
                prefs: AppSettings
            }
        }
    }

    AddInstallDialog {
        id: addInstallDialog

        installs: installModel
        prefs: AppSettings
        onInstallStarted: (version, path) => installProgressDialog.openFor(version, path)
    }

    InstallProgressDialog {
        id: installProgressDialog

        installs: installModel
    }

    ConfirmDialog {
        id: uninstallDialog

        // Set when the entry is Invalid. Its folder is deleted whatever it holds, and the entry leaves the list.
        property bool invalidEntry: false

        title: uninstallDialog.invalidEntry
            ? qsTrId("main.uninstalldialog.title_invalid")
            : qsTrId("main.uninstalldialog.title")
        question: uninstallDialog.invalidEntry
            ? qsTrId("main.uninstalldialog.question_invalid").arg(uninstallDialog.version)
            : qsTrId("main.uninstalldialog.question").arg(uninstallDialog.version)
        confirmText: uninstallDialog.invalidEntry
            ? qsTrId("main.uninstalldialog.confirm_invalid")
            : qsTrId("main.uninstalldialog.confirm")

        onConfirmed: (version) => installModel.uninstall(version)
    }

    ConfirmDialog {
        id: clearFolderDialog

        title: qsTrId("main.clearfolderdialog.title")
        question: qsTrId("main.clearfolderdialog.question").arg(clearFolderDialog.version)
        confirmText: qsTrId("main.clearfolderdialog.confirm")

        onConfirmed: (version) => installModel.fix(version, true)
    }

    ConfirmDialog {
        id: removeFolderDialog

        // The project the question names. The Projects view sets it before the dialog opens.
        property string projectName: ""

        title: qsTrId("main.removefolderdialog.title")
        question: qsTrId("main.removefolderdialog.question").arg(removeFolderDialog.projectName)
        confirmText: qsTrId("main.removefolderdialog.confirm")

        onConfirmed: (version, path) => projectModel.removeWithFolder(path)
    }

    NewProjectDialog {
        id: newProjectDialog

        projects: projectModel
        installs: installModel
        installedEngines: installedEngineModel
        prefs: AppSettings
    }

    ConfirmDialog {
        id: quitDialog

        title: qsTrId("main.quitdialog.title")
        question: qsTrId("main.quitdialog.question")
        confirmText: qsTrId("main.quitdialog.confirm")
        cancelText: qsTrId("main.quitdialog.cancel")

        onConfirmed: {
            window.quitConfirmed = true;
            window.close();
        }
    }

    // Last, so it opens above the other dialogs, including one that is still open, such as New project.
    NoticeDialog {
        id: noticeDialog
    }
}
