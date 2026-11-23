# Installation and builds

**[Application](../README.md)** · **[Documentation](README.md)** ·
**[Compatibility](philosophy/compatibility.md)** · **[Caveats](CAVEATS.md)**

The default installer builds the WebEngine reader and installs to
`$HOME/.local`. See `./install.sh --help` for all options.

## Install

```bash
./install.sh
```

The installer selects a package manager from `/etc/os-release`, checks installed
packages and requests privileges only for missing system packages or a protected
installation prefix. It fails instead of selecting an unrelated package manager.
On unsupported distributions, install dependencies manually and use `--no-deps`.

> [!WARNING]
>
> On `pacman`, installing missing packages runs `pacman -Syu`, which also
> upgrades the system. Unavailable optional integration packages are skipped
> with a warning.

## Options

| Option                | Effect                                                                                                                                     |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| `--check-deps`        | Check installer-listed packages without installing or building; exit nonzero if any are missing. Available optional packages are included. |
| `--no-deps`           | Skip dependency checks and package installation.                                                                                           |
| `--build-only`        | Build without installing the application. Missing dependencies may still be installed; combine with `--no-deps` to avoid that.             |
| `--prefix PATH`       | Install to a custom prefix instead of `~/.local`.                                                                                          |
| `--build-dir PATH`    | Use a custom build directory.                                                                                                              |
| `--with-webengine`    | Require the full WebEngine reader; this is the default.                                                                                    |
| `--without-webengine` | Build without message viewing; this is an incomplete mail client.                                                                          |

Dependency checks do not verify package versions or CMake-only requirements.
`PLASMA_IMPRESARIO_OS_RELEASE` can specify an alternate os-release file; the
older `PLASMA_MAIL_OS_RELEASE` name is also supported.

For example:

```bash
./install.sh --prefix "$HOME/apps/plasma-impresario" --build-dir "$HOME/plasma-impresario-build"
```

## Launch and troubleshooting

- Add `$HOME/.local/bin` to `PATH` for command-line use. The installed
  application-menu launcher does not depend on it.
- For a custom prefix, Plasma must search its `share` directory through
  `XDG_DATA_HOME` or `XDG_DATA_DIRS`. Configure the session environment before
  logging in, or use the default prefix.
- An in-place upgrade may leave `share/metainfo/org.kde.trojita.appdata.xml`
  under the prefix. Remove that obsolete metadata if it remains.
- The installed Trojitá handbook and man page come from `docs/old` and may
  describe historical behavior.

## Development checks

Use the [manual build commands](../README.md#build-and-run) for a separate
development build. Installer checks use mock commands rather than installing
packages:

```bash
bash tests/test_install.sh
```

The [compatibility baseline](philosophy/compatibility.md) records tested
environments and reproducible component checks. Live account and desktop
workflows remain subject to the
[pending verification](TODO.md#pending-verification).
