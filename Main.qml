import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtCore
import FlaxHub


ApplicationWindow {
    id: window
    width: 1100
    height: 700
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: qsTr("FlaxHub")
    color: Theme.windowBg

    // Set once the user has chosen to quit during an install, so the close is not asked again.
    property bool quitConfirmed: false

    // Quitting cancels a running install, so ask first. Keep installing leaves the window open.
    onClosing: (close) => {
        if (installs.busy && !quitConfirmed) {
            close.accepted = false;
            quitDialog.open();
        }
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
    // Menus and lists fill the highlighted item with palette.light, so that item takes the accent too.
    palette.light: Theme.accent
    // A pressed menu or list item fills with palette.midlight. The progress bars set their own track color.
    palette.midlight: Theme.surfaceHover
    palette.placeholderText: Theme.textMuted
    palette.mid: Theme.border

    // Saved values. Settings can only persist properties exposed through an alias.
    QtObject {
        id: prefs

        property string theme: "system"
        property string installDirectory: "~/FlaxHub/Engines"
        property string projectsDirectory: "~/FlaxHub/Projects"
        property string editorArguments: ""
    }

    Settings {
        category: "General"

        property alias theme: prefs.theme
        property alias installDirectory: prefs.installDirectory
        property alias projectsDirectory: prefs.projectsDirectory
        property alias editorArguments: prefs.editorArguments
    }

    Binding {
        target: Theme
        property: "preference"
        value: prefs.theme
    }

    InstallListModel {
        id: installs

        // Once per run. The Add engine dialog lists the result.
        Component.onCompleted: fetchAvailableVersions()
    }

    ProjectListModel {
        id: projects
        installs: installs
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            id: sidebar
            Layout.fillHeight: true
            collapsed: window.width < 760
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sidebar.currentIndex

            ProjectsView {
                projects: projects
                installs: installs
                prefs: prefs
                onInstallRequested: (version) => addInstallDialog.openFor(version)
                onNewProjectRequested: newProjectDialog.openFor()
            }

            InstallsView {
                installs: installs
                prefs: prefs
                onAddVersionRequested: addInstallDialog.openFor("")
                onUninstallRequested: (version, path) => uninstallDialog.openFor(version, path)
                onFixStarted: (version, path) => installProgressDialog.openFor(version, path)
            }

            SettingsView {
                prefs: prefs
            }
        }
    }

    AddInstallDialog {
        id: addInstallDialog

        installs: installs
        prefs: prefs
        onInstallStarted: (version, path) => installProgressDialog.openFor(version, path)
    }

    InstallProgressDialog {
        id: installProgressDialog

        installs: installs
    }

    UninstallDialog {
        id: uninstallDialog

        installs: installs
    }

    NewProjectDialog {
        id: newProjectDialog

        projects: projects
        installs: installs
        prefs: prefs
    }

    QuitDialog {
        id: quitDialog

        onQuitAnyway: {
            window.quitConfirmed = true;
            window.close();
        }
    }
}
