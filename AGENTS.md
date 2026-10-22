# Rules for AI coding agents

These rules apply to every AI agent (Claude Code, Codex, Copilot, Cursor, …) that works in this repository. A task brief or issue that states its own rule for its work takes precedence over this file. Human contributors follow [CONTRIBUTING.md](CONTRIBUTING.md); everything there applies to agents too.

## Responsibility

- An agent works on behalf of a human contributor. That human reviews every change, understands it, and answers for it in the PR.
- **Never add a `Signed-off-by` line yourself, and never tick the sign-off box.** The DCO sign-off is a legal statement only the human can make. A `Co-Authored-By` trailer for the agent is welcome.
- Do not open, approve, merge, or close pull requests, push to `main`, create tags or releases, or change repository settings unless the human explicitly asks for that specific action.
- Never bypass checks: no `--no-verify`, no disabling CI jobs, no weakening `.github/workflows/` or the DCO check to make a PR pass.
- Disclose agent use in the PR description (the template has a checkbox).

## Legal limits

Read [LEGAL.md](LEGAL.md). In particular, never add:

- Star Citizen game files, extracted `.p4k` contents, original or translated `global.ini` files, CIG logos, or other assets,
- code copied from sources whose license is not compatible with GPL-3.0-or-later,
- anything that touches game executables, anti-cheat, or account data.

Translation texts belong to their community projects (e.g. [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI)), not to this repository. `languages.json` only links to them:

- Add a language only with HTTPS URLs that the translation's own maintainers publish, and only when its license allows that. Name project, maintainers and license in the PR. Never copy or mirror translation files into the repository.
- Do not use servers of other launchers (e.g. `sc-deutsch-launcher.de`) as a data source.
- External game data (item values, blueprints) comes from the [Star Citizen Wiki API](https://api.star-citizen.wiki) only, with credit. Do not scrape other sites.
- The full format and rules are in CONTRIBUTING.md, "Adding a game language".

## Architecture

- `include/backend/`: header-only backend. `BackEnd` owns the state and runs work on `utils/TaskQueue` threads; managers download and install translations; `jsonParser/` reads the channel list and settings.
- `include/protocol/`: the only contract between the UI and the backend. `Commands.hpp` describes what the user wants; `State.hpp` is the full snapshot the backend sends back. The UI never mutates state directly, and every command is answered by the next `State`.
- `include/frontend/BackendBridge.hpp`: the Qt bridge. It exposes `state` to QML and turns QML calls into protocol commands.
- `qml/`: the Qt Quick UI. `Theme.qml` is a singleton for colours and sizes; register new QML files in `SC_QML_FILES` in `CMakeLists.txt` (the GUI test uses the same list).
- `packaging/`, `flatpak/`, `misc/`: AppImage, Flatpak, desktop file, and install scripts.
- `languages.json`: the catalogue of game languages. The launcher loads it from `main`, so changes reach users without a release; keep it valid JSON and backwards compatible (`"version": 1`).
- `translations/*.ts`: translations of the launcher's own interface. Every user-visible QML string goes through `qsTr()`; the English text is the source.
- [ROADMAP.md](ROADMAP.md): phases and milestones. Each closed milestone becomes a release. Work on a roadmap item references its issue, and a change to a phase updates ROADMAP.md in the same PR.

## Code

- C++20, `.clang-format` style (Allman braces, 4-space indent, 150 columns). Format only the lines you change: `git clang-format origin/main`.
- Comments and log messages are written in English. A comment states intent the code cannot show; do not narrate history or the change you just made.
- Log through the project's `LOG_DEBUG` / `LOG_INFO` / `LOG_ERROR` macros, not `std::cout` or `qDebug`.
- Network access uses libcurl with HTTPS only. Downloaded files must never be written outside the target localization folder.
- Qt 6.4 is the oldest supported Qt (Ubuntu 24.04). Do not use newer Qt APIs without a version guard.
- Match the surrounding code: naming, structure, and comment density. Do not reformat or refactor code unrelated to the task.

## Build and verify

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
git clang-format --diff origin/main
```

A change is done only when it builds and the tests pass. Backend behaviour belongs in `tests/BackendTest.cpp`, UI flows in `tests/GuiTest.cpp`. Say so in the PR if you could not run the app or test a code path by hand.

## Working in a shared checkout

- Changes you did not make belong to the human or another agent. Do not commit, revert, stash, or unstage them. Check `git diff --cached --name-only` before you commit.
- Do not commit build output (`build/`, `release/`, `build-appimage/`) or downloaded tools (`misc/*.appimage`).
