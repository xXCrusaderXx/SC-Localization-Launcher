# StarCitizen Deutsch Launcher

[![CI](https://github.com/xXCrusaderXx/StarCitizenDeutschLinux/actions/workflows/ci.yml/badge.svg)](https://github.com/xXCrusaderXx/StarCitizenDeutschLinux/actions/workflows/ci.yml)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/xXCrusaderXx/StarCitizenDeutschLinux)](https://github.com/xXCrusaderXx/StarCitizenDeutschLinux/releases)

A Linux launcher that installs and updates the community-made **German translation for Star Citizen**. It does this per game channel (LIVE, PTU, EPTU, Hotfix, Tech Preview).

> This is an unofficial fan project. It is not affiliated with Cloud Imperium Games or Roberts Space Industries. See [LEGAL.md](LEGAL.md).

## Features

- Finds the installation folder of every Star Citizen channel and checks which translation version is installed.
- Installs and updates the German translation with one click, or automatically at startup.
- Can start the RSI Launcher after an update, minimise itself, and start with the system.
- Ships as an AppImage, `.deb`, `.rpm`, and Flatpak.

The translation texts themselves are maintained by the community project [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI). Report wrong or missing texts there, not here.

## Installation

Download the latest build from the [releases page](https://github.com/xXCrusaderXx/StarCitizenDeutschLinux/releases):

| Format   | Install                                                                   |
| -------- | ------------------------------------------------------------------------- |
| AppImage | `chmod +x SC-Deutsch-Launcher-x86_64.AppImage && ./SC-Deutsch-Launcher-x86_64.AppImage` |
| Debian / Ubuntu | `sudo apt install ./sc-deutsch-launcher_*.deb`                     |
| Fedora / openSUSE | `sudo dnf install ./sc-deutsch-launcher-*.rpm`                   |

## Building from source

Requirements: CMake ≥ 3.16, a C++20 compiler, Qt 6.4 or newer (Quick/QML), libcurl, and nlohmann_json ≥ 3.11 (downloaded automatically if missing).

```sh
# Arch Linux
sudo pacman -S --needed base-devel cmake ninja qt6-declarative curl nlohmann-json
# Debian / Ubuntu
sudo apt install build-essential cmake ninja-build qt6-declarative-dev libcurl4-openssl-dev nlohmann-json3-dev

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build          # portable layout in release/
```

For distribution packages and Flatpak, use the FHS layout:

```sh
cmake -B build -DSC_SYSTEM_INSTALL=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
cd build && cpack -G DEB       # or -G RPM
```

To build the AppImage the same way the release pipeline does (Ubuntu 22.04 in Docker), run:

```sh
packaging/appimage/build-in-docker.sh
```

## Contributing

Contributions are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) first. Every commit needs a DCO sign-off (`git commit -s`). Pull requests are checked by CI and merge automatically once they are approved.

- Security problems: [SECURITY.md](SECURITY.md)
- Code of conduct: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- Legal and trademark notes: [LEGAL.md](LEGAL.md)

## License

The launcher is licensed under the [GNU General Public License v3.0 or later](LICENSE). The translation texts it downloads have their own license (see [LEGAL.md](LEGAL.md)).
