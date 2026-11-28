# Settings and notifications refinement

**[Documentation](../README.md)** · **[Roadmap: M13](../ROADMAP.md#m13-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Security and privacy](06-security-and-privacy.md)** ·
**[Next: Desktop, services and assets](08-desktop-services-and-assets.md)**

Refines settings organization, defaults, workspace preferences, notifications,
status output and background-task feedback. Board statuses are as of
2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                                  | Issue                                                               | Board               |
| ------------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M13.08: Settings and notifications](../ROADMAP.md#m13-refinements) | [#27](https://github.com/mikepadiernos/plasma-impresario/issues/27) | ![Backlog][backlog] |

## Current state

Settings open from **Application › Settings…** and account settings from
**Application › Account Settings…** or an account's header menu. When a
background account needs attention, a desktop notification offers **Open
Account** and, where relevant, **Sign In**. Settings and workspace
configuration work is planned in
[M08](../ROADMAP.md#m08-settings-and-workspace-configuration)
([#9](https://github.com/mikepadiernos/plasma-impresario/issues/9),
![Backlog][backlog]). A real Plasma session has not been verified.

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                                                | Issue                                                                 |
| ------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Refine settings organization, defaults, workspace preferences, notifications, status output, and background-task feedback | [#188](https://github.com/mikepadiernos/plasma-impresario/issues/188) |
| Make global, account, and folder settings distinguishable and explain inherited defaults                                  | [#189](https://github.com/mikepadiernos/plasma-impresario/issues/189) |
| Keep settings searchable and validate persistence, reset behavior, and unavailable integration states                     | [#190](https://github.com/mikepadiernos/plasma-impresario/issues/190) |
| Offer per-account and per-folder notification controls with privacy-conscious message previews                            | [#191](https://github.com/mikepadiernos/plasma-impresario/issues/191) |
| Surface sync, rule, and background-task failures with actionable details rather than silent status changes                | [#192](https://github.com/mikepadiernos/plasma-impresario/issues/192) |
| Explain Apply, OK, and Cancel behavior, including live previews and changes committed immediately by external dialogs     | [#193](https://github.com/mikepadiernos/plasma-impresario/issues/193) |
| Validate settings inline, explain unavailable options, and report save failures or conflicting external changes           | [#194](https://github.com/mikepadiernos/plasma-impresario/issues/194) |
| Confirm page-specific or global resets and distinguish app-owned settings from shared KDE data and credentials            | [#195](https://github.com/mikepadiernos/plasma-impresario/issues/195) |
| Keep settings search, keyboard navigation, and unsaved edits responsive while loading larger account or provider lists    | [#196](https://github.com/mikepadiernos/plasma-impresario/issues/196) |

## Acceptance criteria

- Settings are discoverable and legible without requiring deep product
  knowledge.
- Notification behavior remains useful without becoming noisy or invasive.
- Background failures are visible, actionable, and understandable.
- Defaults and overrides remain predictable across app restarts and account
  changes.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
