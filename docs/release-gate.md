# Release gate

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Release readiness](RELEASE.md)** · **[Pending tasks](TODO.md)** ·
**[Caveats](CAVEATS.md)** · **[Stabilization gate](stabilization-gate.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

The [M11](ROADMAP.md#m11-release-readiness-and-polish) checks a release
candidate has to pass before it is tagged
([#482](https://github.com/mikepadiernos/plasma-impresario/issues/482)):
formatting, supported dependency versions, a reproducible build, the full test
suite with a coverage report, and static analysis without unreviewed findings.
It is criterion [R5](RELEASE.md#release-criteria). This page lists the checks,
how to run them, the result recorded on 2026-11-28 and the review of every
accepted finding.

Legend: ![Pass][pass] passes · ![Excluded][excluded] left out, with the
reason · ![Baseline][baseline] accepted after review · ![Residual risk][risk]
known gap, tracked

> [!NOTE]
>
> The gate is a script that runs in the headless Linux development container.
> No hosted CI runs it, so run it by hand on the commit to be released. The
> [stabilization gate](stabilization-gate.md) adds the live server tests and
> the sanitizers; run both. See [residual risk](#residual-risk).

## Contents

- [Checks](#checks)
- [How to run](#how-to-run)
- [Result](#result)
- [Baseline review](#baseline-review)
  - [Formatting baseline](#formatting-baseline)
  - [Static analysis baseline](#static-analysis-baseline)
- [Found while running the gate](#found-while-running-the-gate)
- [Residual risk](#residual-risk)

## Checks

| Check               | What it runs                                                                                                                                                                                         | Fails when                                                |
| ------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------- |
| Formatting          | `clang-format --dry-run -Werror` with the project style, `tools/release/clang-format`, on every C++ file in `src/` and `tests/`                                                                      | A file outside the formatting baseline is not formatted   |
| Dependency versions | Configures the coverage build; `cmake/DependencyVersions.cmake` writes what CMake found, and the gate compares each version with the declared minimum                                                | A dependency is older than its minimum                    |
| Reproducible build  | Two clean Release builds in `build-release-repro` with `SOURCE_DATE_EPOCH` and `QT_RCC_SOURCE_DATE_OVERRIDE` set to the commit time and ccache off; compares the SHA-256 of every binary and library | Any binary differs between the builds                     |
| Tests with coverage | `ctest` in a Debug build with `--coverage`, then `gcovr` over `src/`, without the bundled `qwwsmtpclient`                                                                                            | Any test fails                                            |
| Static analysis     | `run-clang-tidy` over `src/` with correctness checks only: the core, C++ and Unix analyzers, `VirtualCall`, and five `bugprone` checks                                                               | A finding is not in the reviewed static analysis baseline |

The dependency minimums are declared in `CMakeLists.txt`: Qt 6.7.0, KDE
Frameworks and ECM 6.26.0, Sonnet 6.21.0, and Gpgmepp and QGpgme 1.24.0.

## How to run

```bash
tools/dev-container/run.sh sh tools/dev-container/release-gate.sh -E Cryptography_PGP
```

Arguments go to `ctest`. The gate keeps going after a failure, ends with a
summary of each check, and exits non-zero if one failed. A full run takes about
50 minutes on the machine below.

| Variable or path                             | Effect                                                                              |
| -------------------------------------------- | ----------------------------------------------------------------------------------- |
| `PLASMA_RELEASE_JOBS`                        | Build, test and analysis jobs, `3` by default                                       |
| `PLASMA_RELEASE_REPRO=0`                     | Skips the reproducible build; the gate then reports itself **Incomplete**           |
| `PLASMA_RELEASE_COVERAGE=0`                  | Skips the tests with coverage; **Incomplete**                                       |
| `PLASMA_RELEASE_TIDY=0`                      | Skips static analysis; **Incomplete**                                               |
| `build-release-gate/`                        | Reports: `summary.txt`, `environment.txt`, `format.txt`, `coverage.txt`, and others |
| `build-release-cov/`, `build-release-repro/` | The coverage and reproducibility builds                                             |

> [!WARNING]
>
> Do not edit or commit tracked files while the gate runs. The version string
> comes from `git describe --dirty`, so a change during the run makes the two
> reproducibility builds differ.

## Result

Recorded on 2026-11-28 on `feature/m12-nextcloud` at `0ebbdbf6`, in the dev
container on an Apple M5 Pro under colima: Fedora 44, aarch64, GCC 16.2.1,
CMake 4.3.0, Ninja 1.13.2, clang-format and clang-tidy 22.1.8, gcovr 8.6.

| Check                   | Result                                                                                                                                              | Status                |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------- |
| Formatting              | 255 files checked, all formatted; 761 in the baseline, none formatted since                                                                         | ![Pass][pass]         |
| Dependency versions     | Qt 6.11.2, KDE Frameworks and ECM 6.32.0, Gpgmepp and QGpgme 2.0.0, KPim 6.8.3, Qt6Keychain 0.16.0; all at or above their minimums                  | ![Pass][pass]         |
| Reproducible build      | 7 binaries identical in both builds                                                                                                                 | ![Pass][pass]         |
| Tests with coverage     | 239 of 239 in 98.20 s. Lines 72.2% (52,621 of 72,871), functions 74.9% (5,783 of 7,717), branches 40.7% (63,407 of 155,622)                         | ![Pass][pass]         |
| Static analysis         | 9 findings, all in the reviewed baseline; none new                                                                                                  | ![Pass][pass]         |
| `test_Cryptography_PGP` | Two decryption rows fail in this container because GpgME reports decryption as failed, as in the [stabilization gate](stabilization-gate.md#result) | ![Excluded][excluded] |

Branch coverage is low partly because GCC counts the branches of exception
cleanup and of Qt's inline templates. No coverage threshold is enforced; the
figures are the baseline for the next release.

## Baseline review

Both baselines live in `tools/release/`. The gate reports a baseline entry that
no longer applies, so the lists only shrink.

### Formatting baseline

`format-baseline.txt` lists the 761 C++ files, mostly inherited from Trojitá,
that the project style would still change. Reformatting them in one commit
would bury the history of every inherited line, so they stay as they are. A
file is formatted in a commit of its own when it is next edited heavily, and
then leaves the baseline. New files are never added.

### Static analysis baseline

`clang-tidy-baseline.txt` lists each accepted finding as file, check and
message, without the line number, which moves with edits.

| Finding                                                                                         | Status                | Why it is accepted                                                                                                                                   |
| ----------------------------------------------------------------------------------------------- | --------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `NewDeleteLeaks` in `AppearanceStyle.cpp`                                                       | ![Baseline][baseline] | `QApplication::setStyle()` takes ownership of the style; the analyzer does not model it.                                                             |
| `NewDeleteLeaks` for `group` in `MessageRowsMenu.cpp` and `ag` in `SimplePartWidget.cpp`        | ![Baseline][baseline] | Each `QActionGroup` gets a parent `QObject`, which deletes it.                                                                                       |
| `NewDeleteLeaks` for `accountsCall` and `sharingCall` in `MailStatusClient.cpp`                 | ![Baseline][baseline] | Each D-Bus call watcher deletes itself with `deleteLater()` when its reply arrives.                                                                  |
| `NewDeleteLeaks` for `flagTask` in `CopyMoveMessagesTask.cpp`                                   | ![Baseline][baseline] | Inherited from Trojitá: every task is a child of the IMAP model, which deletes it when it finishes.                                                  |
| `VirtualCall` in `FlowLayout.cpp`, `ObtainSynchronizedMailboxTask.cpp` and `IODeviceSocket.cpp` | ![Baseline][baseline] | Inherited code that calls its own class's override during construction or destruction, which is the intended target; no subclass overrides it there. |

## Found while running the gate

- ![Pass][pass] **Three panels called a virtual method from their
  constructors.** `ContactPanel`, `MessageDetailsPanel` and
  `RelatedMessagesPanel` called `refresh()` while being constructed. The calls
  now name the class's own `refresh()` explicitly, which is what ran before.
- ![Pass][pass] **A link menu relied on an implicit check.** In
  `SimplePartWidget`, the context menu read the link text from a request that
  was checked only through an empty link address, which the analyzer could
  not follow. The menu now checks the request itself.
- ![Pass][pass] **The first run passed without checking anything.** The
  pattern that extracts findings rejected check names with capitals, such as
  `NewDeleteLeaks`, so the first analysis reported nothing. The pattern was
  fixed and the gate rerun.

- ![Pass][pass] **A settings handler hid a label after guarding itself.**
  In M12, the Nextcloud **Connect…** handler in `SettingsGroups.cpp`
  created a `QPointer` guard of the group before hiding the status label,
  so the analyzer treated the group as possibly null. The label is now hidden
  first.

## Residual risk

| Risk                                                                                        | Status                 | Tracked in                                                            |
| ------------------------------------------------------------------------------------------- | ---------------------- | --------------------------------------------------------------------- |
| No hosted CI runs the gate; a regression shows up only at the next manual run               | ![Residual risk][risk] | [#511](https://github.com/mikepadiernos/plasma-impresario/issues/511) |
| Reproducibility is checked in one directory on one machine, not across build paths or hosts | ![Residual risk][risk] | [#511](https://github.com/mikepadiernos/plasma-impresario/issues/511) |
| 763 inherited files are outside the formatting check                                        | ![Residual risk][risk] | [#511](https://github.com/mikepadiernos/plasma-impresario/issues/511) |
| `test_Cryptography_PGP` is left out in the container                                        | ![Residual risk][risk] | [#511](https://github.com/mikepadiernos/plasma-impresario/issues/511) |
| Data races are not checked; ThreadSanitizer needs a Qt built for it                         | ![Residual risk][risk] | [#507](https://github.com/mikepadiernos/plasma-impresario/issues/507) |
| Nothing here runs in a Plasma session                                                       | ![Residual risk][risk] | [M15](ROADMAP.md#m15-real-plasma-session-verification)                |

[baseline]: https://img.shields.io/badge/Baseline-0969da
[excluded]: https://img.shields.io/badge/Excluded-6e7781
[pass]: https://img.shields.io/badge/Pass-1a7f37
[risk]: https://img.shields.io/badge/Residual_risk-bc4c00
