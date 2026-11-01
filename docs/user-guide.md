# User guide

How to install the SC Localization Launcher, set it up, and fix the usual problems. [Deutsche Fassung](user-guide.de.md).

- [Install](#install)
- [First start](#first-start)
- [The main window](#the-main-window)
- [Languages](#languages)
- [Updates](#updates)
- [Tools](#tools)
- [Troubleshooting](#troubleshooting)
- [Reporting a problem](#reporting-a-problem)

## Install

Pick the file for your system on the [releases page](https://github.com/xXCrusaderXx/SC-Localization-Launcher/releases); the [README](../README.md#installation) lists the install command for each one.

- **AppImage**: works on every distribution with glibc 2.35 or newer and updates itself.
- **Flatpak** (also Steam Deck): updates through `flatpak update` or your software centre.
- **deb, rpm, Arch package**: installed with your package manager; the launcher offers new versions and installs them with your password.

The launcher needs Star Citizen installed with the [LUG Helper](https://github.com/starcitizen-lug/lug-helper), Lutris, or any other Wine prefix. It does not install the game.

## First start

On the first start the setup assistant opens. It takes four steps, and every step can be skipped:

1. **Welcome**: the language of the launcher itself.
2. **Find Star Citizen**: *Search automatically* looks through your drives for the RSI Launcher and the game channels (LIVE, PTU, …). If nothing is found, *Choose folder …* and pick the channel folder, e.g. `…/StarCitizen/LIVE`.
3. **Choose a translation**: the language Star Citizen should use, set for every channel found.
4. **Almost done**: automatic updates and the system tray. *Install now* downloads the translation; the game is not started.

You can open the assistant again under Settings → Help → *Setup assistant …*.

## The main window

Each game channel with a folder has a row:

- The **dot** shows the state: green = up to date, amber = an update is waiting, red = the files were changed or the last update failed.
- The **drop-down** chooses the translation for this channel. *English* removes the translation again.
- **↻ Repair** downloads the translation again and rewrites its lines in `user.cfg`, e.g. after a game update replaced the files.
- **✎** changes the channel's folder.

The big button at the bottom right installs what is waiting. With *Start Star Citizen afterwards* switched on, it then starts the RSI Launcher through the LUG Helper's `sc-launch.sh` or Lutris; if nothing is waiting, it only starts the game.

While Star Citizen runs, translations are never changed. A waiting update follows when the game is closed.

## Languages

**Game translations** come from community projects such as [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI); Settings → Translations names the project and the licence of each. Report wrong or missing texts to that project, not to the launcher.

- A language that a source does not offer for a channel right now is shown but cannot be picked.
- *Own source …* in the drop-down takes the HTTPS address of any `global.ini` and the language folder it belongs in.

**Item values** (Settings → Behaviour, both off by default) come from the [Star Citizen Wiki](https://star-citizen.wiki) and are added to the installed translation with the next update:

- *Add size, class and grade to item names*, e.g. "Holdstrong (S4 Military A)".
- *Add item values to descriptions*, e.g. "HP 100,000 · Regen 19,000/s" for shields.

**The launcher's own language** is set under Settings → Behaviour → *Language of the launcher*: *Automatic (system language)* or one of English, Deutsch, Español, Français, Italiano, Nederlands, Polski, Português. *Appearance* chooses a dark or light theme, or follows the system. *Streamer mode* hides your user name and home folder in the paths the launcher shows, while you stream or record.

## Updates

- **Translations**: while the launcher is open, it checks every hour. With *Update translations automatically* it installs new versions by itself, and waits for the game to close if it runs.
- **System tray**: the tray icon shows the same coloured dot; its menu has *Update translations*, *Open* and *Quit*. With *Keep running in the system tray*, closing the window leaves the launcher running there. GNOME needs the AppIndicator extension for a tray.
- **Without a window**: `sc-localization-launcher --update` updates from a script or a `systemd --user` timer, see [Command line](../README.md#command-line).
- **The launcher itself**: *Check for launcher updates on start*, or the ⇩ button in the title bar. Every release is signed and checked before it is installed.

## Tools

All under Settings → Tools.

- **Clear caches …**: removes the shader caches of the game, DXVK/VKD3D, Mesa and NVIDIA. Helps against stutter after a game or driver update; the first start afterwards takes longer. Graphics settings stay. Not while Star Citizen runs.
- **Backups …**: keybindings, profile settings, saved characters and `user.cfg` per channel. A restore first backs up what it replaces; it is refused while Star Citizen runs. The newest 10 backups per channel are kept.
- **Game settings …** per channel:
  - frame limits in the game and in menus, and your own `key = value` lines, written into `user.cfg`. After the next game start the launcher shows which of them Star Citizen ignored ("not whitelisted").
  - the graphics API (Vulkan or DirectX 11), written into the game's `GraphicsSettings.json`. It is written when the game is not running, and again after the first start of every new game version, which begins with the game's defaults.
- **Programs …**: your own programs or scripts that start when Star Citizen starts, e.g. a head-tracking tool. Each can be ended again when the game quits. Quotes work as in a shell; for variables or pipes, start a script or `sh -c '…'`. The launcher has to be open (the tray is enough).

## Troubleshooting

**The game is English again after a game update.**
The update replaced the translation; the channel's dot is red. Click ↻ *Repair*, or let *Update translations automatically* do it.

**The game stays English although the translation is installed.**
Check that the channel's folder is the one the game runs from (e.g. `…/StarCitizen/LIVE`, not `…/StarCitizen`). Settings → Help → *Check installation …* shows whether `global.ini` and the language lines in `user.cfg` are in place. Restart Star Citizen completely.

**"Cannot start the game: install it with the LUG Helper or Lutris".**
The launcher starts the game through the LUG Helper's `sc-launch.sh` next to the Wine prefix or through Lutris. With another setup, start the game yourself; the translation is installed all the same.

**"The RSI Launcher did not start".**
The link in the message opens the log. The end of `sc-launch.log` (LUG Helper) usually says why. Starting the game once through the LUG Helper or Lutris directly often shows the same error more clearly.

**A setting from "Game settings" has no effect.**
If the dialog lists it as ignored ("not whitelisted"), Star Citizen does not accept it from `user.cfg`. The graphics API only changes when the game is not running; the dialog says when it is still waiting.

**Starting the launcher only brings an open window to the front.**
Only one launcher runs at a time. If it is in the tray, a second start opens that one. To start a different version, quit the running one first: tray → *Quit*.

**The Flatpak does not find the game.**
Use *Choose folder …*. The Flatpak may read all folders and starts the game outside its sandbox.

## Reporting a problem

1. Settings → Help → *Check installation …* → *Save report (ZIP)*. It contains the checks, the settings and the launcher's logs, with your home folder and user name replaced; `Game.log` is not included.
2. Open an [issue](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues) and attach the ZIP.

Wrong or missing texts in the game belong to the translation project named in Settings → Translations.
