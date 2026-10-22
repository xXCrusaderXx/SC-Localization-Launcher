# Security policy

## Supported versions

Only the latest release gets security fixes.

## Reporting a vulnerability

Please **don't** open a public issue for security problems. Use [GitHub private vulnerability reporting](https://github.com/xXCrusaderXx/StarCitizenDeutschLinux/security/advisories/new) instead. It creates a private advisory that only the maintainers can see.

Relevant topics include:

- unsafe handling of downloaded files (path traversal, overwriting files outside the game's localization folder),
- missing integrity or TLS checks on downloads or updates,
- unsafe handling of local paths, the `.desktop` autostart entry, or install scripts,
- legal or takedown requests that contain confidential details (see [LEGAL.md](LEGAL.md)).

Please include the affected version or commit, the impact, and the steps to reproduce. A minimal proof of concept helps.

You will get a first answer within 7 days. Please give us a reasonable amount of time to release a fix before you disclose the issue publicly.

## Everything else

Use a public [issue](https://github.com/xXCrusaderXx/StarCitizenDeutschLinux/issues) for ordinary bugs, feature requests, and documentation problems.
