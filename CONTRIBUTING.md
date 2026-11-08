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

- **You** are responsible for the PR. Review the changes, test them, and make sure they do what the PR says. You should be able to answer questions about them in review.
- The DCO sign-off is yours to give. An AI tool must never add `Signed-off-by` for you.
- Mark AI use at least in the commit: add a `Co-Authored-By` trailer that names the tool, for example `Co-Authored-By: Claude <noreply@anthropic.com>`. GitHub keeps the trailer in the squash commit. A note in the PR description is welcome too.
- Issues, bug reports and review comments describe what you observed yourself. Mark text that an AI tool wrote, and check every log, stack trace and claim in it before you post it. Reports that nobody can reproduce are closed.
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

Game languages are listed in [`languages.json`](languages.json) in the repository root. Every release publishes this file as a release asset, and the launcher downloads it from the latest release (with a cached and a built-in copy as fallback). A language merged into `main` therefore reaches users with the next release, **without a launcher update**.

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
| `credit` | Required. Shown under the language in the picker and in the settings. `project`, `authors` and `license` must not be empty, `url` (the project page) must use HTTPS, `licenseUrl` is optional and must use HTTPS if set. If the `global.ini` names its contributors in a comment line at the top (`; Contributors : Alice - Bob`, also `Contributeurs` or `Mitwirkende`), the settings list those names under the credit once the translation is installed. |
| `notices` | Optional. A short notice per channel, written in the language itself, e.g. `{"LIVE": "Übersetzung wird vorbereitet", "*": "Beta"}`. The key is a channel name or `*` for all channels; a channel's own entry wins over `*`. Shown as plain text on one line, cut to 120 characters. |
| `source` | Either `channels`: per channel (`LIVE`, `HOTFIX`, `PTU`, `TECH-PREVIEW`, `EPTU`) a list of HTTPS URLs, primary first, then fallbacks; channels that are missing are not offered for this language. Or `channelList` + `server`: a channel list in the format the German project uses; the launcher takes `<server>` and `<server>_fallback` of each channel and respects its `active` flag. |

Rules for a new language:

1. The URLs point to files that the **translation's own maintainers publish**. We never host translation files and never mirror them into this repository.
2. The translation's license must allow downloading it this way. Name the license, the project, and the maintainers in the PR. Use the [new language](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/new?template=new_language.yml) template if you'd rather not edit the file yourself.
3. All URLs use HTTPS.
4. Test the language in the game (at least LIVE) before you open the PR.
5. A new language and every changed URL need a confirmation from the translation's maintainers: a comment by them in the PR, or a link to the page where they publish the URLs themselves.
6. URLs point to hosts that the translation project controls or names as its mirror: its own repository, its release assets, or its own domain. No link shorteners, redirect services, or third-party mirrors.
7. A PR that changes `languages.json` is never merged automatically, also when a maintainer opens it. A maintainer reviews it and merges it by hand. The `Auto-merge` workflow enforces this.

The launcher checks every entry when it loads the file. An invalid entry is skipped with an error in the log; all other languages still load. An entry is valid when:

- `code` is not empty, unique, and not `en`;
- `name` is not empty;
- `folder` contains only letters, digits, `_`, `(`, `)` and `-` (no slashes or dots: it becomes a folder name inside the game directory);
- `source` has exactly one of `channels` or `channelList`;
- `credit` is complete (see the table above);
- every URL uses HTTPS. A `channels` language without any usable URL is dropped.

### Translating the interface

The launcher's own interface is English by default and follows the system language, or the language picked under Settings → Behaviour. Translations live in `translations/*.ts` (Qt Linguist). To add one, add `translations/sc-localization-launcher_<code>.ts` to `SC_TS_FILES` in `CMakeLists.txt`, run `cmake --build build --target update_translations`, and translate the new file with Qt Linguist (`linguist`). Strings in QML use `qsTr()`.

French, Spanish, Italian, Polish, Dutch and Portuguese (Brazil) started as drafts and have not all been checked by native speakers yet. Corrections are welcome as small PRs that change only the `.ts` file. `GuiTest` fails if a shipped language has an unfinished string or loses a placeholder (`%1`, `%n`); the English file is the exception, it only holds plural forms.

---

## Workflow

1. Open an issue first for larger changes, so the approach can be agreed on before you invest time.
2. Fork the repository and create a branch from `main` (`feat/…`, `fix/…`, `docs/…`, `chore/…`).
3. Keep each PR focused on one topic. Use a [Conventional Commits](https://www.conventionalcommits.org/)-style PR title of at most 72 characters, for example `fix: detect PTU folder with custom library path`. It becomes the title of the squashed commit on `main` and the changelog entry. Put issue references in the PR description (`Closes #12`) and keep them out of the title; GitHub appends the PR number to the commit itself.
4. Open a pull request against `main` and fill in the template.

### Branches

| Branch | Purpose |
| ------ | ------- |
| `main` | Default branch. Every feature and fix PR is merged here once it is reviewed and its checks are green. A release is a tag on `main`. |
| `feat/…`, `fix/…`, `docs/…`, `chore/…` | One topic each, branched from `main`, deleted after the merge. |

PRs are squashed: each PR becomes one commit, so `main` is a straight line of topics. Its title is the PR title, its message the commit messages of the branch (keep them clear; they end up in that one commit). There is no integration branch.

**Update a branch by rebasing, never by merging `main` into it** (`git fetch && git rebase origin/main`, then `git push --force-with-lease`). Merging `main` into a feature branch adds nothing after the squash and only makes the branch harder to review.

Users get only what a release tag contains. Every release publishes `languages.json` and `item-mapping.json` as release assets, and the launcher reads both from the latest release, so they must stay valid for every released launcher version. Launchers up to v1.2.0 read both files from `main` instead, so a change reaches them as soon as it is merged.

### Urgent fixes

An urgent fix for the released version (a crash, a broken download, a wrong install path) is an ordinary `fix/…` PR against `main`. After the merge, a maintainer runs the `Release` workflow on `main` with the next patch version (e.g. `1.2.1`). No milestone is needed.

### How a PR gets merged

A PR can only be merged when all required checks are green (`format`, `build (ubuntu)`, `build (arch)` including the tests, `DCO sign-off`). The branch does not need to be up to date with `main`; if two PRs that pass on their own break `main` together, the CI run on `main` shows it and a small follow-up PR fixes it.

- **Contributors' PRs:** a maintainer reviews the change. When it is good, the maintainer merges it, or enables auto-merge so GitHub merges it as soon as the checks are green. Nothing from outside is merged without a maintainer looking at it.
- **Maintainers' own PRs:** auto-merge is enabled automatically. Ticking the sign-off box in the PR description is the maintainer's one action; the PR merges once the checks are green. PRs that change `languages.json` are the exception (see [Adding a game language](#adding-a-game-language)).
- **Dependabot:** minor and patch updates of GitHub Actions merge automatically once CI is green. Major updates wait for a maintainer.

A PR with a merge conflict needs a rebase by its author.

Draft PRs are never merged; mark the PR "Ready for review" when you are done.

---

## Local CI gate

Run the same checks as CI before you push:

```sh
# Formatting of the lines you changed (.clang-format in the repository root)
git fetch origin && git clang-format --diff origin/main

# Build and test (the tests need no internet and never touch your settings)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Optional: `.githooks/pre-push` stops a push when a commit's author or committer email matches a pattern you set locally, so a work or private address never reaches GitHub by accident (Git GUIs often use their own identity). Enable it with `git config core.hooksPath .githooks` and `git config --add hooks.blockedEmail 'example\.com'`. The patterns stay in your local config.

Optional: `.githooks/post-merge` cleans up after every `git pull`: local branches whose PR was merged (remote branch gone) are deleted, together with their worktree if it has no changes. Enable it with `git config core.hooksPath .githooks` and `git config fetch.prune true`.

CI checks only the lines your PR changes. To fix them in place, run `git clang-format origin/main`. Don't reformat code you didn't otherwise touch.

To check the AppImage, run `packaging/appimage/build-in-docker.sh`. It builds in Ubuntu 22.04, exactly like the release pipeline.

## Code style

- C++20, formatted with the repository's `.clang-format` (Allman braces, 4-space indent, 150 columns).
- **Code comments and log messages are written in English.**
- Backend code lives in headers under `include/backend/`, the UI is QML in `qml/`, and `include/frontend/BackendBridge.hpp` connects the two. The backend and UI talk only through the types in `include/protocol/`.
- Match the surrounding code: naming, comment density, and use of the project's `LOG_*` macros instead of `std::cout`.
- Never commit build output, AppImage tools, game translation files (`global.ini`), or game assets.
- `item-mapping.json` is generated by `tools/build-item-mapping.py` in a weekly workflow, which opens its own PR. Never edit it by hand; change the script instead.

## Pull request checklist

- [ ] Signed off (`git commit -s`, or one `Signed-off-by` line in the PR description).
- [ ] `git clang-format --diff origin/main` reports no changes.
- [ ] The project builds with `cmake --build build`, and `ctest --test-dir build` passes.
- [ ] A bug fix comes with a test that fails without the fix, if the bug can be tested without the game or the internet.
- [ ] You tested the change by hand. Describe how in the PR.
- [ ] No Star Citizen assets, game translation files (`global.ini`), or binaries are committed (see [LEGAL.md](LEGAL.md)).
- [ ] The PR title describes the user-visible change (it becomes the changelog entry). Don't edit [CHANGELOG.md](CHANGELOG.md) in feature PRs.

## Releases

`CHANGELOG.md` is written by a maintainer once per release, from the titles of the merged PRs. Feature PRs don't edit it: with several PRs in flight it was the most frequent merge conflict.

Releases follow the [roadmap](ROADMAP.md): every phase is a milestone named `vX.Y.Z – Name`. When the maintainer closes the milestone, the `Milestone release` workflow checks that all its issues are closed and runs the `Release` workflow for `X.Y.Z` on `main`. That workflow builds and tests every package and publishes GitHub release `vX.Y.Z`, with the milestone's issues and PRs as release notes. The version comes from the milestone title, so no version-bump commit is needed. Before closing a milestone, work through the [release checklist](https://github.com/xXCrusaderXx/SC-Localization-Launcher/issues/new?template=release.yml).

For a release outside a milestone, either run the `Release` workflow by hand with a version (without a version it is a dry run that builds and tests everything but publishes nothing), or set `SC_DEFAULT_VERSION` in `CMakeLists.txt` and push a matching tag `vX.Y.Z` on `main`. A version with a suffix (`0.6.0-rc.1`) is published as a pre-release.

## Conduct

All participation falls under the [Code of Conduct](CODE_OF_CONDUCT.md). Be respectful and constructive.
