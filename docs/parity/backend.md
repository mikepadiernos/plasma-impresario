# IMAP sync and account operations

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Caveats](../CAVEATS.md)** · **[UI parity](ui.md)**

This page records the backend reference patterns that Plasma Impresario aims for
and compares them with the current backend, as of 2026-11-25. Trojitá is the
primary reference for protocol behavior; Kontact is a secondary reference for
desktop integration and account ergonomics.

> [!NOTE]
>
> Parity guides are reference patterns, not a guaranteed description of the
> current implementation. The code in this checkout is a Trojitá-based IMAP
> client: only the [current baseline](#current-baseline) tables describe
> implemented behavior, and the parity expectations further down are targets,
> not claims that the reworked product already provides them.

Legend: ![Done][done] · ![Needs verification][needs-verification] ·
![Planned][planned] · ![Limitation][limitation]

## Contents

- [Current baseline](#current-baseline)
- [Reference hierarchy](#reference-hierarchy)
- [Core implementation standards](#core-implementation-standards)
- [Account and connection behavior](#account-and-connection-behavior)
- [Sync and retrieval expectations](#sync-and-retrieval-expectations)
- [Search, metadata and performance](#search-metadata-and-performance)
- [Parity checklist](#parity-checklist)

## Current baseline

The IMAP parser, tasks and models live in `src/Imap/` (`Parser/`, `Tasks/`,
`Model/`), separate from the widgets in `src/Gui/`. The server remains the
canonical mailbox.

| Area                          | Status                                    | Current behavior                                                                                                                                                                                                                                                                                                                                                                                                                                                    | Source                                                                               |
| ----------------------------- | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| Synchronization               | ![Done][done]                             | Incremental; message metadata and MIME structure are separate from lazily fetched parts                                                                                                                                                                                                                                                                                                                                                                             | `src/Imap/Tasks/`, `src/Imap/Model/`                                                 |
| Caching                       | ![Done][done]                             | Memory, SQL and on-disk part caches retain metadata and fetched parts, with configurable offline cache behavior                                                                                                                                                                                                                                                                                                                                                     | `MemoryCache`, `SQLCache`, `DiskPartCache`                                           |
| IMAP features                 | ![Done][done]                             | IDLE, threading, server-side search, offline mode, reconnect handling and mailbox operations have implementation and/or QtTest coverage                                                                                                                                                                                                                                                                                                                             | [`tests/Imap/`](../../tests/Imap/)                                                   |
| Multiple accounts             | ![Done][done]                             | `AccountRegistry` gives each account its own settings; each account has its own `ImapAccess`, models and connection-status tracking                                                                                                                                                                                                                                                                                                                                 | `src/Common/AccountRegistry.cpp`, `src/Common/AccountStatus.cpp`                     |
| Profiles                      | ![Done][done]                             | `--profile` selects a separate configuration, each with its own accounts                                                                                                                                                                                                                                                                                                                                                                                            | `src/Gui/main.cpp`                                                                   |
| Account capabilities          | ![Done][done]                             | `AccountCapabilities` describes which operations IMAP, POP3 and JMAP accounts support; `CapabilityActionGate` hides actions the active account cannot perform ([#215](https://github.com/mikepadiernos/plasma-impresario/issues/215))                                                                                                                                                                                                                               | `src/Imap/Model/AccountCapabilities.cpp`                                             |
| POP3 accounts                 | ![Needs verification][needs-verification] | `Pop3::FetchJob` downloads into a `LocalMail::Store`, which the mail views read through an in-process IMAP session; `Gui::Pop3Fetcher` checks on a timer. Tested against a scripted server and Dovecot ([#216](https://github.com/mikepadiernos/plasma-impresario/issues/216), [caveat](../CAVEATS.md#pop3-accounts))                                                                                                                                               | `src/Pop3/FetchJob.cpp`, `src/LocalMail/`, `src/Gui/Pop3Fetcher.cpp`                 |
| Virus and spam scanning hooks | ![Done][done]                             | An optional ClamAV scan through a local `clamd` socket before **Open**, and junk handling from IMAP keywords; merged with M06.06 ([#44](https://github.com/mikepadiernos/plasma-impresario/issues/44)); see [caveats](../CAVEATS.md)                                                                                                                                                                                                                                | `src/UiUtils/ClamdScanner.cpp`, `src/Gui/MailSafetyPrompt.cpp`                       |
| Outgoing mail                 | ![Done][done]                             | SMTP, IMAP submission or sendmail                                                                                                                                                                                                                                                                                                                                                                                                                                   | `src/MSA/`                                                                           |
| Connection methods            | ![Done][done]                             | Cleartext, STARTTLS, TLS, or a process-backed connection that wraps a user-configured transport                                                                                                                                                                                                                                                                                                                                                                     | `src/Common/ConnectionMethod.h`                                                      |
| Credentials                   | ![Limitation][limitation]                 | Password plugins: QtKeychain (default for new profiles, optional at build time) or cleartext in `QSettings`                                                                                                                                                                                                                                                                                                                                                         | `src/Plugins/QtKeyChain/`, `src/Plugins/ClearTextPassword/`                          |
| OAuth sign-in                 | ![Needs verification][needs-verification] | XOAUTH2 and OAUTHBEARER for IMAP and SMTP with Google and Microsoft ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)); see [OAuth caveats](../CAVEATS.md#oauth-sign-in)                                                                                                                                                                                                                                                                       | `src/OAuth/`                                                                         |
| Live IMAP tests               | ![Done][done]                             | Opt-in GUI tests (`test_LiveImap`, `test_LiveImapAccounts`, `test_LiveImapPanels`, `test_LiveImapCompose`) run against a disposable, loopback-only Dovecot 2.4 with synthetic users in the development container. They cover account setup and restore, multiple accounts, embedded panels, and reading through composing and failed-send recovery. They are built but not registered with CTest; see [live verification](../TODO.md#live-verification-environment) | `tests/Gui/test_LiveImap*.cpp`, [`tools/dev-container/`](../../tools/dev-container/) |

Not yet implemented:

| Area                                | Status                                    | Tracking                                                                                                                                                                                                                                                                                                                                                                             |
| ----------------------------------- | ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| JMAP                                | ![Needs verification][needs-verification] | M06.05 in [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support) ([#217](https://github.com/mikepadiernos/plasma-impresario/issues/217)); `Jmap::SyncJob` synchronizes a `LocalMail::Store` both ways and `Jmap::Submission` sends, driven by `Gui::JmapSyncer`. Tested against an in-memory server and Stalwart; see [Mail providers](../integrations/providers.md#jmap) |
| Outlook, Microsoft 365 and Exchange | ![Planned][planned]                       | [M15](../ROADMAP.md#m15-microsoft-accounts-and-exchange) ([#388](https://github.com/mikepadiernos/plasma-impresario/issues/388)); see the [Microsoft guide](../integrations/microsoft.md)                                                                                                                                                                                            |
| Rules engine                        | ![Planned][planned]                       | [M07](../ROADMAP.md#m07-output-notifications-and-rules-automation)                                                                                                                                                                                                                                                                                                                   |

Transport, credential and cache limitations are documented in
[caveats and security](../CAVEATS.md#security-and-data-protection); account
limitations are in [multiple accounts](../CAVEATS.md#multiple-accounts).

## Reference hierarchy

### Primary: Trojitá

Trojitá is the authoritative baseline for IMAP correctness and the desktop-mail
data model because it is built around protocol fidelity, local mail operations
and efficient communication with remote mail stores.

Parity expectations:

- IMAP account handling is explicit, reliable and easy to reason about.
- Folder discovery and mailbox enumeration work across common server
  implementations.
- Sync favors correctness and incremental updates over aggressive
  re-downloading.
- Message retrieval and state updates preserve server semantics such as flags,
  threading context and folder state.
- Mail operations stay stable with large accounts and imperfect network
  conditions.

### Secondary: Kontact suite

The Kontact suite is the KDE desktop reference for account integration and
user-visible mail behavior around the client core.

Parity expectations:

- Account setup and state feel native to a KDE desktop.
- The backend supports desktop-style account management and system-level
  integration without compromising IMAP reliability.
- Error states and recovery paths are understandable to users working in a
  broader KDE workflow.
- Mail operations meet the expectations of a mature personal information
  manager, even though the backend stays intentionally focused.

## Core implementation standards

- Prefer explicit IMAP state handling over implicit assumptions about server
  behavior.
- Design sync as incremental and resumable, with clear handling for reconnects
  and transient failures.
- Track message metadata and folder status so it stays stable across partial
  updates.
- Treat search, indexing and folder scans as performance-sensitive features that
  must not slow normal mail flow.
- Keep failure states actionable: connection errors, authentication failures and
  retry windows are visible and explainable.

## Account and connection behavior

- Support standard IMAP account setup with secure transport defaults and clear
  encryption expectations.
- Handle authentication failures distinctly from server availability problems.
- Preserve mailbox structure and synchronization state across reconnects.
- Separate temporary transport issues from permanent account configuration
  problems.
- Retry with sensible backoff without creating duplicate or conflicting mail
  operations.

## Sync and retrieval expectations

- Update folders incrementally rather than re-fetching the entire mailbox.
- Fetch efficiently and avoid full-body downloads for summary views.
- Reconcile flag changes, folder moves and deletion state carefully with server
  state.
- Keep large mail stores usable through bounded fetches, selective updates and
  graceful degradation.

## Search, metadata and performance

- Keep folder metadata trustworthy during partial sync and reconnects.
- Support fast local discovery through search indexing without sacrificing
  correctness.
- Avoid pathological behavior in large-folder operations and keep UI feedback
  responsive.
- Degrade gracefully under latency, server lag or throttling.

## Parity checklist

| Item                                        | Status                                    | Current state                                                                                                                                                                                                                                                                           |
| ------------------------------------------- | ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Confirm supported authentication paths      | ![Needs verification][needs-verification] | Password and OAuth sign-in work in tests ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)); live provider sign-in is pending in [M14](../ROADMAP.md#m14-real-plasma-session-verification) ([#317](https://github.com/mikepadiernos/plasma-impresario/issues/317)) |
| Document the account setup flow             | ![Needs verification][needs-verification] | Account setup ([#78](https://github.com/mikepadiernos/plasma-impresario/issues/78)) merged in M02 and is exercised against a disposable Dovecot by `test_LiveImap` and `test_LiveImapAccounts`; a written setup guide and real-provider checks are pending                              |
| Capture connection and retry failure states | ![Done][done]                             | The footer and account selector report connection, offline and authentication states ([#79](https://github.com/mikepadiernos/plasma-impresario/issues/79))                                                                                                                              |
| Define mailbox synchronization expectations | ![Planned][planned]                       | Planned with reliability work in [M10](../ROADMAP.md#m10-reliability-sync-and-notification-quality)                                                                                                                                                                                     |
| Assess folder discovery and metadata        | ![Planned][planned]                       | Not yet assessed across servers                                                                                                                                                                                                                                                         |
| Review large-folder performance             | ![Planned][planned]                       | No large-mailbox benchmarks are recorded                                                                                                                                                                                                                                                |
| Respect KDE-native account integration      | ![Planned][planned]                       | Account settings use `QSettings`; see the [KDE Plasma integration guide](../integrations/kde-plasma.md)                                                                                                                                                                                 |

[done]: https://img.shields.io/badge/Done-1a7f37
[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
