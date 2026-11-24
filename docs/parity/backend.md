# IMAP sync and account operations

**[Documentation](../README.md)** · **[UI parity](ui.md)** ·
**[Roadmap](../ROADMAP.md)** · **[Caveats](../CAVEATS.md)**

This page compares the current backend with the parity targets for Plasma
Impresario. Trojitá is the primary reference for protocol behavior; Kontact is a
secondary reference for desktop integration and account ergonomics.

Status: ✅ done or verified · 🧪 implemented, needs verification · 📋 planned ·
⚠️ limitation

> [!NOTE]
>
> The code in this checkout is a Trojitá-based IMAP client. **Current baseline**
> describes implemented behavior; **parity expectations** are targets, not
> claims that the reworked product already provides them.

## Contents

- [Current baseline](#current-baseline)
- [Reference hierarchy](#reference-hierarchy)
- [Core implementation standards](#core-implementation-standards)
- [Account and connection behavior](#account-and-connection-behavior)
- [Sync and retrieval expectations](#sync-and-retrieval-expectations)
- [Search, metadata, and performance](#search-metadata-and-performance)
- [Parity checklist](#parity-checklist)

## Current baseline

The IMAP parser, tasks and models live in `src/Imap/` (`Parser/`, `Tasks/`,
`Model/`), separate from the widgets in `src/Gui/`. The server remains the
canonical mailbox.

| Area                    | Status | Current behavior                                                                                                                                             | Source                                                                               |
| ----------------------- | ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------ |
| 🔄 Synchronization      | ✅     | Incremental; message metadata and MIME structure are separate from lazily fetched parts                                                                      | `src/Imap/Tasks/`, `src/Imap/Model/`                                                 |
| 💾 Caching              | ✅     | Memory, SQL and on-disk part caches retain metadata and fetched parts, with configurable offline cache behavior                                              | `MemoryCache`, `SQLCache`, `DiskPartCache`                                           |
| 📡 IMAP features        | ✅     | IDLE, threading, server-side search, offline mode, reconnect handling and mailbox operations have implementation and/or QtTest coverage                      | [`tests/Imap/`](../../tests/Imap/)                                                   |
| 📬 Multiple accounts    | ✅     | `AccountRegistry` gives each account its own settings; each account has its own `ImapAccess`, models and connection-status tracking                          | `src/Common/AccountRegistry.cpp`, `src/Common/AccountStatus.cpp`                     |
| 👤 Profiles             | ✅     | `--profile` selects a separate configuration, each with its own accounts                                                                                     | `src/Gui/main.cpp`                                                                   |
| 🧩 Account capabilities | 🧪     | `AccountCapabilities` describes which operations IMAP, POP3 and JMAP accounts support; only its unit test uses it, and the interface does not consult it yet | `src/Imap/Model/AccountCapabilities.cpp`                                             |
| 📤 Outgoing mail        | ✅     | SMTP, IMAP submission or sendmail                                                                                                                            | `src/MSA/`                                                                           |
| 🔌 Connection methods   | ✅     | Cleartext, STARTTLS, TLS, or a process-backed connection that wraps a user-configured transport                                                              | `src/Common/ConnectionMethod.h`                                                      |
| 🔑 Credentials          | ⚠️     | Password plugins: QtKeychain (default for new profiles, optional at build time) or cleartext in `QSettings`                                                  | `src/Plugins/QtKeyChain/`, `src/Plugins/ClearTextPassword/`                          |
| 🧪 Live IMAP tests      | ✅     | Opt-in GUI tests run against a synthetic Dovecot for accounts and panels                                                                                     | `tests/Gui/test_LiveImap*.cpp`, [`tools/dev-container/`](../../tools/dev-container/) |

Not yet implemented:

| Area                                    | Status | Tracking                                                                                |
| --------------------------------------- | ------ | --------------------------------------------------------------------------------------- |
| 🧩 Protocol-aware account model in use  | 📋     | M02 [#215](https://github.com/mikepadiernos/plasma-impresario/issues/215) (Development) |
| 🔐 OAuth authentication                 | 📋     | M02 [#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)               |
| 📮 POP3, JMAP, Gmail, Outlook, Exchange | 📋     | M05.05 in [M05](../ROADMAP.md#m05-desktop-integration-and-provider-support)             |
| 🛡️ Virus and spam scanning hooks        | 📋     | M05.06 in [M05](../ROADMAP.md#m05-desktop-integration-and-provider-support)             |
| ⚙️ Rules engine                         | 📋     | [M06](../ROADMAP.md#m06-output-notifications-and-rules-automation)                      |

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

## Search, metadata, and performance

- Keep folder metadata trustworthy during partial sync and reconnects.
- Support fast local discovery through search indexing without sacrificing
  correctness.
- Avoid pathological behavior in large-folder operations and keep UI feedback
  responsive.
- Degrade gracefully under latency, server lag or throttling.

## Parity checklist

| Item                                           | Status | Current state                                                                                                                                              |
| ---------------------------------------------- | ------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🔐 Confirm supported authentication paths      | 📋     | Password authentication works; OAuth is planned ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218))                                    |
| 📝 Document the account setup flow             | 🧪     | Exercised against a disposable Dovecot; setup work continues in M02 [#78](https://github.com/mikepadiernos/plasma-impresario/issues/78)                    |
| 📶 Capture connection and retry failure states | ✅     | The footer and account selector report connection, offline and authentication states ([#79](https://github.com/mikepadiernos/plasma-impresario/issues/79)) |
| 🔄 Define mailbox synchronization expectations | 📋     | Planned with reliability work in [M09](../ROADMAP.md#m09-reliability-sync-and-notification-quality)                                                        |
| 🗂️ Assess folder discovery and metadata        | 📋     | Not yet assessed across servers                                                                                                                            |
| 📈 Review large-folder performance             | 📋     | No large-mailbox benchmarks are recorded                                                                                                                   |
| 🖥️ Respect KDE-native account integration      | 📋     | Account settings use `QSettings`; see the [KDE Plasma integration guide](../integrations/kde-plasma.md)                                                    |
