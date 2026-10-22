#!/usr/bin/env bash
# Builds the distributable AppImage locally, the same way the CI does.
# Result: build-appimage/SC-Deutsch-Launcher-x86_64.AppImage
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

docker run --rm \
    -v "$ROOT:/src" \
    -v sc-deutsch-launcher-qt:/opt/qt \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
    ubuntu:22.04 \
    bash /src/packaging/appimage/build.sh /src
