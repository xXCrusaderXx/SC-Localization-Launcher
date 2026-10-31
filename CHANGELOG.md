# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.0.0] - 2026-10-31

The first stable version: a setup assistant, a light theme and a user guide.

### Added

- Setup assistant on the first start: the launcher's language, finding the game, the translation for every game version found, and the main options, in four steps that can each be skipped. Settings → Help → "Setup assistant …" opens it again. Existing installations don't see it.
- Settings → Behaviour → "Appearance": like the system, dark or light. The launcher follows a change of the desktop's colour scheme while it runs.
- User guide in English and German ([docs/user-guide.md](docs/user-guide.md)), opened from Settings → Help → "User guide".

### Changed

- The description of "Game settings …" mentions the graphics API, and the German interface says "Infobereich" for the tray everywhere.

## [0.11.0] - 2026-10-31

The launcher speaks more languages.

### Added

- Settings → Behaviour → "Language of the launcher": pick the interface language or keep "Automatic (system language)". The change applies at once, without a restart.
- The interface is now also available in French, Spanish, Italian, Polish, Dutch and Portuguese (Brazil). These translations are first versions; corrections by native speakers are welcome (see CONTRIBUTING.md).

## [0.10.0] - 2026-10-30

Tools: programs that start with the game, and the graphics API.

### Added

- Settings → Tools → "Programs …": your own programs or scripts start together with Star Citizen, also when the game was started without the launcher (the launcher has to be open, e.g. in the tray). Each can be switched off or ended again when the game quits. In the Flatpak they run on the host.
- "Game settings …" can set the graphics API per channel (Vulkan or DirectX 11) in the game's `GraphicsSettings.json`. Never while the game runs; set again after every game version update, which starts with a fresh file.

## [0.9.1] - 2026-10-26

Item values in descriptions, and game settings in `user.cfg`.

### Added

- New setting "Add item values to descriptions" (off by default): item descriptions start with a line of values from the [Star Citizen Wiki API](https://api.star-citizen.wiki), e.g. "HP 100.000 · Regen 19.000/s" for shields or "DPS 304 · Alpha 73 · Reichweite 2.703 m" for ship weapons. It covers shields, power plants, coolers, quantum drives, ship and personal weapons, missiles and missile racks, with German labels and numbers for German translations.
- Settings → Tools → "Game settings …" writes frame limits (`sys_maxFps`, `sys_maxIdleFps`) and your own `key = value` lines into `user.cfg` per channel. The launcher shows which of them the game ignored ("Not Whitelisted" in `Game.log`).

### Changed

- Installing a translation keeps the order and line endings of `user.cfg`.
- Builds from source show their version from git (e.g. `0.10.0-dev+1a2b3c4`) instead of 0.1.0.

## [0.9.0] - 2026-10-24

Item names with their values.

### Added

- New setting "Add size, class and grade to item names" (off by default): ship components, ship and personal weapons, missiles and turrets show size, class and grade in their name, e.g. "Holdstrong (S4 Militär A)". The data comes from the [Star Citizen Wiki API](https://api.star-citizen.wiki) (CC BY-NC-SA 4.0, credited in the settings) and is applied to every freshly downloaded translation.
- `item-mapping.json` links `global.ini` keys to Wiki items. It is rebuilt every week and contains keys and IDs only, no game texts.

### Changed

- The Star Citizen Wiki content is credited under CC BY-NC-SA 4.0 (was wrongly listed as CC BY-SA).

## [0.8.0] - 2026-10-24

The launcher now works in the background.

### Added

- Tray icon with a status light for all channels and an Update/Open menu. "Close to tray" is offered only when the desktop has a tray.
- Translations are checked every hour; if wanted, they are updated automatically after the game exits.
- Command line mode: `--check` and `--update` run without a window, for scripts and systemd timers. The packages install a daily update timer that is switched off by default.
- "What's new" window: after an update, the launcher shows the release notes of the new version once.
- AppImage updates download only the changed blocks (zsync), typically a few hundred kilobytes instead of the whole AppImage.
- Shader cache cleaner in the settings: removes the game's shader cache and the DXVK, vkd3d, Mesa and NVIDIA caches, never while the game is running.
- Problem analysis: a read-only check of the game folders, Wine prefix, permissions, `global.ini` and `user.cfg`, with a ZIP report for bug reports (home paths anonymized).

### Changed

- The launcher needs Qt Widgets (for the tray icon on KDE Plasma).
- Releases can update the AUR package automatically.

### Fixed

- GUI tests no longer fail when other test runs start fake game processes at the same time.

## [0.7.0] - 2026-10-24

### Added

- Own translation source per channel: any `global.ini` link (HTTPS) can be chosen as "Own source" in the channel's language list, for example to test a translation.
- Translation projects can show a short notice per channel and language, set in `languages.json`.
- Own signed Flatpak repository: the Flatpak installs with one click (`.flatpakref`) and then updates through `flatpak update` or the software centre.
- The launcher updates itself in the Flatpak (from the project's Flatpak repository; bundle installs switch to it once) and on Linux Mint, Pop!_OS, LMDE, Nobara and other systems based on Ubuntu 24.04, Debian 12 or Fedora.
- The update button follows the game. It shows "Start" when every translation is current and "Update & start" only when a channel needs an update. During the start it shows "Starting game …", then "Running" while the RSI Launcher is open and "Game running" while Star Citizen runs. This also works when the game was started without the launcher.
- The status line reports when the RSI Launcher did not start, with a link to the log, or when neither the LUG Helper's `sc-launch.sh` nor Lutris was found. The end of `sc-launch.log` is written to the launcher's log.

### Changed

- Translations are not updated or repaired while Star Citizen runs.
- Software centres show the real launcher version and release date.
- A second start that only brings the running launcher to the front no longer creates a log file, and the last 10 log files are kept instead of 5.

### Fixed

- The RSI Launcher now also starts when the launcher itself was started from VS Code or another Electron app; before, it quit at once with "bad option".
- The launcher update now recognises a running Star Citizen under Wine and waits; before, it did not notice the game.
- The launcher starts again after it crashed or was killed, and the Flatpak starts even if a native launcher left its lock behind. Before, it quit without a window until the next login.

## [0.6.1] - 2026-10-23

### Fixed

- The AppImage no longer crashes at startup on Wayland desktops (KDE Plasma, GNOME, …). It lacked Qt's Wayland EGL plugin. The `.deb`, `.rpm`, Arch and Flatpak packages were not affected.

## [0.6.0] - 2026-10-23

First release as *SC Localization Launcher*.

### Added

- New Qt Quick (QML) user interface.
- AppImage, `.deb`, `.rpm`, and Flatpak packaging.
- Contributor documentation, CI with tests on the oldest supported Qt (6.4), DCO check, automatic merging and release pipeline.
- Automated backend and GUI tests (`-DSC_BUILD_TESTS=ON`, run with `ctest`), also in CI.
- The status line reports when a translation update failed.
- Multiple translation languages from a community-maintained `languages.json`, loaded from the repository so new languages arrive without a release. German, German (full) and Swiss German are included.
- The interface is available in English and German and follows the system language.
- Public [roadmap](ROADMAP.md). Closing a roadmap milestone publishes a release automatically.
- Issue templates for new game languages, roadmap proposals and release checklists.
- Each channel shows whether its translation is installed, missing or outdated; the launcher checks for new translation versions at start without downloading them.
- Updates download only translations that changed (HTTP ETag); unchanged channels are skipped.
- Signed releases: `SHA256SUMS` is signed with minisign (`SHA256SUMS.minisig`, public key in `packaging/minisign.pub`). Every package is installed and started on fresh distributions before a release is published.
- The AppImage carries update information and a `.zsync` file for AppImageUpdate.
- Launcher update button in the title bar: checks the GitHub releases for a newer launcher version and shows its release notes and download page. Optional check at start (off by default).
- The launcher installs its own updates from signed GitHub releases: the AppImage replaces itself, `.deb`, `.rpm` and Arch packages are installed through `pkexec`; the Flatpak and source builds link to the release page. Nothing is installed without a valid signature, and never while Star Citizen runs.
- "Repair" button per channel: downloads the translation again and rewrites `user.cfg`. Channels whose `global.ini` or language setting was changed (for example by a game update) or whose last update failed are shown in red.

### Changed

- Renamed from *StarCitizen Deutsch Launcher* to *SC Localization Launcher*: the launcher now covers community translations in several languages.
- The project is now licensed under GPL-3.0-or-later.
- The window opens right away; the channel list is downloaded in the background.
- New `settings.json` format. Existing settings are migrated automatically on the first start; older launcher versions cannot read the new file.
- A missing or broken `settings.json` no longer stops the launcher, it starts with default settings instead.
- Inside the Flatpak, Star Citizen is started on the host (`flatpak-spawn --host`).
- A language drop-down per channel replaces the fixed language buttons.
- The automatic search checks the usual places first (`~/Games/star-citizen`, `~/.wine`, Lutris prefixes, common folders on mounted disks) and finds the game in milliseconds; the full scan stays as fallback.
- The main page only lists channels with a game folder. Without any, it offers the automatic search and the settings; all channels stay configurable in the settings.
- Translations are downloaded directly from the translation projects; the launcher no longer contacts the servers of other launchers.
- Settings moved to `~/.config/sc-localization-launcher/` (migrated automatically from `~/SC-Deutsch-Launcher`), logs to `~/.local/state/sc-localization-launcher/logs/`.
- The settings show the folder with the log files, and a failed update links straight to it.
- Settings that are not implemented yet are hidden instead of being shown as "coming soon".

### Fixed

- "Choose folder" works in the `.deb` packages for Debian and Ubuntu (missing dependency `qml6-module-qt-labs-folderlistmodel`).
- Long option texts or install paths no longer push the settings panel wider than the window; the texts wrap and paths are shortened.
- If a chosen language is temporarily not offered for a channel, updates skip that channel instead of removing the installed translation.
- When switching languages, the previous language's folder is removed only after the new download succeeded.
- Only one launcher runs at a time; starting it again brings the open window to the front instead of running two launchers that write the same settings and game folders.

[Unreleased]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.11.0...v1.0.0
[0.11.0]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.10.0...v0.11.0
[0.10.0]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.9.1...v0.10.0
[0.9.1]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.9.0...v0.9.1
[0.9.0]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.8.0...v0.9.0
[0.8.0]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.6.1...v0.7.0
[0.6.1]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/tag/v0.6.0
