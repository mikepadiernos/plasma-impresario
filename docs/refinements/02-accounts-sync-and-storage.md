# Accounts, sync, and storage refinement

**[Documentation](../README.md)** · **[Roadmap: M14](../ROADMAP.md#m14-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Shell and interaction](01-shell-and-interaction.md)** ·
**[Next: Mail organization and triage](03-mail-organization-and-triage.md)**

Refines account setup, authentication, provider compatibility, folder sync,
offline recovery and storage feedback. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started ·
![Merged][merged] merged into `main`

| Roadmap subsection                                                   | Issue                                                               | Board               |
| -------------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M14.03: Accounts, sync, and storage](../ROADMAP.md#m14-refinements) | [#22](https://github.com/mikepadiernos/plasma-impresario/issues/22) | ![Backlog][backlog] |

## Current state

Multiple accounts were built in M02: a toolbar account switcher, a header per
account in the folder sidebar, and an account menu on each header with
Account Settings, Rename, Sign In for OAuth accounts, and Remove. The
limitations once recorded in
[Caveats: multiple accounts](../CAVEATS.md#multiple-accounts)
([#262](https://github.com/mikepadiernos/plasma-impresario/issues/262),
![Merged][merged]) are resolved and the caveat issue is closed. A real Plasma
session has not been verified.

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                                        | Issue                                                                 |
| ----------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Refine account setup, authentication, provider compatibility, folder sync, offline recovery, and storage feedback | [#158](https://github.com/mikepadiernos/plasma-impresario/issues/158) |
| Clarify account setup errors, authentication renewal, certificate failures, and provider-specific limitations     | [#159](https://github.com/mikepadiernos/plasma-impresario/issues/159) |
| Show per-account sync progress, last successful sync, offline state, and actionable retry options                 | [#160](https://github.com/mikepadiernos/plasma-impresario/issues/160) |
| Preserve local changes during interruptions and make conflicts or unsent changes visible before recovery          | [#161](https://github.com/mikepadiernos/plasma-impresario/issues/161) |
| Explain local cache usage and allow users to manage storage without deleting server messages unintentionally      | [#162](https://github.com/mikepadiernos/plasma-impresario/issues/162) |
| Check new or changed account settings by signing in to IMAP and SMTP before saving, and explain failures by stage | [#382](https://github.com/mikepadiernos/plasma-impresario/issues/382) |
| Keep an account's previous OAuth grant until Account Settings is saved                                            | [#428](https://github.com/mikepadiernos/plasma-impresario/issues/428) |
| Skip revocation when signing out in Account Settings if another account shares the OAuth grant                    | [#429](https://github.com/mikepadiernos/plasma-impresario/issues/429) |

## Acceptance criteria

- Account errors are understandable and recoverable.
- Sync state remains visible and actionable during reconnect, retry, and offline
  transitions.
- Local changes survive interruptions without silent loss or unsupported
  rollback.
- Storage and cache behavior remain explainable to users managing large
  accounts.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
[merged]: https://img.shields.io/badge/Merged-8250df
