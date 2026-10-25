# FlaxHub

FlaxHub is a desktop hub for [FlaxEngine](https://github.com/FlaxEngine/FlaxEngine), in the style of Unity Hub. It is
a Qt Quick app with three views: Projects, Installs and Settings.

- **Installs** lists the FlaxEngine release tags from GitHub. An install clones the tag, runs the engine's
  `GenerateProjectFiles` script and builds `FlaxEditor` in the Release configuration. An installed engine can be made
  the default, fixed, pointed at a moved folder or removed.
- **Projects** lists your projects, adds an existing folder with a `.flaxproj` file, creates a new project with an
  installed engine, and opens a project in its engine's editor.

## Prerequisites

- Qt 6.10 or newer, with the Qt Quick, Qt Quick Dialogs 2, Qt Network, Qt Quick Effects and Qt Concurrent modules.
  The tests also need Qt Test.
- CMake 3.22 or newer.
- A C++ compiler that your Qt installation supports.
- On Linux, the OpenGL development files, which CMake's configure step needs (`mesa-libGL-devel` and
  `libglvnd-devel` on Fedora).
- `git` on your `PATH`. The Hub runs `git clone` and `git` checks on installs.
- Network access to `github.com` and `api.github.com`, for the version list and clones.

The engine's own generate and build scripts have their own requirements. The Hub does not check for them.

## Build and run

Replace the Qt path with your own Qt 6.10+ installation:

```
cmake -S . -B build/Desktop_Qt_6_12_0_Debug -DQt6_DIR=/path/to/Qt/6.12.0/gcc_64/lib/cmake/Qt6 -DCMAKE_BUILD_TYPE=Debug
cmake --build build/Desktop_Qt_6_12_0_Debug
./build/Desktop_Qt_6_12_0_Debug/appFlaxHub
```

The build directory name follows Qt Creator's `build/<kit>` layout and is ignored by git. To skip the tests, add
`-DBUILD_TESTING=OFF` to the configure command.

## Tests

The tests use QtTest and run through CTest. Build the project as above, then:

```
ctest --test-dir build/Desktop_Qt_6_12_0_Debug --output-on-failure
```

The tests cover the install folder rules and the install validator. They use temporary folders only, and the install
rules test points `HOME` at a temporary folder while it runs.

## License

FlaxHub is released under the MIT License. See [LICENSE](LICENSE).
