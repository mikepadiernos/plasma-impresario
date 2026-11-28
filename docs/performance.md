# Performance and resource use

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Caveats](CAVEATS.md)** · **[Edge cases](EDGE.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

Workload measurements for large folders, long conversations, bulk changes and
attachments, recorded on 2026-11-27 for
[#131](https://github.com/mikepadiernos/plasma-impresario/issues/131). Limits
found here are tracked as the
[performance caveat](CAVEATS.md#performance-and-resource-use)
([#506](https://github.com/mikepadiernos/plasma-impresario/issues/506)).

Legend: ![Within budget][within-budget] stays inside its time and
responsiveness budgets · ![Limitation][limitation] known limitation, see
[caveats](CAVEATS.md) · ![Not measured][not-measured] no measurement yet

> [!NOTE]
>
> These are measurements of the IMAP model against fake servers in a Debug
> build, not of a real server or a Plasma session. Use them to compare changes
> and to spot growth, not as user-facing timings.

## Contents

- [How to run](#how-to-run)
- [Environment](#environment)
- [Results](#results)
- [Four times the size](#four-times-the-size)
- [Findings](#findings)
- [Budgets](#budgets)
- [Not measured yet](#not-measured-yet)

## How to run

The workloads live in `tests/Imap/test_Imap_Workloads.cpp` and run with the
rest of the suite. By default they only check correctness; the variables below
turn on the budgets, change the sizes and write a report.

```bash
cmake --build build --target test_Imap_Workloads
cd build
IMPRESARIO_WORKLOAD_BUDGETS=1 QT_QPA_PLATFORM=offscreen ./test_Imap_Workloads
```

| Variable                      | Effect                                                                                               |
| ----------------------------- | ---------------------------------------------------------------------------------------------------- |
| `IMPRESARIO_WORKLOAD_BUDGETS` | `1` fails a workload that runs over its time budget or blocks the event loop for longer than allowed |
| `IMPRESARIO_WORKLOAD_SCALE`   | Multiplies every size, for example `4` for the [larger run](#four-times-the-size)                    |
| `IMPRESARIO_WORKLOAD_REPORT`  | Appends one tab-separated line per workload: name, items, ms, longest pass, RSS change, peak, budget |

Memory is read from `/proc/self/status` (`VmRSS` and `VmHWM`), so memory
columns are filled in on Linux only. The longest pass is the longest single
event-loop pass while the workload runs, which is how long the window would
stop responding.

## Environment

| Item    | Value                                                                      |
| ------- | -------------------------------------------------------------------------- |
| Host    | Apple M5 Pro, dev container under colima with 4 CPUs and 8 GB of memory    |
| System  | Fedora 44, aarch64                                                         |
| Toolkit | Qt 6.11.2, GCC 16.2.1                                                      |
| Build   | Debug, offscreen platform                                                  |
| Server  | Fake IMAP connection in the test fixture; responses are injected, not sent |
| Method  | Median of three runs at scale 1                                            |

## Results

| Workload                                 | Size                                 | Time       | Longest pass | Memory                             | Status                          |
| ---------------------------------------- | ------------------------------------ | ---------- | ------------ | ---------------------------------- | ------------------------------- |
| Large-folder sync                        | 50,000 messages                      | ~215 ms    | not measured | +44 MiB, about 0.9 KiB per message | ![Within budget][within-budget] |
| Unread quick filter on and off           | 50,000 messages                      | ~110 ms    | 70 to 100 ms | —                                  | ![Within budget][within-budget] |
| Long conversation                        | 2,000 messages, 200-deep reply chain | 1 to 2 ms  | —            | —                                  | ![Within budget][within-budget] |
| Bulk mark as read                        | 10,000 messages                      | ~33 ms     | ~21 ms       | +2 to 4 MiB                        | ![Within budget][within-budget] |
| Removal reported one `EXPUNGE` at a time | 10,000 of 50,000 messages            | ~790 ms    | ~20 ms       | —                                  | ![Within budget][within-budget] |
| Removal reported as one `VANISHED` range | 10,000 of 50,000 messages            | ~465 ms    | ~465 ms      | —                                  | ![Limitation][limitation]       |
| Message with many attachments            | 200 attachments                      | under 2 ms | —            | +0.5 MiB                           | ![Within budget][within-budget] |
| Large attachment opened                  | 20 MiB                               | ~53 ms     | ~35 ms       | +47 MiB, about 2.3 times its size  | ![Within budget][within-budget] |

The sync workload runs synchronously inside the fixture, so its longest pass is
not measured separately.

## Four times the size

The same workloads with `IMPRESARIO_WORKLOAD_SCALE=4`, run once to show how
they grow. Budgets are not enforced at this scale.

| Workload                                 | Size                       | Time   | Longest pass | Memory   |
| ---------------------------------------- | -------------------------- | ------ | ------------ | -------- |
| Large-folder sync                        | 200,000 messages           | 1.24 s | —            | +173 MiB |
| Unread quick filter on and off           | 200,000 messages           | 1.45 s | 1.2 s        | —        |
| Long conversation                        | 8,000 messages             | 4 ms   | —            | —        |
| Bulk mark as read                        | 40,000 messages            | 126 ms | —            | —        |
| Removal reported one `EXPUNGE` at a time | 40,000 of 200,000 messages | 13.9 s | —            | —        |
| Removal reported as one `VANISHED` range | 40,000 of 200,000 messages | 8.06 s | 8.06 s       | —        |
| Message with many attachments            | 800 attachments            | 5 ms   | —            | —        |
| Large attachment opened                  | 80 MiB                     | 193 ms | 157 ms       | +83 MiB  |

## Findings

- ![Within budget][within-budget] Sync, conversations, bulk flag changes and
  attachments grow about linearly with their size.
- ![Limitation][limitation] **Removals grow with the square of the size.** Each
  removed message renumbers every message after it. One `EXPUNGE` at a time
  keeps the window responding, because each response is a separate pass, but
  takes about 14 s for 40,000 of 200,000 messages. One `VANISHED` range is
  handled in a single pass, so the window stops responding for its whole
  duration: about 0.5 s at 10,000 of 50,000 and 8 s at 40,000 of 200,000.
- ![Limitation][limitation] **Quick filters re-filter the whole list in one
  pass**, which blocks the window for about 1.2 s at 200,000 messages.
- ![Limitation][limitation] **An opened attachment stays in memory** at about
  2.3 times its decoded size while it is open.

Details, workarounds and proposed improvements are in the
[performance caveat](CAVEATS.md#performance-and-resource-use).

## Budgets

With `IMPRESARIO_WORKLOAD_BUDGETS=1`, each workload must finish within its time
budget, and no single event-loop pass may take longer than 250 ms. The
exception is the `VANISHED` workload, which is allowed 1,500 ms per pass
because of the limitation above; lower it when removals are batched.

| Workload              | Time budget | Pass budget  |
| --------------------- | ----------- | ------------ |
| `sync-large-folder`   | 3,000 ms    | not measured |
| `filter-large-folder` | 1,500 ms    | 250 ms       |
| `long-conversation`   | 500 ms      | 250 ms       |
| `bulk-mark-read`      | 1,000 ms    | 250 ms       |
| `bulk-expunge`        | 8,000 ms    | 250 ms       |
| `bulk-vanished`       | 3,000 ms    | 1,500 ms     |
| `many-attachments`    | 500 ms      | 250 ms       |
| `large-attachment`    | 2,000 ms    | 250 ms       |

The budgets are several times the measured values so that slower machines and
loaded CI runners pass; they catch large regressions, not small ones.

## Not measured yet

| Area                                 | Status                        | Plan                                                                                                               |
| ------------------------------------ | ----------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| A live server over the network       | ![Not measured][not-measured] | Repeat against the disposable Dovecot server ([#52](https://github.com/mikepadiernos/plasma-impresario/issues/52)) |
| A Release build                      | ![Not measured][not-measured] | Repeat the same workloads in a Release build                                                                       |
| Painting the message list and reader | ![Not measured][not-measured] | Measure with the GUI drawing, in a Plasma session                                                                  |
| Startup time                         | ![Not measured][not-measured] | Planned in M14 ([#145](https://github.com/mikepadiernos/plasma-impresario/issues/145))                             |

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[not-measured]: https://img.shields.io/badge/Not_measured-6e7781
[within-budget]: https://img.shields.io/badge/Within_budget-1a7f37
