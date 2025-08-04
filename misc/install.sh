#!/bin/bash

# Basisverzeichnis bestimmen
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_APPIMAGE="${SCRIPT_DIR}/SC-Deutsch-Launcher.AppImage"

# Zielverzeichnis für AppImage im Home
TARGET_DIR="$HOME/SC-Deutsch-Launcher"
TARGET_APPIMAGE="${TARGET_DIR}/SC-Deutsch-Launcher.AppImage"

# Ziel für extrahiertes Icon
ICON_NAME="logo"
ICON_TARGET_PATH="$HOME/.local/share/icons/${ICON_NAME}.png"

# .desktop-Datei
APP_ID="starcitizen-deutsch-launcher"
APP_NAME="SC-Deutsch-Launcher"
DESKTOP_FILE="$HOME/.local/share/applications/${APP_ID}.desktop"

# 1. Prüfen, ob AppImage existiert
if [[ ! -f "$SOURCE_APPIMAGE" ]]; then
    echo "❌ AppImage nicht gefunden: $SOURCE_APPIMAGE"
    exit 1
fi

# 2. AppImage ins Ziel kopieren
mkdir -p "$TARGET_DIR"
cp "$SOURCE_APPIMAGE" "$TARGET_APPIMAGE"
chmod +x "$TARGET_APPIMAGE"

# 3. Icon extrahieren aus /resources/
echo "📦 Extrahiere Icon aus AppImage..."
rm -rf squashfs-root  # für den Fall, dass es von vorher noch existiert

"$TARGET_APPIMAGE" --appimage-extract > /dev/null

# Ziel: PNG- oder SVG-Icon unter /resources/
ICON_SOURCE=$(find squashfs-root/resources/ -type f \( -iname '*.png' -o -iname '*.svg' \) | head -n 1)

if [[ -f "$ICON_SOURCE" ]]; then
    cp "$ICON_SOURCE" "$ICON_TARGET_PATH"
    echo "✅ Icon extrahiert nach: $ICON_TARGET_PATH"
else
    echo "⚠️ Kein Icon unter /resources/ gefunden. Standard-Icon wird verwendet."
    ICON_TARGET_PATH=""
fi

# Aufräumen
rm -rf squashfs-root

# 4. Desktop-Datei erstellen
mkdir -p ~/.local/share/applications

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Name=${APP_NAME}
Comment=Launcher für Star Citizen Übersetzungen
Exec="${TARGET_APPIMAGE}"
Icon=${ICON_TARGET_PATH}
Terminal=false
Type=Application
Categories=Game;Utility;
StartupNotify=true
EOF

chmod +x "$DESKTOP_FILE"

echo "✅ Desktop-Datei installiert unter:"
echo "   $DESKTOP_FILE"
echo "👉 Das Programm sollte jetzt im Startmenü erscheinen."

# 5. Optional: Desktop-Datenbank aktualisieren
update-desktop-database ~/.local/share/applications/ &> /dev/null
