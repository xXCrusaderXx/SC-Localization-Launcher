# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

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

[Unreleased]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/compare/v0.6.0...HEAD
[0.6.0]: https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases/tag/v0.6.0
