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
- Multiple translation languages from a community-maintained `languages.json`, loaded from the repository so new languages arrive without a release. German, German (full) and Swiss German are included.
- The interface is available in English and German and follows the system language.
- Public [roadmap](ROADMAP.md). Closing a roadmap milestone publishes a release automatically.
- Issue templates for new game languages, roadmap proposals and release checklists.

### Changed

- Renamed from *StarCitizen Deutsch Launcher* to *SC Localization Launcher*: the launcher now covers community translations in several languages.
- The project is now licensed under GPL-3.0-or-later.
- The window opens right away; the channel list is downloaded in the background.
- New `settings.json` format. Existing settings are migrated automatically on the first start; older launcher versions cannot read the new file.
- A missing or broken `settings.json` no longer stops the launcher, it starts with default settings instead.
- Inside the Flatpak, Star Citizen is started on the host (`flatpak-spawn --host`).
- A language drop-down per channel replaces the fixed language buttons.
- Translations are downloaded directly from the translation projects; the launcher no longer contacts the servers of other launchers.
- Settings moved to `~/.config/sc-localization-launcher/` (migrated automatically from `~/SC-Deutsch-Launcher`), logs to `~/.local/state/sc-localization-launcher/logs/`.

### Fixed

- If a chosen language is temporarily not offered for a channel, updates skip that channel instead of removing the installed translation.
- When switching languages, the previous language's folder is removed only after the new download succeeded.
