#!/usr/bin/env bash
# Adds a menu entry for the AppImage where it is, without copying it (install.sh copies it to ~/Applications).
# Expects SC-Localization-Launcher.AppImage next to this script.
set -euo pipefail

APP_ID="io.github.xXCrusaderXx.SCLocalizationLauncher"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APPIMAGE="${SCRIPT_DIR}/SC-Localization-Launcher.AppImage"
DESKTOP_FILE="$HOME/.local/share/applications/${APP_ID}.desktop"

if [[ ! -f "$APPIMAGE" ]]; then
    echo "AppImage not found: $APPIMAGE" >&2
    exit 1
fi

chmod +x "$APPIMAGE"
mkdir -p "$(dirname "$DESKTOP_FILE")"

ICON="${SCRIPT_DIR}/logo.png"
[[ -f "$ICON" ]] || ICON="$APP_ID"

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=SC Localization Launcher
Comment=Installs and updates community translations for Star Citizen
Comment[de]=Installiert und aktualisiert Community-Übersetzungen für Star Citizen
Exec="${APPIMAGE}"
Icon=${ICON}
Terminal=false
StartupNotify=true
StartupWMClass=sc-localization-launcher
Categories=Game;
EOF

update-desktop-database "$HOME/.local/share/applications" &>/dev/null || true

echo "Menu entry: $DESKTOP_FILE"
