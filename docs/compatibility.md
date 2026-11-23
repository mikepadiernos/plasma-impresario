# M01 compatibility baseline

This is the initial release target and an evidence record, not a claim that a
complete reader is supported today. M01 requires an integrated renderer and
the [M01 release gate](ROADMAP.md#m01-kde--qt6-compatibility-baseline) before a
configuration can be advertised as verified.

## Initial target and observed environment

| Area | Initial M01 target | Evidence and limits |
| --- | --- | --- |
| Distribution and architecture | CachyOS on x86_64, using its Arch-family `pacman` packages | A renderer-free build and user-prefix install were performed on CachyOS. Other Arch derivatives and other architectures are not verified. |
| Desktop and session | KDE Plasma 6, Wayland | Plasma Workspace 6.8.3 and a KDE Wayland session were present on the observed host. Account setup and message reading in the desktop session have not been verified. X11 is not yet a release target. |
| Compiler | GCC with C++20 support | GCC 16.2.1 built the renderer-free application on the observed host. No older GCC minimum or Clang version has been validated. |
| Qt | Qt 6.7 or newer as required by CMake | Qt Core 6.12.0 was used for the observed build; the experimental WebEngine tests used the installed WebEngine package reporting 6.140.0. These results do not validate every version from 6.7 onward. |
| KDE Frameworks | KF6 ItemModels and ECM compatible with the selected Qt | ItemModels and ECM 6.32.0 were present. CMake does not currently declare a minimum version for either package; earlier versions are not verified. |
| Build tools | CMake and Ninja | CMake 4.5.1 and Ninja 1.13.2 were used. CMake declares 3.16 as its minimum, but that minimum has not been tested with the required ECM/Qt toolchain. |

Debian/Ubuntu (`apt-get`), Fedora/RHEL-family (`dnf`), and openSUSE/SUSE
(`zypper`) have installer branches but are **not verified release targets**.
The installer can also select `paru`, `yay`, or `pamac` on Arch-derived systems
when earlier candidates are unavailable; those paths are not equivalent to a
verified installation. Package names and repository availability vary between
releases. Unsupported distributions must install dependencies manually and
use `--no-deps`.

## Dependencies and limitations

The current CMake configuration requires Qt Core, Core5Compat, DBus, Network,
Sql, Svg, Test and Widgets, plus ECM and KF6 ItemModels. Qt Test and DBus are
currently found unconditionally even when tests or DBus integration are
disabled. The minimum effective versions can be raised by the installed Qt or
KF6 packages.

On the observed CachyOS host the installer names `base-devel`, `cmake`,
`ninja`, `pkgconf`, `extra-cmake-modules`, `qt6-base`, `qt6-declarative`,
`qt6-svg`, `qt6-tools`, `qt6-5compat`, and `kitemmodels` were installed.
That observation does not establish that all of these are minimal CMake
requirements or that the installer package list works on every repository
configuration. On `pacman`, the installer attempts to install optional
integrations only when they are installed or available in configured
repositories, and warns about unavailable ones. `mimetic` was absent locally
and its availability depends on configured repositories. Other package-manager
branches have not yet been audited for this behavior. With `pacman`, installing
missing packages runs `pacman -Syu`, which also upgrades the system.

| Area | Dependency or behavior | Current limitation |
| --- | --- | --- |
| IMAP and TLS | Qt Network supplies sockets and TLS; the observed host has the Qt OpenSSL TLS plugin. | A working TLS backend and trusted certificates are needed for encrypted connections. The selected account can still use cleartext. Real server and certificate behavior have not been verified for M01. |
| Local storage | Persistent cache uses Qt SQL's `QSQLITE` driver and the filesystem. | The observed host has the SQLite driver. Cache setup reports an error and falls back to memory if persistent cache cannot open; cached mail is not application-encrypted. |
| Message rendering | The existing reader requires Qt6WebKitWidgets. The experimental bridge additionally requires Qt6 WebEngineWidgets when enabled. | WebEngine is not connected to the reader. Without WebKit the current application cannot display message contents, even if the bridge is built. |
| Credentials and desktop services | QtKeychain, DBus, Sonnet, Akonadi and KDE documentation tools are optional integrations. | Availability and behavior depend on installed plugins and session services; neither a secure credential provider nor working desktop integration has been verified for the target session. The cleartext password plugin is available. |
| MIME and OpenPGP | Mimetic, GpgME++ and QGpgme are optional; encrypted-message support depends on the configured MIME and crypto features. | The observed build disabled Mimetic and encrypted messages. |

## Reproducing the current component checks

The following build is deliberately **not a complete mail reader**. Use a
separate development build directory so an existing installation is untouched:

```bash
cmake -S . -B build-m01 -G Ninja \
  -DWITH_WEBKIT=OFF -DWITH_WEBENGINE_BRIDGE=ON -DBUILD_TESTING=ON
cmake --build build-m01 --target test_SqlCache test_Imap_Tasks_OpenConnection \
  test_Imap_MsgPartNetAccessManager test_WebEngineMessageProfile
QT_QPA_PLATFORM=offscreen ctest --test-dir build-m01 \
  -R '^(test_SqlCache|test_Imap_Tasks_OpenConnection|test_Imap_MsgPartNetAccessManager|test_WebEngineMessageProfile)$' \
  --output-on-failure
```

These are component tests, including WebEngine with synthetic message data;
they do not verify a real IMAP account, Plasma desktop integration, an
installed application's plugins, or message-reading workflows. Full
distribution, session, installation, and account smoke tests remain open.
