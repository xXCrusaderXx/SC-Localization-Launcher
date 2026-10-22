#!/usr/bin/env bash
# Builds the native package for the distro it runs on: .deb on Debian/Ubuntu, .rpm on Fedora/openSUSE.
# Used by the CI and locally via packaging/native/build-in-docker.sh. Runs as root inside the container.
#
# Usage: build.sh <source dir> <output dir>
set -euo pipefail

SRC="$1"
OUT="$2"
BUILD="$(mktemp -d)"

if command -v apt-get >/dev/null; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build file dpkg-dev ca-certificates \
        libcurl4-openssl-dev nlohmann-json3-dev libgl-dev \
        qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-declarative-dev-tools
    GENERATOR=DEB
elif command -v dnf >/dev/null; then
    dnf install -y gcc-c++ cmake ninja-build rpm-build libcurl-devel json-devel \
        qt6-qtbase-devel qt6-qtdeclarative-devel
    GENERATOR=RPM
elif command -v zypper >/dev/null; then
    zypper --non-interactive install gcc-c++ cmake ninja rpm-build libcurl-devel nlohmann_json-devel \
        qt6-base-devel qt6-declarative-devel
    GENERATOR=RPM
else
    echo "Unsupported distro" >&2
    exit 1
fi

cmake -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DSC_SYSTEM_INSTALL=ON
cmake --build "$BUILD"
(cd "$BUILD" && cpack -G "$GENERATOR")

mkdir -p "$OUT"
cp "$BUILD"/*.deb "$BUILD"/*.rpm "$OUT"/ 2>/dev/null || true
if [ -n "${HOST_UID:-}" ]; then
    chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" "$OUT"
fi
ls -la "$OUT"
