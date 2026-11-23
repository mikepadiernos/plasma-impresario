# Mail client inspirations

**[Documentation](../README.md)** · **[Scope](scope.md)** ·
**[Design](designs.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Contributing](../../CONTRIBUTING.md)**

These products inform workflow, layout, and implementation decisions. They
are references, not feature-cloning specifications.

## KDE / Trojitá

- Project: https://github.com/KDE/trojita
- Why it matters: Trojitá is the closest baseline for this project because it is a Qt-based IMAP client built in the KDE ecosystem.
- Takeaways:
  - Local-first, desktop-native mail interaction
  - Clear separation between account/folder navigation and message reading
  - Strong emphasis on IMAP correctness and protocol cleanliness
  - KDE-style app behavior, especially as a lightweight desktop client

## Qt mail client implementation references

| Project | Relevant reference |
| --- | --- |
| [Korrespond](https://github.com/topke63/Korrespond) | Qt Quick and KMime message rendering, remote-image controls, and MIME-based reply text. |
| [Javelin Mail](https://github.com/ripdog/Javelin-Mail) | Qt Widgets WebEngine viewer, inline-resource scheme, context menus, and asynchronous search; its backend uses JMAP rather than IMAP. |
| [Mailove](https://github.com/nekromoff/mailove) | Qt Quick WebEngine sandbox, CID scheme, request interceptor, and network-egress tests. |
| [LoveLetter](https://github.com/guoyunhe/loveletter) | Early KDE/Qt Quick mail interface prototype; its message page is a placeholder rather than a renderer implementation. |
| [Argonaut Emails](https://github.com/jgiannop/argonaut-emails) | Qt Widgets IMAP client with a private WebEngine profile, CID handler, remote-content controls, and viewer tests. |

Adapt concepts to Trojitá's on-demand IMAP model and verify message isolation
and privacy independently. Review each project's license before reusing code.

## Aqua Mail

- Project: https://www.aqua-mail.com/
- Why it matters: Aqua Mail is a strong reference for modern mobile-first mail workflow clarity.
- Takeaways:
  - Simple, readable message list density
  - Thoughtful handling of quick actions and conversation flow
  - Strong support for account grouping and low-friction navigation
  - Fast, readable filters and search flows

## Vivaldi Mail

- Project: https://vivaldi.com/features/mail/
- Why it matters: Vivaldi Mail offers desktop workflows for managing and
  finding messages across high-volume, multi-account inboxes.
- Takeaways:
  - Unified access to multiple IMAP and POP3 accounts
  - Indexed mail and saved searches for quick retrieval, including offline
  - Distinct unseen and unread indicators
  - View toggles and multiple views of a message without moving it
  - Keyboard shortcuts, navigation history, and queued outgoing messages

Adapt these ideas to a standalone KDE Plasma client; they are inspiration,
not claims about existing features.

## The Bat!

- Project: https://www.ritlabs.com/en/products/thebat/
- Why it matters: The Bat! provides a classic power-user messaging interface with a traditional desktop layout.
- Takeaways:
  - Dense but highly functional email management
  - Robust folder and message organization patterns
  - Useful mail-management conventions for keyboard-centric users
  - Traditional workflow structure with strong prioritization of information density

## Kontact

- Project: https://kontact.kde.org/
- Why it matters: Kontact is the most relevant KDE integration reference for cross-application behavior and desktop polish.
- Takeaways:
  - Natural fit with KDE and Plasma conventions
  - Integration-minded interaction modes and common desktop patterns
  - Clean treatment of personal information management tasks
  - Strong alignment with KDE account and system integration expectations

## Adoptable Aqua Mail features

The following Aqua Mail patterns are useful to adopt selectively for Plasma Impresario:

- Quick actions directly in the message list: archive, move, flag, mark read, and delete
- Compact triage workflow: clear, low-noise reading and fast inbox scanning
- Fast search and filter patterns for inbox triage and quick retrieval
- Message grouping and conversation visibility with reduced visual clutter
- Strong focus on readable density without losing action clarity
- Per-account and per-folder notification settings for low interruption and better focus
- Lightweight, low-friction mailbox navigation that keeps actions close to the message list
- Attachment and message-loading behavior that loads content on demand rather than overwhelming the UI

These should be adapted to a desktop-first KDE layout rather than copied as a mobile design pattern.

## The Bat! power-user workflow patterns

- Project: https://www.ritlabs.com/en/products/thebat/
- Why it matters: The Bat! is a strong reference for classic desktop-mail power-user behavior and rule-driven organization.
- Takeaways:
  - Rule-based filtering and automatic sorting for incoming mail
  - Folder-based organization for large mail volumes and active queues
  - Power-user bulk actions and keyboard-first processing flows
  - Strong emphasis on structured, high-density message handling
  - Flexible attachment handling, batching, and workflow automation
  - Advanced sorting models with rules, prioritization, and message triage logic
  - Thread-aware behavior that keeps conversations coherent while preserving dense desktop organization

Plasma Impresario should adapt The Bat! patterns as a desktop-first productivity layer rather than copy its full classic workflow. The goal is to keep advanced organization, batching, and filtering available without creating a cluttered or intimidating interface.

## ePim and workspace-oriented patterns

- Project: https://www.epim.com/
- Why it matters: ePim is a useful reference for structured mail organization, local-mail workflow discipline, and richer personal-information management patterns.
- Takeaways:
  - Strong support for organized mail structures and local storage patterns
  - Useful conventions for tags, grouping, and personal organization systems
  - Practical ideas for message categorization beyond basic folders
  - A desktop-oriented mindset that can complement KDE and Plasma workflows

Plasma Impresario should borrow the organization ideas from ePim without forcing a heavy enterprise-style system. The goal is a cleaner mail model that supports tags, local mail organization, and workspace-aware context.

## Design synthesis

Plasma Impresario should combine:

- The KDE-native and IMAP-first discipline of Trojitá
- The clarity and quick-action flow of Aqua Mail
- The cross-account organization and searchable views of Vivaldi Mail
- The power-user density and organization model of The Bat!
- The desktop integration and KDE polish of Kontact

This should produce a client that feels fast, familiar, and well-integrated with Plasma without simply copying any single application.
