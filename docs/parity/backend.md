# IMAP sync and account operations

This page defines the backend parity target for Plasma Mail. The primary reference is Trojitá, with the Kontact suite acting as a secondary reference for desktop integration, account ergonomics, and KDE ecosystem behavior.

## Reference hierarchy

### Primary: Trojitá

Trojitá is the authoritative baseline for IMAP correctness and the desktop-mail data model because it is built around protocol fidelity, local mail operations, and efficient communication with remote mail stores.

Parity expectations:

- IMAP account handling should be explicit, reliable, and easy to reason about.
- Folder discovery and mailbox enumeration should be robust across common server implementations.
- Sync behavior should favor correctness and incremental updates over aggressive re-downloading.
- Message retrieval and state updates should preserve server semantics such as flags, threading context, and folder state.
- Mail operations should remain stable under large account sizes and imperfect network conditions.

### Secondary: Kontact suite

The Kontact suite provides a KDE desktop reference for account ecosystem integration and user-visible mail behavior around the client core.

Parity expectations:

- Account setup and state should feel native to a KDE desktop environment.
- The backend should support desktop-style account management and system-level integration without compromising IMAP reliability.
- Error states and recovery paths should be understandable to users working inside a broader KDE workflow.
- Mail operations should align with expectations for a mature personal information manager, even when the backend logic remains intentionally focused.

## Core implementation standards

- Prefer explicit IMAP state handling over implicit assumptions about server behavior.
- Design sync as incremental and resumable, with clear handling for reconnects and transient failures.
- Track message metadata and folder status in a way that remains stable across partial updates.
- Treat search, indexing, and folder scans as performance-sensitive features that must not degrade normal mail flow.
- Keep failure states actionable: connection errors, auth failures, and retry windows should be visible and explainable.

## Account and connection behavior

- Support standard IMAP account setup with secure transport defaults and clear encryption expectations.
- Handle authentication failures distinctly from server availability problems.
- Preserve mailbox structure and synchronization state across reconnects.
- Separate temporary transport issues from permanent account configuration problems.
- Provide sensible retry and backoff behavior without creating duplicate or conflicting mail operations.

## Sync and retrieval expectations

- Synchronization should update folders incrementally rather than re-fetching the entire mailbox needlessly.
- Message fetches should be efficient and avoid unnecessary full-body downloads for summary views.
- Flag changes, folder movement, and deletion state should be reconciled carefully with server state.
- Large mail stores should remain usable through bounded fetches, selective updates, and graceful degradation.

## Search, metadata, and performance

- Folder metadata should remain trustworthy even under partial sync or reconnect scenarios.
- Search indexing should support fast local discovery without sacrificing correctness.
- Large-folder operations should avoid pathological behavior and maintain responsive UI feedback.
- Network and mailbox operations should degrade gracefully under latency, server lag, or throttling.

## Parity checklist

- Confirm supported authentication paths
- Document account setup flow
- Capture connection and retry failure states
- Define mailbox synchronization expectations
- Assess folder discovery and metadata consistency
- Review large-folder performance behavior
- Ensure KDE-native account integration patterns are respected
