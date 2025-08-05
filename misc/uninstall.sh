#!/bin/bash

APP_ID="starcitizen-deutsch-launcher"
APP_NAME="SC-Deutsch-Launcher"
TARGET_DIR="$HOME/SC-Deutsch-Launcher"
DESKTOP_FILE="$HOME/.local/share/applications/${APP_ID}.desktop"

echo "🗑️  Deinstalliere ${APP_NAME}..."

# .desktop-Datei entfernen
if [[ -f "$DESKTOP_FILE" ]]; then
    rm "$DESKTOP_FILE"
    echo "✅ Desktop-Datei entfernt: $DESKTOP_FILE"
else
    echo "ℹ️  Keine Desktop-Datei gefunden: $DESKTOP_FILE"
fi

# AppImage-Verzeichnis entfernen
if [[ -d "$TARGET_DIR" ]]; then
    rm -rf "$TARGET_DIR"
    echo "✅ Verzeichnis entfernt: $TARGET_DIR"
else
    echo "ℹ️  Kein Installationsverzeichnis gefunden: $TARGET_DIR"
fi

# Desktop-Datenbank aktualisieren
update-desktop-database ~/.local/share/applications/ &> /dev/null

echo "✅ ${APP_NAME} wurde vollständig deinstalliert."
