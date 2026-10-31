pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// The window's menu bar. On macOS Qt moves it to the screen's menu bar. Qt 6 has no menu roles here, so Qt matches the
// text instead: "About ...", "Settings..." and "Quit ..." land in the application menu. The menu only emits signals,
// and Main.qml does the work, so the quit and close paths keep their guard in Main.qml's onClosing.
MenuBar {
    id: root

    // True while a project is created. New Project is unavailable meanwhile, as in its dialog.
    required property bool projectCreating
    // True while any dialog or folder picker is open. The items that change the view or open a dialog are unavailable
    // meanwhile, and so are their shortcuts. About, Quit and Help stay available.
    required property bool dialogOpen

    signal aboutRequested()
    signal settingsRequested()
    signal quitRequested()
    signal newProjectRequested()
    signal addExistingProjectRequested()
    signal addInstallRequested()
    signal projectsRequested()
    signal installsRequested()

    Menu {
        title: qsTrId("main.menu.flaxhub_menu")

        MenuItem {
            text: qsTrId("main.menu.about")
            onTriggered: root.aboutRequested()
        }

        MenuSeparator {}

        MenuItem {
            action: Action {
                text: qsTrId("main.menu.settings")
                shortcut: StandardKey.Preferences
                enabled: !root.dialogOpen
                onTriggered: root.settingsRequested()
            }
        }

        MenuSeparator {}

        MenuItem {
            action: Action {
                text: qsTrId("main.menu.quit")
                shortcut: StandardKey.Quit
                onTriggered: root.quitRequested()
            }
        }
    }

    Menu {
        title: qsTrId("main.menu.project_menu")

        MenuItem {
            action: Action {
                text: qsTrId("main.menu.new_project")
                shortcut: StandardKey.New
                enabled: !root.projectCreating && !root.dialogOpen
                onTriggered: {
                    root.projectsRequested();
                    root.newProjectRequested();
                }
            }
        }

        MenuItem {
            action: Action {
                text: qsTrId("main.menu.add_existing_project")
                shortcut: StandardKey.Open
                enabled: !root.dialogOpen
                onTriggered: {
                    root.projectsRequested();
                    root.addExistingProjectRequested();
                }
            }
        }

    }

    Menu {
        title: qsTrId("main.menu.engine_menu")

        MenuItem {
            action: Action {
                text: qsTrId("main.menu.add_install")
                shortcut: "Shift+Ctrl+N"
                enabled: !root.dialogOpen
                onTriggered: {
                    root.installsRequested();
                    root.addInstallRequested();
                }
            }
        }
    }

    Menu {
        title: qsTrId("main.menu.view_menu")

        MenuItem {
            action: Action {
                text: qsTrId("main.menu.view_projects")
                shortcut: "Ctrl+1"
                enabled: !root.dialogOpen
                onTriggered: root.projectsRequested()
            }
        }

        MenuItem {
            action: Action {
                text: qsTrId("main.menu.view_installs")
                shortcut: "Ctrl+2"
                enabled: !root.dialogOpen
                onTriggered: root.installsRequested()
            }
        }

        MenuItem {
            action: Action {
                text: qsTrId("main.menu.view_settings")
                shortcut: "Ctrl+3"
                enabled: !root.dialogOpen
                onTriggered: root.settingsRequested()
            }
        }
    }

    Menu {
        title: qsTrId("main.menu.help_menu")

        MenuItem {
            text: qsTrId("main.menu.help_github")
            onTriggered: Qt.openUrlExternally("https://github.com/duarteroso/FlaxHub")
        }
    }
}
