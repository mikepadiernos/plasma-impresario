# Desktop, services, and assets refinement

**[Refinements](README.md)** ·
**[M14 in roadmap](../ROADMAP.md#m14-refinements)** ·
**[Previous: Settings and notifications](07-settings-and-notifications.md)** ·
**[Next: Calendar and contacts](09-calendar-and-contacts.md)**

Refines KDE and Plasma integration, provider behavior and Nextcloud asset
workflows.

**Roadmap subsection:**
[M14.09: Desktop, services, and assets](../ROADMAP.md#m14-refinements) ·
**Issue:** [#28](https://github.com/mikepadiernos/plasma-impresario/issues/28)

## Focus

- Refine KDE and Plasma integration, provider behavior, and Nextcloud asset
  workflows; define refinement criteria for potential Plasma Opus integration
- Verify Plasma theme, icons, notifications, file dialogs, and session behavior
  against desktop conventions
- Explain provider authentication, capability differences, and recovery when
  access expires
- Make Nextcloud file access, shared-link visibility, upload progress, and
  permission failures clear before sending
- Define a potential Plasma Opus integration contract for context, permissions,
  and behavior when the other app is unavailable

## Acceptance criteria

- The app behaves like a KDE-native desktop client rather than a disconnected
  utility.
- Provider and asset workflows degrade gracefully when services are unavailable
  or restricted.
- Desktop file and notification conventions remain understandable and
  consistent.
- Shared and external assets remain safe, visible, and permission-aware.
