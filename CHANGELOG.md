# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- New Qt Quick (QML) user interface.
- AppImage, `.deb`, `.rpm`, and Flatpak packaging.
- Contributor documentation, CI, DCO check, and automatic merging of approved pull requests.
- Automated backend and GUI tests (`-DSC_BUILD_TESTS=ON`, run with `ctest`), also in CI.
- The status line reports when a translation update failed.

### Changed

- The project is now licensed under GPL-3.0-or-later.
- The window opens right away; the channel list is downloaded in the background.
- New `settings.json` format. Existing settings are migrated automatically on the first start; older launcher versions cannot read the new file.
- A missing or broken `settings.json` no longer stops the launcher, it starts with default settings instead.
- Inside the Flatpak, Star Citizen is started on the host (`flatpak-spawn --host`).
