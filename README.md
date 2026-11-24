# Plasma Impresario

<p align="center">
  <img src="src/icons/plasma-impresario.svg" alt="Plasma Impresario envelope and golden spiral icon" width="240" height="240">
</p>

**A focused IMAP mail client for the KDE Plasma desktop**, built with Qt 6 and
KDE Frameworks. Other desktop environments are outside the supported scope.

**[Get started](#build-and-run)** · **[Features](#implemented-today)** ·
**[Roadmap](docs/ROADMAP.md)** · **[Documentation](docs/README.md)** ·
**[Security and limitations](docs/CAVEATS.md#security-and-data-protection)**

> [!NOTE]
>
> The current application is based on Trojitá. Mail workflows are implemented,
> but live account and desktop-session verification is pending; the Plasma
> Impresario redesign and several integrations remain planned. See the
> [UI](docs/parity/ui.md) and [backend](docs/parity/backend.md) baselines.

## Build and run

**Requirements:** CMake, Ninja, a C++20 compiler, Qt 6, Qt WebEngineWidgets and
KF6 ItemModels, Config and I18n. Optional integrations have additional
dependencies; see the [compatibility baseline](docs/philosophy/compatibility.md).

To install locally:

```bash
./install.sh
```

The default prefix is `~/.local`. See the [installation guide](docs/INSTALL.md)
for dependency checks, build-only mode, custom prefixes and troubleshooting.

To build and test manually:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQT_MAJOR_VERSION=6 -DBUILD_TESTING=ON -DWITH_WEBENGINE=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/plasma-impresario
```

> [!WARNING]
>
> A renderer-free build (`-DWITH_WEBENGINE=OFF` or
> `./install.sh --without-webengine`) cannot display message contents.

## Implemented today

- IMAP mailbox discovery, synchronization, offline modes, caching and threading.
- Folder navigation, search, sorting, flags and tags.
- Message reading, MIME alternatives, HTML rendering and attachments.
- Composition, replies and forwards, local drafts and configurable delivery.
- Compact, wide and one-panel layouts; optional password, address-book,
  spell-check and OpenPGP plugins.

An application instance uses one account model. Profiles (`-p NAME`) are
separate configurations, not simultaneous accounts in one window. Run
`./build/plasma-impresario --help` for launch options; `-c` opens the composer,
`-a` opens the address book when available, and `mailto:...` prefills a message.

To save a search, enter a query and choose **Save Current Search** from the
search options menu. **Saved Searches** in the same menu runs or deletes saved
queries for the current account profile; a query runs in the selected folder.

## Product direction

The roadmap prioritizes reliable mail and account boundaries before the
Plasma-native shell, provider integrations and productivity workflows.
Multi-account management, rules, provider OAuth and workspace features remain
planned. See the [roadmap](docs/ROADMAP.md) for milestones and status.

## Application icon

The [application icon](src/icons/plasma-impresario.svg) combines an envelope
with the Plasma Opus golden spiral. It is used by the application and its
desktop entry.

## Documentation

| Start with                                          | For                                                |
| --------------------------------------------------- | -------------------------------------------------- |
| [Installation](docs/INSTALL.md)                     | Options, prerequisites and troubleshooting         |
| [Documentation index](docs/README.md)               | Product guides, architecture and specifications    |
| [Roadmap](docs/ROADMAP.md)                          | Milestones and project-board links                 |
| [Pending tasks](docs/TODO.md)                       | Blockers and handoffs                              |
| [Bugs](docs/BUGS.md) and [caveats](docs/CAVEATS.md) | Reported issues, limitations and security guidance |
| [Contributing](CONTRIBUTING.md)                     | Development workflow                               |
