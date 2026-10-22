#!/usr/bin/env bash
# Builds the native packages locally, the same way the CI does.
# Result: build-packages/*.deb and build-packages/*.rpm
#
# Usage: build-in-docker.sh [image ...]   (default: ubuntu:24.04 debian:12 fedora:latest)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGES=("$@")
[ ${#IMAGES[@]} -eq 0 ] && IMAGES=(ubuntu:24.04 debian:12 fedora:latest)

for image in "${IMAGES[@]}"; do
    out="build-packages/$(echo "$image" | tr ':/' '--')"
    docker run --rm \
        -v "$ROOT:/src:ro" \
        -v "$ROOT/build-packages:/src/build-packages" \
        -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
        "$image" \
        bash /src/packaging/native/build.sh /src "/src/$out"
done
