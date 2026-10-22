#!/usr/bin/env bash
# Builds the PKGBUILD from a local source tarball instead of the GitHub tag, so it can be tested before a release.
# Runs as root inside an archlinux container.
#
# Usage: test.sh <source tarball> <output dir>
#   PKGVER (optional): package version to build instead of the one in the PKGBUILD, "-" replaced by "_"
#   The tarball must contain SC-Localization-Launcher-<pkgver>/, e.g. from:
#   git archive --prefix=SC-Localization-Launcher-0.1.0/ -o src.tar.gz HEAD
set -euo pipefail

TARBALL="$(realpath "$1")"
OUT="$(realpath "$2")"
PKGBUILD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

pacman -Syu --noconfirm --needed base-devel sudo >/dev/null

# makepkg refuses to run as root
id builder >/dev/null 2>&1 || useradd -m builder
echo 'builder ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/builder

WORK=/home/builder/pkg
rm -rf "$WORK" && mkdir -p "$WORK"
cp "$PKGBUILD_DIR/PKGBUILD" "$WORK/"
cp "$TARBALL" "$WORK/local-source.tar.gz"
sed -i 's#^source=.*#source=("local-source.tar.gz")#' "$WORK/PKGBUILD"
if [ -n "${PKGVER:-}" ]; then
    sed -i "s#^pkgver=.*#pkgver=$PKGVER#" "$WORK/PKGBUILD"
fi
chown -R builder "$WORK"

su builder -c "cd $WORK && makepkg -s --noconfirm"

mkdir -p "$OUT"
cp "$WORK"/sc-localization-launcher-[0-9]*.pkg.tar.zst "$OUT"/
ls -la "$OUT"
