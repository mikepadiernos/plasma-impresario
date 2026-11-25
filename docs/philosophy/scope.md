# Plasma Impresario scope and objective

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Designs](designs.md)** · **[Inspirations](inspirations.md)** ·
**[Compatibility](compatibility.md)**

Plasma Impresario is a KDE Plasma-only IMAP desktop mail client, forked from
Trojitá, for users who need reliable account management, fast triage and
readable conversations. Other desktop environments are outside the supported
product scope; milestones and their order live in the [roadmap](../ROADMAP.md).

Legend: ✅ done · 🧪 implemented, needs verification · 📋 planned · ⛔ blocked

> [!IMPORTANT]
>
> IMAP and SMTP reliability, account isolation and predictable message state are
> foundational. Provider-specific, workspace and desktop integrations expand the
> product after those fundamentals are complete.

## Current baseline

The client is built with C++20, Qt 6 and KDE Frameworks 6. IMAP mailboxes stay
on the server; the application synchronizes metadata and fetches message parts
on demand, with an optional local cache. The
[backend parity guide](../parity/backend.md) describes the inherited
implementation, [designs](designs.md#current-interface) describes the current
interface, and [caveats](../CAVEATS.md) records security and platform
limitations.

| Capability                                                       | Status | Notes                                                                                                                                                                                                                                   |
| ---------------------------------------------------------------- | ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🧱 KDE / Qt 6 baseline (M01)                                     | ✅     | Merged into `main`; see [compatibility](compatibility.md).                                                                                                                                                                              |
| 📬 IMAP reading with on-demand fetching and local cache          | 🧪     | Inherited from Trojitá. Live reading with a real account is pending in M04.                                                                                                                                                             |
| ✉️ SMTP and other submission paths                               | 🧪     | Inherited from Trojitá. Live sending is pending in M04.                                                                                                                                                                                 |
| 📬 Multiple simultaneous IMAP accounts                           | ✅     | Completed in M02 ([#222](https://github.com/mikepadiernos/plasma-impresario/issues/222)).                                                                                                                                               |
| 🧭 Mail shell and embedded panels                                | 🧪     | Completed ([#35](https://github.com/mikepadiernos/plasma-impresario/issues/35)); Plasma check in [#315](https://github.com/mikepadiernos/plasma-impresario/issues/315); see [designs](designs.md#shell-status).                         |
| 🔐 OAuth account authentication                                  | 🧪     | Merged in M02 ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)); live sign-in in [#317](https://github.com/mikepadiernos/plasma-impresario/issues/317) and limitations in [caveats](../CAVEATS.md#oauth-sign-in). |
| 🔍 Search across folders and accounts with an opt-in local index | 📋     | M05. Server search stays the default; the index covers downloaded mail only.                                                                                                                                                            |
| 📮 POP3 retrieval with SMTP submission                           | 📋     | M06.05. POP3 stores retrieved mail locally and has no IMAP-style server folder synchronization.                                                                                                                                         |
| 📡 JMAP accounts                                                 | 📋     | M06.05. Capabilities depend on the server.                                                                                                                                                                                              |
| 🏢 Consumer Outlook, Exchange Online and Exchange Server         | 📋     | M13; see the [Microsoft guide](../integrations/microsoft.md). Exchange Online and on-premises Exchange Server differ in interfaces, authentication options and administrator policies.                                                  |

## Target experience

- Configure and manage multiple simultaneous accounts and mail identities.
- Triage selected accounts together in a unified inbox with visible message
  origins and account-aware actions.
- Synchronize folders and preserve unread and offline state.
- Read and organize threaded conversations.
- Search mail across folders and accounts, saving searches as virtual
  folders and optionally indexing downloaded mail for offline search.
- Compose and recover drafts without losing account context.
- Perform bulk actions with clear progress and failure recovery.
- Use Plasma-native notifications, credentials, files and settings.

## Scope boundaries

| In scope                                                     | Requires explicit validation                                               |
| ------------------------------------------------------------ | -------------------------------------------------------------------------- |
| KDE Plasma desktop with Qt Widgets and KDE Frameworks        | Supported distributions, Plasma versions, sessions and dependencies        |
| IMAP and SMTP workflows; planned POP3 and JMAP support       | Protocol and provider authentication, capabilities and storage differences |
| OAuth sign-in; planned Microsoft and Exchange accounts (M13) | Provider consent, supported interfaces and administrator policies          |
| Local cache and optional Maildir organization                | Cache retention, storage limits and migration behavior                     |
| Plasma and KDE desktop services                              | Session, notification and service availability                             |
| Accessibility, localization and keyboard workflows           | Screen-reader and scaled-display coverage                                  |

## Product principles

| Principle          | Meaning                                                                                                 |
| ------------------ | ------------------------------------------------------------------------------------------------------- |
| 🖥️ Native          | Follow Plasma conventions and KDE facilities rather than duplicating desktop services.                  |
| 📶 Reliable        | Surface connection, authentication and partial-operation state; never imply that failed work completed. |
| 📖 Readable        | Keep default layouts compact but prioritize hierarchy, selection and focus visibility.                  |
| 🔒 User-controlled | Make remote content, external services and destructive actions explicit.                                |
| 🧩 Composable      | Separate account, mailbox, desktop and cross-app behavior so integrations remain optional.              |

## Reference model

Trojitá provides the IMAP-first, KDE-native baseline. Aqua Mail, Vivaldi Mail,
Thunderbird, The Bat!, Kontact, ePim and E-Mail inform triage, cross-account
views, optional tabs, dense power-user workflows, Plasma integration and safe
defaults. The
[inspirations](inspirations.md#design-synthesis) document lists what each
contributes.

## Configuration model

> [!NOTE]
>
> The YAML below is an illustrative product concept, not a supported
> configuration file. Today the client stores profile settings through
> `QSettings`, appearance and embedded-panel preferences in the `Appearance`
> group of `plasma-impresariorc` (see
> [designs](designs.md#current-embedded-panels)), and session selection in
> KConfig state.

<details>
<summary>Illustrative configuration concept</summary>

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

</details>
