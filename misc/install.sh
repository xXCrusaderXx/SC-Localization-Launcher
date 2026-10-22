#!/usr/bin/env bash
# Installs the AppImage for the current user: copies it to ~/Applications and adds a menu entry with icon.
# Expects SC-Localization-Launcher.AppImage next to this script (the portable release/appimage/ folder).
set -euo pipefail

APP_ID="io.github.xXCrusaderXx.SCLocalizationLauncher"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_APPIMAGE="${SCRIPT_DIR}/SC-Localization-Launcher.AppImage"

TARGET_DIR="$HOME/Applications"
TARGET_APPIMAGE="${TARGET_DIR}/SC-Localization-Launcher.AppImage"
DESKTOP_FILE="$HOME/.local/share/applications/${APP_ID}.desktop"
ICON_FILE="$HOME/.local/share/icons/hicolor/256x256/apps/${APP_ID}.png"

if [[ ! -f "$SOURCE_APPIMAGE" ]]; then
    echo "AppImage not found: $SOURCE_APPIMAGE" >&2
    exit 1
fi

mkdir -p "$TARGET_DIR" "$(dirname "$DESKTOP_FILE")" "$(dirname "$ICON_FILE")"
cp "$SOURCE_APPIMAGE" "$TARGET_APPIMAGE"
chmod +x "$TARGET_APPIMAGE"

# The AppImage carries its icon as .DirIcon; extract only that file into a temp dir
EXTRACT_DIR="$(mktemp -d)"
trap 'rm -rf "$EXTRACT_DIR"' EXIT
(cd "$EXTRACT_DIR" && "$TARGET_APPIMAGE" --appimage-extract .DirIcon >/dev/null 2>&1) || true
if [[ -e "$EXTRACT_DIR/squashfs-root/.DirIcon" ]]; then
    cp -L "$EXTRACT_DIR/squashfs-root/.DirIcon" "$ICON_FILE"
elif [[ -f "${SCRIPT_DIR}/logo.png" ]]; then
    cp "${SCRIPT_DIR}/logo.png" "$ICON_FILE"
fi

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=SC Localization Launcher
Comment=Installs and updates community translations for Star Citizen
Comment[de]=Installiert und aktualisiert Community-Übersetzungen für Star Citizen
Exec="${TARGET_APPIMAGE}"
Icon=${APP_ID}
Terminal=false
StartupNotify=true
StartupWMClass=sc-localization-launcher
Categories=Game;
EOF

update-desktop-database "$HOME/.local/share/applications" &>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" &>/dev/null || true

echo "Installed: $TARGET_APPIMAGE"
echo "Menu entry: $DESKTOP_FILE"
