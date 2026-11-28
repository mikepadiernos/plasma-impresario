# Installation and builds

**[Application](../README.md)** · **[Documentation](README.md)** ·
**[Roadmap](ROADMAP.md)** · **[Caveats](CAVEATS.md)** ·
**[Compatibility](philosophy/compatibility.md)** ·
**[Security](../SECURITY.md)** · **[Contributing](../CONTRIBUTING.md)**

`install.sh` installs missing build dependencies, builds Plasma Impresario with
the WebEngine message reader and installs it to `$HOME/.local`. This guide is
the source of truth for its options, supported distributions, installed files
and troubleshooting, as of 2026-11-28.

Legend: ![Verified][verified] · ![Needs verification][needs-verification] ·
![Limitation][limitation] · ![Unsupported][unsupported]

## Contents

- [Install](#install)
- [Options](#options)
- [Supported platforms](#supported-platforms)
  - [Renderer sandbox on Ubuntu](#renderer-sandbox-on-ubuntu)
- [Requirements by distribution](#requirements-by-distribution)
- [What gets installed](#what-gets-installed)
- [Updating Plasma Impresario](#updating-plasma-impresario)
- [Upgrading from Trojitá or Plasma
  Mail](#upgrading-from-trojitá-or-plasma-mail)
- [Where data is stored](#where-data-is-stored)
- [Uninstalling](#uninstalling)
- [Launch and troubleshooting](#launch-and-troubleshooting)
- [Security](#security)
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
5. Where the system restricts unprivileged user namespaces, as Ubuntu 24.04
   and later do, installs a [renderer sandbox profile](#renderer-sandbox-on-ubuntu)
   for AppArmor, using `sudo`.
6. [Converts older Trojitá or Plasma Mail
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
| `--no-apparmor`       | Do not install the [renderer sandbox profile](#renderer-sandbox-on-ubuntu); the installer warns instead.                                                    |
| `--help`, `-h`        | Show the option summary and exit.                                                                                                                           |

The installer also reads these environment variables:

| Variable                                                            | Effect                                                                                                                                                  |
| ------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `PREFIX`                                                            | Default installation prefix; `--prefix` overrides it.                                                                                                   |
| `BUILD_DIR`                                                         | Default build directory; `--build-dir` overrides it.                                                                                                    |
| `WITH_WEBENGINE`                                                    | `true` (default) or `false`; the WebEngine options override it.                                                                                         |
| `INSTALL_DEPS`                                                      | Any value other than `true` skips dependency handling, like `--no-deps`.                                                                                |
| `CMAKE_BUILD_PARALLEL_LEVEL`                                        | Number of parallel build jobs; defaults to `nproc`.                                                                                                     |
| `CONVERT_LEGACY`                                                    | Any value other than `true` skips legacy conversion, like `--keep-legacy`.                                                                              |
| `LEGACY_PREFIXES`                                                   | Colon-separated prefixes searched for older installations; defaults to `/usr/local:$HOME/.local`.                                                       |
| `PLASMA_IMPRESARIO_OS_RELEASE`                                      | Alternate os-release file. The older `PLASMA_MAIL_OS_RELEASE` name is also supported.                                                                   |
| `INSTALL_APPARMOR`                                                  | Any value other than `true` skips the AppArmor profile, like `--no-apparmor`.                                                                           |
| `PLASMA_IMPRESARIO_USERNS_SYSCTL`, `PLASMA_IMPRESARIO_APPARMOR_DIR` | Alternate restriction setting and profile directory, for tests; default `/proc/sys/kernel/apparmor_restrict_unprivileged_userns` and `/etc/apparmor.d`. |

For example:

```bash
./install.sh --prefix "$HOME/apps/plasma-impresario" --build-dir "$HOME/plasma-impresario-build"
```

> [!TIP]
>
> The installer reconfigures its build directory for a release build without
> tests. Use `--build-dir` to keep it apart from a `build/` directory that you
> use for development and CTest.

## Supported platforms

Plasma Impresario is a KDE Plasma application for Linux. Other desktops and
operating systems are outside the [product scope](philosophy/scope.md), and no
Flatpak, Snap or distribution package is published; build from source with the
installer.

| Platform                                    | Status                                                                                     | Notes                                                                                                                                                                                                                                                                               |
| ------------------------------------------- | ------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| KDE Plasma 6 on Wayland, Linux x86_64       | ![Verified][verified] build and install; ![Needs verification][needs-verification] session | The release target. Built, tested and installed on CachyOS; use in a real Plasma session is checked in M15 ([#319](https://github.com/mikepadiernos/plasma-impresario/issues/319)).                                                                                                 |
| Linux aarch64                               | ![Verified][verified] build and tests                                                      | The full test suite runs offscreen in the headless Fedora 44 [dev container](../CONTRIBUTING.md#building-outside-plasma). No desktop session has been tried.                                                                                                                        |
| Arch, Debian and Ubuntu, Fedora, openSUSE   | ![Needs verification][needs-verification]                                                  | The installer has a package list for each; see [requirements by distribution](#requirements-by-distribution).                                                                                                                                                                       |
| Ubuntu 24.04 and later                      | ![Needs verification][needs-verification]                                                  | These releases restrict unprivileged user namespaces, which the QtWebEngine renderer sandbox uses; the installer adds a [profile](#renderer-sandbox-on-ubuntu) for them. Untried on Ubuntu; checked in M15 ([#512](https://github.com/mikepadiernos/plasma-impresario/issues/512)). |
| KDE Plasma 6 on X11                         | ![Limitation][limitation]                                                                  | Not a release target. It may work, but problems specific to X11 are not fixed.                                                                                                                                                                                                      |
| KDE Plasma 5 and earlier                    | ![Unsupported][unsupported]                                                                | The build needs Qt 6 and KDE Frameworks 6.                                                                                                                                                                                                                                          |
| Other Linux desktops, such as GNOME or Xfce | ![Unsupported][unsupported]                                                                | Outside the scope. Desktop integration expects Plasma services.                                                                                                                                                                                                                     |
| macOS and Windows                           | ![Unsupported][unsupported]                                                                | macOS serves only as a host for the dev container. The inherited Windows installer files in `packaging/` are not maintained.                                                                                                                                                        |

The [compatibility baseline](philosophy/compatibility.md) records the exact
versions observed and what each check covered.

### Renderer sandbox on Ubuntu

The message reader runs web content in QtWebEngine's sandboxed renderer, which
needs unprivileged user namespaces. Ubuntu 24.04 and later allow them only to
programs with an AppArmor profile, which the system turns on with
`kernel.apparmor_restrict_unprivileged_userns = 1`. When that setting is on,
the installer writes `/etc/apparmor.d/plasma-impresario` for the installed
binary and loads it with `apparmor_parser`. Like the distribution's profiles for
browsers and for `QtWebEngineProcess`, it allows user namespaces and confines
nothing else. The template is
[`packaging/apparmor/plasma-impresario.in`](../packaging/apparmor/plasma-impresario.in).

The installer never turns the sandbox off. If it cannot install the profile,
because `apparmor_parser` is missing, `--no-apparmor` was given or the prefix
contains characters that AppArmor treats as patterns, it prints a warning and
the installation continues.

> [!CAUTION]
>
> This step has only been tested with mock commands, not on Ubuntu; that check
> is [#512](https://github.com/mikepadiernos/plasma-impresario/issues/512). Do not work around a failing
> sandbox with `QTWEBENGINE_DISABLE_SANDBOX` or `--no-sandbox`: they remove the
> renderer's isolation from untrusted mail.

The profile is not in the install manifest. To remove it:

```bash
sudo apparmor_parser -R /etc/apparmor.d/plasma-impresario
sudo rm /etc/apparmor.d/plasma-impresario
```

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
| `lib*/libtrojita_plugins.so`                                            | Shared plugin library for password storage, address books and spelling    |
| `bin/be.contacts`                                                       | Address book editor of the built-in address book plugin                   |
| `share/knotifications6/plasma-impresario.notifyrc`                      | Notification events, when built with KNotifications                       |
| `share/krunner/dbusplugins/plasma-runner-plasma-impresario.desktop`     | Plasma Search provider, when built with D-Bus                             |
| `share/plasma/plasmoids/io.github.mikepadiernos.plasma_impresario.*`    | Status and ticker Plasma widgets, with their QML module                   |
| `share/locale/…`                                                        | Translations                                                              |
| `share/man/man1/trojita.1` and the KDE handbook                         | Historical Trojitá documentation from [`docs/old`](old/README.md)         |

Optional files appear only when their dependency was found at configure time;
the CMake feature summary lists which. The handbook is installed only when KF6
DocTools is available.

## Updating Plasma Impresario

Update the checkout and run the installer again with the same options:

```bash
git pull
./install.sh
```

The installer reconfigures and rebuilds its build directory, then installs
over the previous files. Settings, accounts, stored passwords, the cache and
local mail are kept; quit Plasma Impresario first so the new binary starts
cleanly. If CMake reports a stale cache after a large update, remove the
installer's build directory (`build/` by default) and run the installer again.

There are no release versions yet: `main` is the only line that receives fixes,
as described in the [security policy](../SECURITY.md#supported-versions).

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

## Where data is stored

Plasma Impresario keeps Trojitá's `flaska.net/trojita` locations so that
existing data keeps working. `PROFILE` stands for the name given with
`--profile`.

| Data                                                                     | Location                                                                |
| ------------------------------------------------------------------------ | ----------------------------------------------------------------------- |
| Accounts and most settings                                               | `~/.config/flaska.net/trojita.conf`, or `trojita-PROFILE.conf`          |
| Passwords with the cleartext plugin                                      | The same settings file                                                  |
| Passwords with the QtKeychain plugin                                     | KWallet or the Secret Service, through QtKeychain                       |
| Newer options, such as quick search, shared by all profiles              | `~/.config/plasma-impresariorc`                                         |
| Window, panel and saved-search state                                     | `trojitastaterc` in the KDE state directory, normally `~/.local/state/` |
| Message cache, offline search index, autosaved drafts and connection log | `~/.cache/flaska.net/trojita/`, or its `PROFILE/` subfolder             |
| Copies of opened and fetched attachments                                 | `Opened Attachments/` and `Fetched Attachments/` in the cache folder    |
| Local mail of POP3 and JMAP accounts                                     | `~/.local/share/flaska.net/trojita/local-mail/`                         |

The cache can be deleted while the application is closed; it is downloaded
again from the server. Local mail cannot: POP3 servers usually keep no copy, so
back it up. Cache and local-mail folders are readable only by their owner; see
[local data](CAVEATS.md#local-data-and-opened-attachments).

## Uninstalling

CMake records the files it installed in the build directory. With the default
prefix and build directory:

```bash
xargs -d '\n' rm -f < build/install_manifest.txt
rm -f ~/.local/bin/trojita
```

If the installer added the [renderer sandbox profile](#renderer-sandbox-on-ubuntu),
remove it as shown there.

The `bin/trojita` compatibility link is created by an install script, so the
manifest does not list it. Add `sudo` for a protected prefix. Then remove the
files in [where data is stored](#where-data-is-stored) to delete settings and
mail. Removing the accounts in the settings first also deletes their stored
passwords; otherwise remove them with KWallet Manager. Run `kbuildsycoca6` or log out and back in if the
launcher remains in the application menu.

## Launch and troubleshooting

Start Plasma Impresario from the application menu or run `plasma-impresario`.

| Problem                                                            | What to do                                                                                                                                                                                                                                                        |
| ------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `plasma-impresario` is not found on the command line               | Add `$HOME/.local/bin` (or `PREFIX/bin`) to `PATH`. The application-menu launcher does not depend on it.                                                                                                                                                          |
| The launcher is missing with a custom prefix                       | Plasma must search the prefix's `share` directory through `XDG_DATA_HOME` or `XDG_DATA_DIRS`. Configure the session environment before logging in, or use the default prefix.                                                                                     |
| "Unsupported distribution" or "No supported package manager found" | Install the dependencies manually and run `./install.sh --no-deps`.                                                                                                                                                                                               |
| "Root privileges or sudo are required"                             | Install `sudo`, run as root, or install the missing packages yourself and use `--no-deps`.                                                                                                                                                                        |
| CMake fails after the dependency check passed                      | The check does not cover versions or CMake-only requirements. Compare the CMake error with the [compatibility baseline](philosophy/compatibility.md).                                                                                                             |
| "Icon cache … may be stale" or "Could not refresh icon cache"      | Install `gtk-update-icon-cache` and run `gtk-update-icon-cache --force --ignore-theme-index "$PREFIX/share/icons/hicolor"` with `PREFIX` set to the prefix. Run `kbuildsycoca6`, or log out and back in, if the old icon remains.                                 |
| Messages do not display                                            | The build is renderer-free. Rebuild with the default `--with-webengine`.                                                                                                                                                                                          |
| Messages do not display on Ubuntu 24.04 or later                   | The renderer sandbox may lack its AppArmor profile. Check that `/etc/apparmor.d/plasma-impresario` names the installed binary, run the installer again without `--no-apparmor`, and see the [renderer sandbox](#renderer-sandbox-on-ubuntu). Keep the sandbox on. |
| The handbook or man page describes Trojitá                         | Both come from the historical [`docs/old`](old/README.md) archive and may describe old behavior.                                                                                                                                                                  |
| The password is asked for at every start                           | Check the password plugin in the general settings. QtKeychain, the default for new profiles, needs `qtkeychain-qt6` at build time and KWallet or the Secret Service running in the session.                                                                       |
| An account fails to connect or behaves oddly                       | Start with `plasma-impresario --log-to-disk` and reproduce the problem. The log is written to `trojita-connection-log` in the cache folder. It can contain mail and personal data; delete it afterwards and redact it before sharing.                             |
| A certificate warning appears                                      | Reject it unless the provider confirms the certificate; see [safe operating guidance](CAVEATS.md#safe-operating-guidance).                                                                                                                                        |
| The theme or layout looks wrong after a change                     | Start with `plasma-impresario --reset-appearance`, or `--no-workspace-appearance` to skip workspace overrides.                                                                                                                                                    |
| A clean configuration is needed to compare                         | Start with `plasma-impresario --profile test`. The profile has its own accounts, settings file and cache; only `plasma-impresariorc` is shared.                                                                                                                   |
| The application menu still shows the old launcher                  | Run `kbuildsycoca6`, or log out and back in.                                                                                                                                                                                                                      |

## Security

Report vulnerabilities privately as described in the
[security policy](../SECURITY.md). The
[security summary](CAVEATS.md#security-and-data-protection) lists how
connections, credentials, message content and cached data are protected today,
and what is not yet protected.

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
[unsupported]: https://img.shields.io/badge/Unsupported-6e7781
[verified]: https://img.shields.io/badge/Verified-1a7f37
