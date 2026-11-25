# Application inspirations

**[Documentation](../README.md)** · **[Scope](scope.md)** ·
**[Designs](designs.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Contributing](../../CONTRIBUTING.md)**

These products inform workflow, layout and implementation decisions. They are
references, not feature-cloning specifications or claims about Plasma
Impresario's current features; the [designs](designs.md) record what is
implemented and what is planned.

References are grouped by how directly they fit Plasma Impresario's desktop,
mail workflows and codebase. Relevance is not a ranking of application quality
or an implementation priority, and references within a group are not ranked.

- **Foundational:** the codebase Impresario is forked from and the Plasma shell
  and integration it targets.
- **Selective:** mail workflow and organization patterns, adapted to
  keyboard-accessible, Plasma-native desktop use. Their platforms, protocols and
  full feature sets are not requirements.
- **Focused:** narrow implementation techniques, not product or architecture
  models.

| Reference                                                              | Relevance    | Kind                                 | Main lesson for Plasma Impresario                           |
| ---------------------------------------------------------------------- | ------------ | ------------------------------------ | ----------------------------------------------------------- |
| [KDE / Trojitá](#kde--trojitá)                                         | Foundational | Qt IMAP client; the forked base      | IMAP correctness and a lightweight KDE-native baseline      |
| [Plasma Opus](#plasma-opus)                                            | Foundational | Plasma file manager                  | Five-region shell, panels and toolbar placement             |
| [Kontact](#kontact)                                                    | Foundational | KDE personal-information suite       | Plasma integration and PIM context                          |
| [Aqua Mail](#aqua-mail)                                                | Selective    | Mobile-first mail client             | Readable density and quick triage actions                   |
| [Vivaldi Mail](#vivaldi-mail)                                          | Selective    | Browser-integrated desktop mail      | Cross-account organization and saved, indexed searches      |
| [Thunderbird](#thunderbird)                                            | Selective    | Desktop mail client                  | Optional tabs for multiple mail views                       |
| [eM Client](#em-client)                                                | Selective    | Desktop mail and productivity client | Contextual history, follow-up and recoverable spam controls |
| [The Bat!](#the-bat)                                                   | Selective    | Classic power-user desktop client    | Dense, keyboard-first, rule-driven processing               |
| [ePim](#epim)                                                          | Selective    | Personal-information manager         | Tags, local organization and workspace context              |
| [E-Mail](#e-mail)                                                      | Selective    | Rust and GTK IMAP and SMTP client    | Safe defaults, guided setup and offline-first sync          |
| [Qt implementation examples](#focused-qt-mail-implementation-examples) | Focused      | Qt mail clients and prototypes       | Renderer isolation, CID handling and responsive search      |

## Foundational: architecture and Plasma experience

### KDE / Trojitá

**Project:** <https://github.com/KDE/trojita>

Trojitá is the closest baseline because Plasma Impresario is forked from it: a
Qt-based IMAP client built in the KDE ecosystem.

- Local-first, desktop-native mail interaction
- Clear separation between account/folder navigation and message reading
- Emphasis on IMAP correctness and protocol cleanliness
- KDE-style behavior as a lightweight desktop client

### Plasma Opus

**Project:** <https://github.com/mikepadiernos/plasma-opus>

Plasma Opus is a Plasma file manager whose shell is the design reference for a
consistent companion-app experience.

- The five-region layout, configurable panels and toolbar placement, adapted to
  mail navigation, message lists and reading
- Theme profiles and query syntax that Impresario can share
- A target design, not an assumed companion-app interface; Plasma Impresario
  stays useful without it

See the [shell design](designs.md#proposed-plasma-impresario-shell) and
[Plasma Opus integrations](../integrations/plasma-opus.md).

### Kontact

**Project:** <https://kontact.kde.org/>

Kontact is the most relevant KDE reference for cross-application behavior and
desktop integration.

- Fit with KDE and Plasma conventions
- Integration-minded interaction modes and common desktop patterns
- Clean treatment of personal-information management tasks
- Alignment with KDE account and system integration expectations

## Selective: mail workflows and organization

### Aqua Mail

**Project:** <https://www.aqua-mail.com/> ·
**Reference:**
[Customization](https://www.aqua-mail.com/android/features/customizations)

Aqua Mail is a reference for clear, mobile-first mail workflows. Adapt its
patterns to a desktop-first KDE layout rather than copying a mobile design:

- Readable message-list density and multi-line summaries without losing action
  clarity
- Quick actions in the message list: archive, move, flag, mark read and delete
- A compact, low-noise triage workflow with fast search and filters
- Message grouping and conversation visibility with little visual clutter
- Account grouping and mailbox navigation that keeps actions close to the
  message list
- Per-account and per-folder notification settings that limit interruptions
- Attachments and message content loaded on demand rather than all at once

See the [message list](designs.md#message-list-presentation) and
[customization](designs.md#aqua-mail-inspired-desktop-customization) designs.

### Vivaldi Mail

**Project:** <https://vivaldi.com/features/mail/>

Vivaldi Mail offers desktop workflows for managing and finding messages across
high-volume, multi-account inboxes.

- Unified access to multiple IMAP and POP3 accounts
- Indexed mail and saved searches for quick retrieval, including offline
- Distinct unseen and unread indicators
- View toggles and multiple views of a message without moving it
- Keyboard shortcuts, navigation history and queued outgoing messages

### Thunderbird

**Project:** <https://www.thunderbird.net/en-US/>

Thunderbird is a desktop reference for keeping multiple mail and productivity
views accessible in tabs.

- Switch between open mail views and messages without losing context.
- Keep the tab bar unobtrusive when it is not needed; Thunderbird's [main-window
  guide](https://support.mozilla.org/en-US/kb/getting-started-thunderbird-main-window-supernova)
  describes its tab bar and mail layout.
- Adapt tab navigation to Plasma Impresario's pane layout rather than
  reproducing Thunderbird's interface.

Tabs are a planned optional feature, **off by default**; that default is a
project decision, not a Thunderbird claim. See
[optional tabs](designs.md#optional-tabs).

### eM Client

**Project:** <https://www.emclient.com/?lang=en> · **References:**
[feature overview](https://www.emclient.com/features-overview),
[email features](https://www.emclient.com/features-email?lang=en) and
[spam controls](https://www.emclient.com/blogpost.aspx?id=748)

eM Client is a desktop reference for contextual mail information, triage and
follow-up workflows.

- Communication and attachment history for the selected contact
- A command palette, pinning, snoozing and reply reminders
- Templates and reusable text
- Local and server-side spam controls that inform a recoverable, account-aware
  design

Provider capabilities, cross-device behavior and integrations need separate
validation. See the [feature candidates and spam
recommendation](designs.md#em-client-inspired-feature-candidates).

<!-- markdownlint-disable MD026 -->

### The Bat!

<!-- markdownlint-enable MD026 -->

**Project:** <https://www.ritlabs.com/en/products/thebat/>

The Bat! provides a classic power-user interface with a traditional desktop
layout and rule-driven organization. Adapt it as a desktop-first productivity
layer rather than copying its full classic workflow, keeping advanced
organization, batching and filtering available without a cluttered or
intimidating interface:

- Dense, keyboard-first processing that prioritizes information density
- Folder-based organization for large volumes and active queues
- Rule-based filtering, automatic sorting, prioritization and triage logic
- Bulk actions, batching, flexible attachment handling and workflow automation
- Thread-aware behavior that keeps conversations coherent within dense folder
  organization

### ePim

**Project:** <https://www.epim.com/>

ePim is a reference for structured mail organization, local-mail discipline and
personal-information management. Borrow its organization ideas without forcing a
heavy enterprise-style system.

- Organized mail structures and local storage patterns
- Conventions for tags, grouping and personal organization
- Message categorization beyond basic folders
- A desktop-oriented mindset that complements KDE and Plasma workflows

### E-Mail

**Project:** <https://github.com/digistalter/email>

E-Mail is a small IMAP and SMTP client for the Linux desktop, written in Rust
with GTK 4 and libadwaita. It is a reference for safe defaults, guided account
setup and an offline-first local store.

- Server settings discovered from the email address, with a real sign-in check
  before an account is saved
- Recent mail first, older headers in the background and an outbox that never
  sends twice
- Header, link and local-file hardening backed by a hostile-mail test suite
- Quiet, combined notifications and short undo for moves and deletions

> [!IMPORTANT]
>
> E-Mail is licensed GPL-3.0-or-later. Plasma Impresario adopts its ideas, not
> its code; implement each pattern independently in Qt and KDE Frameworks.

See [Adoptable E-Mail patterns](#adoptable-e-mail-patterns) for the patterns
to adopt.

## Focused: Qt mail implementation examples

| Project                                                        | Relevant reference                                                                                                                   |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| [Korrespond](https://github.com/topke63/Korrespond)            | Qt Quick and KMime message rendering, remote-image controls, and MIME-based reply text.                                              |
| [Javelin Mail](https://github.com/ripdog/Javelin-Mail)         | Qt Widgets WebEngine viewer, inline-resource scheme, context menus, and asynchronous search; its backend uses JMAP rather than IMAP. |
| [Mailove](https://github.com/nekromoff/mailove)                | Qt Quick WebEngine sandbox, CID scheme, request interceptor, and network-egress tests.                                               |
| [LoveLetter](https://github.com/guoyunhe/loveletter)           | Early KDE/Qt Quick mail interface prototype; its message page is a placeholder rather than a renderer implementation.                |
| [Argonaut Emails](https://github.com/jgiannop/argonaut-emails) | Qt Widgets IMAP client with a private WebEngine profile, CID handler, remote-content controls, and viewer tests.                     |

> [!CAUTION]
>
> Adapt concepts to Trojitá's on-demand IMAP model and verify message isolation
> and privacy independently. Review each project's license before reusing code.

## Adoptable E-Mail patterns

Adapt these patterns to Trojitá's IMAP model and KDE Frameworks. Each has a
roadmap item and a board issue.

| Area            | Pattern                                                                                                                  | Roadmap                                                                                                                                                                                                                                                                                |
| --------------- | ------------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Threading       | References first, then a subject fallback that strips localized prefixes and list tags within a time window              | [M04](../ROADMAP.md#m04-reading-and-conversation-flow) [#369](https://github.com/mikepadiernos/plasma-impresario/issues/369)                                                                                                                                                           |
| Reader          | Mail without its own background on a light page in dark color schemes                                                    | [M04](../ROADMAP.md#m04-reading-and-conversation-flow) [#370](https://github.com/mikepadiernos/plasma-impresario/issues/370)                                                                                                                                                           |
| Background      | Opt-in background running and autostart through the Background portal or XDG autostart                                   | [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support).01 [#371](https://github.com/mikepadiernos/plasma-impresario/issues/371)                                                                                                                                             |
| Account setup   | Discovery from providers, autoconfig, ISPDB, SRV and MX, with app-password guidance and confirmation of unsigned servers | [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support).05 [#372](https://github.com/mikepadiernos/plasma-impresario/issues/372)                                                                                                                                             |
| Hardening       | Header sanitizing, Bcc removal, Message-IDs without the host name, real link targets and user-only local files           | [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support).06 [#373](https://github.com/mikepadiernos/plasma-impresario/issues/373) [#374](https://github.com/mikepadiernos/plasma-impresario/issues/374) [#375](https://github.com/mikepadiernos/plasma-impresario/issues/375) |
| Notifications   | One updatable notification per account that stays quiet for old and open mail                                            | [M07](../ROADMAP.md#m07-output-notifications-and-rules-automation) [#377](https://github.com/mikepadiernos/plasma-impresario/issues/377)                                                                                                                                               |
| Outbox and sync | A persistent outbox that never sends twice; recent headers first and older ones in the background                        | [M10](../ROADMAP.md#m10-reliability-sync-and-notification-quality) [#378](https://github.com/mikepadiernos/plasma-impresario/issues/378) [#379](https://github.com/mikepadiernos/plasma-impresario/issues/379)                                                                         |
| Testing         | AT-SPI end-to-end scenarios with screenshots; the Ubuntu AppArmor profile for the web-engine sandbox                     | [M11](../ROADMAP.md#m11-release-readiness-and-polish) [#380](https://github.com/mikepadiernos/plasma-impresario/issues/380) [#381](https://github.com/mikepadiernos/plasma-impresario/issues/381)                                                                                      |
| Sign-in check   | A real IMAP and SMTP sign-in before saving, with failures explained by stage                                             | [M14](../ROADMAP.md#m14-refinements).03 [#382](https://github.com/mikepadiernos/plasma-impresario/issues/382)                                                                                                                                                                          |
| Undo            | Moves and deletions applied locally and sent after a short delay so they can be undone                                   | [M14](../ROADMAP.md#m14-refinements).04 [#383](https://github.com/mikepadiernos/plasma-impresario/issues/383)                                                                                                                                                                          |

E-Mail's one database per account, which lets account removal delete all of its
local data, supports the proposal in caveat
[#262](https://github.com/mikepadiernos/plasma-impresario/issues/262). Its
search patterns are listed under [Search patterns](#search-patterns).

## Search patterns

Search in [M05](../ROADMAP.md#m05-search-and-indexing) starts from Trojitá's
per-mailbox IMAP quick search and adds the patterns below. Plasma Opus
contributes its query syntax and indexing conventions; see [Plasma Opus search
and indexing](../integrations/plasma-opus.md#search-and-indexing).

| Reference         | Pattern                                                                                                                                                                               | Roadmap                 |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------- |
| Trojitá           | Server-side quick search per mailbox with field toggles, raw IMAP queries and fuzzy matching                                                                                          | Baseline kept in M05.01 |
| Thunderbird       | Global search with facets, saved searches as virtual folders, a match-all or match-any search builder and quick filters                                                               | M05.02, M05.03, M05.05  |
| Vivaldi Mail      | A full-text index across accounts, cross-account results and mailing-list views                                                                                                       | M05.01, M05.03, M05.04  |
| Aqua Mail         | Local results first with optional server search, and search history                                                                                                                   | M05.02, M05.04          |
| The Bat!          | Detailed search conditions and rules created from searches                                                                                                                            | M05.02, M07             |
| KMail and Kontact | Search folders over an index and desktop search integration                                                                                                                           | M05.03, M05.04, M06.01  |
| Javelin Mail      | Asynchronous search that keeps the interface responsive                                                                                                                               | M05.01                  |
| E-Mail            | Local full-text results first, server search only for folders not fully stored, merged without duplicates; user input quoted so it is never read as query syntax; highlighted matches | M05.01, M05.02          |

## Design synthesis

Plasma Impresario combines these influences into a client that feels fast,
familiar and integrated with Plasma without copying any single application.

| Reference    | Contribution                                       | Where it applies                                                                                                                                                                                                                                                                                                  |
| ------------ | -------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Trojitá      | KDE-native, IMAP-first discipline                  | The [M01](../ROADMAP.md#m01-kde--qt6-compatibility-baseline) baseline and the [current interface](designs.md#current-interface)                                                                                                                                                                                   |
| Plasma Opus  | Shell regions, panels and companion workflows      | The [shell](designs.md#proposed-plasma-impresario-shell) in M02 and [Plasma Opus integrations](../integrations/plasma-opus.md)                                                                                                                                                                                    |
| Aqua Mail    | Clarity, quick actions and unified triage          | Unified inbox and quick actions in [M03](../ROADMAP.md#m03-mail-operations-sorting-and-folder-management)                                                                                                                                                                                                         |
| Vivaldi Mail | Cross-account organization and searchable views    | Saved searches in M02; cross-account search, an opt-in index and list views in [M05](../ROADMAP.md#m05-search-and-indexing)                                                                                                                                                                                       |
| Thunderbird  | Optional multi-view tabs                           | [Optional tabs](designs.md#optional-tabs) in [M08](../ROADMAP.md#m08-settings-and-workspace-configuration)                                                                                                                                                                                                        |
| eM Client    | Contextual history, follow-up and spam controls    | [Feature candidates](designs.md#em-client-inspired-feature-candidates) and spam controls in [M14](../ROADMAP.md#m14-refinements).07                                                                                                                                                                               |
| The Bat!     | Power-user density, batching and rules             | Batch workflows in M03, rules in [M07](../ROADMAP.md#m07-output-notifications-and-rules-automation) and the MailTicker-inspired widget in [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)                                                                                                       |
| Kontact      | Desktop integration and KDE polish                 | KDE services and agenda/contacts in [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)                                                                                                                                                                                                             |
| ePim         | Tags, local organization and workspace context     | [My Day / Agenda](designs.md#embedded-panels) and workspaces in [M08](../ROADMAP.md#m08-settings-and-workspace-configuration)                                                                                                                                                                                     |
| E-Mail       | Safe defaults, guided setup and offline-first sync | Account setup in [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support) and [M14](../ROADMAP.md#m14-refinements), notifications in [M07](../ROADMAP.md#m07-output-notifications-and-rules-automation), and outbox and sync order in [M10](../ROADMAP.md#m10-reliability-sync-and-notification-quality) |
