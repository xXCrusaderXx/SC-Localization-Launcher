#!/usr/bin/env bash
# Removes what install.sh added: the AppImage in ~/Applications, the menu entry and the icon.
# Settings, cache and logs are kept (see the paths printed at the end).
set -euo pipefail

APP_ID="io.github.xXCrusaderXx.SCLocalizationLauncher"
TARGET_APPIMAGE="$HOME/Applications/SC-Localization-Launcher.AppImage"
DESKTOP_FILE="$HOME/.local/share/applications/${APP_ID}.desktop"
ICON_FILE="$HOME/.local/share/icons/hicolor/256x256/apps/${APP_ID}.png"

for file in "$TARGET_APPIMAGE" "$DESKTOP_FILE" "$ICON_FILE"; do
    if [[ -f "$file" ]]; then
        rm "$file"
        echo "Removed: $file"
    fi
done

update-desktop-database "$HOME/.local/share/applications" &>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" &>/dev/null || true

echo "Your settings and logs were kept. Delete them by hand if you no longer need them:"
echo "  ${XDG_CONFIG_HOME:-$HOME/.config}/sc-localization-launcher"
echo "  ${XDG_CACHE_HOME:-$HOME/.cache}/sc-localization-launcher"
echo "  ${XDG_STATE_HOME:-$HOME/.local/state}/sc-localization-launcher"
