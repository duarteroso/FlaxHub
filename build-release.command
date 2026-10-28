#!/usr/bin/env bash
# Builds a Release FlaxHub and installs it to build/macos. macdeployqt bundles the Qt frameworks into the app.
# Set QT_ROOT to the Qt kit folder (the one holding bin/ and lib/cmake/Qt6) if it is not the default.
set -euo pipefail

cd "$(dirname "$0")"
root="$(pwd)"
qt_root="${QT_ROOT:-$HOME/Qt/6.12.0/macos}"
# A fresh build tree each run, removed on exit, so no stale state is reused.
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT
out_dir="$root/build/macos"

if [ ! -d "$qt_root/lib/cmake/Qt6" ]; then
    echo "Qt not found at $qt_root. Set QT_ROOT to your Qt kit folder." >&2
    exit 1
fi

cmake -S "$root/flaxhub" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DQt6_DIR="$qt_root/lib/cmake/Qt6" \
    -DBUILD_TESTING=OFF
cmake --build "$build_dir" --config Release --parallel

rm -rf "$out_dir"
cmake --install "$build_dir" --config Release --prefix "$out_dir"

"$qt_root/bin/macdeployqt" "$out_dir/appFlaxHub.app" -qmldir="$root/flaxhub"

echo "Release build installed to $out_dir"
