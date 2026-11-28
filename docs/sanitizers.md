# Sanitizer runs

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Caveats](CAVEATS.md)** · **[Edge cases](EDGE.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

How to build and test Plasma Impresario with AddressSanitizer (ASan),
LeakSanitizer (LSan) and UndefinedBehaviorSanitizer (UBSan), what the first
full run on 2026-11-27 found, and which reports are suppressed and why. This is
[#481](https://github.com/mikepadiernos/plasma-impresario/issues/481) in
[M10](ROADMAP.md#m10-reliability-and-performance).

Legend: ![Fixed][fixed] found and fixed · ![Suppressed][suppressed]
third-party report, suppressed with a reason · ![Limitation][limitation]
known limitation, see [caveats](CAVEATS.md)

> [!NOTE]
>
> A sanitizer report fails the test that caused it. The suppression files name
> third-party code only; a report in Plasma Impresario code is a bug to fix,
> not to suppress.

## Contents

- [How to run](#how-to-run)
- [Environment](#environment)
- [Results](#results)
- [Findings](#findings)
- [Suppressions](#suppressions)
- [ThreadSanitizer](#threadsanitizer)

## How to run

The `IMPRESARIO_SANITIZERS` CMake option takes a list of sanitizers, which
GCC or Clang builds into every target. With it set, every test gets
`ASAN_OPTIONS`, `LSAN_OPTIONS`, `UBSAN_OPTIONS` and `TSAN_OPTIONS` that point
at the suppression files in `tests/`.

```bash
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    "-DIMPRESARIO_SANITIZERS=address;undefined"
cmake --build build-asan
cd build-asan && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

`tools/dev-container/sanitizers.sh` does the same in the
[headless container](../CONTRIBUTING.md), then runs every opt-in live test
against disposable Dovecot and POP3 servers: accounts, operations, panels,
search, compose, POP3 and the 5,000-message live workloads.

```bash
tools/dev-container/run.sh sh tools/dev-container/sanitizers.sh build-asan -E Cryptography_PGP
```

| Argument or variable    | Effect                                                                       |
| ----------------------- | ---------------------------------------------------------------------------- |
| First argument          | Build directory, `build-asan` by default                                     |
| Further arguments       | Passed to `ctest`, for example `-R Imap` or `-E Cryptography_PGP`            |
| `PLASMA_SANITIZERS`     | Sanitizers to build with, `address;undefined` by default                     |
| `PLASMA_SANITIZER_JOBS` | Parallel build jobs, 2 by default; a failed parallel build finishes with one |
| `PLASMA_SANITIZER_LIVE` | `0` skips the live tests                                                     |

> [!TIP]
>
> A sanitized build needs much more memory. With 8 GB, two jobs can run out of
> memory on `Window.cpp`; the script then finishes the build one job at a time.

UBSan stops at the first report in each test (`halt_on_error=1`), so after a
fix, run the test again to see whether it had more.

## Environment

| Item       | Value                                                                             |
| ---------- | --------------------------------------------------------------------------------- |
| Host       | Apple M5 Pro, dev container under colima with 4 CPUs and 8 GB of memory           |
| System     | Fedora 44, aarch64                                                                |
| Toolkit    | Qt 6.11.2 (distribution build, not instrumented), GCC 16.2.1                      |
| Build      | Debug configuration with `-fsanitize=address,undefined`, offscreen                |
| Sanitizers | ASan with LSan, `detect_stack_use_after_return` and `strict_string_checks`; UBSan |

## Results

| Run                                       | Result                                   |
| ----------------------------------------- | ---------------------------------------- |
| Unit and offscreen tests                  | 232 of 232 pass with no sanitizer report |
| Live IMAP accounts, operations and panels | Pass with no sanitizer report            |
| Live IMAP search and compose              | Pass with no sanitizer report            |
| Live POP3                                 | Pass with no sanitizer report            |
| Live workloads, 5,000 messages            | Pass with no sanitizer report            |

`test_Cryptography_PGP` is left out of these runs: in this container, its
`testDecryption(valid)` and `testDecryption(expiredKey)` rows fail the same way
without sanitizers, because GpgME reports decryption as failed. Its other rows
pass under the sanitizers with no report.

## Findings

Each finding below was fixed in Plasma Impresario code; none is suppressed.

| Kind                 | Where                                       | Cause                                                                                                          | Status          |
| -------------------- | ------------------------------------------- | -------------------------------------------------------------------------------------------------------------- | --------------- |
| UBSan, bad downcast  | `MsgListView::event`                        | Cast every event to `QKeyEvent` before checking its type                                                       | ![Fixed][fixed] |
| UBSan, bad downcast  | Message header event filter in `Window.cpp` | Cast every event to `QMouseEvent` before checking its type                                                     | ![Fixed][fixed] |
| UBSan, bad downcast  | `MessageSelectionRestorer::eventFilter`     | Compared a `QPointer<QTreeView>` with a view that was being destroyed; event types are now checked first       | ![Fixed][fixed] |
| UBSan, bad downcast  | `WebEngineMessagePage` script callbacks     | `~QWebEnginePage` runs pending callbacks after the page's own part is gone, while a `QPointer` still looks set | ![Fixed][fixed] |
| UBSan, overflow      | `test_Imap_Workloads`                       | Signed integer overflow while generating attachment data                                                       | ![Fixed][fixed] |
| ASan, use after free | `test_Imap_CopyAndFlagOperations`           | Read a task's results after the task had deleted itself; they are now taken when it completes                  | ![Fixed][fixed] |
| LSan, leak           | `EnvelopeView::setMessage`                  | A layout item taken out of the layout was never deleted                                                        | ![Fixed][fixed] |
| LSan, leak           | Network status actions in `Window.cpp`      | The offline, expensive and online actions had no parent                                                        | ![Fixed][fixed] |
| LSan, leak           | `TagListWidget`                             | The add button had no parent until it was put in a layout                                                      | ![Fixed][fixed] |
| LSan, leak           | `Streams::IODeviceSocket`                   | The delayed disconnect timer had no parent, so each closed IMAP connection leaked it                           | ![Fixed][fixed] |

## Suppressions

| File               | Entry                                    | Reason                                                                                                    | Status                    |
| ------------------ | ---------------------------------------- | --------------------------------------------------------------------------------------------------------- | ------------------------- |
| `tests/lsan.supp`  | `leak:libQt6WebEngineCore.so`            | Chromium inside Qt WebEngine keeps caches and singletons for the life of the process and never frees them | ![Suppressed][suppressed] |
| `tests/ubsan.supp` | `function:QtPrivate::QSlotObjectBase::*` | Qt calls slot objects through function pointers of another type; this is how Qt's signals work            | ![Suppressed][suppressed] |
| `tests/ubsan.supp` | `function:qt_message_print`              | Qt calls the message handler through a function pointer of another type                                   | ![Suppressed][suppressed] |
| `tests/ubsan.supp` | `vptr:QDBusConnection`, `vptr:boost::…`  | Type checks on objects whose type information comes from another shared library                           | ![Suppressed][suppressed] |
| `tests/tsan.supp`  | None                                     | Kept for a [ThreadSanitizer](#threadsanitizer) run with an instrumented Qt                                | ![Limitation][limitation] |

`fake-dev-random`, which the PGP tests preload into the system's `gpg`, is
built without sanitizers, because `gpg` has no sanitizer runtime.

## ThreadSanitizer

![Limitation][limitation] ThreadSanitizer has not been run. It needs every
library that shares memory between threads, Qt included, to be built with it;
otherwise Qt's own atomics and locks are reported as races. The container's Qt
is a distribution build, and the image has no `libtsan`. The CMake option
accepts `thread`, and `tests/tsan.supp` is in place for a run with a
TSan-built Qt. See the
[sanitizers caveat](CAVEATS.md#sanitizers-and-data-races).

[fixed]: https://img.shields.io/badge/Fixed-1a7f37
[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[suppressed]: https://img.shields.io/badge/Suppressed-6e7781
