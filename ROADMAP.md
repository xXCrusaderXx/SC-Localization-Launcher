# Roadmap

This roadmap is a **living document**. Phases, features, and their order change as the project learns. New ideas are welcome at any time; see [Proposing changes](#proposing-changes).

Each phase is a GitHub [milestone](https://github.com/xXCrusaderXx/SC-Localization-Launcher/milestones) named `vX.Y.Z – Name`, and each feature is an issue in it. **When a milestone is closed, a release is created automatically** (see [Releases](#releases)).

## Phases

| Release | Phase | Content |
| ------- | ----- | ------- |
| **0.6.0** | Foundation & rename | Qt Quick interface, command/state protocol, tests, packaging (AppImage, deb, rpm, Arch, Flatpak). Rename to *SC Localization Launcher*. Language catalogue in [`languages.json`](languages.json), loaded straight from the translation projects (no third-party launcher servers), with German, German (full) and Swiss German. Interface follows the system language (English, German). Visible credit and license for every translation. Milestone releases. |
| **0.7.0** | Languages & updates | Own translation source (`global.ini` URL) per channel. Notice text per channel and language. Own signed Flatpak repository, updated through `flatpak update`. "Game is running" detection: the update button starts the game, shows when the RSI Launcher or the game runs, and translations are not changed while the game runs. Shipped early in 0.6.0: update check with ETag, "Repair" button, traffic-light status per channel, signed in-app self-update. Further European translations are added to `languages.json` by PR at any time ([#12](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/12)); they don't need a launcher release. |
| **0.8.0** | Background operation | Tray icon with status light, periodic update check, updates after the game exits. Headless CLI (`--update`, `--check`, `--version`, … with exit codes 0/1/2) for scripts and `systemd --user` timers, lock against parallel runs. Automatic update checks in the background, delta updates for the AppImage (zsync). "What's new" dialog. |
| **0.9.0** | Item values & strings | Item stats and type/size/class added to item names and descriptions in the installed `global.ini`, each category optional. Data from the [Star Citizen Wiki API](https://api.star-citizen.wiki). Re-applied after every translation update. |
| **0.10.0** | Tools | Profile backup (keybindings, `attributes.xml`, characters, `user.cfg`) per channel with restore. Cache cleaner for Linux (game shader cache in the Wine prefix, DXVK/vkd3d, Mesa, NVIDIA). Read-only problem analysis with a report for bug reports. Game settings: frame limits and own lines in `user.cfg`, graphics API (Vulkan or DirectX 11). Programs started together with the game. Moved before Blueprints, which wait for `Game.log` lines of earned blueprints ([#22](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/22)). |
| **0.11.0** | Interface languages | Interface language selectable in the settings (automatic = system language), applied without a restart. Interface translations for French, Spanish, Italian, Polish, Dutch and Portuguese next to English and German, reviewed by native speakers. |
| **1.0.0** | Stable | First-start assistant, light/dark theme, user documentation. The Flatpak stays in the project's own signed repository; a Flathub submission is not planned ([#36](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/36)). |
| **1.1.1** | Statistics & dashboard | Play time, sessions, ships, quantum jumps, missions, kills and crashes from `Game.log`, with an HTML export ([#30](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/30)). Dashboard tiles ([#31](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/31)). System information with the game's CPU and GPU performance index per session, to see what changed after a driver or game update ([#32](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/32)). Streamer mode that hides names and locations ([#139](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/139)). |
| **1.2.0** | More item values | Values for armour and mining lasers in item names and descriptions, from the Star Citizen Wiki API ([#98](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/98)). |
| **1.3.0** | Tools & diagnostics | Programs that start with the RSI Launcher ([#140](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/140)). Server reachability and duplicate installations in the problem analysis ([#141](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/141)). Backups with the launcher's settings and a comparison before restoring ([#142](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/142)). Desktop notifications per channel ([#143](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/143)). Start with the system ([#144](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/144)). |

Later phases are open; new releases are added below the last one. (1.1.0 was published by mistake with the code of 1.0.0; the statistics phase shipped as 1.1.1.)

## Pending: waiting for information

Planned, but something must be found out before work can start. These items have no version; they sit in the milestone [Pending – needs information](https://github.com/xXCrusaderXx/SC-Localization-Launcher/milestones). Once the missing information is there, an item gets the next free release. A release never waits for a pending item: if a release item turns out to miss information, it moves here and the release goes ahead without it.

| Feature | Missing |
| ------- | ------- |
| Blueprints: details in mission descriptions, list of earned blueprints, import/export ([#22](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/22), [#23](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/23), [#24](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/24)) | `Game.log` lines of an earned blueprint in a current game version ([#22](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/22) asks players for them) |
| Window mode, resolution, VSync, FOV and upscaling from the launcher ([#120](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/120)) | What the values in `attributes.xml` mean; needs a before/after comparison in the game |
| More game languages, e.g. French, Italian, Portuguese, Japanese ([#12](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/12)) | Translation projects that publish their files over HTTPS under a licence that allows it |
| More launch methods: Heroic, Bottles, Steam with Proton ([#145](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/145)) | A test installation per launcher: start command, prefix location, how to see that the game runs |

## Open ideas

Unscheduled, not planned yet. A maintainer moves an idea into a release or into Pending once it is clear.

- An in-game overlay (location, ship, mission). Hard on Linux and Wayland.
- Editing joystick curves and key bindings from the launcher.

Add ideas through a [roadmap proposal](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/new?template=roadmap_proposal.yml).

## Data sources

| Data | Source | License / terms |
| ---- | ------ | --------------- |
| Translations | The community projects listed in [`languages.json`](languages.json), e.g. [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI) | Set by each project, e.g. CC BY-NC-SA 4.0. Credited in the launcher. |
| Item values, blueprints, missions | [Star Citizen Wiki API](https://api.star-citizen.wiki) | Wiki content under CC BY-NC-SA 4.0; credited in the launcher. |
| Which `global.ini` entry belongs to which Wiki item | [`item-mapping.json`](item-mapping.json), generated weekly by `tools/build-item-mapping.py` | Keys and Wiki item IDs only, no game texts; see [LEGAL.md](LEGAL.md#game-data). |
| Earned blueprints, statistics | The player's own `Game.log` | Stays on the player's computer. |

The launcher never hosts translation texts or game data itself. See [LEGAL.md](LEGAL.md).

## Proposing changes

- **New feature or change to a phase:** open a [roadmap proposal](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/new?template=roadmap_proposal.yml). Proposals are discussed in the issue. Accepted ones get a milestone, or land under [Open ideas](#open-ideas).
- **New game language:** open a [new language request](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/new?template=new_language.yml), or send a PR that edits `languages.json` (see [CONTRIBUTING.md](CONTRIBUTING.md#adding-a-game-language)). The launcher reads `languages.json` from `main`, so a new language reaches users when `dev` is merged into `main` at the next release point; no launcher update is needed.
- This file is updated in the same PR that changes a milestone.

## Releases

- Work happens on feature branches that are merged into `dev`. At a release point, `dev` is merged into `main` by the maintainer.
- Closing a milestone `vX.Y.Z – Name` starts the `Milestone release` workflow. It checks that every issue in the milestone is closed, collects the closed issues and pull requests as release notes, and runs the `Release` workflow for version `X.Y.Z` on `main`.
- The `Release` workflow builds and tests all packages and publishes them as GitHub release `vX.Y.Z`.
- If a milestone is closed by mistake while issues are still open, no release is made. Reopen the milestone, finish the issues, and close it again.
- Bug-fix releases (`X.Y.Z+1`) use their own small milestone, or a manual run of the `Release` workflow.
