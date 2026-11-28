# Release readiness

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Caveats](CAVEATS.md)** · **[Installation](INSTALL.md)** ·
**[Security](../SECURITY.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

The [M11](ROADMAP.md#m11-release-readiness-and-polish) criteria a Plasma
Impresario release has to meet, the known issues its notes must state, and how
to hand the work on
([#54](https://github.com/mikepadiernos/plasma-impresario/issues/54)). This
page records the state on 2026-11-28.

Legend: ![Met][met] · ![Pending][pending] · ![Not met][not-met] ·
![Blocker][blocker] · ![Known issue][known-issue] ·
![Session check][session-check]

> [!IMPORTANT]
>
> Plasma Impresario is **not ready to ship**. No open bug or caveat blocks a
> release, but two criteria are still open: the renderer sandbox on Ubuntu
> and use in a real Plasma session. See the
> [decision](#release-decision).

## Contents

- [Release decision](#release-decision)
- [Release criteria](#release-criteria)
- [Known issues](#known-issues)
- [Triage](#triage)
- [Handoff](#handoff)
  - [Where the work is tracked](#where-the-work-is-tracked)
  - [Cutting a release](#cutting-a-release)

## Release decision

| Question                                 | Answer                                                                                                                                                                                                                                     |
| ---------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Can a release be made today?             | No.                                                                                                                                                                                                                                        |
| What is missing?                         | [R7](#release-criteria) a real Plasma session ([M15](ROADMAP.md#m15-real-plasma-session-verification)) and [R8](#release-criteria) the renderer sandbox on Ubuntu ([#512](https://github.com/mikepadiernos/plasma-impresario/issues/512)). |
| Do open bugs or caveats block a release? | No. There are no open bugs, and every open caveat is a documented known issue.                                                                                                                                                             |
| What do users have to be told?           | The [known issues](#known-issues), in the release notes.                                                                                                                                                                                   |

## Release criteria

A release needs every criterion met. Evidence from offscreen tests and
disposable mail servers counts for R1 to R6; R7 asks for a real session.

| ID     | Criterion                                                                                                                                                                                                                             | Evidence                                                                                                                                                                                                                                                                             | Status              |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------- |
| **R1** | Data safety: no known way to lose or corrupt mail on the server or on disk without a warning or an explicit choice by the user.                                                                                                       | Live server tests, the persistent outbox and its **Interrupted** state, the draft warning about attachments, and the [triage](#triage)                                                                                                                                               | ![Met][met]         |
| **R2** | Secure defaults: encrypted connections are offered, new profiles store passwords through QtKeychain, remote content and scripts are blocked, and signature results tell invalid, unknown-key and failed checks apart from valid ones. | [Security summary](CAVEATS.md#security-and-data-protection) and the [security policy](../SECURITY.md) with private reporting                                                                                                                                                         | ![Met][met]         |
| **R3** | Core workflows pass end to end: account setup, reading, triage and reply; compose, send, attachments, search, rules, notifications and offline recovery.                                                                              | [End-to-end coverage](e2e-coverage.md) and its [desktop scenarios](e2e-coverage.md#desktop-scenarios) ([#133](https://github.com/mikepadiernos/plasma-impresario/issues/133), [#134](https://github.com/mikepadiernos/plasma-impresario/issues/134))                                 | ![Met][met]         |
| **R4** | Accessibility: every control has an accessible name, the core workflows work from the keyboard, and user-visible strings are marked for translation.                                                                                  | [Accessibility audit](accessibility.md) ([#53](https://github.com/mikepadiernos/plasma-impresario/issues/53), [#135](https://github.com/mikepadiernos/plasma-impresario/issues/135))                                                                                                 | ![Met][met]         |
| **R5** | Quality gate: clean formatting, a reproducible build, the full test suite passing, a coverage report, recorded dependency versions and static analysis without unresolved findings.                                                   | The [release gate](release-gate.md) passed with every check; its residual risk is [#511](https://github.com/mikepadiernos/plasma-impresario/issues/511)                                                                                                                              | ![Met][met]         |
| **R6** | Documentation: installation, security and supported platforms are current, and every known limitation has a caveat with a linked issue and recommendation.                                                                            | [Installation](INSTALL.md), [security policy](../SECURITY.md), [caveats](CAVEATS.md) and the [triage](#triage)                                                                                                                                                                       | ![Met][met]         |
| **R7** | Plasma session: the core workflows and the desktop integration work in a real KDE Plasma 6 Wayland session and with at least one real provider.                                                                                       | [M15](ROADMAP.md#m15-real-plasma-session-verification) ([#319](https://github.com/mikepadiernos/plasma-impresario/issues/319))                                                                                                                                                       | ![Not met][not-met] |
| **R8** | Renderer sandbox: the QtWebEngine sandbox stays on for every supported distribution, including Ubuntu 24.04 and later.                                                                                                                | The installer adds an [AppArmor profile](INSTALL.md#renderer-sandbox-on-ubuntu) instead of disabling the sandbox ([#381](https://github.com/mikepadiernos/plasma-impresario/issues/381)); a check on Ubuntu is [#512](https://github.com/mikepadiernos/plasma-impresario/issues/512) | ![Pending][pending] |

## Known issues

Release notes must state these limitations. Each links its caveat, where the
current behavior, workarounds and proposed improvements are recorded.

| Area           | What users should know                                                                                                        | Caveat                                                                                                                                       |
| -------------- | ----------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Plasma session | Nothing has been checked in a real Plasma session or with real providers yet.                                                 | [M15](ROADMAP.md#m15-real-plasma-session-verification)                                                                                       |
| Language       | The interface is in English only; no translation catalog exists yet.                                                          | [#472](https://github.com/mikepadiernos/plasma-impresario/issues/472)                                                                        |
| Drafts         | Drafts saved locally do not keep attachments; the save dialog warns about it.                                                 | [#450](https://github.com/mikepadiernos/plasma-impresario/issues/450)                                                                        |
| Sending        | Every refused send leaves another copy in Sent. A message being sent when the session ends can reopen after it was delivered. | [#450](https://github.com/mikepadiernos/plasma-impresario/issues/450), [#468](https://github.com/mikepadiernos/plasma-impresario/issues/468) |
| OAuth          | Google and Microsoft sign-in need client IDs set at build time, and OAuth requires QtKeychain.                                | [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362)                                                                        |
| POP3           | Mail downloaded over POP3 lives only on this computer and is deleted with the account; back it up.                            | [#478](https://github.com/mikepadiernos/plasma-impresario/issues/478)                                                                        |
| Signed mail    | Inline PGP is shown as text, not verified.                                                                                    | [#442](https://github.com/mikepadiernos/plasma-impresario/issues/442)                                                                        |
| Local data     | The cache and offline search index are not encrypted; use disk encryption.                                                    | [#458](https://github.com/mikepadiernos/plasma-impresario/issues/458)                                                                        |
| Privacy        | The IMAP `ID` command and the `User-Agent` header include the version and operating system.                                   | [#475](https://github.com/mikepadiernos/plasma-impresario/issues/475)                                                                        |
| Gmail          | Gmail labels appear as folders.                                                                                               | [#473](https://github.com/mikepadiernos/plasma-impresario/issues/473)                                                                        |
| Large folders  | Removing thousands of messages, or filtering 200,000, can freeze the window for about a second.                               | [#506](https://github.com/mikepadiernos/plasma-impresario/issues/506)                                                                        |
| Legacy names   | Settings, cache and the `trojita` command keep Trojitá's names so existing data keeps working.                                | [#503](https://github.com/mikepadiernos/plasma-impresario/issues/503)                                                                        |

## Triage

Every open bug and caveat was checked against the
[release criteria](#release-criteria) on 2026-11-28
([#137](https://github.com/mikepadiernos/plasma-impresario/issues/137)).

| Result                          | Count | Meaning                                                                                      |
| ------------------------------- | ----- | -------------------------------------------------------------------------------------------- |
| ![Blocker][blocker]             | 0     | Breaks a criterion; must be fixed before a release.                                          |
| ![Known issue][known-issue]     | 46    | Meets the criteria; documented, with the main ones in the [known issues](#known-issues).     |
| ![Session check][session-check] | 18    | Also a known issue, and part of it can be confirmed only in a real Plasma session (R7, M15). |

**Bugs:** none open. All 9 reported bugs are closed; see [reported bugs](BUGS.md).

**Caveats:** 46 open, all known issues.

<details>
<summary>Verdict for each open caveat</summary>

| Issue                                                                 | Caveat                                                                                        | M15                             | Criteria | Why it does not block                                                                                              |
| --------------------------------------------------------------------- | --------------------------------------------------------------------------------------------- | ------------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------ |
| [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264) | [Related messages and details panels](CAVEATS.md#related-messages-and-message-details-panels) |                                 | R3       | Searches are bounded and read loaded data; the panel says when limits left something out.                          |
| [#312](https://github.com/mikepadiernos/plasma-impresario/issues/312) | [Mail operations](CAVEATS.md#mail-operations)                                                 |                                 | R1, R3   | Localized special-folder names and undo without UIDPLUS are limited; no operation loses mail without confirmation. |
| [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362) | [OAuth sign-in](CAVEATS.md#oauth-sign-in)                                                     | ![Session check][session-check] | R2       | Refused with cleartext storage; client IDs are a build-time choice; real providers wait for M15.                   |
| [#442](https://github.com/mikepadiernos/plasma-impresario/issues/442) | [Message security](CAVEATS.md#message-security)                                               |                                 | R2       | Inline PGP is labelled not verified, never shown as valid.                                                         |
| [#443](https://github.com/mikepadiernos/plasma-impresario/issues/443) | [Reading privacy](CAVEATS.md#reading-privacy)                                                 |                                 | R2       | Remote content stays blocked by default; link warnings are an extra hint.                                          |
| [#444](https://github.com/mikepadiernos/plasma-impresario/issues/444) | [Loading on demand](CAVEATS.md#loading-on-demand)                                             |                                 | R3       | A failed part loads again with Try Again or on reopening.                                                          |
| [#445](https://github.com/mikepadiernos/plasma-impresario/issues/445) | [Reader action bar](CAVEATS.md#reader-actions)                                                |                                 | R3       | Only separate message windows lack the bar.                                                                        |
| [#449](https://github.com/mikepadiernos/plasma-impresario/issues/449) | [Conversations](CAVEATS.md#conversations)                                                     |                                 | R3       | Conversations cover loaded messages of the current folder.                                                         |
| [#450](https://github.com/mikepadiernos/plasma-impresario/issues/450) | [Composing and sending](CAVEATS.md#composing-and-sending)                                     |                                 | R1       | Drafts lose attachments only after a warning; extra Sent copies are duplicates, not losses.                        |
| [#454](https://github.com/mikepadiernos/plasma-impresario/issues/454) | [Search](CAVEATS.md#search)                                                                   |                                 | R3       | Date and folder limits are documented; results are complete within them.                                           |
| [#457](https://github.com/mikepadiernos/plasma-impresario/issues/457) | [Running in the background](CAVEATS.md#desktop-integration)                                   | ![Session check][session-check] | R7       | Behavior is tested offscreen; the session check is M15.                                                            |
| [#458](https://github.com/mikepadiernos/plasma-impresario/issues/458) | [Local data and opened attachments](CAVEATS.md#local-data-and-opened-attachments)             |                                 | R2       | User-only permissions; encryption at rest is left to the disk.                                                     |
| [#459](https://github.com/mikepadiernos/plasma-impresario/issues/459) | [Outgoing headers and Bcc](CAVEATS.md#outgoing-headers-and-bcc)                               |                                 | R2       | Bcc never reaches recipients; only the Sent copy omits it with BURL.                                               |
| [#460](https://github.com/mikepadiernos/plasma-impresario/issues/460) | [Saved files in the file manager](CAVEATS.md#saved-files-in-the-file-manager)                 | ![Session check][session-check] | R7       | Needs Dolphin and Plasma Opus in a session.                                                                        |
| [#461](https://github.com/mikepadiernos/plasma-impresario/issues/461) | [Files handed over by file managers](CAVEATS.md#files-handed-over-by-file-managers)           | ![Session check][session-check] | R7       | Needs the portal and file managers in a session.                                                                   |
| [#462](https://github.com/mikepadiernos/plasma-impresario/issues/462) | [Files fetched from other computers](CAVEATS.md#files-fetched-from-other-computers)           | ![Session check][session-check] | R7       | Only HTTP is tested; other KIO workers need a session.                                                             |
| [#463](https://github.com/mikepadiernos/plasma-impresario/issues/463) | [Plasma Search mail](CAVEATS.md#plasma-search-mail)                                           | ![Session check][session-check] | R7       | Needs KRunner in a session; answers only once turned on in the app.                                                |
| [#464](https://github.com/mikepadiernos/plasma-impresario/issues/464) | [Contacts from the address book](CAVEATS.md#contacts-from-the-address-book)                   | ![Session check][session-check] | R7       | Akonadi is built but untried against a server.                                                                     |
| [#465](https://github.com/mikepadiernos/plasma-impresario/issues/465) | [My Day calendars and follow-up mail](CAVEATS.md#my-day-calendars-and-follow-up-mail)         | ![Session check][session-check] | R7       | The Akonadi calendar source is untried against a server.                                                           |
| [#466](https://github.com/mikepadiernos/plasma-impresario/issues/466) | [Server settings discovery](CAVEATS.md#server-settings-discovery)                             | ![Session check][session-check] | R2       | Found settings open in the account dialog for review; real providers wait for M15.                                 |
| [#467](https://github.com/mikepadiernos/plasma-impresario/issues/467) | [Window activation for requests](CAVEATS.md#window-activation-for-requests)                   | ![Session check][session-check] | R7       | Activation tokens need a Wayland session.                                                                          |
| [#468](https://github.com/mikepadiernos/plasma-impresario/issues/468) | [Saving unsent messages at session end](CAVEATS.md#saving-unsent-messages-at-session-end)     | ![Session check][session-check] | R1       | Recovers text rather than losing it; a reopened delivered message is a duplicate, not a loss.                      |
| [#470](https://github.com/mikepadiernos/plasma-impresario/issues/470) | [Portal file chooser](CAVEATS.md#portal-file-chooser)                                         | ![Session check][session-check] | R7       | Tested with a fake portal.                                                                                         |
| [#471](https://github.com/mikepadiernos/plasma-impresario/issues/471) | [Unread count on the task manager](CAVEATS.md#unread-count-on-the-task-manager)               | ![Session check][session-check] | R7       | Needs the task manager in a session.                                                                               |
| [#472](https://github.com/mikepadiernos/plasma-impresario/issues/472) | [Accessibility and theming](CAVEATS.md#accessibility-and-theming)                             | ![Session check][session-check] | R4       | Strings are marked for translation; catalogs and Orca checks come later.                                           |
| [#473](https://github.com/mikepadiernos/plasma-impresario/issues/473) | [Provider setup and Gmail](CAVEATS.md#provider-setup-and-gmail)                               | ![Session check][session-check] | R3       | Labels as folders are documented; real providers wait for M15.                                                     |
| [#474](https://github.com/mikepadiernos/plasma-impresario/issues/474) | [Junk and attachment safety](CAVEATS.md#junk-and-attachment-safety)                           |                                 | R2       | Opening asks first and scans when `clamd` is present; other paths are documented.                                  |
| [#475](https://github.com/mikepadiernos/plasma-impresario/issues/475) | [External services and outbound calls](CAVEATS.md#external-services-and-outbound-calls)       |                                 | R2       | Message content goes only to the account's servers; identifying headers are documented.                            |
| [#476](https://github.com/mikepadiernos/plasma-impresario/issues/476) | [System sleep and account notifications](CAVEATS.md#system-sleep-and-account-notifications)   | ![Session check][session-check] | R3       | Connections recover through the normal reconnect.                                                                  |
| [#477](https://github.com/mikepadiernos/plasma-impresario/issues/477) | [Plasma widgets](CAVEATS.md#plasma-widgets)                                                   | ![Session check][session-check] | R7       | Optional; untried on a Plasma panel.                                                                               |
| [#478](https://github.com/mikepadiernos/plasma-impresario/issues/478) | [POP3 accounts](CAVEATS.md#pop3-accounts)                                                     |                                 | R1       | Local mail is removed only when the user removes the account.                                                      |
| [#488](https://github.com/mikepadiernos/plasma-impresario/issues/488) | [JMAP accounts](CAVEATS.md#jmap-accounts)                                                     |                                 | R3       | Slower first sync and no push; mail is complete.                                                                   |
| [#491](https://github.com/mikepadiernos/plasma-impresario/issues/491) | [Mail rules](CAVEATS.md#mail-rules)                                                           |                                 | R3       | Rules cover IMAP arrivals; the limits are documented.                                                              |
| [#492](https://github.com/mikepadiernos/plasma-impresario/issues/492) | [New-mail notifications and activity](CAVEATS.md#new-mail-notifications-and-activity)         | ![Session check][session-check] | R3       | Some folders are announced by count; notifications need a session check.                                           |
| [#496](https://github.com/mikepadiernos/plasma-impresario/issues/496) | [Settings and workspaces](CAVEATS.md#settings-and-workspaces)                                 |                                 | R3       | Activities need the Activity Manager; mail works without it.                                                       |
| [#497](https://github.com/mikepadiernos/plasma-impresario/issues/497) | [Mail tabs](CAVEATS.md#mail-tabs)                                                             |                                 | R3       | Restored tabs show their folder until reopened.                                                                    |
| [#498](https://github.com/mikepadiernos/plasma-impresario/issues/498) | [Vim mode and shared shortcuts](CAVEATS.md#vim-mode-and-shared-shortcuts)                     |                                 | R4       | Off by default; standard shortcuts are unaffected.                                                                 |
| [#499](https://github.com/mikepadiernos/plasma-impresario/issues/499) | [Messages to calendar and notes](CAVEATS.md#messages-to-calendar-and-notes)                   |                                 | R2       | Encrypted text is not written to notes.                                                                            |
| [#502](https://github.com/mikepadiernos/plasma-impresario/issues/502) | [Themes and appearance](CAVEATS.md#themes-and-appearance)                                     |                                 | R3       | Some options apply when a window next opens.                                                                       |
| [#503](https://github.com/mikepadiernos/plasma-impresario/issues/503) | [Project naming and compatibility](CAVEATS.md#project-naming-and-compatibility)               |                                 | R6       | Legacy names keep existing data working.                                                                           |
| [#505](https://github.com/mikepadiernos/plasma-impresario/issues/505) | [Read state and unread counts](CAVEATS.md#read-state-and-unread-counts)                       |                                 | R1       | A change cut off by a lost connection stays in the activity log with Retry.                                        |
| [#506](https://github.com/mikepadiernos/plasma-impresario/issues/506) | [Performance and resource use](CAVEATS.md#performance-and-resource-use)                       |                                 | R5       | Within the recorded budgets; long pauses only with very large folders.                                             |
| [#507](https://github.com/mikepadiernos/plasma-impresario/issues/507) | [Sanitizers and data races](CAVEATS.md#sanitizers-and-data-races)                             |                                 | R5       | Residual risk: ThreadSanitizer needs a Qt built for it.                                                            |
| [#508](https://github.com/mikepadiernos/plasma-impresario/issues/508) | [Persistent outbox](CAVEATS.md#persistent-outbox)                                             |                                 | R1       | Interrupted messages wait for the user instead of risking a second delivery.                                       |
| [#509](https://github.com/mikepadiernos/plasma-impresario/issues/509) | [Background header loading](CAVEATS.md#background-header-loading)                             |                                 | R3       | Bounded loading; other folders load when opened.                                                                   |
| [#511](https://github.com/mikepadiernos/plasma-impresario/issues/511) | [Release gate scope](CAVEATS.md#release-gate-scope)                                           |                                 | R5       | Gate passed; remaining gaps are documented residual risk.                                                          |

</details>

## Handoff

### Where the work is tracked

| What                                | Where                                                                                            |
| ----------------------------------- | ------------------------------------------------------------------------------------------------ |
| Milestones and their order          | [Roadmap](ROADMAP.md) and the [project board](https://github.com/users/mikepadiernos/projects/4) |
| Blocked work and resume steps       | [Pending tasks](TODO.md)                                                                         |
| Bugs and their status               | [Reported bugs](BUGS.md), issues labelled `bug`                                                  |
| Limitations and recommendations     | [Caveats](CAVEATS.md), issues labelled `caveat`                                                  |
| Which test covers which workflow    | [End-to-end coverage](e2e-coverage.md)                                                           |
| How to build, test and run the gate | [Contributing](../CONTRIBUTING.md) and the [stabilization gate](stabilization-gate.md)           |
| Release gate checks and results     | [Release gate](release-gate.md)                                                                  |

### Cutting a release

Once every criterion is met:

1. Run the [release gate](release-gate.md#how-to-run) and the
   [stabilization gate](stabilization-gate.md) on the commit to be
   released, and record their results on their pages.
2. Repeat the [triage](#triage) with the commands below; every open bug
   and caveat needs a verdict against the criteria.
3. Set the version in `src/plasma-impresario-version`, which still reads
   Trojitá's `0.7-git`, and add a `<release>` entry to
   `src/Gui/io.github.mikepadiernos.plasma_impresario.metainfo.xml`.
4. Tag the commit on `main` and publish release notes that list the known
   issues.
5. Update the [supported versions](../SECURITY.md#supported-versions) in the
   security policy.

```bash
gh issue list --label bug --state open
gh issue list --label caveat --state open --limit 100
```

[blocker]: https://img.shields.io/badge/Blocker-cf222e
[known-issue]: https://img.shields.io/badge/Known_issue-bc4c00
[met]: https://img.shields.io/badge/Met-1a7f37
[not-met]: https://img.shields.io/badge/Not_met-cf222e
[pending]: https://img.shields.io/badge/Pending-9a6700
[session-check]: https://img.shields.io/badge/Session_check-0969da
