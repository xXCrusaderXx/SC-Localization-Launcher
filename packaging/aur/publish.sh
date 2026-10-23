#!/usr/bin/env bash
# Publishes packaging/aur/PKGBUILD for a release to the AUR: sets the version and the checksum of the release
# tarball, generates .SRCINFO and pushes both to the package's AUR git repository.
# Runs as root inside an archlinux container (makepkg itself runs as an unprivileged user).
#
# Usage: publish.sh <version>          e.g. 0.7.0; pre-releases (0.7.0-rc1) are not published
#   AUR_SSH_PRIVATE_KEY  private key of the AUR account (skips with a warning when empty)
#   AUR_REPO             git URL of the package (default: the AUR package sc-localization-launcher)
#   SOURCE_URL           release tarball (default: the GitHub tag v<version>); tests point it at a local file
set -euo pipefail

VERSION="${1#v}"
PKGBUILD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AUR_REPO="${AUR_REPO:-ssh://aur@aur.archlinux.org/sc-localization-launcher.git}"
SOURCE_URL="${SOURCE_URL:-https://github.com/xXCrusaderXx/SC-Localization-Launcher/archive/refs/tags/v$VERSION.tar.gz}"
# Published on https://aur.archlinux.org (SHA256:RFzBCUItH9LZS0cKB5UE6ceAYhBD5C8GeOBip8Z11+4)
AUR_HOST_KEY="aur.archlinux.org ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIEuBKrPzbawxA/k2g6NcyV5jmqwJ2s+zpgZGZ7tpLIcN"

if [[ "$VERSION" == *-* ]]; then
    echo "Pre-release $VERSION is not published to the AUR."
    exit 0
fi
if [[ "$AUR_REPO" == ssh://* && -z "${AUR_SSH_PRIVATE_KEY:-}" ]]; then
    echo "::warning::Secret AUR_SSH_PRIVATE_KEY is missing, the AUR package is not updated."
    exit 0
fi

pacman -Syu --noconfirm --needed base-devel git openssh >/dev/null

# makepkg refuses to run as root
id builder >/dev/null 2>&1 || useradd -m builder
WORK="$(mktemp -d)"
chown builder "$WORK"

if [[ "$AUR_REPO" == ssh://* ]]; then
    mkdir -p -m 700 ~/.ssh
    printf '%s\n' "$AUR_SSH_PRIVATE_KEY" > ~/.ssh/aur
    chmod 600 ~/.ssh/aur
    printf '%s\n' "$AUR_HOST_KEY" > ~/.ssh/known_hosts
    export GIT_SSH_COMMAND="ssh -i ~/.ssh/aur -o IdentitiesOnly=yes -o StrictHostKeyChecking=yes"
fi

# A new package base is created by its first push, so an empty clone is fine
git clone --quiet "$AUR_REPO" "$WORK/aur" 2>/dev/null || { git init --quiet "$WORK/aur"; git -C "$WORK/aur" remote add origin "$AUR_REPO"; }
cd "$WORK/aur"
# The checkout belongs to the makepkg user, git runs as root
git config --global --add safe.directory "$WORK/aur"
git checkout --quiet -B master

checksum="$(curl --fail --silent --show-error --location "$SOURCE_URL" | sha256sum | cut -d' ' -f1)"
sed -e "s/^pkgver=.*/pkgver=$VERSION/" \
    -e "s/^pkgrel=.*/pkgrel=1/" \
    -e "s/^sha256sums=.*/sha256sums=('$checksum')/" \
    -e '/^# pkgver and the checksum are filled in/d' \
    "$PKGBUILD_DIR/PKGBUILD" > PKGBUILD
chown -R builder .
su builder -c "makepkg --printsrcinfo" > .SRCINFO

git add PKGBUILD .SRCINFO
if git diff --cached --quiet; then
    echo "The AUR package already has version $VERSION."
    exit 0
fi
git -c user.name="Klein, Marko" -c user.email="55139200+xXCrusaderXx@users.noreply.github.com" \
    commit --quiet -m "Update to $VERSION"
git push --quiet origin master
echo "Published sc-localization-launcher $VERSION to the AUR."
