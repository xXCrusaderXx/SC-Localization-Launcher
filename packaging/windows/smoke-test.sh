#!/usr/bin/env bash
# Unpacks the Windows ZIP into an empty folder and starts it with a PATH that has only Windows itself, so a DLL that
# the ZIP lacks cannot come from MSYS2. Saves a screenshot of the main window and checks that HTTPS works.
# Runs in an MSYS2 shell on Windows.
#
# Usage: smoke-test.sh <zip> <screenshot.png>
set -euo pipefail

ZIP="$(realpath "$1")"
SCREENSHOT="$(cygpath -w "$(realpath -m "$2")")"
WORK="$(mktemp -d)"
cd "$WORK"
unzip -q "$ZIP"
PROGRAM="$(cygpath -w "$(find "$WORK" -name sc-localization-launcher.exe | head -1)")"

# A fresh profile: no settings, no logs of earlier runs
PROFILE="$(cygpath -w "$WORK/profile")"
mkdir -p "$WORK/profile/Local" "$WORK/profile/Roaming"
run () {
    timeout 120 env -i SYSTEMROOT="$SYSTEMROOT" WINDIR="$WINDIR" TEMP="$TEMP" TMP="$TMP" USERPROFILE="$PROFILE" \
        LOCALAPPDATA="$PROFILE\\Local" APPDATA="$PROFILE\\Roaming" PATH="/c/Windows/System32:/c/Windows" "$@"
}

run "$PROGRAM" --version

# Without a game folder --check ends with exit code 2, but it loads the language list over HTTPS first
run "$PROGRAM" --check || true
LOGS="$WORK/profile/Local/sc-localization-launcher/logs"
cat "$LOGS"/*.log
if grep -h "download() - .* failed" "$LOGS"/*.log; then
    echo "HTTPS does not work in the ZIP" >&2
    exit 1
fi

run SC_SCREENSHOT="$SCREENSHOT" "$PROGRAM"
test -s "$(cygpath -u "$SCREENSHOT")"
echo "Started from the ZIP and saved $SCREENSHOT"
