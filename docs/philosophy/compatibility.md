# M01 compatibility baseline

**[Documentation](../README.md)** · **[Installation](../INSTALL.md)** ·
**[Roadmap](../ROADMAP.md)** · **[TODO](../TODO.md)** ·
**[Caveats](../CAVEATS.md)**

This is the supported build target and the evidence recorded for it, not a claim
of full reader or desktop-session support. The
[M01 release gate](../ROADMAP.md#m01-kde--qt6-compatibility-baseline) is merged
into `main`; live account, reading and desktop-session checks are deferred to
later milestones, as recorded in
[TODO](../TODO.md#m01-release-gate-and-integration).

Legend: ✅ verified · 🧪 implemented, needs verification · 🚧 in progress · 📋
planned · ⚠️ limitation

## Initial target and observed environment

| Area                                     | Target                                    | Status | Evidence and limits                                                                                                                                                                       |
| ---------------------------------------- | ----------------------------------------- | ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🐧 Distribution and architecture         | CachyOS on x86_64 with `pacman`           | ✅     | Renderer-free and WebEngine builds, user-prefix and temporary-prefix installs, and installer checks passed. Other Arch derivatives and architectures are not verified.                    |
| 🖥️ Desktop and session                   | KDE Plasma 6, Wayland                     | 🧪     | Plasma Workspace 6.8.3 and a KDE Wayland session were present on the observed host. Account setup and message reading in the session have not been verified.                              |
| 🖥️ X11 session                           | None                                      | ⚠️     | Not a release target.                                                                                                                                                                     |
| 🛠️ Compiler                              | GCC with C++20 support                    | ✅     | GCC 16.2.1. No older GCC minimum or any Clang version has been validated.                                                                                                                 |
| 🧱 Qt                                    | Qt 6.7.0 or newer, as required by CMake   | ✅     | Qt 6.12.0. This does not validate every version from 6.7 onward.                                                                                                                          |
| 🧱 KDE Frameworks                        | KF6 and ECM compatible with the chosen Qt | ✅     | ECM and KF6 6.32.0. The project declares no minimums for ECM, ItemModels, Config or I18n; the observed CMake package summary reported an effective 6.25.0 minimum for ItemModels and ECM. |
| 🛠️ Build tools                           | CMake and Ninja                           | ✅     | CMake 4.5.1 and Ninja 1.13.2. CMake declares 3.16 as its minimum, which has not been tested with the required ECM and Qt toolchain.                                                       |
| 📖 WebEngine reader                      | Qt6 WebEngineWidgets, on by default       | ✅     | Renderer-enabled build with the `qt6-webengine` 6.140.0 package passed all 35 CTest cases at M01 completion; live-session reading remains unverified.                                     |
| 📬 Live account setup                    | Isolated test account                     | 🚧     | Deferred to M02 ([#78](https://github.com/mikepadiernos/plasma-impresario/issues/78)); exercised only against a disposable Dovecot container so far.                                      |
| 📖 Live reading, replies and attachments | Isolated test mail                        | 📋     | Deferred to M04 ([#93](https://github.com/mikepadiernos/plasma-impresario/issues/93)).                                                                                                    |
| 🖥️ Plasma-session startup and recovery   | Real Plasma session                       | 📋     | Deferred to M06.01 ([#101](https://github.com/mikepadiernos/plasma-impresario/issues/101)).                                                                                               |
| 📦 System-prefix installs                | Renderer-free and WebEngine               | 📋     | Pending; only user and temporary prefixes have been verified.                                                                                                                             |

### Installer package managers

`install.sh` selects a package manager from `/etc/os-release` and fails rather
than choosing an unrelated one. Package names and repository availability vary
between releases. On unsupported distributions, install dependencies manually
and run `./install.sh --no-deps`; see [installation](../INSTALL.md).

| Distribution family    | Package manager          | Status | Notes                                             |
| ---------------------- | ------------------------ | ------ | ------------------------------------------------- |
| CachyOS                | `pacman`                 | ✅     | The only verified target                          |
| Arch, CachyOS, Manjaro | `paru`, `yay` or `pamac` | 🧪     | Installer branch exists; unverified               |
| Arch and Manjaro       | `pacman`                 | 🧪     | Same package list as CachyOS; unverified          |
| Debian and Ubuntu      | `apt-get`                | 🧪     | Installer branch exists; package list not audited |
| Fedora and RHEL family | `dnf`                    | 🧪     | Installer branch exists; package list not audited |
| openSUSE and SLES      | `zypper`                 | 🧪     | Installer branch exists; package list not audited |

## Dependencies and limitations

### CMake and CachyOS package audit

`CMakeLists.txt` declares CMake 3.16, C++20 and Qt 6.7.0 as minimums. ECM and
the KF6 ItemModels, Config and I18n components are required without
project-level minimum versions; Config and I18n are required by the default
`WITH_DESKTOP=ON`. Qt Test and DBus are required unconditionally, even when
`BUILD_TESTING` or `WITH_DBUS` is disabled.

| Required build dependency                                        | CachyOS package       | Observed version |
| ---------------------------------------------------------------- | --------------------- | ---------------- |
| CMake 3.16 or newer                                              | `cmake`               | 4.5.1            |
| C++20 compiler                                                   | `base-devel`          | GCC 16.2.1       |
| Ninja generator                                                  | `ninja`               | 1.13.2           |
| ECM CMake modules                                                | `extra-cmake-modules` | 6.32.0           |
| Qt Core, DBus, Network, Sql, Test and Widgets, Qt 6.7.0 or newer | `qt6-base`            | 6.12.0           |
| Qt Core5Compat, Qt 6.7.0 or newer                                | `qt6-5compat`         | 6.12.0           |
| Qt Svg, Qt 6.7.0 or newer                                        | `qt6-svg`             | 6.12.0           |
| Qt WebEngineWidgets, Qt 6.7.0 or newer (`WITH_WEBENGINE=ON`)     | `qt6-webengine`       | 6.140.0          |
| KF6 ItemModels                                                   | `kitemmodels`         | 6.32.0           |
| KF6 Config and I18n                                              | `kconfig`, `ki18n`    | 6.32.0           |

> [!NOTE]
>
> `6.140.0` is the version of the Arch `qt6-webengine` package, as reported by
> pacman and the
> [Arch package database](https://archlinux.org/packages/extra/x86_64/qt6-webengine/).
> It is numbered separately from the Qt 6.12.0 libraries it was built against
> and is not a typo.

The installer also installs `pkgconf` 3.0.11, `qt6-tools` 6.12.0 and
`qt6-declarative` 6.12.0. These are not direct components of the root CMake Qt
requirement; `qt6-declarative` belongs to the WebEngine dependency chain.

> [!NOTE]
>
> The installer does not pin or validate package versions. The versions above
> are observations from the tested rolling CachyOS repositories, not
> constraints; the CMake package summary is the source for effective minimums.

| Optional feature            | CMake dependency and constraint             | CachyOS package and evidence                                                   |
| --------------------------- | ------------------------------------------- | ------------------------------------------------------------------------------ |
| 🧩 MIME parsing             | Mimetic (`WITH_MIMETIC`)                    | `mimetic` was not installed, so CMake disabled it.                             |
| 🔒 OpenPGP messages         | Gpgmepp and QGpgmeQt6, each 1.24.0 or newer | `gpgme` 2.3.1, `gpgmepp` 2.2.0 and `qgpgme` 2.2.0 were installed and detected. |
| 🔑 Secure credential plugin | Qt6Keychain                                 | `qtkeychain-qt6` 0.18.0 was installed and detected.                            |
| ✍️ Spell checking           | KF6Sonnet 6.21.0 or newer                   | `sonnet` 6.32.0 was installed and detected.                                    |
| 👥 Akonadi address book     | KPim6AkonadiContactCore                     | `akonadi` and `akonadi-contacts` 26.08.3 were installed and detected.          |
| 📖 Handbook generation      | KF6DocTools 6.26.0 or newer                 | `kdoctools` 6.32.0 was installed and detected.                                 |

On `pacman`, optional packages missing from the configured repositories are
skipped with a warning. Package lists for other managers have not been audited.

> [!WARNING]
>
> On `pacman`, installing missing packages runs `pacman -Syu`, which also
> upgrades the system.

| Area                                | Dependency or behavior                                                                                       | Current limitation                                                                                                                                                                                                      |
| ----------------------------------- | ------------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🔒 IMAP and TLS                     | Qt Network supplies sockets and TLS; the observed host had the Qt OpenSSL TLS plugin.                        | ⚠️ A working TLS backend and trusted certificates are needed for encrypted connections. An account can still be configured for cleartext. Real server and certificate behavior were not verified for M01.               |
| 💾 Local storage                    | The persistent cache uses Qt SQL's `QSQLITE` driver and the filesystem.                                      | ⚠️ The observed host had the SQLite driver. If the persistent cache cannot open, setup reports an error and falls back to memory. Cached mail is not application-encrypted.                                             |
| 📖 Message rendering                | Qt6 WebEngineWidgets is required when `WITH_WEBENGINE=ON`.                                                   | ⚠️ The observed renderer-enabled build and tests pass, but live-session and cross-distribution verification remain. A build with WebEngine disabled cannot display messages.                                            |
| 🔑 Credentials and desktop services | QtKeychain, DBus, Sonnet, Akonadi and KDE documentation tools are optional integrations.                     | ⚠️ Behavior depends on installed plugins and session services. Neither a secure credential provider nor desktop integration has been verified in the target session. The cleartext password plugin is built by default. |
| 🔒 MIME and OpenPGP                 | Mimetic, GpgME++ and QGpgme are optional; encrypted-message support needs both the MIME and crypto features. | ⚠️ The observed build disabled Mimetic, and therefore encrypted-message support.                                                                                                                                        |

See [CAVEATS](../CAVEATS.md#security-and-data-protection) for security
recommendations.

## Reproducing the current component checks

Use a separate development build directory so an existing installation is
untouched. The first configuration is deliberately **not a complete mail
reader**:

```bash
cmake -S . -B build-m01 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DQT_MAJOR_VERSION=6 \
  -DWITH_WEBENGINE=OFF -DBUILD_TESTING=ON
cmake --build build-m01 --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build-m01 --output-on-failure
```

This renderer-free configuration builds and passes the available CTest suites,
but it does not exercise message rendering, a real IMAP account, Plasma desktop
integration, installed plugins or message-reading workflows. The
renderer-enabled configuration was verified on the observed CachyOS and Qt
toolchain with:

```bash
cmake -S . -B build-m01-webengine -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DQT_MAJOR_VERSION=6 \
  -DWITH_WEBENGINE=ON -DBUILD_TESTING=ON
cmake --build build-m01-webengine --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build-m01-webengine --output-on-failure
```

At M01 completion all 35 CTest cases passed, along with a temporary-prefix
installation, `--version` startup and the installer checks. Later work adds test
cases, so current counts differ. Live account setup, message reading, replies,
attachments and Plasma-session behavior remain unverified; on hosts without
Plasma, use the dev container described in
[building outside Plasma](../../CONTRIBUTING.md#building-outside-plasma).
