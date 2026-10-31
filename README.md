# SC Localization Launcher

[![CI](https://github.com/xXCrusaderXx/SC-Localization-Launcher/actions/workflows/ci.yml/badge.svg)](https://github.com/xXCrusaderXx/SC-Localization-Launcher/actions/workflows/ci.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/xXCrusaderXx/SC-Localization-Launcher)](https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases)

A Linux launcher that installs and updates **community translations for Star Citizen**, separately for each game channel (LIVE, PTU, EPTU, HOTFIX, TECH-PREVIEW). It works with installations made by the [LUG Helper](https://github.com/starcitizen-lug/lug-helper), Lutris or any other Wine prefix.

German (standard, complete and Swiss German) is available today. More European languages are being added. Each language is listed in [`languages.json`](languages.json); adding one needs no programming, see [Adding a game language](CONTRIBUTING.md#adding-a-game-language).

What comes next — item values, blueprints, tray and background updates, backups and more — is in the [roadmap](ROADMAP.md). It is open for proposals.

New here? The [user guide](docs/user-guide.md) ([Deutsch](docs/user-guide.de.md)) explains installation, the first start and common problems.

> This is an unofficial fan project. It is not affiliated with Cloud Imperium Games or Roberts Space Industries. See [LEGAL.md](LEGAL.md).

## Features

- A setup assistant on the first start finds the game and sets the translation in four steps.
- Choose the translation for each channel, or switch a channel back to English.
- Optionally adds size, class and grade to item names and a line of values to item descriptions, from the [Star Citizen Wiki](https://star-citizen.wiki).
- Finds Star Citizen installations automatically, or lets you pick the folder.
- Downloads the translation safely: a failed download never replaces a working one, and only the language lines in your `user.cfg` are changed.
- Optionally starts Star Citizen after the update, through the LUG Helper's `sc-launch.sh` or Lutris.
- Keeps working offline with the last known channel list.
- Checks and updates from the command line, e.g. from a `systemd --user` timer, without opening a window.
- Checks for new translation versions every hour while it is open. With "Update translations automatically" it installs them, and waits until Star Citizen has been closed if the game runs.
- Shows the overall status as a coloured light in the system tray, with "Update translations" and "Open" in its menu. With "Keep running in the system tray", closing the window leaves the launcher running there. Desktops without a tray (GNOME without the AppIndicator extension) simply don't show it.
- Clears shader caches (Settings → Tools): the game's own cache in the Wine prefix, DXVK/VKD3D, and the Mesa and NVIDIA caches, with the size and the result per entry. Graphics settings are kept, and nothing is deleted while Star Citizen runs.
- Checks the installation without changing anything (Settings → Help → "Check installation …"): Wine prefix, folders, write permissions, `global.ini` and `user.cfg` per channel, free disk space. "Save report (ZIP)" puts the results, the settings and the launcher's logs into your Downloads folder for a bug report, with your home folder and user name replaced; `Game.log` is not included.
- Backs up keybindings, profile settings, saved characters and `user.cfg` per channel (Settings → Tools → "Backups …") and restores them after asking once; a restore first backs up what it replaces, and nothing is restored while Star Citizen runs. The newest 10 backups per channel are kept in `~/.local/share/sc-localization-launcher/backups`.
- Sets the frame limit (in game and in menus), your own `key = value` lines and the graphics API (Vulkan or DirectX 11) per channel (Settings → Tools → "Game settings …"). Every other line of `user.cfg` stays byte for byte, the language lines included. After the next game start the launcher reads `Game.log` and shows which of these settings Star Citizen ignored because they are not whitelisted.
- Starts your own programs or scripts together with the game, and ends them with it if you like (Settings → Tools → "Programs …").
- The interface is available in English, German, French, Spanish, Italian, Polish, Dutch and Portuguese, following the system language or the choice in the settings, with a light or dark theme.

Planned, not available yet: starting with the system, and a progress view.

The translation texts are maintained by community projects such as [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI). Report wrong or missing texts there, not here.

## Installation

Download the file for your system from the [releases page](https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases). Every package is installed and started on a fresh system before a release is published.

| System | File | Install |
| ------ | ---- | ------- |
| Any distribution | `SC-Localization-Launcher-*-x86_64.AppImage` | `chmod +x SC-Localization-Launcher-*.AppImage && ./SC-Localization-Launcher-*.AppImage` |
| Any distribution, Steam Deck | Flatpak repository (updates through `flatpak update`) | `flatpak install --user https://xxcrusaderxx.github.io/SC-Localization-Launcher/io.github.xXCrusaderXx.SCLocalizationLauncher.flatpakref` |
| Ubuntu 24.04 or newer | `sc-localization-launcher-*-ubuntu24.04-x86_64.deb` | `sudo apt install ./sc-localization-launcher-*-ubuntu24.04-x86_64.deb` |
| Debian 12 or newer | `sc-localization-launcher-*-debian12-x86_64.deb` | `sudo apt install ./sc-localization-launcher-*-debian12-x86_64.deb` |
| Fedora | `sc-localization-launcher-*-fedora-x86_64.rpm` | `sudo dnf install ./sc-localization-launcher-*-fedora-x86_64.rpm` |
| Arch Linux, Manjaro, CachyOS | `sc-localization-launcher-*-arch-x86_64.pkg.tar.zst` | `sudo pacman -U ./sc-localization-launcher-*-arch-x86_64.pkg.tar.zst` |

For openSUSE, Ubuntu 22.04 and other distributions, use the AppImage or the Flatpak. The AppImage brings its own Qt and runs on every distribution with glibc 2.35 or newer.

The Flatpak repository is signed with [`packaging/flatpak-signing-key.asc`](packaging/flatpak-signing-key.asc); the `.flatpakref` carries that key and pulls the KDE runtime from Flathub. Every release also has a single-file bundle (`SC-Localization-Launcher-*-x86_64.flatpak`) for offline installs, which does not update by itself.

The Flatpak can read game folders anywhere (`--filesystem=host`) and starts the game outside the sandbox through `flatpak-spawn --host` (`--talk-name=org.freedesktop.Flatpak`).

### Verifying a download

Every release has a `SHA256SUMS` file signed with [minisign](https://jedisct1.github.io/minisign/). The public key is [`packaging/minisign.pub`](packaging/minisign.pub):

```sh
minisign -Vm SHA256SUMS -p minisign.pub
sha256sum --check --ignore-missing SHA256SUMS
```

The launcher updates its AppImage itself and downloads only the blocks that changed (usually about 1 % of the file), using the release's `.zsync` file. The same update information also works with [AppImageUpdate](https://github.com/AppImageCommunity/AppImageUpdate).

### Where your data is stored

| What | Path |
| ---- | ---- |
| Settings | `~/.config/sc-localization-launcher/settings.json` |
| Cache (language and channel lists) | `~/.cache/sc-localization-launcher/` |
| Logs | `~/.local/state/sc-localization-launcher/logs/` |

The `XDG_*_HOME` variables are respected. The Flatpak keeps its data under `~/.var/app/io.github.xXCrusaderXx.SCLocalizationLauncher/`. Settings from the old `~/SC-Deutsch-Launcher` folder are taken over on the first start.

## Command line

Without options the launcher opens its window. With one of these options it works without a window and needs no display:

```sh
sc-localization-launcher --check               # which channels need an update; changes nothing
sc-localization-launcher --update              # update every channel that has a game folder
sc-localization-launcher --update --channel LIVE --channel PTU
sc-localization-launcher --version
```

It uses the channels, languages and game folders set in the window. It never starts the game, even with "Start Star Citizen afterwards" switched on.

| Exit code | Meaning |
| --------- | ------- |
| 0 | Every channel is up to date, or the update succeeded |
| 1 | `--check` only: at least one channel needs an update |
| 2 | Error: no game folder set, unknown channel, update failed, the launcher window is open, or Star Citizen is running |

While the window is open or Star Citizen runs, the command line changes nothing and exits with 2.

In the Flatpak, call it as `flatpak run io.github.xXCrusaderXx.SCLocalizationLauncher --update`, and for the AppImage as `./SC-Localization-Launcher-*.AppImage --update`.

### Updating daily with a systemd timer

A user service and timer run `--update` once a day and catch up after the computer was off. The `.deb`, `.rpm` and Arch packages install them, switched off; enable the timer for your user:

```sh
systemctl --user enable --now sc-localization-launcher-update.timer
```

For other installations, copy them from [`packaging/systemd`](packaging/systemd) to `~/.config/systemd/user/` first:

```sh
cp packaging/systemd/sc-localization-launcher-update.* ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now sc-localization-launcher-update.timer
```

The service runs `/usr/bin/sc-localization-launcher`, as installed by the `.deb`, `.rpm` and Arch packages. For the Flatpak or the AppImage, replace the command with `systemctl --user edit sc-localization-launcher-update.service`:

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/flatpak run io.github.xXCrusaderXx.SCLocalizationLauncher --update
```

`systemctl --user status sc-localization-launcher-update.service` shows the last result; the log is in the folder named under [Where your data is stored](#where-your-data-is-stored).

## Building from source

Requirements: CMake 3.16 or newer, a C++20 compiler, Qt 6.4 or newer (Quick/QML), libcurl, libsodium 1.0.18 or newer and nlohmann_json 3.11 or newer (downloaded automatically if missing). The Qt Linguist tools are optional; without them the interface is English only.

```sh
# Arch Linux
sudo pacman -S --needed base-devel cmake ninja qt6-declarative qt6-tools curl libsodium nlohmann-json
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build pkg-config libgl-dev libcurl4-openssl-dev libsodium-dev nlohmann-json3-dev \
    qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools qt6-tools-dev qt6-l10n-tools \
    qml6-module-qtquick-controls qml6-module-qtquick-dialogs qml6-module-qtquick-layouts
# Fedora
sudo dnf install gcc-c++ cmake ninja-build libcurl-devel libsodium-devel json-devel \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel qt6-linguist

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/sc-localization-launcher
```

### Tests

```sh
cmake -B build -G Ninja -DSC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests need no internet connection and run without a display.

### Packages

The release pipeline builds every package with the scripts in [`packaging/`](packaging). You can run them locally with Docker:

```sh
packaging/appimage/build-in-docker.sh     # AppImage, built on Ubuntu 22.04
packaging/native/build-in-docker.sh       # .deb (Ubuntu 24.04, Debian 12) and .rpm (Fedora)
flatpak-builder --user --install build-flatpak flatpak/io.github.xXCrusaderXx.SCLocalizationLauncher.json
packaging/flatpak/build-repo.sh . 0.0.0-dev flatpak-out  # repository + bundle as in a release (unsigned without the key)
```

Every release also updates the AUR package `sc-localization-launcher` through [`packaging/aur/publish.sh`](packaging/aur/publish.sh). It needs the secret `AUR_SSH_PRIVATE_KEY` (the SSH key of the AUR account); without it the step is skipped with a warning.

For your own distribution package, use the FHS layout:

```sh
cmake -B build -DSC_SYSTEM_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
cd build && cpack -G DEB       # or -G RPM
```

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) first. Every commit needs a DCO sign-off (`git commit -s`). Pull requests are checked by CI; a maintainer reviews and merges them.

- Security problems: [SECURITY.md](SECURITY.md)
- Code of conduct: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- Legal and trademark notes: [LEGAL.md](LEGAL.md)

## License

The launcher is licensed under the [GNU General Public License v3.0 or later](LICENSE). The translation texts it downloads have their own licenses (see [LEGAL.md](LEGAL.md)).
