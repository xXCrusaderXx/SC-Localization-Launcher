#!/usr/bin/env bash
# Installs a built package on a fresh distro, starts the launcher and saves a screenshot.
# Proves that the package declares all runtime dependencies. Runs as root inside a distro container.
#
# Usage: install-test.sh <package file> <screenshot.png>
set -euo pipefail

PACKAGE="$1"
SCREENSHOT="$2"

if command -v apt-get >/dev/null; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq "$PACKAGE" xvfb xauth fonts-dejavu-core >/dev/null
elif command -v dnf >/dev/null; then
    dnf install -y -q "$PACKAGE" xorg-x11-server-Xvfb xauth dejavu-sans-fonts which >/dev/null
elif command -v zypper >/dev/null; then
    zypper --non-interactive --no-gpg-checks install "$PACKAGE" xvfb-run dejavu-fonts >/dev/null
elif command -v pacman >/dev/null; then
    pacman -Sy --noconfirm >/dev/null
    pacman -U --noconfirm "$PACKAGE" >/dev/null
    pacman -S --noconfirm --needed xorg-server-xvfb xorg-xauth ttf-dejavu >/dev/null
fi

test -f /usr/share/applications/io.github.xXCrusaderXx.StarCitizenDeutschLinux.desktop
test -f /usr/share/icons/hicolor/256x256/apps/io.github.xXCrusaderXx.StarCitizenDeutschLinux.png
test -f /usr/share/metainfo/io.github.xXCrusaderXx.StarCitizenDeutschLinux.metainfo.xml

export HOME="$(mktemp -d)"
export QT_QUICK_BACKEND=software
export QT_FORCE_STDERR_LOGGING=1
export SC_SCREENSHOT="$SCREENSHOT"

xvfb-run -a -s "-screen 0 1280x800x24" timeout 60 SC-Deutsch-Launcher
test -s "$SCREENSHOT"
echo "OK: $SCREENSHOT"
