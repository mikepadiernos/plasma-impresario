# Plasma Mail

<p align="center">
  <img src="src/icons/plasma-mail.svg" alt="Plasma Mail envelope and golden spiral icon" width="240" height="240">
</p>

**Plasma Mail is the product direction for this KDE desktop mail client.**
This checkout currently builds a Trojitá-based Qt 6 IMAP client; the proposed
Plasma Mail workflows and redesign are not all implemented yet.

**[Build and run](#build-and-run)** · **[Current features](#implemented-today)** ·
**[Icon](#application-icon)** ·
**[Security and limitations](docs/CAVEATS.md#security-and-data-protection)** ·
**[Roadmap](docs/ROADMAP.md)** ·
**[Documentation](docs/README.md)**

> [!NOTE] The app identity, main window and source architecture are still
> Trojitá. The [backend](docs/parity/backend.md) and
> [UI](docs/parity/ui.md) references distinguish existing behavior from
> product goals; [caveats](docs/CAVEATS.md) records current limitations.

## Build and run

**Requirements:** CMake, Ninja, a C++ compiler, Qt 6 and KF6 ItemModels.
Reading messages requires Qt6WebKitWidgets, which is not provided by Qt
WebEngine and may not be available in your distribution. OpenPGP, QtKeychain,
Sonnet and other integrations depend on the configured build. See
`CMakeLists.txt` for exact requirements.

For a local build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DWITH_WEBKIT=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/trojita
```

For an explicitly renderer-free development build, use `-DWITH_WEBKIT=OFF`
instead. It cannot display message contents.

For the local automated setup (requires Qt6WebKitWidgets):

```bash
./install.sh
./install.sh --help
```

The installer defaults to `$HOME/.local`, including the executable in
`$HOME/.local/bin`. Ensure that directory is on your `PATH`. It uses sudo for
missing system packages or a protected installation prefix and skips packages
it detects as already installed. Use `--no-deps` to skip package installation.
Qt6WebKitWidgets may need to be installed separately.

If Qt6WebKitWidgets is unavailable, `./install.sh --without-webkit` installs
an **incomplete build that cannot display message contents**. This option is
not a substitute for a working mail reader.

The installer also supports custom build and installation locations:

```bash
./install.sh --prefix "$HOME/apps/plasma-mail" --build-dir "$HOME/plasma-mail-build"
```

The installed Trojitá handbook and man page come from `docs/old` and may
describe historical behavior.

Installer checks can be run separately with `bash tests/test_install.sh`;
they use mock commands rather than installing packages.

## Implemented today

The current Trojitá-based application provides:

- IMAP mailbox discovery, synchronization, offline modes, caching and threading;
- folder navigation, message search/sorting and common flag/tag operations;
- message reading, MIME parts, attachment handling and optional HTML rendering;
- composition, replies/forwards, local drafts and configurable message delivery;
- compact, wide and one-panel layouts with persisted window state;
- optional password, address-book, spell-check and OpenPGP plugins.

The current application is single-account per instance. Rules, provider OAuth,
workspace workflows, a redesigned Plasma shell and several cross-app
integrations remain planned rather than shipped.

The IMAP client separates protocol parsing, tasks, Qt item models, cache and
widgets. It fetches message parts on demand and can cache them for later reads.
Profiles (`-p NAME`) run as separate configurations, not as multiple accounts
in one window. Use `./build/trojita --help` for all launch options; `-c`
composes a message, `-a` opens the address book when available, and
`mailto:...` opens a prefilled composer.

## Product direction

The planned product builds on this baseline with a clearer, Plasma-native
reading and triage experience. Priorities include a reproducible installation
and secure baseline, a maintained message renderer, account isolation, and a
more reliable desktop shell. Provider, productivity and cross-app workflows
follow the core mail experience. See the [roadmap](docs/ROADMAP.md) for
milestones and priority order.

## Application icon

The new [Plasma Mail SVG](src/icons/plasma-mail.svg) combines an envelope
with the Plasma Opus golden-spiral motif at 256 × 256 pixels. KDE's icon
loader can apply the Plasma accent color; other SVG renderers use the default
blue. It is an artwork asset only: the application and installed desktop
entry still use the existing Trojitá icon.

## Documentation

Start with the [documentation index](docs/README.md) for project philosophy,
plans, implementation references and tracking.

| Guide | Purpose |
| --- | --- |
| [Roadmap](docs/ROADMAP.md) | Milestones, architecture and delivery order |
| [Pending tasks](docs/TODO.md) | Current blockers and resumable handoffs |
| [Reported bugs](docs/BUGS.md) | Confirmed regressions and resolution status |
| [Caveats](docs/CAVEATS.md) | Current limitations and workarounds |
| [Edge cases](docs/EDGE.md) | Scenarios requiring validation or testing |
| [Refinements](docs/refinements/README.md) | Quality plans, menu specifications and toolbar specifications |
| [Integrations](docs/integrations/README.md) | Planned KDE Plasma and Plasma Opus workflows |
| [Backend parity](docs/parity/backend.md) and [UI parity](docs/parity/ui.md) | Existing functionality and target behavior |
| [Contributing](CONTRIBUTING.md) | Contribution workflow and project contact points |

> [!IMPORTANT] Roadmap items are not feature-complete merely because they are
> implemented. Completion status follows issue review and merge state recorded
> in the project tracking documents.
