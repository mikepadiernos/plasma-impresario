# Stabilization gate

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Caveats](CAVEATS.md)** · **[Edge cases](EDGE.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

[M10](ROADMAP.md#m10-reliability-sync-and-notification-quality) is the
stabilization gate before broader product expansion
([#128](https://github.com/mikepadiernos/plasma-impresario/issues/128)): the
milestones after it build on the mail core, so its reliability work has to be
finished and its checks have to pass first. This page lists what the gate asks
for, how to run it, and the result recorded on 2026-11-28.

Legend: ![Pass][pass] passes · ![Excluded][excluded] left out, with the
reason · ![Not covered][not-covered] outside the gate, see the notes

> [!NOTE]
>
> The gate runs in the headless Linux development container, against fake
> servers and disposable local Dovecot and POP3 servers. It does not replace
> testing in a Plasma session or against real providers; see
> [what the gate does not cover](#what-the-gate-does-not-cover).

## Contents

- [Criteria](#criteria)
- [How to run](#how-to-run)
- [Result](#result)
- [Found while running the gate](#found-while-running-the-gate)
- [Open caveats](#open-caveats)
- [What the gate does not cover](#what-the-gate-does-not-cover)

## Criteria

Every M10 feature is finished, and each one's evidence is part of the gate.

| Criterion                                                                                                 | Evidence                                                   | Issue                                                                                                                                      |
| --------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| Retries, offline handling and background task reporting                                                   | Offscreen and live account tests                           | [#51](https://github.com/mikepadiernos/plasma-impresario/issues/51)                                                                        |
| Connection state, folder sync stability and message consistency                                           | Offscreen and live account tests                           | [#127](https://github.com/mikepadiernos/plasma-impresario/issues/127)                                                                      |
| Recovery from intermittent connections, expired credentials and server errors without silent message loss | `test_Imap_Reliability`, live account tests                | [#129](https://github.com/mikepadiernos/plasma-impresario/issues/129)                                                                      |
| Folder and unread counts stay consistent across accounts and concurrent changes                           | Read state tests, live account tests                       | [#130](https://github.com/mikepadiernos/plasma-impresario/issues/130)                                                                      |
| Responsiveness and resource use for large folders, long threads, bulk actions and attachments             | [Workload budgets](performance.md#budgets), live workloads | [#52](https://github.com/mikepadiernos/plasma-impresario/issues/52), [#131](https://github.com/mikepadiernos/plasma-impresario/issues/131) |
| Failed background operations stay visible and can be retried or resolved                                  | Activity and outbox tests                                  | [#132](https://github.com/mikepadiernos/plasma-impresario/issues/132)                                                                      |
| No memory, lifetime or undefined-behavior reports in network, cache and background paths                  | [Sanitizer run](sanitizers.md)                             | [#481](https://github.com/mikepadiernos/plasma-impresario/issues/481)                                                                      |
| The search index stays consistent with incremental updates                                                | `test_SearchIndex` and its budgets                         | [#357](https://github.com/mikepadiernos/plasma-impresario/issues/357)                                                                      |
| Unsent messages survive restarts and are never sent twice                                                 | `test_Composer_Outbox`, `test_OutboxDialog`                | [#378](https://github.com/mikepadiernos/plasma-impresario/issues/378)                                                                      |
| Recent headers load first, and background loading never delays the user                                   | `test_Imap_HeaderBackfill`, live operations                | [#379](https://github.com/mikepadiernos/plasma-impresario/issues/379)                                                                      |

## How to run

`tools/dev-container/stabilization-gate.sh` runs the checks below in order,
keeps going after a failure, and ends with a summary of each check. It exits
non-zero if any check failed.

```bash
tools/dev-container/run.sh sh tools/dev-container/stabilization-gate.sh build -E Cryptography_PGP
PLASMA_GATE_SANITIZERS=1 tools/dev-container/run.sh sh tools/dev-container/stabilization-gate.sh build -E Cryptography_PGP
```

| Check               | What it runs                                                                              | Fails when                                                |
| ------------------- | ----------------------------------------------------------------------------------------- | --------------------------------------------------------- |
| Build               | Configures a Debug build with tests and builds it                                         | The build fails                                           |
| Test suite          | `ctest` with the given arguments                                                          | Any test fails                                            |
| Workload budgets    | `test_Imap_Workloads` and `test_SearchIndex` with `IMPRESARIO_WORKLOAD_BUDGETS=1`         | A workload runs over its [budget](performance.md#budgets) |
| Roadmap issue links | `tools/roadmap-issue-links.py --check`                                                    | The roadmap still links a board item instead of its issue |
| Live servers        | The live account, operations, panels, search, compose, POP3 and workload scripts          | Any live test fails against its disposable server         |
| Sanitizers          | `tools/dev-container/sanitizers.sh` in `build-asan`, only with `PLASMA_GATE_SANITIZERS=1` | Any test or live test fails, or a sanitizer reports       |

| Argument or variable            | Effect                                                 |
| ------------------------------- | ------------------------------------------------------ |
| First argument                  | Build directory, `build` by default                    |
| Further arguments               | Passed to `ctest`, and to the sanitizer run            |
| `PLASMA_GATE_LIVE`              | `0` skips the live server tests                        |
| `PLASMA_GATE_SANITIZERS`        | `1` adds the sanitizer run, which takes much longer    |
| `PLASMA_GATE_JOBS`              | Build and test jobs, `2` by default                    |
| `PLASMA_TEST_WORKLOAD_MESSAGES` | Messages in the live workloads, `5000` by default here |

## Result

Recorded on 2026-11-28 on `feature/m10-reliability`, in the dev container on
an Apple M5 Pro under colima (Fedora 44, aarch64, Qt 6.11.2, GCC 16.2.1,
Dovecot 2.4.5).

| Check                   | Result                                                                                                                                        | Status                      |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------- |
| Build                   | Debug build with tests                                                                                                                        | ![Pass][pass]               |
| Test suite              | 235 of 235                                                                                                                                    | ![Pass][pass]               |
| Workload budgets        | `test_Imap_Workloads` 11 of 11, `test_SearchIndex` 83 of 83                                                                                   | ![Pass][pass]               |
| Roadmap issue links     | No board item links                                                                                                                           | ![Pass][pass]               |
| Live servers            | All seven scripts                                                                                                                             | ![Pass][pass]               |
| Sanitizers              | AddressSanitizer and UndefinedBehaviorSanitizer: 235 of 235 and all seven live scripts, with no report                                        | ![Pass][pass]               |
| `test_Cryptography_PGP` | Two decryption rows fail in this container with and without sanitizers, because GpgME reports decryption as failed ([details](sanitizers.md)) | ![Excluded][excluded]       |
| ThreadSanitizer         | Not run ([#507](https://github.com/mikepadiernos/plasma-impresario/issues/507))                                                               | ![Not covered][not-covered] |

## Found while running the gate

- ![Pass][pass] **Renaming the open folder could end the session.** Dovecot
  closes the connection with "IMAP session state is inconsistent" when the
  selected mailbox is renamed while another command on it is still running.
  Background header loading made this likely: the live operations test failed
  in 7 of 10 runs. A rename now waits until no other command runs on its
  connection, and the test passed 10 of 10 runs afterwards. Covered by
  `test_Imap_Tasks_RenameMailbox::testRenameWaitsForRunningCommand`.

## Open caveats

The gate passes with these limitations recorded. None loses mail; each has a
workaround and proposed improvements in [caveats](CAVEATS.md).

| Caveat                                                                  | Issue                                                                 |
| ----------------------------------------------------------------------- | --------------------------------------------------------------------- |
| [Read state and unread counts](CAVEATS.md#read-state-and-unread-counts) | [#505](https://github.com/mikepadiernos/plasma-impresario/issues/505) |
| [Performance and resource use](CAVEATS.md#performance-and-resource-use) | [#506](https://github.com/mikepadiernos/plasma-impresario/issues/506) |
| [ThreadSanitizer is not run](CAVEATS.md#sanitizers-and-data-races)      | [#507](https://github.com/mikepadiernos/plasma-impresario/issues/507) |
| [Persistent outbox](CAVEATS.md#persistent-outbox)                       | [#508](https://github.com/mikepadiernos/plasma-impresario/issues/508) |
| [Background header loading](CAVEATS.md#background-header-loading)       | [#509](https://github.com/mikepadiernos/plasma-impresario/issues/509) |

## What the gate does not cover

| Area                                               | Status                      | Where it is planned or tracked                                           |
| -------------------------------------------------- | --------------------------- | ------------------------------------------------------------------------ |
| A Plasma session, Breeze and real desktop services | ![Not covered][not-covered] | [M11](ROADMAP.md#m11-release-readiness-and-polish) end-to-end validation |
| Real providers, remote servers and network latency | ![Not covered][not-covered] | [Edge cases](EDGE.md), [performance](performance.md#not-measured-yet)    |
| A Release build                                    | ![Not covered][not-covered] | [Performance](performance.md#not-measured-yet)                           |
| Data races                                         | ![Not covered][not-covered] | [#507](https://github.com/mikepadiernos/plasma-impresario/issues/507)    |

[excluded]: https://img.shields.io/badge/Excluded-6e7781
[not-covered]: https://img.shields.io/badge/Not_covered-bc4c00
[pass]: https://img.shields.io/badge/Pass-1a7f37
