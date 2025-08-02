#!/bin/bash

# Pfade relativ zu diesem Skript
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="${SCRIPT_DIR}/StarCitizen-Launcher"
ICON="${SCRIPT_DIR}/resources/logo.png"
DESKTOP_FILE="${SCRIPT_DIR}/StarCitizen-Launcher.desktop"

# Prüfen, ob Binary existiert
if [[ ! -f "$BINARY" ]]; then
    echo "❌ Binary nicht gefunden: $BINARY"
    exit 1
fi

# Desktop-Datei schreiben
cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Name=Star Citizen Deutsch Launcher
Comment=Launcher für Star Citizen Übersetzungen
Exec=${BINARY}
Icon=${ICON}
Path=${SCRIPT_DIR}
Type=Application
Terminal=false
StartupNotify=true
StartupWMClass=StarCitizenLauncher
Categories=Game;Utility;
EOF

# Optional: Datei ausführbar machen
chmod +x "$DESKTOP_FILE"

# Hinweis zur Installation (optional)
echo "✅ Desktop-Datei erstellt: $DESKTOP_FILE"
echo "👉 Du kannst sie mit folgendem Befehl ins Menü integrieren:"
echo "   cp \"$DESKTOP_FILE\" ~/.local/share/applications/"
