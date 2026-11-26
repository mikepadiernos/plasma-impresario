# Desktop, services, and assets refinement

**[Documentation](../README.md)** · **[Roadmap: M14](../ROADMAP.md#m14-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Settings and notifications](07-settings-and-notifications.md)** ·
**[Next: Calendar and contacts](09-calendar-and-contacts.md)**

Refines KDE and Plasma integration, provider behavior and Nextcloud asset
workflows. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                                     | Issue                                                               | Board               |
| ---------------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M14.09: Desktop, services, and assets](../ROADMAP.md#m14-refinements) | [#28](https://github.com/mikepadiernos/plasma-impresario/issues/28) | ![Backlog][backlog] |

## Current state

The app already uses KDE notifications for accounts that need attention and a
system tray icon; see the
[system tray context menu](menus/system-tray-context-menu.md). Desktop
integration and provider support are planned in
[M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)
([#7](https://github.com/mikepadiernos/plasma-impresario/issues/7)) and
Nextcloud in
[M12](../ROADMAP.md#m12-nextcloud-and-digital-asset-management)
([#31](https://github.com/mikepadiernos/plasma-impresario/issues/31)); both are
![Backlog][backlog], and no Nextcloud or Plasma Opus code exists in `src/` yet.
A real Plasma session has not been verified.

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                                                                                                                            | Issue                                                                 |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Refine KDE and Plasma integration, provider behavior, and Nextcloud asset workflows; define refinement criteria for potential Plasma Opus integration                                                 | [#197](https://github.com/mikepadiernos/plasma-impresario/issues/197) |
| Verify Plasma theme, icons, notifications, file dialogs, and session behavior against desktop conventions                                                                                             | [#198](https://github.com/mikepadiernos/plasma-impresario/issues/198) |
| Explain provider authentication, capability differences, and recovery when access expires                                                                                                             | [#199](https://github.com/mikepadiernos/plasma-impresario/issues/199) |
| Make Nextcloud file access, shared-link visibility, upload progress, and permission failures clear before sending                                                                                     | [#200](https://github.com/mikepadiernos/plasma-impresario/issues/200) |
| Define a potential Plasma Opus integration contract for context, permissions, and behavior when the other app is unavailable                                                                          | [#201](https://github.com/mikepadiernos/plasma-impresario/issues/201) |
| Review mail integration with KDE Plasma services such as notifications, KIO, file handling, session integration, account storage, and desktop-level actions                                           | [#202](https://github.com/mikepadiernos/plasma-impresario/issues/202) |
| Define the user-facing contract for Plasma Opus-aware mail flows: calendar handoff, notes capture, contact lookup, workspace-aware context, and graceful fallback when a companion app is unavailable | [#203](https://github.com/mikepadiernos/plasma-impresario/issues/203) |

## Acceptance criteria

- The app behaves like a KDE-native desktop client rather than a disconnected
  utility.
- Provider and asset workflows degrade gracefully when services are unavailable
  or restricted.
- Desktop file and notification conventions remain understandable and
  consistent.
- Shared and external assets remain safe, visible, and permission-aware.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
