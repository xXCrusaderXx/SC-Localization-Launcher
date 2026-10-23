#!/usr/bin/env bash
# Builds the Flatpak as a signed repository and as a single-file bundle. The repository is published on GitHub Pages,
# so installs from it update through `flatpak update`. Used by the Release workflow (flatpak-github-actions container).
#
# Usage: build-repo.sh <source dir> <version> <output dir>
#   FLATPAK_GPG_PRIVATE_KEY  armored secret key; must belong to packaging/flatpak-signing-key.asc
#   REQUIRE_SIGNATURE=true   fail without the key (real releases); otherwise build unsigned with a warning
#   SITE_URL                 public URL of the published site (default: the project's GitHub Pages)
#
# Output: <output dir>/SC-Localization-Launcher-<version>-x86_64.flatpak and <output dir>/site/ (repo, .flatpakref,
# .flatpakrepo, index.html), ready for actions/upload-pages-artifact.
set -euo pipefail

SRC="$(realpath "$1")"
VERSION="$2"
OUT="$(realpath -m "$3")"

APP_ID="io.github.xXCrusaderXx.SCLocalizationLauncher"
SITE_URL="${SITE_URL:-https://xxcrusaderxx.github.io/SC-Localization-Launcher}"
RUNTIME_REPO="https://dl.flathub.org/repo/flathub.flatpakrepo"
PUBLIC_KEY="$SRC/packaging/flatpak-signing-key.asc"
BUNDLE="$OUT/SC-Localization-Launcher-$VERSION-x86_64.flatpak"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT/site"

# The repository is only trustworthy when signed with the key users import from packaging/flatpak-signing-key.asc
sign_args=()
if [ -n "${FLATPAK_GPG_PRIVATE_KEY:-}" ]; then
    export GNUPGHOME="$WORK/gnupg"
    mkdir -m 700 "$GNUPGHOME"
    printf '%s\n' "$FLATPAK_GPG_PRIVATE_KEY" | gpg --batch --quiet --import
    key="$(gpg --batch --with-colons --list-secret-keys | awk -F: '/^fpr/ { print $10; exit }')"
    expected="$(gpg --batch --with-colons --show-keys "$PUBLIC_KEY" | awk -F: '/^fpr/ { print $10; exit }')"
    if [ "$key" != "$expected" ]; then
        echo "::error::FLATPAK_GPG_PRIVATE_KEY ($key) does not belong to packaging/flatpak-signing-key.asc ($expected)"
        exit 1
    fi
    sign_args=(--gpg-sign="$key" --gpg-homedir="$GNUPGHOME")
elif [ "${REQUIRE_SIGNATURE:-false}" = "true" ]; then
    echo "::error::Secret FLATPAK_GPG_PRIVATE_KEY is missing; the Flatpak repository is only published signed"
    exit 1
else
    echo "::warning::Secret FLATPAK_GPG_PRIVATE_KEY is missing; this Flatpak repository is not signed"
fi

# Same manifest, plus the release version (binary and metainfo) and an absolute source path
jq --arg version "$VERSION" --arg src "$SRC" \
    '(.modules[] | select(.name == "sc-localization-launcher")) |=
        (."config-opts" += ["-DSC_VERSION_OVERRIDE=" + $version] | .sources[0].path = $src)' \
    "$SRC/flatpak/$APP_ID.json" > "$WORK/manifest.json"

flatpak-builder --disable-rofiles-fuse --force-clean --install-deps-from=flathub \
    --state-dir="$WORK/state" --default-branch=stable --repo="$OUT/site/repo" "${sign_args[@]}" \
    "$WORK/build" "$WORK/manifest.json"
flatpak build-update-repo --generate-static-deltas --prune "${sign_args[@]}" "$OUT/site/repo"
flatpak build-bundle --runtime-repo="$RUNTIME_REPO" "$OUT/site/repo" "$BUNDLE" "$APP_ID" stable

# Install files: a .flatpakref installs the app (and pulls the runtime from Flathub), a .flatpakrepo adds the remote
gpg_key="$(gpg --batch --dearmor < "$PUBLIC_KEY" | base64 -w0)"
cat > "$OUT/site/$APP_ID.flatpakref" <<EOF
[Flatpak Ref]
Name=$APP_ID
Branch=stable
Title=SC Localization Launcher
Url=$SITE_URL/repo/
RuntimeRepo=$RUNTIME_REPO
IsRuntime=false
GPGKey=$gpg_key
EOF
cat > "$OUT/site/sc-localization-launcher.flatpakrepo" <<EOF
[Flatpak Repo]
Title=SC Localization Launcher
Url=$SITE_URL/repo/
Homepage=https://github.com/xXCrusaderXx/SC-Localization-Launcher
Comment=Community translations for Star Citizen on Linux
GPGKey=$gpg_key
EOF
cat > "$OUT/site/index.html" <<EOF
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<title>SC Localization Launcher – Flatpak repository</title>
<h1>SC Localization Launcher – Flatpak repository</h1>
<p>Version $VERSION. Install it and get updates through <code>flatpak update</code> or your software centre:</p>
<pre>flatpak install --user $SITE_URL/$APP_ID.flatpakref</pre>
<p>Or add the repository first:</p>
<pre>flatpak remote-add --user --if-not-exists sc-localization-launcher $SITE_URL/sc-localization-launcher.flatpakrepo
flatpak install --user sc-localization-launcher $APP_ID</pre>
<p><a href="https://github.com/xXCrusaderXx/SC-Localization-Launcher">Project page</a></p>
</html>
EOF

# A client that trusts only packaging/flatpak-signing-key.asc must accept the repository
if [ ${#sign_args[@]} -gt 0 ]; then
    export FLATPAK_USER_DIR="$WORK/verify"
    flatpak remote-add --user --gpg-import="$PUBLIC_KEY" verify "file://$OUT/site/repo"
    flatpak remote-ls --user --columns=application,branch verify | grep -q "^$APP_ID[[:space:]]stable"
    echo "Signature of the repository verified with packaging/flatpak-signing-key.asc"
fi

ls -la "$OUT" "$OUT/site"
