# Legal notes and contribution policy

This document explains the project's legal position and the rules that keep it in place. Contributors must follow it. If a contribution you want to make conflicts with it, **don't make the contribution**. Open an issue and ask first.

Hobbyists wrote this document, not lawyers. It is not legal advice; it describes how the project operates.

---

## No affiliation

SC Localization Launcher is an unofficial, non-commercial fan project. It is **not affiliated with, endorsed by, or sponsored by Cloud Imperium Games (CIG) or Roberts Space Industries (RSI)**.

*Star Citizen®*, *Squadron 42®*, *Roberts Space Industries®*, and *Cloud Imperium®* are trademarks of Cloud Imperium Rights LLC and Cloud Imperium Rights Ltd. All other trademarks belong to their owners. This project uses these names only to describe which game the launcher works with.

## What the launcher does, and what it doesn't

The launcher installs community translation files into the localization folder of a Star Citizen installation and sets the matching language option in `user.cfg`. CIG provides this mechanism for community localization.

The launcher **does not**:

- modify, patch, or inject anything into game executables or game archives (`.p4k`),
- interact with, disable, or work around anti-cheat (Easy Anti-Cheat) or any other protection measure,
- read or transmit account data, credentials, or game memory,
- provide any gameplay advantage.

Contributions that would change any of these points will not be accepted.

## Translation texts

The translation texts are **not part of this repository** and are not covered by its license. They are made by independent community projects. [`languages.json`](languages.json) only lists where each project publishes its files, and the launcher downloads them from there at runtime. For example, the German translation comes from [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI), licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).

- We **link, we don't host**: no translation file is stored in, mirrored to, or redistributed through this repository or its releases.
- A language is only listed when its maintainers publish it themselves and its license allows this. The launcher shows the project and license of every translation it installs.
- The launcher does not use the servers of other launchers or projects beyond the files their maintainers publish for download.
- Additions the launcher writes into the installed `global.ini` on the player's computer (item values, blueprint details) turn it into a modified copy. That copy stays on the player's computer, is never redistributed, and remains under the translation's license.
- The original English game texts belong to CIG.

## Game data

Item values, blueprint and mission data come from the [Star Citizen Wiki API](https://api.star-citizen.wiki). Wiki content is licensed under CC BY-SA; the launcher credits the source wherever it shows or uses this data. Data about the player's own progress (earned blueprints, statistics) is read from the local `Game.log` and never leaves the computer.

To know which entry of `global.ini` belongs to which Wiki item, a build step matches the Wiki data against the English `global.ini` that the community translation project publishes. That file is only read during the build; it is never committed, cached or redistributed. The published mapping contains only `global.ini` keys and Wiki item IDs, no game texts.

## Forbidden content

Don't add any of the following to this repository, and don't base a contribution on it:

1. **Game assets or game data**: files extracted from Star Citizen installations, `.p4k` contents, original `global.ini` files, images, logos, fonts, or audio from CIG.
2. **Leaked, datamined-under-NDA, or confidential material**: anything covered by the Evocati/PTU NDA or obtained by breaking an agreement or access control.
3. **Code or assets under incompatible licenses.** Third-party code must be compatible with GPL-3.0-or-later, and its license and origin must be named in the PR.
4. **Tooling that circumvents protection measures** or automates anything against CIG's Terms of Service.
5. **Credentials, tokens, or personal data** of any kind.

## Privacy

The launcher only sends HTTPS requests to fetch the language list, the channel lists and translation files, and (once those features exist) game data from the Star Citizen Wiki API and its own update information from GitHub. It has no telemetry, collects no personal data, and has no accounts. Settings, logs and anything read from `Game.log` stay on the computer.

## License and warranty

The launcher's source code is licensed under the [GNU General Public License v3.0 or later](LICENSE). The software comes **without any warranty**, as stated in sections 15 and 16 of the GPL. You use it at your own risk.

## Concerns and takedown requests

If you believe this repository contains material that infringes your rights, or that violates the rules above:

- Open a public [issue](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues) for non-sensitive concerns.
- Use private vulnerability reporting as described in [SECURITY.md](SECURITY.md) if the report contains confidential or personal details.

Please name the affected files or commits and the basis of your claim. The maintainers will reply promptly and remove infringing material.
