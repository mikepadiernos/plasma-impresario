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
> These are measurements in a Debug configuration, which this project compiles
> with `-O2` and assertions on, in a Linux container: of the IMAP model against
> fake servers, and of the whole window against a local Dovecot server. They are
> not measurements of a remote server, a Release build or a Plasma session. Use
> them to compare changes and to spot growth, not as user-facing timings.

## Contents

- [How to run](#how-to-run)
- [Environment](#environment)
- [Results](#results)
- [Four times the size](#four-times-the-size)
- [Live server](#live-server)
- [Search index](#search-index)
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
| Build   | Debug configuration, compiled with `-O2 -g` and assertions on; offscreen   |
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

## Live server

The live workloads drive the whole main window, offscreen, against a disposable
Dovecot server in the development container, recorded on 2026-11-27 for
[#52](https://github.com/mikepadiernos/plasma-impresario/issues/52). The
script seeds an INBOX of 50,000 messages, every tenth one unread, a 500-message
reply chain and messages with many attachments and with one large attachment,
lets the server index them, and deletes the server and its data when it exits.

```bash
tools/dev-container/run.sh sh tools/dev-container/live-imap-workloads.sh build
```

| Variable                            | Default | Effect                                                    |
| ----------------------------------- | ------- | --------------------------------------------------------- |
| `PLASMA_TEST_WORKLOAD_MESSAGES`     | 50000   | Messages in INBOX                                         |
| `PLASMA_TEST_WORKLOAD_CONVERSATION` | 500     | Messages in the reply chain                               |
| `PLASMA_TEST_WORKLOAD_ATTACHMENTS`  | 200     | Attachments in one message                                |
| `PLASMA_TEST_WORKLOAD_MIB`          | 20      | Size of the large attachment in MiB                       |
| `PLASMA_TEST_WORKLOAD_REPORT`       | unset   | Appends one tab-separated line per step to the named file |

The server is Dovecot 2.4.5 with QRESYNC on the same machine, so the times
include the server but no network latency. The removal step deletes a fifth of
INBOX through `doveadm` while the window is open, as another client would; the
server then reports it as `VANISHED` ranges. The test itself is
`tests/Gui/test_LiveImapWorkloads.cpp`; it is built with the other tests but is
not part of `ctest`.

The first profile showed three costs that the fake-server workloads could not
see, because they do not include the window:

- Every changed message updated every message action at once, and the Delete
  action set its icon again each time, which repainted every button showing
  it. The window now updates its actions once after each burst of changes, and
  only changes the Delete action when its mode changes.
- The message list restored its scroll position after each run of inserted or
  removed rows, which laid out the whole list again each time. It now restores
  the position once, before the list is painted again.

Median of three runs, before and after these changes:

| Step                            | Size                      | Before: time | Before: longest pass | After: time | After: longest pass | Memory after |
| ------------------------------- | ------------------------- | ------------ | -------------------- | ----------- | ------------------- | ------------ |
| Open the folder                 | 50,000 messages           | ~56 s        | ~12.6 s              | ~0.57 s     | ~75 ms              | +20 MiB      |
| Unread quick filter             | 50,000 messages           | ~16 s        | ~16 s                | ~97 ms      | ~97 ms              | —            |
| Mark the folder read            | 5,000 unread messages     | ~7.4 s       | ~6.8 s               | ~171 ms     | ~29 ms              | —            |
| Another client removes messages | 10,000 of 50,000 messages | ~22 s        | ~17 s                | ~6.2 s      | ~463 ms             | —            |
| Long conversation in threads    | 500 messages              | ~2.4 s       | ~0.44 s              | ~77 ms      | ~24 ms              | —            |
| Message with many attachments   | 200 attachments           | ~35 ms       | —                    | ~32 ms      | ~32 ms              | +2.5 MiB     |
| Large attachment opened         | 20 MiB                    | ~480 ms      | ~113 ms              | ~494 ms     | ~117 ms             | +7 MiB       |

The server's own removal through `doveadm` takes about 0.2 s, so the removal
step is almost entirely the window applying it. The whole process uses about
215 MiB after opening the folder. At 5,000 messages the folder opened in 0.8 s
before the changes, so the old costs grew with the square of the folder size.

## Search index

The offline search index has its own workload, `testWorkload` in
`tests/Misc/test_SearchIndex.cpp`, recorded on 2026-11-28 for
[#357](https://github.com/mikepadiernos/plasma-impresario/issues/357). It
fills a persistent cache with 2,000 messages, times `IMPRESARIO_WORKLOAD_SCALE`,
a quarter of them without downloaded text, builds the index the way the
application does, without the pause between batches, and then searches,
updates and checks it. It reads the same variables as the IMAP workloads.

```bash
cmake --build build --target test_SearchIndex
cd build
IMPRESARIO_WORKLOAD_SCALE=25 IMPRESARIO_WORKLOAD_BUDGETS=1 QT_QPA_PLATFORM=offscreen ./test_SearchIndex testWorkload
```

One run in the [same environment](#environment), with the cache in a
temporary folder:

| Step                                       | 2,000 messages | 50,000 messages | Longest pass at 50,000 | Status                          |
| ------------------------------------------ | -------------- | --------------- | ---------------------- | ------------------------------- |
| Build the index                            | 122 ms         | 3.4 s           | 49 ms                  | ![Within budget][within-budget] |
| Disk use                                   | 784 B/message  | 811 B/message   | —                      | ![Within budget][within-budget] |
| Search a subject word                      | 0.2 ms         | 3.9 ms          | —                      | ![Within budget][within-budget] |
| Search a sender                            | 0.2 ms         | 3.2 ms          | —                      | ![Within budget][within-budget] |
| Search a word in 75% of messages           | 0.8 ms         | 22 ms           | —                      | ![Within budget][within-budget] |
| Search a rare word                         | 0.2 ms         | 5.8 ms          | —                      | ![Within budget][within-budget] |
| Search a flag (`UNSEEN`)                   | 0.2 ms         | 5.8 ms          | —                      | ![Within budget][within-budget] |
| Search text, a date and a flag             | 0.3 ms         | 9.2 ms          | —                      | ![Within budget][within-budget] |
| Update, nothing changed, with CONDSTORE    | under 1 ms     | 1 ms            | under 1 ms             | ![Within budget][within-budget] |
| Update, nothing changed, without CONDSTORE | 5 ms           | 156 ms          | 28 ms                  | ![Within budget][within-budget] |
| Quick check, run when the index opens      | under 1 ms     | 6 ms            | 6 ms                   | ![Within budget][within-budget] |
| Thorough check (FTS5 `integrity-check`)    | 8 ms           | 210 ms          | 210 ms                 | ![Limitation][limitation]       |

Searches are the median of five runs. Disk use is what SQLite's `dbstat`
table reports for the index tables; a library built without it reports -1.
With CONDSTORE, an update skips each folder whose HIGHESTMODSEQ, UIDNEXT and
message count are unchanged and which the model did not report as changed;
without it, every message's flags are compared again. The thorough check reads
the whole full-text index in one pass, so it is not run on its own; the quick
check, which compares the message and text counts, runs whenever the index is
opened.

The budgets, enforced with `IMPRESARIO_WORKLOAD_BUDGETS=1`, are 250 ms per
event-loop pass while building or updating, 500 ms per search, a build of
5,000 ms or 0.2 ms per message, whichever is larger, and an unchanged update
with CONDSTORE of 100 ms.

## Findings

- ![Within budget][within-budget] Sync, conversations, bulk flag changes and
  attachments grow about linearly with their size.
- ![Within budget][within-budget] The [search index](#search-index) grows
  about linearly, at about 0.8 KB per message, and keeps each event-loop pass
  under 50 ms while it builds. Without CONDSTORE, every update compares every
  cached message's flags again, about 156 ms per 50,000 messages.
- ![Limitation][limitation] **Removals grow with the square of the size.** Each
  removed message renumbers every message after it. One `EXPUNGE` at a time
  keeps the window responding, because each response is a separate pass, but
  takes about 14 s for 40,000 of 200,000 messages. One `VANISHED` range is
  handled in a single pass, so the window stops responding for its whole
  duration: about 0.5 s at 10,000 of 50,000 and 8 s at 40,000 of 200,000.
  Against the [live server](#live-server), another client removing 10,000 of
  50,000 messages takes about 6 s, and the window stops responding for up to
  about 0.5 s at a time.
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

| Area                                 | Status                        | Plan                                                                                          |
| ------------------------------------ | ----------------------------- | --------------------------------------------------------------------------------------------- |
| Network latency                      | ![Not measured][not-measured] | Repeat the [live workloads](#live-server) with added latency, or against a remote test server |
| A Release build                      | ![Not measured][not-measured] | Repeat the same workloads in a Release build                                                  |
| Painting the message list and reader | ![Not measured][not-measured] | Measure with the GUI drawing, in a Plasma session                                             |
| Startup time                         | ![Not measured][not-measured] | Planned in M14 ([#145](https://github.com/mikepadiernos/plasma-impresario/issues/145))        |

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[not-measured]: https://img.shields.io/badge/Not_measured-6e7781
[within-budget]: https://img.shields.io/badge/Within_budget-1a7f37
