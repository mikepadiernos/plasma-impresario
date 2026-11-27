# Security and privacy refinement

**[Documentation](../README.md)** · **[Roadmap: M14](../ROADMAP.md#m14-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Search, sorting and automation](05-search-sorting-and-automation.md)** ·
**[Next: Settings and notifications](07-settings-and-notifications.md)** ·
**[Security caveats](../CAVEATS.md#security-and-data-protection)** ·
**[External services](../integrations/external-services.md)**

Refines credential handling, remote-content controls, attachment safety, ClamAV
and spam feedback, and privacy settings. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                            | Issue                                                               | Board               |
| ------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M14.07: Security and privacy](../ROADMAP.md#m14-refinements) | [#26](https://github.com/mikepadiernos/plasma-impresario/issues/26) | ![Backlog][backlog] |

## Current state

Passwords go through a password plugin: QtKeychain
(`src/Plugins/QtKeyChain`) or a clear-text fallback
(`src/Plugins/ClearTextPassword`); which one a given build selects was not
checked for this plan. Remote content in a message stays blocked until the
user chooses **Load Remote Content**, and the message list offers **Mark as
Junk** and **Mark as Not Junk**. No ClamAV integration exists in `src/` yet.
Known limitations are tracked in
[Caveats: security and data protection](../CAVEATS.md#security-and-data-protection).
A real Plasma session has not been verified.

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                                                      | Issue                                                                 |
| ------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Refine credential handling, remote-content controls, attachment safety, ClamAV and spam feedback, and privacy settings          | [#181](https://github.com/mikepadiernos/plasma-impresario/issues/181) |
| Use secure credential storage and explain authentication, certificate, and account-access errors                                | [#182](https://github.com/mikepadiernos/plasma-impresario/issues/182) |
| Keep remote content blocked or user-controlled and make external-link destinations visible before opening                       | [#183](https://github.com/mikepadiernos/plasma-impresario/issues/183) |
| Distinguish clean, suspicious, unscanned, and failed scan states without treating unavailable scanning as success               | [#184](https://github.com/mikepadiernos/plasma-impresario/issues/184) |
| Provide understandable spam controls and recovery from misclassification without exposing mail content unnecessarily            | [#185](https://github.com/mikepadiernos/plasma-impresario/issues/185) |
| Test untrusted message content, attachment names, links, and imported data across validation, permissions, and error boundaries | [#186](https://github.com/mikepadiernos/plasma-impresario/issues/186) |
| Check that settings, logs, temporary files, and backups do not inadvertently expose credentials or message contents             | [#187](https://github.com/mikepadiernos/plasma-impresario/issues/187) |

## Acceptance criteria

- User security and privacy choices are visible, explicit, and recoverable.
- Remote content and external links remain controlled by the user.
- Security state and scan outcomes are understandable without claiming
  successful protection that was not performed.
- Sensitive data remains protected in settings, logs, and temporary files.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
