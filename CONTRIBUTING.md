# Contributing

Thanks for looking at emini Home. The most valuable contribution right now is
simple: **tell us how installation went on your NOTE4C.** This release has been
tested on one unit, so every
[hardware report](https://github.com/fiedoruk/emini-home/issues/new?template=hardware-report.yml)
helps others decide whether it is safe to install.

## Issues

Search existing issues first, then use one of the templates. Keep private data
out of issues: no flash backups, Wi-Fi passwords, pairing codes, tokens or
photos of the setup screen.

## Pull requests

- Open an issue before large changes, so we can agree on the direction first.
- Keep a pull request focused on one change.
- Build it with the steps in [docs/BUILD.md](docs/BUILD.md).
- Format C changes with clang-format and `firmware/.clang-format`, and panel
  JavaScript and CSS with `npx prettier@3 --print-width 100`. Format only the
  lines you change.
- Describe how you tested it, and say clearly if you have not tried it on a
  real NOTE4C.

Changes to the partition layout, the settings storage format or the display
driver can make existing devices lose their settings or stop at start. Those
need a test on hardware before they can be merged.

By contributing, you agree that your work is released under the
[MIT License](LICENSE).
