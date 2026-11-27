# Installation and builds

**[Application](../README.md)** · **[Documentation](README.md)** ·
**[Roadmap](ROADMAP.md)** · **[Caveats](CAVEATS.md)** ·
**[Compatibility](philosophy/compatibility.md)** ·
**[Contributing](../CONTRIBUTING.md)**

`install.sh` installs missing build dependencies, builds Plasma Impresario with
the WebEngine message reader and installs it to `$HOME/.local`. This guide is
the source of truth for its options, supported distributions, installed files
and troubleshooting, as of 2026-11-25.

Legend: ![Verified][verified] · ![Needs verification][needs-verification] ·
![Limitation][limitation]

## Contents

- [Install](#install)
- [Options](#options)
- [Requirements by distribution](#requirements-by-distribution)
- [What gets installed](#what-gets-installed)
- [Upgrading from Trojitá or Plasma
  Mail](#upgrading-from-trojitá-or-plasma-mail)
- [Launch and troubleshooting](#launch-and-troubleshooting)
- [Development checks](#development-checks)

## Install

```bash
./install.sh
```

The installer then:

1. Reads `/etc/os-release` and selects a package manager from the first
   recognized value of `ID` or `ID_LIKE`. It fails instead of selecting an
   unrelated package manager.
2. Checks the [listed packages](#requirements-by-distribution) and installs
   only the missing ones, using `sudo` when not run as root.
3. Configures `build/` in the repository with Ninja, `RelWithDebInfo`,
   `BUILD_TESTING=OFF` and `WITH_WEBENGINE=ON`, then builds in parallel.
4. Installs to the prefix, using `sudo` only when the prefix (or its nearest
   existing parent) is not writable.
5. [Converts older Trojitá or Plasma Mail
   installations](#upgrading-from-trojitá-or-plasma-mail) and makes Plasma
   Impresario the mail handler wherever the user chose them.

On unsupported distributions, install the dependencies manually and use
`--no-deps`.

> [!WARNING]
>
> A renderer-free build (`--without-webengine` or `-DWITH_WEBENGINE=OFF`) cannot
> display message contents. It is an incomplete mail client; the installer warns
> about it and reports the installation as incomplete.

## Options

Run `./install.sh --help` for the same list.

| Option                | Effect                                                                                                                                                      |
| --------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `--check-deps`        | Check the installer-listed packages without installing or building; exit nonzero if any are missing. Cannot be combined with `--no-deps` or `--build-only`. |
| `--no-deps`           | Skip dependency checks and package installation.                                                                                                            |
| `--build-only`        | Build without installing the application. Missing dependencies may still be installed; combine with `--no-deps` to avoid that.                              |
| `--prefix PATH`       | Install to `PATH` instead of `$HOME/.local`. The path must be absolute.                                                                                     |
| `--build-dir PATH`    | Use `PATH` as the build directory instead of `build/` in the repository.                                                                                    |
| `--with-webengine`    | Require Qt 6 WebEngineWidgets for the message reader. This is the default.                                                                                  |
| `--without-webengine` | ![Limitation][limitation] Build without message viewing and skip the WebEngine package. See the warning above.                                              |
| `--keep-legacy`       | Leave Trojitá and Plasma Mail installations and mail handler settings unchanged.                                                                            |
| `--help`, `-h`        | Show the option summary and exit.                                                                                                                           |

The installer also reads these environment variables:

| Variable                       | Effect                                                                                            |
| ------------------------------ | ------------------------------------------------------------------------------------------------- |
| `PREFIX`                       | Default installation prefix; `--prefix` overrides it.                                             |
| `BUILD_DIR`                    | Default build directory; `--build-dir` overrides it.                                              |
| `WITH_WEBENGINE`               | `true` (default) or `false`; the WebEngine options override it.                                   |
| `INSTALL_DEPS`                 | Any value other than `true` skips dependency handling, like `--no-deps`.                          |
| `CMAKE_BUILD_PARALLEL_LEVEL`   | Number of parallel build jobs; defaults to `nproc`.                                               |
| `CONVERT_LEGACY`               | Any value other than `true` skips legacy conversion, like `--keep-legacy`.                        |
| `LEGACY_PREFIXES`              | Colon-separated prefixes searched for older installations; defaults to `/usr/local:$HOME/.local`. |
| `PLASMA_IMPRESARIO_OS_RELEASE` | Alternate os-release file. The older `PLASMA_MAIL_OS_RELEASE` name is also supported.             |

For example:

```bash
./install.sh --prefix "$HOME/apps/plasma-impresario" --build-dir "$HOME/plasma-impresario-build"
```

> [!TIP]
>
> The installer reconfigures its build directory for a release build without
> tests. Use `--build-dir` to keep it apart from a `build/` directory that you
> use for development and CTest.

## Requirements by distribution

The [compatibility baseline](philosophy/compatibility.md) records the verified
environment and the CMake-level dependencies: Qt 6.7 or later and KDE
Frameworks 6.26 or later.

| Family        | `ID` or `ID_LIKE` values                         | Package manager                       | WebEngine package            | Status                                                                                              |
| ------------- | ------------------------------------------------ | ------------------------------------- | ---------------------------- | --------------------------------------------------------------------------------------------------- |
| Arch          | `arch`, `cachyos`, `manjaro`                     | `pacman`; else `paru`, `yay`, `pamac` | `qt6-webengine`              | ![Verified][verified] CachyOS with `pacman`; ![Needs verification][needs-verification] the others   |
| Debian/Ubuntu | `debian`, `ubuntu`                               | `apt-get`                             | `qt6-webengine-dev`          | ![Needs verification][needs-verification]                                                           |
| Fedora/RHEL   | `fedora`, `rhel`, `centos`, `rocky`, `almalinux` | `dnf`                                 | `qt6-qtwebengine-devel`      | ![Needs verification][needs-verification] installer; the headless dev container builds on Fedora 44 |
| openSUSE/SUSE | `opensuse*`, `suse`, `sles`                      | `zypper`                              | `qt6-webenginewidgets-devel` | ![Needs verification][needs-verification]                                                           |

> [!WARNING]
>
> On Arch-family systems, installing missing packages runs `pacman -Syu` (or
> `paru`/`yay -Syu`), which also upgrades the system. `paru` and `yay` must run
> as a regular user.

With `pacman`, optional integration packages that are neither installed nor
found in the configured repositories are skipped with a warning. With `paru`,
`yay` and `pamac`, the same packages are required. Dependency checks do not
verify package versions or CMake-only requirements, so CMake can still fail
after a successful check.

<details>
<summary>Packages checked for each package manager</summary>

The WebEngine package from the table above is added unless `--without-webengine`
is used.

| Package manager        | Packages                                                                                                                                                                                                                                                                                                                                                                                                                |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `pacman`               | `base-devel` `cmake` `ninja` `pkgconf` `extra-cmake-modules` `qt6-base` `qt6-declarative` `qt6-svg` `qt6-tools` `qt6-5compat` `kitemmodels` `kconfig` `ki18n` `kio`; optional when available: `mimetic` `gpgme` `gpgmepp` `qgpgme` `qtkeychain-qt6` `kdoctools` `sonnet` `knotifications` `akonadi` `akonadi-contacts`                                                                                                  |
| `paru`, `yay`, `pamac` | The `pacman` list, with the optional packages required                                                                                                                                                                                                                                                                                                                                                                  |
| `apt-get`              | `build-essential` `cmake` `ninja-build` `pkg-config` `extra-cmake-modules` `qt6-base-dev` `qt6-declarative-dev` `qt6-base-private-dev` `qt6-tools-dev` `qt6-tools-dev-tools` `qt6-svg-dev` `qt6-5compat-dev` `libqt6sql6-dev` `libqt6dbus6-dev` `libkf6itemmodels-dev` `libkf6config-dev` `libkf6i18n-dev` `libkf6kio-dev` `libkf6sonnet-dev` `libkf6notifications-dev` `libkf6akonadicore-dev` `libkf6akonadimime-dev` |
| `dnf`                  | `gcc-c++` `cmake` `ninja-build` `make` `pkgconf-pkg-config` `extra-cmake-modules` `qt6-qtbase-devel` `qt6-qtdeclarative-devel` `qt6-qtsvg-devel` `qt6-qttools-devel` `qt6-qt5compat-devel` `kf6-kitemmodels-devel` `kf6-kconfig-devel` `kf6-ki18n-devel` `kf6-kio-devel` `kf6-sonnet-devel` `kf6-knotifications-devel` `kf6-akonadi-devel`                                                                              |
| `zypper`               | `gcc-c++` `cmake` `ninja` `pkg-config` `extra-cmake-modules` `libqt6-qtbase-common-devel` `libqt6-qtdeclarative-devel` `libQt6Svg6-devel` `qt6-tools-devel` `libqt6-qt5compat-devel` `kitemmodels-devel` `kf6-kconfig-devel` `kf6-ki18n-devel` `kf6-kio-devel` `sonnet-devel` `kf6-knotifications-devel` `akonadi-devel`                                                                                                |

</details>

## What gets installed

Paths are relative to the prefix.

| File                                                                    | Purpose                                                                   |
| ----------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| `bin/plasma-impresario`                                                 | The application                                                           |
| `bin/trojita`                                                           | Compatibility alias (a symbolic link on Linux)                            |
| `share/applications/io.github.mikepadiernos.plasma_impresario.desktop`  | Application-menu launcher, pointing at the installed binary               |
| `share/applications/org.kde.trojita.desktop`                            | Hidden compatibility launcher                                             |
| `share/metainfo/io.github.mikepadiernos.plasma_impresario.metainfo.xml` | AppStream metadata                                                        |
| `share/icons/hicolor/…`                                                 | Application icons; an existing icon cache is refreshed after installation |
| `share/man/man1/trojita.1` and the KDE handbook                         | Historical Trojitá documentation from [`docs/old`](old/README.md)         |

The handbook is installed only when KF6 DocTools is available.

## Upgrading from Trojitá or Plasma Mail

Plasma Impresario reads Trojitá's settings, cache, and stored mail in place, so
accounts and preferences need no migration. After installing, the installer
replaces older installations:

- In `/usr/local` and `~/.local` (or `LEGACY_PREFIXES`), a prefix with a
  Trojitá or Plasma Mail `bin/trojita` program and no Plasma Impresario binary
  has its Trojitá files removed: launcher, metadata, icons, plugins,
  translations, handbook, and man page. Its `bin/trojita` becomes a link to the
  new `plasma-impresario`, so scripts and launchers that call `trojita` keep
  working. Package-manager prefixes such as `/usr` are never changed.
- Obsolete `share/metainfo/org.kde.trojita.appdata.xml` metadata is removed
  from the installation prefix.
- Where the user's `mimeapps.list` chose Trojitá or Plasma Mail, for example as
  the default mail client, it now chooses Plasma Impresario.

Panel entries pinned to the old launcher keep working through the hidden
compatibility launcher. A protected prefix requests administrator privileges;
if conversion fails, the installer prints a warning and leaves the files in
place. An upstream Trojitá built from source into one of these prefixes is also
replaced; use `--keep-legacy` to keep it.

## Launch and troubleshooting

Start Plasma Impresario from the application menu or run `plasma-impresario`.

| Problem                                                            | What to do                                                                                                                                                                                                                        |
| ------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `plasma-impresario` is not found on the command line               | Add `$HOME/.local/bin` (or `PREFIX/bin`) to `PATH`. The application-menu launcher does not depend on it.                                                                                                                          |
| The launcher is missing with a custom prefix                       | Plasma must search the prefix's `share` directory through `XDG_DATA_HOME` or `XDG_DATA_DIRS`. Configure the session environment before logging in, or use the default prefix.                                                     |
| "Unsupported distribution" or "No supported package manager found" | Install the dependencies manually and run `./install.sh --no-deps`.                                                                                                                                                               |
| "Root privileges or sudo are required"                             | Install `sudo`, run as root, or install the missing packages yourself and use `--no-deps`.                                                                                                                                        |
| CMake fails after the dependency check passed                      | The check does not cover versions or CMake-only requirements. Compare the CMake error with the [compatibility baseline](philosophy/compatibility.md).                                                                             |
| "Icon cache … may be stale" or "Could not refresh icon cache"      | Install `gtk-update-icon-cache` and run `gtk-update-icon-cache --force --ignore-theme-index "$PREFIX/share/icons/hicolor"` with `PREFIX` set to the prefix. Run `kbuildsycoca6`, or log out and back in, if the old icon remains. |
| Messages do not display                                            | The build is renderer-free. Rebuild with the default `--with-webengine`.                                                                                                                                                          |
| The handbook or man page describes Trojitá                         | Both come from the historical [`docs/old`](old/README.md) archive and may describe old behavior.                                                                                                                                  |

## Development checks

For a separate development build, use the
[manual build commands](../README.md#build-and-run). Installer checks use mock
commands rather than installing packages:

```bash
bash tests/test_install.sh
```

After installing to a temporary prefix, check the installed binary, launchers,
metadata and icons:

```bash
python3 tests/test_installed_identity.py "$PREFIX" share
```

On hosts without Plasma, use the headless dev container described in
[building outside Plasma](../CONTRIBUTING.md#building-outside-plasma).

The [compatibility baseline](philosophy/compatibility.md) records tested
environments and reproducible component checks. Live account and desktop
workflows remain subject to
[pending verification](TODO.md#pending-verification).

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[verified]: https://img.shields.io/badge/Verified-1a7f37
