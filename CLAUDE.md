# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and run

Qt 6 Qt Quick app built with CMake (≥ 3.16). Qt 6.12 is installed at `/home/duarteroso/Qt/6.12.0/gcc_64`; the project requires Qt 6.10+.

```
cmake -S . -B build/Desktop_Qt_6_12_0_Debug -DQt6_DIR=/home/duarteroso/Qt/6.12.0/gcc_64/lib/cmake/Qt6 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Desktop_Qt_6_12_0_Debug
./build/Desktop_Qt_6_12_0_Debug/appFlaxHub
```

- `build*/` is gitignored and follows Qt Creator's `build/<kit>` layout. The existing `build/Desktop_Qt_6_12_0_Debug` was configured with Homebrew clang++ (`/home/linuxbrew/.linuxbrew/bin/clang++`); a fresh configure uses the default compiler unless you pass `-DCMAKE_CXX_COMPILER`.
- Configure needs OpenGL development files (`mesa-libGL-devel` and `libglvnd-devel` on Fedora). Without them, configure fails with `Could NOT find OpenGL` → `Qt6Gui` → `Qt6Quick` not found. They are installed on the Fedora machine.
- The repo has no tests and no lint or format configuration, so there is no test or lint command.

## Architecture

A Qt Quick hub app (Unity Hub style): a sidebar with Projects, Installs and Settings views. C++ provides the data models and the bootstrap; the UI lives in QML.

- `main.cpp` sets the organization and application names (used by `Settings`), creates `QGuiApplication` and `QQmlApplicationEngine`, then calls `engine.loadFromModule("FlaxHub", "Main")`. It exits with -1 if the root object fails to create.
- `CMakeLists.txt` defines the executable `appFlaxHub` (project `FlaxHub`) and a QML module with `URI FlaxHub`. Every QML file goes in `QML_FILES` and every C++ model goes in `SOURCES`; a file missing from the list is not in the module. The module URI and the `Main` type in `main.cpp` must match. Renaming `Main.qml`, changing the URI, or renaming the target means updating `main.cpp` and `CMakeLists.txt` together.
- `Main.qml` is the window. It owns the models (`InstallListModel`, `ProjectListModel`), the saved preferences (`prefs`, a `QtObject` that `Settings` persists through `property alias`), the sidebar with a `StackLayout` of the three views, and the `AddInstallDialog`. It binds `Theme.preference` to the saved theme.
- `Theme.qml` is a singleton of color tokens (resolved from the light/dark choice), spacing, radius and font sizes. Use its tokens rather than hard-coded colors. `Main.qml` also maps the tokens onto `palette`, so Basic controls (ComboBox, Menu, TextField) follow the theme.
- Subfolders (`components/`, `views/`, `dialogs/`) use `import FlaxHub` to reach `Theme` and each other.
- `src/installlistmodel.*` and `src/projectlistmodel.*` are `QAbstractListModel`s registered as QML types. `InstallListModel` saves the installs to `QSettings` (group `Installs`: version, path and default flag per entry) and restores them at startup. Uninstall deletes the folder on a worker thread, with the entry in the `Removing` state until the delete finishes. `src/installvalidator.*` checks each saved folder on load, and a failing one stays listed as `Invalid`. The clone and generate steps are real processes (`runStep`: `git clone`, then the engine's `GenerateProjectFiles` script), but the build step is still a timer. `ProjectListModel` saves its projects to `QSettings` (group `Projects`: name, path, engine version, last opened and pinned per entry) and restores them at startup. A saved project whose folder is gone stays listed, and `open()` warns and returns false for it. `addExisting` (behind Add existing) takes a folder holding a `.flaxproj`, names the project after that file, and takes its engine from `MinEngineVersion`, falling back to `InstallListModel::preferredVersion`. Milestone 2 loads projects from disk. `ProjectListModel::create` (behind `NewProjectDialog`) runs the installed engine's editor as `FlaxEditor -new -exit -headless -std -project="<dir>"`, which writes the project files. The project joins the list only if its `.flaxproj` exists afterwards. `InstallListModel::editorExecutable` gives the editor path per platform (`Binaries/Editor/<platform>/Development`).
- `src/filemanager.*` is a `QObject` singleton (`FileManager` in QML). `reveal()` opens a folder through `QDesktopServices`, falling back to the nearest existing parent for a missing path and to the containing folder for a file. `revealLabel` is the menu text for the platform: Finder on macOS, File Explorer on Windows, "Show in folder" elsewhere. `folderUrl()` gives the URL a folder picker opens at (expanding `~`, falling back to the nearest existing folder), and `toLocalPath()` turns the picked URL back into a path. `components/FolderField.qml` is the path field with a Browse button that the Settings view uses; `expandHome()` lives in `src/pathutils.h`.
- The Projects view's engine picker lists installed engines and writes the choice back through `engineVersion` via `setData`.
- `importedcontent/` is for Figma-to-Qt exports (see its README). The top-level `CMakeLists.txt` calls `add_subdirectory` on it only if `importedcontent/CMakeLists.txt` exists, but nothing links its targets into `appFlaxHub` yet. Once that folder has a CMake file, wire it into the app target.
