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

Legend: ![Met][met] · ![Pending][pending] · ![Not met][not-met]

> [!IMPORTANT]
>
> Plasma Impresario is **not ready to ship**. No open bug or caveat blocks a
> release, but three criteria are still open: the release gate, the renderer
> sandbox on Ubuntu, and use in a real Plasma session. See the
> [decision](#release-decision).

## Contents

- [Release decision](#release-decision)
- [Release criteria](#release-criteria)
- [Known issues](#known-issues)
- [Handoff](#handoff)
  - [Where the work is tracked](#where-the-work-is-tracked)
  - [Cutting a release](#cutting-a-release)

## Release decision

| Question                                 | Answer                                                                                                                                                                                                                                                                                                                                             |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Can a release be made today?             | No.                                                                                                                                                                                                                                                                                                                                                |
| What is missing?                         | [R5](#release-criteria) the release gate ([#482](https://github.com/mikepadiernos/plasma-impresario/issues/482)), [R7](#release-criteria) a real Plasma session ([M15](ROADMAP.md#m15-real-plasma-session-verification)) and [R8](#release-criteria) the renderer sandbox ([#381](https://github.com/mikepadiernos/plasma-impresario/issues/381)). |
| Do open bugs or caveats block a release? | No. There are no open bugs, and every open caveat is a documented known issue.                                                                                                                                                                                                                                                                     |
| What do users have to be told?           | The [known issues](#known-issues), in the release notes.                                                                                                                                                                                                                                                                                           |

## Release criteria

A release needs every criterion met. Evidence from offscreen tests and
disposable mail servers counts for R1 to R6; R7 asks for a real session.

| ID     | Criterion                                                                                                                                                                                                                             | Evidence                                                                                                                                                                                                                                                  | Status              |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------- |
| **R1** | Data safety: no known way to lose or corrupt mail on the server or on disk without a warning or an explicit choice by the user.                                                                                                       | Live server tests, the persistent outbox and its **Interrupted** state, the draft warning about attachments, and the [triage](#known-issues)                                                                                                              | ![Met][met]         |
| **R2** | Secure defaults: encrypted connections are offered, new profiles store passwords through QtKeychain, remote content and scripts are blocked, and signature results tell invalid, unknown-key and failed checks apart from valid ones. | [Security summary](CAVEATS.md#security-and-data-protection) and the [security policy](../SECURITY.md) with private reporting                                                                                                                              | ![Met][met]         |
| **R3** | Core workflows pass end to end: account setup, reading, triage and reply; compose, send, attachments, search, rules, notifications and offline recovery.                                                                              | [End-to-end coverage](e2e-coverage.md) ([#133](https://github.com/mikepadiernos/plasma-impresario/issues/133), [#134](https://github.com/mikepadiernos/plasma-impresario/issues/134))                                                                     | ![Met][met]         |
| **R4** | Accessibility: every control has an accessible name, the core workflows work from the keyboard, and user-visible strings are marked for translation.                                                                                  | [Accessibility audit](accessibility.md) ([#53](https://github.com/mikepadiernos/plasma-impresario/issues/53), [#135](https://github.com/mikepadiernos/plasma-impresario/issues/135))                                                                      | ![Met][met]         |
| **R5** | Quality gate: clean formatting, a reproducible build, the full test suite passing, a coverage report, recorded dependency versions and static analysis without unresolved findings.                                                   | The [stabilization gate](stabilization-gate.md) covers the suite but leaves out `test_Cryptography_PGP`, whose decryption rows fail with the container's GpgME; the release gate is [#482](https://github.com/mikepadiernos/plasma-impresario/issues/482) | ![Pending][pending] |
| **R6** | Documentation: installation, security and supported platforms are current, and every known limitation has a caveat with a linked issue and recommendation.                                                                            | [Installation](INSTALL.md), [security policy](../SECURITY.md), [caveats](CAVEATS.md) and the [triage](#known-issues)                                                                                                                                      | ![Met][met]         |
| **R7** | Plasma session: the core workflows and the desktop integration work in a real KDE Plasma 6 Wayland session and with at least one real provider.                                                                                       | [M15](ROADMAP.md#m15-real-plasma-session-verification) ([#319](https://github.com/mikepadiernos/plasma-impresario/issues/319))                                                                                                                            | ![Not met][not-met] |
| **R8** | Renderer sandbox: the QtWebEngine sandbox stays on for every supported distribution, including Ubuntu 24.04 and later.                                                                                                                | [#381](https://github.com/mikepadiernos/plasma-impresario/issues/381)                                                                                                                                                                                     | ![Pending][pending] |

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

### Cutting a release

Once every criterion is met:

1. Run the release gate and the stabilization gate on the commit to be
   released, and record their results on their pages.
2. Repeat the [triage](#known-issues) with the commands below; every open bug
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

[met]: https://img.shields.io/badge/Met-1a7f37
[not-met]: https://img.shields.io/badge/Not_met-cf222e
[pending]: https://img.shields.io/badge/Pending-9a6700
