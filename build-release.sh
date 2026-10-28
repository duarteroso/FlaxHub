#!/usr/bin/env bash
# Builds a Release FlaxHub and installs it to build/linux.
# Set QT_ROOT to the Qt kit folder (the one holding bin/ and lib/cmake/Qt6) if it is not the default.
set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
qt_root="${QT_ROOT:-$HOME/Qt/6.12.0/gcc_64}"
# A fresh build tree each run, removed on exit, so no stale state is reused.
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
out_dir="$root/build/linux"

if [ ! -d "$qt_root/lib/cmake/Qt6" ]; then
    echo "Qt not found at $qt_root. Set QT_ROOT to your Qt kit folder." >&2
    exit 1
fi

# CMAKE_INSTALL_RPATH points the installed binary at the same Qt kit as the build, not at a system Qt.
cmake -S "$root/flaxhub" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_DIR="$qt_root/lib/cmake/Qt6" \
    -DCMAKE_INSTALL_RPATH="$qt_root/lib" \
    -DBUILD_TESTING=OFF
cmake --build "$build_dir" --config Release --parallel

rm -rf "$out_dir"
cmake --install "$build_dir" --config Release --prefix "$out_dir"

echo "Release build installed to $out_dir"
