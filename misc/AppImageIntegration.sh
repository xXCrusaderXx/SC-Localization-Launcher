#!/bin/bash

# Basisverzeichnis bestimmen (Verzeichnis, in dem dieses Skript liegt)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# AppImage und Icon annehmen
APPIMAGE="${SCRIPT_DIR}/SC-Deutsch-Launcher.AppImage"
ICON="${SCRIPT_DIR}/resources/logo.png"  # Falls AppImage kein eingebettetes Icon hat

# Name und Zielpfad der Desktop-Datei
APP_ID="starcitizen-deutsch-launcher"
APP_NAME="SC-Deutsch-Launcher"
DESKTOP_FILE="$HOME/.local/share/applications/${APP_ID}.desktop"

# Prüfen, ob AppImage existiert
if [[ ! -f "$APPIMAGE" ]]; then
    echo "❌ AppImage nicht gefunden: $APPIMAGE"
    exit 1
fi

# Desktop-Datei schreiben
mkdir -p ~/.local/share/applications

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Name=${APP_NAME}
Comment=Launcher für Star Citizen Übersetzungen
Exec="${APPIMAGE}"
Icon=${APPIMAGE}  # oder: Icon=${ICON}
Terminal=false
Type=Application
Categories=Game;Utility;
StartupNotify=true
EOF

# Rechte setzen
chmod +x "$DESKTOP_FILE"
chmod +x "$APPIMAGE"


echo "✅ Desktop-Datei installiert:"
echo "   $DESKTOP_FILE"
echo "👉 Das Programm sollte nun im Startmenü erscheinen."

# Optional: Desktop-Datenbank neu laden (meist nicht nötig)
update-desktop-database ~/.local/share/applications/ &> /dev/null
