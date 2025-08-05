#!/bin/bash

# 0. Basisverzeichnis bestimmen
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_APPIMAGE="${SCRIPT_DIR}/SC-Deutsch-Launcher.AppImage"

# 1. Zielverzeichnis im Home
TARGET_DIR="$HOME/SC-Deutsch-Launcher"
TARGET_APPIMAGE="${TARGET_DIR}/SC-Deutsch-Launcher.AppImage"
DESKTOP_FILE="$HOME/.local/share/applications/starcitizen-deutsch-launcher.desktop"

# 2. Icon vorbereiten
ICON_NAME="starcitizen-deutsch-launcher"
ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"
ICON_TARGET="${ICON_DIR}/${ICON_NAME}.png"

# 3. Prüfen, ob AppImage existiert
if [[ ! -f "$SOURCE_APPIMAGE" ]]; then
    echo "❌ AppImage nicht gefunden: $SOURCE_APPIMAGE"
    exit 1
fi

# 4. AppImage extrahieren, um Icon zu finden (falls möglich)
rm -rf squashfs-root
"$SOURCE_APPIMAGE" --appimage-extract &> /dev/null

ICON_FROM_APPIMAGE="$(find squashfs-root -type f -iname '*.png' | head -n1)"

if [[ -f "$ICON_FROM_APPIMAGE" ]]; then
    echo "🎨 Icon aus AppImage extrahiert: $ICON_FROM_APPIMAGE"
    mkdir -p "$ICON_DIR"
    cp "$ICON_FROM_APPIMAGE" "$ICON_TARGET"
else
    # Fallback auf logo.png im gleichen Verzeichnis wie das Skript
    FALLBACK_ICON="${SCRIPT_DIR}/logo.png"
    if [[ ! -f "$FALLBACK_ICON" ]]; then
        echo "❌ Weder Icon im AppImage noch logo.png gefunden!"
        exit 1
    fi
    echo "🎨 Fallback: eigenes Icon verwendet: $FALLBACK_ICON"
    mkdir -p "$ICON_DIR"
    cp "$FALLBACK_ICON" "$ICON_TARGET"
fi

# 5. AppImage kopieren und ausführbar machen
mkdir -p "$TARGET_DIR"
cp "$SOURCE_APPIMAGE" "$TARGET_APPIMAGE"
chmod +x "$TARGET_APPIMAGE"

# 6. Desktop-Datei erzeugen
mkdir -p ~/.local/share/applications

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Name=SC Deutsch Launcher
Comment=Launcher für Star Citizen Übersetzungen
Exec=${TARGET_APPIMAGE}
Icon=${ICON_NAME}
Terminal=false
Type=Application
Categories=Game;Utility;
StartupNotify=true
StartupWMClass=SC-Deutsch-Launcher
EOF

chmod +x "$DESKTOP_FILE"

# 7. Caches aktualisieren
update-desktop-database ~/.local/share/applications/ &> /dev/null
gtk-update-icon-cache ~/.local/share/icons/hicolor/ &> /dev/null

# 8. Aufräumen
rm -rf squashfs-root

# 9. Erfolgsmeldung
echo "✅ Desktop-Datei installiert unter:"
echo "   $DESKTOP_FILE"
echo "👉 Das Programm sollte jetzt im Startmenü erscheinen."
