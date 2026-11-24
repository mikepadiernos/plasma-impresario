# Contributing

**[Documentation](docs/README.md)** · **[Roadmap](docs/ROADMAP.md)** ·
**[Project instructions](CLAUDE.md)**

Contributions are welcome for application code, tests, documentation and bug
reports. Follow the documented milestone and issue workflow before changing
product behavior.

## Development workflow

1. Read the [scope](docs/philosophy/scope.md) and relevant roadmap milestone.
2. Record blockers in [pending tasks](docs/TODO.md) and confirmed defects in
   [reported bugs](docs/BUGS.md).
3. Use a focused feature or fix branch and add or update QtTest coverage.
4. Run the smallest relevant test set, then broader CTest coverage when
   practical.
5. Keep user-visible strings translatable and follow KDE/Plasma conventions.

## Building outside Plasma

Plasma Impresario targets KDE Plasma on Linux. On other hosts, such as macOS,
use the headless Fedora container in `tools/dev-container/` to build and run
the test suite:

```sh
tools/dev-container/run.sh cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
tools/dev-container/run.sh cmake --build build
tools/dev-container/run.sh ctest --test-dir build --output-on-failure
```

Run `tools/dev-container/run.sh` without arguments for a shell, or pass
`--rebuild` to rebuild the image. If a TLS-intercepting proxy is in use, set
`EXTRA_CA_CERT` to its root certificate before the first run.

The container has no Plasma session. Verify desktop integration, including
KWallet, notifications, Akonadi, icon theming, and window behavior, in a real
Plasma session.

## Submitting changes

This repository originated in KDE's Trojitá project. Upstream KDE uses merge
requests in [KDE Invent](https://invent.kde.org/pim/trojita); GitHub merge
requests are not the upstream review path.

For a quick introduction to KDE merge requests, see the
[community infrastructure guide](https://community.kde.org/Infrastructure/GitLab).
