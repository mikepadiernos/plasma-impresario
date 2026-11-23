# Plasma Impresario

<p align="center">
  <img src="src/icons/plasma-impresario.svg" alt="Plasma Impresario envelope and golden spiral icon" width="240" height="240">
</p>

**Plasma Impresario is a mail client for the KDE Plasma desktop only.**
Other desktop environments are outside its supported product scope.
This checkout currently builds a Trojitá-based Qt 6 IMAP client; the proposed
Plasma Impresario workflows and redesign are not all implemented yet.

**[Build and run](#build-and-run)** · **[Current features](#implemented-today)** ·
**[Icon](#application-icon)** ·
**[Security and limitations](docs/CAVEATS.md#security-and-data-protection)** ·
**[Roadmap](docs/ROADMAP.md)** ·
**[Documentation](docs/README.md)**

> [!NOTE] The main window and source architecture are based on Trojitá.
> The [backend](docs/parity/backend.md) and
> [UI](docs/parity/ui.md) references distinguish existing behavior from
> product goals; [caveats](docs/CAVEATS.md) records current limitations.

## Build and run

**Requirements:** CMake, Ninja, a C++ compiler, Qt 6 and KF6 ItemModels.
Reading messages requires Qt6WebKitWidgets, which is not provided by Qt
WebEngine and may not be available in your distribution. OpenPGP, QtKeychain,
Sonnet and other integrations depend on the configured build. See
`CMakeLists.txt` for exact requirements and the
[compatibility baseline](docs/compatibility.md) for tested environments and
remaining limits.

For a local build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DWITH_WEBKIT=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/plasma-impresario
```

For an explicitly renderer-free development build, use `-DWITH_WEBKIT=OFF`
instead. It cannot display message contents.

To develop the experimental WebEngine resource bridge, configure with
`-DWITH_WEBKIT=OFF -DWITH_WEBENGINE_BRIDGE=ON -DBUILD_TESTING=ON`, then build
and run the `test_WebEngineMessageProfile` target. This bridge is not yet
connected to the message viewer. It includes a message-page navigation policy;
custom schemes must be registered before creating `QApplication`.

For the local automated setup (requires Qt6WebKitWidgets):

```bash
./install.sh
./install.sh --help
```

The installer defaults to `$HOME/.local`, including the executable in
`$HOME/.local/bin`. Add that directory to your `PATH` for command-line use;
the application-menu launcher does not depend on it. It requests
privileges for missing system packages or a protected installation prefix and
skips packages it detects as already installed. Use `--no-deps` to skip
package installation.
Package-manager selection uses `/etc/os-release` and fails rather than trying
an unrelated distribution's package manager. On unsupported distributions,
install dependencies manually and use `--no-deps`.
For `pacman`, unavailable optional integration packages are reported and
skipped; installing missing packages runs a full `pacman -Syu` system upgrade.
`PLASMA_IMPRESARIO_OS_RELEASE` can point to an alternate os-release file when
necessary; the old `PLASMA_MAIL_OS_RELEASE` name remains supported.
Qt6WebKitWidgets may need to be installed separately.

If Qt6WebKitWidgets is unavailable, `./install.sh --without-webkit` installs
an **incomplete build that cannot display message contents**. This option is
not a substitute for a working mail reader.

The installer also supports custom build and installation locations:

```bash
./install.sh --prefix "$HOME/apps/plasma-impresario" --build-dir "$HOME/plasma-impresario-build"
```

For a custom prefix, Plasma must search its `share` directory (through
`XDG_DATA_HOME` or `XDG_DATA_DIRS`) to discover the desktop entry. Configure
the session environment before logging in, or use the default prefix.

The installed Trojitá handbook and man page come from `docs/old` and may
describe historical behavior.

An in-place upgrade may leave the old
`share/metainfo/org.kde.trojita.appdata.xml` under the installation prefix.
Remove that obsolete metadata if it remains after the upgrade.

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
in one window. Use `./build/plasma-impresario --help` for all launch options; `-c`
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

The [Plasma Impresario SVG](src/icons/plasma-impresario.svg) combines an envelope
with the Plasma Opus golden-spiral motif at 256 × 256 pixels. The application
and desktop entry use this icon.

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
