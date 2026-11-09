#!/usr/bin/env bash
# Builds the Windows ZIP in an MSYS2 UCRT64 shell: the program, the Qt libraries and plugins it needs (windeployqt)
# and every other DLL from MSYS2 it loads, so it runs on a Windows without MSYS2.
#
# Usage: build.sh <source dir> <version>
set -euo pipefail

SRC="$(cd "${1:-$PWD}" && pwd)"
VERSION="${2:?version missing}"
BUILD="${BUILD_DIR:-$SRC/build-windows}"
NAME="SC-Localization-Launcher-$VERSION-windows-x86_64"
STAGE="$BUILD/$NAME"

cmake -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSC_VERSION_OVERRIDE="$VERSION"
cmake --build "$BUILD"

rm -rf "$STAGE" "$BUILD/$NAME.zip"
mkdir -p "$STAGE"
cp "$BUILD/sc-localization-launcher.exe" "$STAGE/"

# The name differs between Qt packages
DEPLOY="$(command -v windeployqt6 || command -v windeployqt-qt6 || command -v windeployqt || true)"
[ -n "$DEPLOY" ] || DEPLOY=/ucrt64/share/qt6/bin/windeployqt.exe
"$DEPLOY" --release --qmldir "$SRC/qml" --no-translations --no-system-d3d-compiler --no-opengl-sw "$STAGE/sc-localization-launcher.exe"

# windeployqt copies Qt only; curl, libsodium, spdlog, the C++ runtime and their dependencies come from MSYS2.
# Repeated until nothing new turns up, because a copied DLL can need further ones.
while true; do
    missing=$(find "$STAGE" -iname '*.exe' -o -iname '*.dll' | while read -r file; do ldd "$file" 2>/dev/null; done |
        awk '$3 ~ /^\/ucrt64\// { print $3 }' | sort -u | while read -r dll; do
            [ -e "$STAGE/$(basename "$dll")" ] || echo "$dll"
        done)
    [ -n "$missing" ] || break
    echo "$missing" | while read -r dll; do cp "$dll" "$STAGE/"; done
done

# MSYS2's Qt looks for its plugins and QML modules below share/qt6; windeployqt puts them next to the program
printf '[Paths]\nPrefix = .\nPlugins = .\nQmlImports = qml\n' > "$STAGE/qt.conf"

cp "$SRC/LICENSE" "$STAGE/LICENSE.txt"
cp "$SRC/README.md" "$STAGE/README.md"

(cd "$BUILD" && rm -f "$NAME.zip" && zip -qr "$NAME.zip" "$NAME")
ls -la "$BUILD/$NAME.zip"
