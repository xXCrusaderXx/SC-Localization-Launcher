# Contributing to StarCitizen Deutsch Launcher

Thanks for helping out! Please read [LEGAL.md](LEGAL.md) before you contribute. It says which material must not end up in this repository.

Translation texts are **not** maintained here. Wrong or missing German texts belong in [StarCitizen-Deutsch-INI](https://github.com/rjcncpt/StarCitizen-Deutsch-INI).

---

## DCO sign-off (required)

This project uses the [Developer Certificate of Origin](https://developercertificate.org/). By signing off, you certify that you have the right to submit your contribution under the project's license. There are two ways to do it:

**Per commit:** sign off each commit:

```sh
git commit -s -m "Your message"
```

**Per PR:** put one `Signed-off-by: Your Name <your@email>` line in the pull request description, or tick the sign-off box in the PR template. PRs are squash-merged, so that one sign-off covers the whole PR.

The `DCO` check enforces this. Use your real identity and a reachable email address. Never sign off for someone else.

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

## Workflow

1. Open an issue first for larger changes, so the approach can be agreed on before you invest time.
2. Fork the repository and create a branch from `main` (`feat/…`, `fix/…`, `docs/…`, `chore/…`).
3. Keep each PR focused on one topic. Use a [Conventional Commits](https://www.conventionalcommits.org/)-style PR title, for example `fix: detect PTU folder with custom library path`. It becomes the squash-commit message.
4. Open a pull request against `main` and fill in the template.

### How a PR gets merged

Every PR gets **auto-merge (squash)** enabled automatically. GitHub merges it once:

- all required checks are green (`format`, `build (ubuntu)`, `build (arch)` including the tests, `DCO sign-off`),
- a maintainer has approved it, and
- the branch is up to date with `main`.

New pushes dismiss earlier approvals. Draft PRs are never merged; mark the PR "Ready for review" when you are done. Dependabot updates for GitHub Actions merge without a human review once CI is green.

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

CI checks only the lines your PR changes. To fix them in place, run `git clang-format origin/main`. Don't reformat code you didn't otherwise touch.

To check the AppImage, run `packaging/appimage/build-in-docker.sh`. It builds in Ubuntu 22.04, exactly like the release pipeline.

## Code style

- C++20, formatted with the repository's `.clang-format` (Allman braces, 4-space indent, 150 columns).
- **Code comments and log messages are written in English.**
- Backend code lives in headers under `include/backend/`, the UI is QML in `qml/`, and `include/frontend/BackendBridge.hpp` connects the two. The backend and UI talk only through the types in `include/protocol/`.
- Match the surrounding code: naming, comment density, and use of the project's `LOG_*` macros instead of `std::cout`.
- Never commit build output, AppImage tools, translation files, or game assets.

## Pull request checklist

- [ ] Signed off (`git commit -s`, or one `Signed-off-by` line in the PR description).
- [ ] `git clang-format --diff origin/main` reports no changes.
- [ ] The project builds with `cmake --build build`, and `ctest --test-dir build` passes.
- [ ] You tested the change by hand. Describe how in the PR.
- [ ] No Star Citizen assets, translation files, or binaries are committed (see [LEGAL.md](LEGAL.md)).
- [ ] User-visible changes are noted under `Unreleased` in [CHANGELOG.md](CHANGELOG.md).

## Releases

Maintainers cut releases by bumping `project(... VERSION x.y.z)` in `CMakeLists.txt` and `CHANGELOG.md`, then pushing a tag `vx.y.z`. The `Release` workflow builds the AppImage, `.deb`, and `.rpm` and publishes a GitHub Release.

## Conduct

All participation falls under the [Code of Conduct](CODE_OF_CONDUCT.md). Be respectful and constructive.
