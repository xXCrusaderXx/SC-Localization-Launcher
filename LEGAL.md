# Legal notes and contribution policy

This document explains the project's legal position and the rules that keep it in place. Contributors must follow it. If a contribution you want to make conflicts with it, **don't make the contribution**. Open an issue and ask first.

Hobbyists wrote this document, not lawyers. It is not legal advice; it describes how the project operates.

---

## No affiliation

StarCitizen Deutsch Launcher is an unofficial, non-commercial fan project. It is **not affiliated with, endorsed by, or sponsored by Cloud Imperium Games (CIG) or Roberts Space Industries (RSI)**.

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

The translation texts are **not part of this repository** and are not covered by its license. The launcher downloads them at runtime:

- the channel/version list from `https://www.sc-deutsch-launcher.de/`,
- the texts from the community project [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI), licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).

The launcher never bundles or relicenses translation texts. The original English game texts belong to CIG.

## Forbidden content

Don't add any of the following to this repository, and don't base a contribution on it:

1. **Game assets or game data**: files extracted from Star Citizen installations, `.p4k` contents, original `global.ini` files, images, logos, fonts, or audio from CIG.
2. **Leaked, datamined-under-NDA, or confidential material**: anything covered by the Evocati/PTU NDA or obtained by breaking an agreement or access control.
3. **Code or assets under incompatible licenses.** Third-party code must be compatible with GPL-3.0-or-later, and its license and origin must be named in the PR.
4. **Tooling that circumvents protection measures** or automates anything against CIG's Terms of Service.
5. **Credentials, tokens, or personal data** of any kind.

## Privacy

The launcher only sends HTTPS requests to fetch the channel list and translation files. It has no telemetry, collects no personal data, and has no accounts. Settings are stored locally.

## License and warranty

The launcher's source code is licensed under the [GNU General Public License v3.0 or later](LICENSE). The software comes **without any warranty**, as stated in sections 15 and 16 of the GPL. You use it at your own risk.

## Concerns and takedown requests

If you believe this repository contains material that infringes your rights, or that violates the rules above:

- Open a public [issue](https://github.com/xXCrusaderXx/StarCitizenDeutschLinux/issues) for non-sensitive concerns.
- Use private vulnerability reporting as described in [SECURITY.md](SECURITY.md) if the report contains confidential or personal details.

Please name the affected files or commits and the basis of your claim. The maintainers will reply promptly and remove infringing material.
