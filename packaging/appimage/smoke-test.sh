#!/usr/bin/env bash
# Starts the AppImage on a distro without Qt and saves a screenshot of the main window.
# Proves that the AppImage brings everything it needs. Runs as root inside a distro container.
#
# Usage: smoke-test.sh <AppImage> <screenshot.png>
set -euo pipefail

APPIMAGE="$1"
SCREENSHOT="$2"

# Only what every desktop distro ships anyway: X server for the test, GL, fonts, xcb
if command -v apt-get >/dev/null; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends xvfb xauth libgl1 libegl1 libfontconfig1 fonts-dejavu-core \
        libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
        libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 libxcb-xkb1 libdbus-1-3 libglib2.0-0 ca-certificates >/dev/null
elif command -v dnf >/dev/null; then
    dnf install -y -q xorg-x11-server-Xvfb xauth mesa-libGL mesa-libEGL fontconfig dejavu-sans-fonts \
        libxkbcommon-x11 xcb-util-cursor xcb-util-wm xcb-util-image xcb-util-keysyms xcb-util-renderutil \
        dbus-libs glib2 which >/dev/null
elif command -v pacman >/dev/null; then
    pacman -Sy --noconfirm --needed xorg-server-xvfb xorg-xauth mesa fontconfig ttf-dejavu \
        libxkbcommon-x11 xcb-util-cursor xcb-util-wm xcb-util-image xcb-util-keysyms xcb-util-renderutil >/dev/null
fi

if ldconfig -p | grep -q libQt6Core; then
    echo "Qt is installed on this system, the test would prove nothing" >&2
    exit 1
fi

export HOME="$(mktemp -d)"
export APPIMAGE_EXTRACT_AND_RUN=1
export QT_QUICK_BACKEND=software
export QT_FORCE_STDERR_LOGGING=1
export SC_SCREENSHOT="$SCREENSHOT"

xvfb-run -a -s "-screen 0 1280x800x24" timeout 60 "$APPIMAGE"
test -s "$SCREENSHOT"
echo "OK: $SCREENSHOT"
