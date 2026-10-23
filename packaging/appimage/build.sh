#!/usr/bin/env bash
# Builds the AppImage on Ubuntu 22.04, the oldest glibc we support. The result runs on every newer distro.
# Used by the CI and locally via packaging/appimage/build-in-docker.sh. Runs as root inside the container.
#
# Usage: build.sh <source dir>
set -euo pipefail

SRC="${1:-$PWD}"
BUILD="${BUILD_DIR:-$SRC/build-appimage}"
QT_VERSION="${QT_VERSION:-6.8.3}"
QT_DIR="/opt/qt/$QT_VERSION/gcc_64"

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config git curl ca-certificates file python3-pip \
    libcurl4-openssl-dev libsodium-dev libglib2.0-0 libopengl0 libgl1-mesa-dev libegl1 libfontconfig1 libdbus-1-3 \
    libxkbcommon-dev libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 libxcb-xkb1 \
    libwayland-client0 libwayland-cursor0 libwayland-egl1

if [ ! -d "$QT_DIR" ]; then
    pip3 install --no-cache-dir aqtinstall
    python3 -m aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 -O /opt/qt
fi

# SC_VERSION_OVERRIDE (optional): release version set by the CI
cmake -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_DIR" \
    -DSC_VERSION_OVERRIDE="${SC_VERSION_OVERRIDE:-}"
cmake --build "$BUILD" --target appimage

# Give the files back to the user who started the container
if [ -n "${HOST_UID:-}" ]; then
    chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" "$BUILD"
fi

ls -la "$BUILD"/*.AppImage "$BUILD"/*.AppImage.zsync
