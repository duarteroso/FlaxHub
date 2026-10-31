# FlaxHub

FlaxHub is an unofficial desktop hub for [Flax Engine](https://flaxengine.com), in the style of Unity Hub. It is a
Qt Quick app that manages engine installs and projects in one place, so you do not have to clone, build and launch
the engine by hand.

> **Status:** early development. Expect rough edges, and please report what you find.

## Features

- **Installs**
  - Lists the Flax Engine release tags from GitHub, newest first, and installs one with a single dialog.
  - An install clones the tag (`git clone --depth 1`), runs the engine's `GenerateProjectFiles` script and builds
    `FlaxEditor` in the Release configuration.
  - Only the newest four versions can be installed.
  - Installs run one at a time. A failed generate or build step is retried from that step, and its clone is kept.
  - Choose a default engine, fix a broken install, or uninstall it.
  - Installs are remembered across restarts and checked at startup.
- **Projects**
  - Create a new project with any installed engine, add an existing folder that holds a `.flaxproj`, or open a project
    in its engine's editor.
  - The engine for each project is picked up from the project file and can be changed in the list.
  - Remove a project from the list (the folder is kept), or remove it together with its folder.
  - Show a project folder in your file manager, or copy its path.
- **Settings**
  - The install folder, the projects folder and extra launch arguments for the editor.
  - Defaults are `~/FlaxHub/Engines` for engines.
- A dark theme that follows Flax Engine's look.

## Install

There is no packaged release yet, so you build FlaxHub from source. The release scripts in the repo root do a Release
build, install it into `build/<platform>` and bundle the Qt libraries where the platform needs it. They use a Qt
6.12 kit at a default path, so set `QT_ROOT` if yours is elsewhere. Each script builds in a temporary folder and
removes it afterwards.

Install the [prerequisites](#prerequisites) first, then use the section for your platform.

### Linux

Build with:
```bash
./build-release.sh
```

Run with:
```bash
./build/linux/bin/appFlaxHub
```

The script uses `$HOME/Qt/6.12.0/gcc_64` when `QT_ROOT` is not set. The binary is linked to the Qt kit it was built with, so keep that Qt installed, or use the Qt libraries your distribution provides.

To build the engine from inside FlaxHub, you also need the engine's Linux build dependencies. See the
[engine's documentation](https://docs.flaxengine.com).

### Windows

> **Note**: FlaxEngine on Windows provides an offical hub for installing the engine and managing projects.
It is recommended to use it on this platform!

From a Visual Studio developer prompt (so MSVC and CMake are on the `PATH`):

```
set QT_ROOT=C:\Qt\6.12.0\msvc2022_64
build-release.bat
build\windows\bin\appFlaxHub.exe
```

The script runs `windeployqt`, so the Qt DLLs are copied next to the app. The folder `build\windows` can then be moved
or zipped. The engine's build also needs Visual Studio with the workloads the
[engine's documentation](https://docs.flaxengine.com) lists.

### macOS

```
./build-release.command
open build/macos/appFlaxHub.app
```

The script runs `macdeployqt`, so the Qt frameworks are bundled into the app. The app is not signed or notarized, so
macOS may ask you to allow it in System Settings > Privacy & Security. The engine's build needs Xcode and its command
line tools.

## Development

[Qt Creator](https://www.qt.io/product/development-tools), to open `flaxhub/CMakeLists.txt` and build, run and debug from the IDE. 

### For building the release scripts:
- Qt 6.10 or newer, with the Qt Quick, Qt Quick Dialogs 2, Qt Network, Qt Quick Effects and Qt Concurrent modules.
  The tests also need Qt Test.
- CMake 3.22 or newer.
- A C++ compiler that your Qt installation supports.
- Optional: 
- On Linux, the OpenGL development files, which CMake's configure step needs.

### For running Flaxhub:
- `git` on your `PATH`. The Hub runs `git clone` and `git` checks on installs.

The engine's own generate and build scripts have their own requirements. The Hub does not check for them.

## Tests

The tests use QtTest and run through CTest. Build the project as above, then:

```
ctest --test-dir build/Desktop_Qt_6_12_0_Debug --output-on-failure
```

They cover the install folder rules, the install validator, the install and project list models, the engine tag
parsing and the file helpers. They use temporary folders only, and they never read or write your settings.

## Project layout

- `flaxhub/src/` has the C++ models, settings and helpers. `flaxhub/views/`, `dialogs/` and `components/` hold the
  QML UI, and `Theme.qml` holds the colors and sizes.
- Texts are translation ids, and the English text lives in `flaxhub/translations/flaxhub_en.ts`.
- `CLAUDE.md` describes the architecture in more detail.

## Contributing

Issues and pull requests are welcome. Keep the build warning-free, add or update tests for model and rule changes. AI assist changes allowed.

## Disclaimers

### Not affiliated with Flax Engine

FlaxHub is an independent project. It is **not** made, endorsed, sponsored or supported by any of the engine's authors.

"Flax", "Flax Engine", "FlaxEngine", "FlaxEditor", their logos and any related names and brands belong to their
respective owners. FlaxHub uses these names only to say what it works with. FlaxHub does not include, copy or
redistribute any part of the engine: it downloads the engine's source from the official
[GitHub repository](https://github.com/FlaxEngine/FlaxEngine) on your machine, at your request. The engine is subject
to its own license, which you accept by using it. Read it in the engine's repository before you build or ship
anything with it. Questions about the engine itself belong with its authors, not here.

The name "FlaxHub" and this project's code are covered by the MIT License below, which does not grant any rights over
Flax Engine's names, brands or code.

### Built with Claude Code

Much of this project was written with the help of [Claude Code](https://claude.com/claude-code), Anthropic's AI coding
assistant, under the direction and review of the maintainer. AI-written code can contain mistakes, so treat FlaxHub
accordingly:

- Review the code before you trust it with anything important.
- FlaxHub runs `git` and the engine's build scripts, and it can delete folders (uninstalling an engine, fixing an
  install, removing a project with its folder). It has safeguards and asks before destructive steps, but back up any
  project you care about, and read each confirmation before accepting it.
- The software is provided as is, with no warranty (see the license).

## License

FlaxHub is released under the MIT License. See [LICENSE](LICENSE). Flax Engine is a separate project under its own
license.
