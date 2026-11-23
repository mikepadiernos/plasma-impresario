# KDE Plasma integrations

**[Integrations](README.md)** · **[Scope](../philosophy/scope.md)** ·
**[Roadmap](../ROADMAP.md)**

Plasma Impresario targets the KDE Plasma desktop. These are integration goals,
not claims that every service is implemented or verified.

## Desktop and session

- Follow Plasma conventions for windows, startup, shutdown and session
  lifecycle.
- Use compact, actionable notifications for new mail, sync failures and account
  state.
- Keep status feedback visible without interrupting reading.

## Files and accounts

- Use KIO, file dialogs and KDE file associations for attachments and local
  files; support drag-and-drop, previews and Open With.
- Use KDE configuration and credential facilities where appropriate, and respect
  account and network settings across sessions.

## Appearance and accessibility

- Follow the active Plasma theme, icons, accent and high-contrast settings.
- Support keyboard shortcuts, visible focus and localized interactions.
- Prefer established KDE Frameworks and desktop patterns to custom plumbing.

> [!IMPORTANT]
>
> Service availability, error recovery and privacy behavior require validation
> in a real desktop session. See [pending tasks](../TODO.md) and
> [caveats](../CAVEATS.md).
