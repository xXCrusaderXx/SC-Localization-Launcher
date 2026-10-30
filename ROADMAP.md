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
| **0.11.0** | Blueprints | Spike first: map mission texts to `global.ini` keys and find the `Game.log` lines for earned blueprints. Then: blueprint details in mission descriptions, list of earned blueprints from `Game.log` (also live while playing), import/export. |
| **0.12.0** | Statistics & dashboard | Play time, ships, quantum jumps, and crashes from `Game.log`, dashboard tiles, HTML export, system information. |
| **1.0.0** | Stable | First-start assistant, light/dark theme, user documentation, Flathub submission. |

Phases after 1.0.0 are open. Ideas that don't belong to a phase yet are collected below.

## Open ideas

Unscheduled proposals, in no particular order. A maintainer moves an idea into a milestone once it is clear and someone works on it.

- More interface languages (`translations/*.ts`).
- Support for more launch methods besides the LUG Helper and Lutris (e.g. Heroic, Bottles, Steam with Proton).
- Optional per-channel notifications on the desktop.
- Window mode, resolution and FOV from the launcher, once their values in `attributes.xml` are confirmed ([#120](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/120)).

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
