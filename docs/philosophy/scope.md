# Plasma Mail scope and objective

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Designs](designs.md)** · **[Inspirations](inspirations.md)**

Plasma Mail is a desktop-first mail client for KDE users who need reliable
account management, fast triage and readable conversations.

> [!IMPORTANT] IMAP and SMTP reliability, account isolation and predictable
> message state are foundational. Provider-specific, workspace and desktop
> integrations expand the product after those fundamentals are complete.

## Current baseline

This checkout currently contains a Trojitá-based, IMAP-first client, not the
proposed multi-provider Plasma Mail product. IMAP mailboxes remain server-side;
the application synchronizes metadata and fetches message parts on demand,
with an optional local cache. SMTP and other submission paths handle outgoing
mail. The [backend parity guide](../parity/backend.md) describes the current
implementation; [caveats](../CAVEATS.md) records its security and platform
limitations.

## Target experience

- configure and manage multiple mail identities;
- synchronize folders and preserve unread and offline state;
- read, search and organize threaded conversations;
- compose and recover drafts without losing account context;
- perform bulk actions with clear progress and failure recovery;
- use Plasma-native notifications, credentials, files and settings.

## Scope boundaries

| In scope | Requires explicit validation |
| --- | --- |
| Desktop Qt/QWidget and KDE Frameworks application | Initial supported distribution and dependency matrix |
| IMAP and SMTP workflows | Provider-specific authentication and capability differences |
| Local cache and optional Maildir organization | Cache retention, storage limits and migration behavior |
| Plasma and KDE desktop services | Session, notification and service availability |
| Accessibility, localization and keyboard workflows | Screen-reader and scaled-display coverage |

## Product principles

- **Native:** Follow Plasma conventions and KDE facilities rather than
  duplicating desktop services.
- **Reliable:** Surface connection, authentication and partial-operation state;
  never imply that failed work completed.
- **Readable:** Keep default layouts compact but prioritize hierarchy,
  selection and focus visibility.
- **User-controlled:** Make remote content, external services and destructive
  actions explicit.
- **Composable:** Separate account, mailbox, desktop and cross-app behavior so
  integrations remain optional.

## Reference model

The project combines the strengths of:

- Trojitá for IMAP correctness and a lightweight KDE-native baseline;
- Aqua Mail for quick, readable triage and low-friction navigation;
- The Bat! for dense desktop operations and keyboard-first workflows;
- Kontact for personal-information management and Plasma integration.

## Configuration model

> [!NOTE] The following YAML is an illustrative product concept, not a
> supported configuration file or the application's current settings format.
> The existing client stores preferences through `QSettings`.

```yaml
appearance:
  theme: "Plasma Default"
  accent: "blue"
  density: "compact"
  high_contrast: false

layout:
  left_pane: visible
  right_pane: visible
  preview_mode: "split"
  focus_mode: false

mail:
  threaded_view: true
  mark_read_on_click: true
  preview_text_lines: 2
  default_sort: "date-desc"

notifications:
  desktop_toasts: true
  sync_alerts: true
  badge_counts: true

productivity:
  quick_reply: true
  keyboard_nav: true
  bulk_actions: true
```
