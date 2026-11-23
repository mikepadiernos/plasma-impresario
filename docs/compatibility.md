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
| Qt | Qt 6.7 or newer as required by CMake | Qt Core 6.12.0 configured the renderer-free build. This does not validate every version from 6.7 onward. |
| KDE Frameworks | KF6 ItemModels and ECM compatible with the selected Qt | ItemModels and ECM 6.32.0 were used. The project does not declare explicit minimums; CMake's package summary reports an effective 6.25.0 minimum for both. |
| Build tools | CMake and Ninja | CMake 4.5.1 and Ninja 1.13.2 were used. CMake declares 3.16 as its minimum, but that minimum has not been tested with the required ECM/Qt toolchain. |

For M01.01, CachyOS x86_64 with `pacman` is the only verified distribution and
package-manager target. This records a build environment, not support for a
complete reader; the M01 release gate remains open. Debian/Ubuntu (`apt-get`),
Fedora/RHEL-family (`dnf`), and openSUSE/SUSE (`zypper`) have installer
branches but are **not verified release targets**. The installer can select
`paru`, `yay`, or `pamac` on Arch-derived systems, but those paths are also
unverified. Package names and repository availability vary between releases.
Unsupported distributions must install dependencies manually and use
`--no-deps`.

## Dependencies and limitations

### CMake and CachyOS package audit

The project declares CMake 3.16, C++20 and Qt 6.7.0 as its minimums. ECM and
KF6 ItemModels are required without explicit project-level minimum versions;
the current configure reports an effective minimum of 6.25.0 for both. The
observed toolchain used CMake 4.5.1, Ninja 1.13.2, GCC 16.2.1, ECM 6.32.0,
KF6 ItemModels 6.32.0 and Qt 6.12.0. No minimum GCC version or tested Clang
version is established. Qt Test and DBus are required unconditionally by
CMake, even if `BUILD_TESTING` or `WITH_DBUS` is disabled.

| Required build dependency | CachyOS package | Observed package version |
| --- | --- | --- |
| CMake 3.16 or newer | `cmake` | 4.5.1 |
| C++20 compiler | `base-devel` | GCC 16.2.1 |
| Ninja generator | `ninja` | 1.13.2 |
| ECM CMake modules | `extra-cmake-modules` | 6.32.0 |
| Qt Core, DBus, Network, Sql, Test and Widgets, Qt 6.7.0 or newer | `qt6-base` | 6.12.0 |
| Qt Core5Compat, Qt 6.7.0 or newer | `qt6-5compat` | 6.12.0 |
| Qt Svg, Qt 6.7.0 or newer | `qt6-svg` | 6.12.0 |
| KF6 ItemModels | `kitemmodels` | 6.32.0 |

The installer also installs `pkgconf` 3.0.11, `qt6-tools` 6.12.0 and
`qt6-declarative` 6.12.0. These are not direct components of the root CMake Qt
requirement; `qt6-declarative` is used by the optional WebEngine bridge
dependency chain. The installer does not pin package versions, so the versions
above are observations from the tested rolling CachyOS repositories, not
package constraints.

| Optional feature | CMake dependency and constraint | CachyOS package and evidence |
| --- | --- | --- |
| Message display | Qt6WebKitWidgets when `WITH_WEBKIT=ON` (the default) | Not installed on the observed host; a complete reader build is not verified. |
| Experimental WebEngine bridge | Qt6 WebEngineWidgets, Qt 6.7.0 or newer | `qt6-webengine` 6.140.0 is installed, but bridge configuration fails in Qt's QML package setup with Qt 6.12.0. This bridge is not a message viewer. |
| MIME parsing | Mimetic | `mimetic` is not installed and is disabled by CMake. |
| OpenPGP messages | Gpgmepp and QGpgmeQt6, each at least 1.24.0 | `gpgme` 2.3.1, `gpgmepp` 2.2.0 and `qgpgme` 2.2.0 are installed and detected by CMake. |
| Secure credential plugin | Qt6Keychain | `qtkeychain-qt6` 0.18.0 is installed and detected by CMake. |
| Spell checking | KF6Sonnet at least 6.21.0 | `sonnet` 6.32.0 is installed and detected by CMake. |
| Akonadi address book plugin | KPim6AkonadiContactCore | `akonadi` and `akonadi-contacts` 26.08.3 are installed and detected by CMake. |
| Handbook generation | KF6DocTools at least 6.26.0 | `kdoctools` 6.32.0 is installed and detected by CMake. |

The CMake package summary is the source for effective dependency constraints;
the installer does not validate package versions before building. Other
installer package lists have not been audited and must not be treated as
supported distribution targets. On `pacman`, unavailable optional integration
packages are skipped with a warning. Installing missing packages runs
`pacman -Syu`, which also upgrades the system.

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
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_WEBKIT=OFF -DWITH_WEBENGINE_BRIDGE=OFF -DBUILD_TESTING=ON
cmake --build build-m01 --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build-m01 --output-on-failure
```

This renderer-free configuration does not verify message rendering, a real
IMAP account, Plasma desktop integration, installed plugins, or message-reading
workflows. The bridge-enabled configuration is currently blocked by the
installed Qt WebEngine/QML package combination. Full distribution, session,
installation, and account smoke tests remain open.
