# Contributing to SC Localization Launcher

Thanks for helping out! Please read [LEGAL.md](LEGAL.md) before you contribute. It says which material must not end up in this repository.

Translation texts are **not** maintained here. Each language is maintained by its own community project. Wrong or missing German texts, for example, belong in [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI). This repository only *lists* translations, in [`languages.json`](languages.json).

What is planned next is in the [roadmap](ROADMAP.md). It is open for additions.

---

## DCO sign-off (required)

This project uses the [Developer Certificate of Origin](https://developercertificate.org/). By signing off, you certify that you have the right to submit your contribution under the project's license. There are two ways to do it:

**Per commit:** sign off each commit:

```sh
git commit -s -m "Your message"
```

**Per PR:** put one `Signed-off-by: Your Name <your@email>` line in the pull request description, or tick the sign-off box in the PR template. That one sign-off covers all commits of the PR.

The `DCO` check enforces this. PRs opened from the repository owner's account are exempt: the owner certifies them as a standing decision. Use your real identity and a reachable email address. Never sign off for someone else.

## AI-assisted contributions

You may use AI coding tools. The rules:

- **You** are the author. You must review and understand every line, and be able to explain and maintain it in review.
- The DCO sign-off is yours to give. An AI tool must never add `Signed-off-by` for you. A `Co-Authored-By` trailer for the tool is fine.
- Say in the PR description that you used AI tools and for what (the PR template has a checkbox).
- Don't send unreviewed, bulk, or drive-by agent PRs. They will be closed.
- Agents working in this repository must follow [AGENTS.md](AGENTS.md).

## Licensing of contributions

By contributing, you agree that your contribution is licensed under the [GNU General Public License v3.0 or later](LICENSE). No separate CLA is needed.

---

## Ways to contribute

- **Add a game language:** see [Adding a game language](#adding-a-game-language). This needs no C++.
- **Translate the launcher's interface:** see [Translating the interface](#translating-the-interface).
- **Code, packaging, docs:** pick an issue from a [milestone](https://github.com/xXCrusaderXx/SC-Localization-Launcher/milestones) or propose something new with a [roadmap proposal](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/new?template=roadmap_proposal.yml).

### Adding a game language

Game languages are listed in [`languages.json`](languages.json) in the repository root. The launcher downloads this file from `main` (with a cached and a built-in copy as fallback). A language merged into `dev` therefore reaches users when `dev` is merged into `main` at the next release point, **without a launcher update**.

```json
{
    "version": 1,
    "languages": [
        {
            "code": "fr",
            "name": "Français",
            "folder": "french_(france)",
            "source": {
                "channels": {
                    "LIVE": ["https://raw.githubusercontent.com/<project>/main/live/global.ini",
                             "https://<mirror>/live/global.ini"],
                    "PTU":  ["https://raw.githubusercontent.com/<project>/main/ptu/global.ini"]
                }
            },
            "credit": {
                "project": "<project name>",
                "authors": "<maintainers, as they want to be credited>",
                "url": "https://github.com/<project>",
                "license": "CC BY-NC-SA 4.0",
                "licenseUrl": "https://creativecommons.org/licenses/by-nc-sa/4.0/"
            }
        }
    ]
}
```

| Field | Meaning |
| ----- | ------- |
| `code` | Unique, lowercase BCP-47 style (`fr`, `es`, `pl`, `de-ch`). `en` is reserved for the original English text. |
| `name` | Written in the language itself (`Français`, `Español`). Shown as-is in the language picker. |
| `folder` | The game's localization folder that the `global.ini` goes into and that `g_language` in `user.cfg` points to, e.g. `french_(france)`, `spanish_(spain)`, `italian_(italy)`, `polish_(poland)`, `portuguese_(brazil)`, `german_(germany)`. Check that the game accepts the folder name. |
| `credit` | Required. Shown under the language in the picker and in the settings. `project`, `authors` and `license` must not be empty, `url` (the project page) must use HTTPS, `licenseUrl` is optional and must use HTTPS if set. |
| `notices` | Optional. A short notice per channel, written in the language itself, e.g. `{"LIVE": "Übersetzung wird vorbereitet", "*": "Beta"}`. The key is a channel name or `*` for all channels; a channel's own entry wins over `*`. Shown as plain text on one line, cut to 120 characters. |
| `source` | Either `channels`: per channel (`LIVE`, `HOTFIX`, `PTU`, `TECH-PREVIEW`, `EPTU`) a list of HTTPS URLs, primary first, then fallbacks; channels that are missing are not offered for this language. Or `channelList` + `server`: a channel list in the format the German project uses; the launcher takes `<server>` and `<server>_fallback` of each channel and respects its `active` flag. |

Rules for a new language:

1. The URLs point to files that the **translation's own maintainers publish**. We never host translation files and never mirror them into this repository.
2. The translation's license must allow downloading it this way. Name the license, the project, and the maintainers in the PR. Use the [new language](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/new?template=new_language.yml) template if you'd rather not edit the file yourself.
3. All URLs use HTTPS.
4. Test the language in the game (at least LIVE) before you open the PR.

The launcher checks every entry when it loads the file. An invalid entry is skipped with an error in the log; all other languages still load. An entry is valid when:

- `code` is not empty, unique, and not `en`;
- `name` is not empty;
- `folder` contains only letters, digits, `_`, `(`, `)` and `-` (no slashes or dots: it becomes a folder name inside the game directory);
- `source` has exactly one of `channels` or `channelList`;
- `credit` is complete (see the table above);
- every URL uses HTTPS. A `channels` language without any usable URL is dropped.

### Translating the interface

The launcher's own interface is English by default and follows the system language. Translations live in `translations/*.ts` (Qt Linguist). To add one, copy an existing `.ts` file, change its `language` attribute, translate it with Qt Linguist (`linguist`), and register it in `CMakeLists.txt` next to the existing ones. Strings in QML use `qsTr()`.

---

## Workflow

1. Open an issue first for larger changes, so the approach can be agreed on before you invest time.
2. Fork the repository and create a branch from `dev` (`feat/…`, `fix/…`, `docs/…`, `chore/…`).
3. Keep each PR focused on one topic. Use a [Conventional Commits](https://www.conventionalcommits.org/)-style PR title, for example `fix: detect PTU folder with custom library path`. It becomes the title of the merge commit and the changelog entry.
4. Open a pull request against `dev` and fill in the template.

### Branches

| Branch | Purpose |
| ------ | ------- |
| `main` | Released code only. Changes come exclusively from `dev`, merged by a maintainer at a release point. |
| `dev` | Integration branch and default branch. All feature and fix PRs go here. |
| `feat/…`, `fix/…`, `docs/…`, `chore/…` | One topic each, branched from `dev`, deleted after the merge. |
| `hotfix/…` | Urgent fix for the released version, branched from `main` (see [Hotfixes](#hotfixes)). |

PRs are merged with a merge commit, never squashed or rebased, so the history shows which commits belong to which branch. After `dev` is merged into `main`, work simply continues on `dev`; the `Back-merge main into dev` workflow keeps `dev` containing everything from `main`.

**Clean up your branch before the PR.** Every commit of your branch ends up in the history. Combine work-in-progress commits such as "fix typo" or "format" into meaningful commits (`git rebase -i origin/dev`, or `git reset --soft origin/dev` and commit again) before you open the PR, and use clear commit messages.

### Hotfixes

A hotfix is only for a problem in the **released** version that cannot wait for the next regular release: a crash, a broken download, a wrong install path. Everything else goes through `dev`.

1. Branch from `main`: `hotfix/<topic>`. Only the fix goes in.
2. Open a PR against `main`. CI runs as usual; a maintainer merges it (never automatic).
3. A maintainer runs the `Release` workflow on `main` with the next patch version (e.g. `0.6.1`). No milestone is needed.
4. The `Back-merge main into dev` workflow then opens a PR `main` → `dev` that merges by itself, so the fix is in `dev` too.

### How a PR gets merged

A PR can only be merged when all required checks are green (`format`, `build (ubuntu)`, `build (arch)` including the tests, `DCO sign-off`). The branch does not need to be up to date with `dev`; if two PRs that pass on their own break `dev` together, the CI run on `dev` shows it and a small follow-up PR fixes it.

- **Contributors' PRs:** a maintainer reviews the change. When it is good, the maintainer merges it, or enables auto-merge so GitHub merges it as soon as the checks are green. Nothing from outside is merged without a maintainer looking at it.
- **Maintainers' own PRs into `dev`:** auto-merge is enabled automatically. Ticking the sign-off box in the PR description is the maintainer's one action; the PR merges once the checks are green.
- **Dependabot:** minor and patch updates of GitHub Actions merge automatically once CI is green. Major updates wait for a maintainer.

A PR with a merge conflict needs a rebase by its author.

Draft PRs are never merged; mark the PR "Ready for review" when you are done.

---

## Local CI gate

Run the same checks as CI before you push:

```sh
# Formatting of the lines you changed (.clang-format in the repository root)
git fetch origin && git clang-format --diff origin/dev

# Build and test (the tests need no internet and never touch your settings)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Optional: `.githooks/pre-push` stops a push when a commit's author or committer email matches a pattern you set locally, so a work or private address never reaches GitHub by accident (Git GUIs often use their own identity). Enable it with `git config core.hooksPath .githooks` and `git config --add hooks.blockedEmail 'example\.com'`. The patterns stay in your local config.

Optional: `.githooks/post-merge` cleans up after every `git pull`: local branches whose PR was merged (remote branch gone) are deleted, together with their worktree if it has no changes. Enable it with `git config core.hooksPath .githooks` and `git config fetch.prune true`.

CI checks only the lines your PR changes. To fix them in place, run `git clang-format origin/dev`. Don't reformat code you didn't otherwise touch.

To check the AppImage, run `packaging/appimage/build-in-docker.sh`. It builds in Ubuntu 22.04, exactly like the release pipeline.

## Code style

- C++20, formatted with the repository's `.clang-format` (Allman braces, 4-space indent, 150 columns).
- **Code comments and log messages are written in English.**
- Backend code lives in headers under `include/backend/`, the UI is QML in `qml/`, and `include/frontend/BackendBridge.hpp` connects the two. The backend and UI talk only through the types in `include/protocol/`.
- Match the surrounding code: naming, comment density, and use of the project's `LOG_*` macros instead of `std::cout`.
- Never commit build output, AppImage tools, translation files, or game assets.

## Pull request checklist

- [ ] Signed off (`git commit -s`, or one `Signed-off-by` line in the PR description).
- [ ] `git clang-format --diff origin/dev` reports no changes.
- [ ] The project builds with `cmake --build build`, and `ctest --test-dir build` passes.
- [ ] You tested the change by hand. Describe how in the PR.
- [ ] No Star Citizen assets, translation files, or binaries are committed (see [LEGAL.md](LEGAL.md)).
- [ ] The PR title describes the user-visible change (it becomes the changelog entry). Don't edit [CHANGELOG.md](CHANGELOG.md) in feature PRs.

## Releases

`CHANGELOG.md` is written by a maintainer once per release, from the titles of the merged PRs. Feature PRs don't edit it: with several PRs in flight it was the most frequent merge conflict.

Releases follow the [roadmap](ROADMAP.md): every phase is a milestone named `vX.Y.Z – Name`. At a release point, a PR `dev` → `main` is opened and the maintainer merges it (merge commit, never automatic). When the maintainer then closes the milestone, the `Milestone release` workflow checks that all its issues are closed and runs the `Release` workflow for `X.Y.Z` on `main`. That workflow builds and tests every package and publishes GitHub release `vX.Y.Z`, with the milestone's issues and PRs as release notes. The version comes from the milestone title, so no version-bump commit is needed. Before closing a milestone, work through the [release checklist](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/new?template=release.yml).

For a release outside a milestone, either run the `Release` workflow by hand with a version (without a version it is a dry run that builds and tests everything but publishes nothing), or set `SC_DEFAULT_VERSION` in `CMakeLists.txt` and push a matching tag `vX.Y.Z`. A version with a suffix (`0.6.0-rc.1`) is published as a pre-release.

## Conduct

All participation falls under the [Code of Conduct](CODE_OF_CONDUCT.md). Be respectful and constructive.
