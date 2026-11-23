# Plasma Impresario roadmap

**[Documentation](README.md)** · **[Scope](philosophy/scope.md)** ·
**[Inspirations](philosophy/inspirations.md)** ·
**[Caveats and security](CAVEATS.md#security-and-data-protection)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

Plasma Impresario is an in-development mail client for the KDE Plasma desktop only,
built with C++, Qt 6 and KDE Frameworks. Other desktop environments are outside
the supported product scope. This roadmap orders foundational mailbox work
before desktop integration, productivity features and release-quality refinements.

> [!NOTE] Feature completion is synchronized with the project board. An item
> is checked only when its issue is **Completed** or **Merged** and its
> implementation is verified. Blocked or unverifiable work remains recorded
> in [pending tasks](TODO.md).
> Check the checkbox only after the roadmap item is completed, following issue
> review and merge state. Do not mark it complete solely because implementation
> work has started or is in review.
> The source already contains a substantial Trojitá IMAP client; the
> [backend](parity/backend.md) and [UI](parity/ui.md) references distinguish
> that baseline from planned Plasma Impresario features.

## Contents

- [Milestones](#milestones)
- [M01-M05: Foundations](#m01-kde--qt6-compatibility-baseline)
- [M06-M10: Workflows and quality](#m06-output-notifications-and-rules-automation)
- [M11-M12: Assets and refinements](#m11-nextcloud-and-digital-asset-management)

## Milestones

This roadmap is ordered by product dependency. Compare proposed milestones
with the current baseline described in the parity guides before treating
foundational mailbox functionality as entirely new.

## Recommended priority

| Priority | Focus | Recommended next work |
| --- | --- | --- |
| **P0** | Reproducible, safe baseline | Confirm product identity and supported build matrix; establish verified TLS and safe credential defaults; document cache and logging behavior; add focused connection/security and launch smoke tests |
| **P1** | Supported rendering and account boundaries | Complete the Qt WebEngine reader or provide a safe text fallback; design stable per-account settings, cache and credentials before adding simultaneous accounts |
| **P2** | Shell and reliability | Prototype the Plasma Impresario layout over existing mailbox/list/reader behavior; validate accessibility, keyboard use, responsive layouts and partial-failure recovery |
| **P3** | Optional productivity features | Add provider APIs, rules, Nextcloud, calendar/contacts and workspace features only after core reliability, permissions and privacy behavior are defined |

These are recommended sequencing priorities, not issue statuses or feature
completion markers. Follow the project workflow below before updating milestone
completion.

The issue synchronization handoff is tracked in [TODO](TODO.md#milestone-feature-issue-synchronization).

### M01: KDE / Qt6 compatibility baseline

**Project:** [M01 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314445774)

**Goal:** Deliver a reproducible Qt 6 / KDE build that users can install,
launch, and use to read mail. Establish the compatibility and smoke-test gate
for later milestones.

**Current blocker:** The WebEngine reader builds, installs to a temporary
prefix, launches, and passes the full test suite. A live message-reading flow
with an account remains unverified, so the M01.02 feature and M01 release gate
remain in Development.
The remaining release checks and integration handoff are in
[TODO](TODO.md#m01-release-gate-and-integration).

Work through the supported environment, renderer, installer, and verification
in that order. Installer improvements can proceed in parallel, but a complete
installation depends on the working renderer.

- **M01.01: Supported environment and dependencies**

  **Project:** [Subsection](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314566982) · [Dependency audit](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314499124) ·
  [Compatibility matrix](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314538695)

  - [x] Define supported distributions, package managers, compilers, Qt 6 and KDE
    Frameworks versions, and KDE Plasma versions and sessions. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314538695)
  - [x] Audit CMake, SDK assumptions, and package names and versions for each
    supported distribution. Separate required dependencies from optional
    integrations and report unsupported or missing requirements clearly. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314499124)
  - [x] Verify dependencies and compatibility limitations for IMAP, local storage,
    message rendering, and desktop services. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568575)
  The [initial compatibility baseline](philosophy/compatibility.md) records the
  first target, component evidence, and outstanding full-reader verification.

- **M01.02: Qt WebEngine message renderer**

  **Project:** [Renderer port](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314539659)

  The WebEngine reader is integrated with per-message profiles and request
  policy. The renderer-enabled build, all 35 CTest cases, temporary-prefix
  installation, executable startup, and installer checks passed. Live
  message-reading, reply, and attachment smoke tests remain unverified; the
  issues remain in Development and their roadmap items unchecked. See
  [TODO](TODO.md#in-progress).

  1. [x] Define the WebEngine profile and request lifecycle, including per-message isolation, cancellation, and cleanup when switching messages. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568588)
  2. [x] Add regression tests for MIME loading, cancellation, message switching, remote-content policy, navigation, quoting, and rendering failures. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568809)
  3. [x] Update CMake to use Qt6::WebEngineWidgets and retain the explicitly labeled renderer-free build. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568819)
  4. [x] Register and implement MIME-part and `cid:` URL handling using the existing IMAP model, including asynchronous fetching, errors, and inline images. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568598)
  5. [x] Enforce message privacy: disable scripts, plugins, and persistent web storage; block remote and local-file access by default; allow remote content only after explicit per-message consent, including redirects. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568610)
  6. [x] Port the message view, headers, source view, and rendering layout to WebEngine while preserving MIME alternatives, nested messages, and attachments. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568620)
  7. [x] Replace synchronous WebKit APIs for reply quoting, selection, search, links, context menus, zoom, and styling with WebEngine-compatible behavior. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568798)

- **M01.03: Installer**

  **Project:** [Installer](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314544368) ·
  [Package manager](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314445891) ·
  [Renderer fallback](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314446004)

  - [x] Complete the existing installer for dependency setup, configuration, build,
    and installation. Provide flags for check-only dependency inspection,
    skipping dependency handling, and building without installing the app.
    Surface errors and request administrator privileges only when needed.
    [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568548)
  - [x] Detect the appropriate supported package manager, prefer Arch/CachyOS
    tooling where applicable, and install only missing dependencies without
    falling back to an unrelated package manager. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314445891)
  - [x] Support user and system prefixes; verify executable and plugin loading,
    runtime libraries, desktop entries, icons, translations, handbook, and man
    page. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568559)
  - [x] Document prerequisites, installation options, troubleshooting, and
    distribution-specific risks. Keep renderer-free builds explicitly labeled
    as unable to display messages. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314446004)
  - [x] Integrate the WebEngine dependency and full reader once the port is ready;
    installing a renderer-free build does not satisfy this milestone. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568565)

  The installer checks for installed packages and provides explicit WebEngine
  and renderer-free options. The latter is an opt-in incomplete build, not the
  default or a substitute for the reader. Remaining validation
  and compatibility work is tracked in
  [mikepadiernos/plasma-impresario#2](https://github.com/mikepadiernos/plasma-impresario/issues/2)
  and [mikepadiernos/plasma-impresario#3](https://github.com/mikepadiernos/plasma-impresario/issues/3).

- **M01.04: Verification and release gate**

  **Project:** [Subsection](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568447) · [Build and launch](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498915) ·
  [Installation and smoke tests](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314539763)

  - [x] Verify clean builds and installs across the observed CachyOS target and
    package managers, including user and system prefixes and the renderer-free
    configuration. Other distribution families and protected system prefixes remain
    pending. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314539763)
  - [x] Check package-manager selection, already-installed and missing dependencies,
    failure handling, and explicit renderer options without modifying the
    system during automated tests. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568832)
  - [ ] Smoke-test launch, account setup, message reading, replies, attachments,
    desktop integration, and recovery from failures. Run renderer and privacy
    regression tests. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498915)
  - [ ] Establish reproducible warning, formatting, test coverage, and dynamic
    analysis commands for clean Debug builds. Use AddressSanitizer and
    UndefinedBehaviorSanitizer for supported desktop configurations; use
    ThreadSanitizer for targeted concurrency investigations where it is
    practical. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498915)
  - [x] Record tested environments, reproducible verification steps, and remaining
    protocol, storage, rendering, and desktop-service limitations. Later
    milestones depend on this gate. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568837)

### M02: Shell, accounts, and core mailbox foundation

**Project:** [M02 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314497855) ·
[Mail shell](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314539964) ·
[Accounts and mailbox state](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540057)

- [ ] Build the header, left navigation, main message list, preview pane, and footer/status layout [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314539964)
- [ ] Define account models, connection state, folder structure, and mailbox lifecycle [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540057)
- [ ] Support multiple simultaneous mail accounts in one window with independent account state and configuration [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314607650)
- [ ] Design protocol-aware account and capability models that preserve existing IMAP behavior and allow POP3 and JMAP integrations in M05 [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314594590)
- [ ] Support OAuth account authentication with secure desktop authorization, per-account token storage, renewal, and recovery [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314595922)
- [ ] Implement the app skeleton in the same overall layout as the design spec [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568848)
- [ ] Support compact and comfortable density modes before moving into feature-heavy workflows [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568860)
- [ ] Add folder counts, unread counts, and saved-search entry points to the mailbox foundation [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568869)
- [ ] Keep this milestone as the blocker for all later mailbox productivity work [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569071)
- [ ] Provide account setup, editing, removal, identities, and secure credential storage for IMAP and sending accounts [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569080)
- [ ] Expose connection, authentication, offline, and loading states without blocking the interface [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569089)
- [ ] Support account grouping, collapsible folder navigation, and keyboard focus across the shell [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569100)
- [ ] Preserve pane layout, selection, and density preferences across sessions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569110)

### M03: Mail operations, sorting, and folder management

**Project:** [M03 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314497950) ·
[Folders and views](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540205) ·
[Sorting and triage](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540321)

- [ ] Implement folder navigation for inbox, sent, drafts, trash, and custom folders [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540205)
- [ ] Provide an Aqua Mail-inspired unified inbox that combines selected accounts while retaining the source account and folder of each message [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314607669)
- [ ] Add sorting, filtering, selection, unread state, and bulk actions for message lists [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540321)
- [ ] Support folder sync, account grouping, and desktop-heavy triage behavior [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569116)
- [ ] Add quick actions for archive, move, flag, mark read, and delete [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569131)
- [ ] Include The Bat!-style power-user workflows: batch actions, filtering, and dense desktop processing [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569139)
- [ ] Add advanced sorting and threading controls for date, relevance, and conversation grouping [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569324)
- [ ] This milestone is the blocker for all reading/search and productivity work that depends on real mailbox behavior [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569335)
- [ ] Provide readable compact message rows with clear sender, subject, date, unread, flag, and attachment indicators [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569342)
- [ ] Add predictable multi-selection, keyboard shortcuts, and progress or error feedback for batch operations [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569349)
- [ ] Define folder actions and confirmation or recovery behavior for destructive message operations [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569359)
- [ ] Support account and folder scoped views and preserve selection and sort state during message updates [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569367)

### M04: Reading, search, and conversation flow

**Project:** [M04 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498050) ·
[Reading](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540493) ·
[Search and conversations](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540557) ·
[Composition and sending](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540653)

- [ ] Build the reading pane with metadata, preview controls, attachments, and action buttons [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540493)
- [ ] Add search, thread-aware grouping, saved filters, and conversation navigation [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540557)
- [ ] Improve the preview panel for readability and fast triage [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569371)
- [ ] Ensure keyboard navigation works for desktop-heavy usage [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569380)
- [ ] Validate the full reading flow from list selection through reading and response actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569544)
- [ ] Load message bodies and attachments on demand and show clear loading, unavailable, and error states [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569551)
- [ ] Support compose, reply, reply-all, forward, drafts, sending, and failed-send recovery [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540653)
- [ ] Preserve the originating account and sending identity in reading, replies, drafts, and composition across individual and unified mail views [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314607717)
- [ ] Provide scoped search by sender, recipient, subject, date, and message content where supported [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569556)
- [ ] Keep threaded and flat views navigable and preserve conversation context across filters [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569569)
- [ ] Protect reading privacy with explicit remote-content and external-link handling [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569578)

### M05: Desktop integration and provider support

**Project:** [M05 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498116) ·
[KDE and Plasma](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540788) ·
[Mail providers](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540857) ·
[Security and privacy](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540934)

- [ ] Connect the app to KDE and Plasma system services, notifications, and account lifecycle behavior [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540788)
- [ ] Add provider support for Gmail, Outlook, and other major mail services alongside IMAP [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540857)
- [ ] Support consumer Outlook.com, Hotmail, and Live accounts using provider-supported OAuth and mail interfaces [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314595964)
- [ ] Support Exchange Online and on-premises Exchange Server mail accounts using supported interfaces and authentication, subject to server and tenant policies [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314595997)
- [ ] Support POP3 mail retrieval with SMTP submission, secure authentication and transport, and explicit local-storage and server-retention behavior [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314594687)
- [ ] Support JMAP mail accounts with capability discovery, secure authentication, mailbox and message synchronization, and supported sending options [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314594740)
- [ ] Define workspace-aware behavior for cross-context desktop usage and Plasma conventions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569584)
- [ ] Add ClamAV and spam-handling hooks for safer mail processing and filtering [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314540934)
- [ ] Keep service integrations in this milestone so provider and desktop features are not mixed into core app deliverables [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569594)
- [ ] Use KDE-native file, credential, session, and notification services where supported [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569603)
- [ ] Implement KDE Plasma desktop integration for session lifecycle, file dialogs, KIO, KConfig, KWallet, and Plasma notification conventions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569755)
- [ ] Define Plasma mail integration expectations for rich desktop feedback without interrupting the reading workflow [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569766)
- [ ] Support accessibility and theming parity with KDE/Plasma, including contrast, focus visibility, localization, and icon consistency [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569771)
- [ ] Document provider authentication, capabilities, limitations, and setup for Gmail, Outlook, and generic IMAP/SMTP [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569776)
- [ ] Define how scan results, unavailable scanners, spam classification, and user overrides are surfaced [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569785)
- [ ] Avoid exposing message content or attachments to external services without informed user action [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569792)

### M06: Output, notifications, and rules automation

**Project:** [M06 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498225) ·
[Output and notifications](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541075) ·
[Rule execution](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541165)

- [ ] Define the output surfaces for sync status, alerts, notifications, and background job feedback [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541075)
- [ ] Add a rules engine for sender, subject, attachment, folder, and tag-based message automation [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541165)
- [ ] Support actions such as move, flag, archive, notify, and tag assignment [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569796)
- [ ] Keep notification and automation behavior separate from the core shell and reader milestones [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569801)
- [ ] This milestone is a blocker for workflow tuning, spam handling, and high-volume mail management [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569967)
- [ ] Support per-account and per-folder notification controls, with summaries that avoid interrupting focused work [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569976)
- [ ] Define rule order, matching conditions, actions, and account or folder scope for incoming mail [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569988)
- [ ] Preview or test rule matches and report execution failures before rules perform destructive actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314569999)
- [ ] Make background sync, scanning, and rule activity visible with actionable error and progress states [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570007)

### M07: Settings and workspace configuration

**Project:** [M07 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498306) ·
[Mail settings](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541330) ·
[Workspace integrations](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541421)

- [ ] Design the settings surface using a Plasma Opus-style layout: grouped, readable, and extensible [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541330)
- [ ] Cover appearance, density, mail behavior, notifications, workspaces, and productivity preferences [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570018)
- [ ] Define account-specific options and default configuration for the app shell [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570030)
- [ ] Keep the settings model separate from the core feature work so it can evolve after the app is working [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570033)
- [ ] Group account, appearance, reading, composition, privacy, notifications, and integration settings consistently [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570214)
- [ ] Provide compact, comfortable, and focus layout preferences with accessible defaults [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570223)
- [ ] Support per-account and per-folder behavior where appropriate and explain inherited defaults [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570232)
- [ ] Validate settings persistence, account isolation, and behavior when an integration is unavailable [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570240)
- [ ] Define product-level workspace settings for mail, notes, contacts, and
  cross-app context as part of a Plasma Opus-aware workflow. Allow email
  accounts to be attached to workspaces and thereby associated with the
  corresponding Plasma Activities.
  [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541421)
- [ ] Add defaults and toggles for mail-to-calendar, mail-to-notes, and work-context-aware productivity actions when those integrations are enabled [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570247)
- [ ] Keep unavailable or optional integrations gracefully hidden and clearly explained in the settings surface [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570281)

### M08: Themes and branding foundation

**Project:** [M08 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498380) ·
[Plasma visual language](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541568) ·
[Accessible density](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541647)

- [ ] Rename the application, installed identity, documentation, repository, and project board to Plasma Impresario while preserving Trojitá configuration and compatibility aliases [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314606649)
- [ ] Define the default visual language, palette, and KDE-native desktop styling [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541568)
- [ ] Establish spacing, density, and readability rules for all mail panes.
  Provide theme spacing options for compact, normal, comfortable, and custom
  spacing. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541647)
- [ ] Align the product shell with Plasma visual expectations for polish and
  clarity. Replace the carets beside left sidebar icons that open menus with
  small theme-accented corner markers on the icons. The marker indicates an
  available menu, not selection. Preserve contrast, accessible menu semantics,
  keyboard access, and visible focus; retain disclosure indicators for
  expandable trees.
  [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570268)
- [ ] Follow the active Plasma color scheme, accent, icon theme, and light or
  dark appearance. Provide an option to use a symbolic icon from the active
  icon theme in the system tray.
  [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570286)
- [ ] Validate contrast, text scaling, focus indicators, and high-contrast behavior across panes [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570471)
- [ ] Keep compact and comfortable layouts readable across common window sizes and display scales [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570493)

### M09: Reliability, sync, and notification quality

**Project:** [M09 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498466) ·
[Connection and recovery](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541795) ·
[Mailbox performance](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541868)

- [ ] Harden retry behavior, offline handling, and background task reporting [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541795)
- [ ] Improve connection-state reporting, folder sync stability, and message consistency [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570499)
- [ ] Validate performance and reliability across real-world email workloads [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314541868)
- [ ] This milestone is the stabilization gate before broader product expansion [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570496)
- [ ] Test intermittent connections, expired credentials, server errors, retries, and recovery without silent message loss [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570514)
- [ ] Verify folder and unread counts remain consistent across multiple accounts and concurrent changes [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570519)
- [ ] Measure responsiveness and resource use for large mailboxes, long threads, bulk actions, and attachments [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570539)
- [ ] Confirm failed background operations remain visible and can be retried or resolved [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570527)
- [ ] Exercise memory, lifetime, undefined-behavior, and data-race-sensitive
  network, cache, and Qt background-task paths with suitable sanitizers before
  extending concurrency or sync behavior [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570499)

### M10: Release readiness and polish

**Project:** [M10 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314498549) ·
[Accessible workflows](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542030) ·
[Release guidance and triage](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542126)

- [ ] Review accessibility, localization, keyboard support, and final visual tuning [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542030)
- [ ] Validate the end-to-end workflow from account setup through reading, triage, and reply [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570681)
- [ ] Finalize release criteria, known issues, and handoff documentation [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542126)
- [ ] Exercise compose, send, attachments, search, rules, notifications, offline recovery, and supported providers end to end [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570692)
- [ ] Verify accessible navigation, screen-reader labels, keyboard-only workflows, localization, and scaled displays [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570702)
- [ ] Publish installation, upgrade, troubleshooting, security, and supported-platform guidance [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570712)
- [ ] Triage outstanding bugs and caveats against explicit release criteria before shipping [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570728)
- [ ] Require clean formatting, reproducible CMake builds, test coverage,
  supported dependency checks, and static analysis results in the release CI
  gate; document tools, environment, findings, and residual risk [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542126)

### M11: Nextcloud and digital asset management

**Project:** [M11 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314506848) ·
[Nextcloud assets](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542272) ·
[Permissions and transfers](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542357)

- [ ] Nextcloud and digital asset management [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570735)
- [ ] Connect accounts securely and browse authorized Nextcloud files without interrupting mail workflows [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542272)
- [ ] Attach or link existing files, and save received attachments to a user-selected Nextcloud location [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570744)
- [ ] Show upload, download, sharing permissions, failures, and retry states before completing file actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314542357)
- [ ] Respect access permissions and make shared-link visibility explicit before sending [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570751)
- [ ] Validate asset workflows with local and provider-backed mail accounts and document offline limitations [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570921)

### M12: Refinements

**Project:** [M12 milestone](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314499855)

Adapt the menu, layout, interaction, settings, performance, and testing principles
from [Plasma Opus refinements](https://github.com/mikepadiernos/plasma-opus-file-manager/blob/main/docs/ROADMAP.md#m32-refinements)
to mail workflows without blocking this milestone on unavailable features.

- **M12.01: Cross-product quality**

  **Project:** [Subsection](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314568448) · [Workflow friction](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500156) ·
  [Usability and accessibility](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500285) ·
  [Performance and reliability](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500423) ·
  [Integration validation](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500560) ·
  [Refinement findings](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500676)

  - [ ] Review end-to-end mail and available productivity workflows and prioritize remaining friction [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500156)
  - [ ] Refine cross-feature usability, accessibility, keyboard navigation, and visual consistency [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500285)
  - [ ] Tune performance, reliability, and resource usage across realistic workloads [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500423)
  - [ ] Validate provider, desktop, Nextcloud, and any available Plasma Opus integration behavior together [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500560)
  - [ ] Resolve refinement findings and document remaining limitations and acceptance results [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314500676)
  - [ ] Compare supported workflows with the inspiration principles and prioritize gaps without assuming feature parity [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570928)
  - [ ] Record reproducible acceptance results and regression coverage for each refinement area [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570935)
  - [ ] Inventory implemented workflows and map their entry points to automated or reproducible manual checks [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570947)
  - [ ] Benchmark cold and warm startup, first interactive window, and settings readiness; record the environment and compare improvements against the baseline [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570953)
  - [ ] Exercise successful operations, cancellation, partial failure, recovery, and persistence with isolated test data and synthetic credentials [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570959)
  - [ ] Review accessibility, translations, display scaling, optional dependencies, and representative desktop configurations; record environments not tested [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570967)
  - [ ] Run suitable static analysis and dependency checks and document security findings and residual risks without claiming testing proves security [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570972)
  - [ ] Use C++ Core Guidelines and project-specific KDE/Qt conventions for
    findings, and evaluate open static analyzers plus PVS-Studio only where
    licensing and host support allow. Report every analyzer limitation
    explicitly. [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314570972)

- **M12.02: Shell and interaction**

  **Project:** [Shell and interaction](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314502413)

  - [ ] Refine navigation, panes, toolbars, layouts, density, focus states, and keyboard interaction [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571161)
  - [ ] Preserve pane sizes, visibility, and focus across restarts and layout changes [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571171)
  - [ ] Make primary actions discoverable without crowding compact layouts; allow toolbar customization [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571181)
  - [ ] Verify keyboard-only navigation, visible focus, and predictable focus restoration after dialogs and actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571193)
  - [ ] Validate compact, comfortable, and focus modes at narrow window sizes and different display scales [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571207)
  - [ ] Keep menu and toolbar actions synchronized with the active account, folder, and selection, including while views refresh [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571217)
  - [ ] Preserve customized menus, toolbars, shortcuts, and saved layouts when defaults change or actions are added [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571227)
  - [ ] Restore windows to a usable visible position after display changes without overriding window-manager placement [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571245)
  - [ ] Keep controls usable with long translations, right-to-left layouts, and reduced-motion preferences [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571407)

- **M12.03: Accounts, sync, and storage**

  **Project:** [Accounts, sync, and storage](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314502624)

  - [ ] Refine account setup, authentication, provider compatibility, folder sync, offline recovery, and storage feedback [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571416)
  - [ ] Clarify account setup errors, authentication renewal, certificate failures, and provider-specific limitations [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571427)
  - [ ] Show per-account sync progress, last successful sync, offline state, and actionable retry options [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571432)
  - [ ] Preserve local changes during interruptions and make conflicts or unsent changes visible before recovery [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571437)
  - [ ] Explain local cache usage and allow users to manage storage without deleting server messages unintentionally [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571443)

- **M12.04: Mail organization and triage**

  **Project:** [Mail organization and triage](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314502794)

  - [ ] Refine folder navigation, message lists, selection, unread counts, quick actions, and bulk operations [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571453)
  - [ ] Keep unread counts and message selection consistent when folders update or accounts reconnect [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571460)
  - [ ] Make quick actions and keyboard shortcuts consistent across individual messages and bulk selections [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571630)
  - [ ] Show progress and partial failures for bulk operations, with confirmation or recovery for destructive actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571640)
  - [ ] Improve folder hierarchy, account grouping, and dense message indicators without obscuring message state [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571650)
  - [ ] Keep context-menu actions bound to the messages for which the menu opened, even if the selection changes [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571660)
  - [ ] Distinguish cancellation from rollback and identify completed, skipped, and failed messages in batch results [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571675)

- **M12.05: Reading, composition, and attachments**

  **Project:** [Reading, composition, and attachments](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314502998)

  - [ ] Refine message rendering, conversation navigation, compose and reply flows, drafts, attachment handling, and editor usability [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571688)
  - [ ] Preserve draft content and attachments across interruptions and clearly report send failures [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571693)
  - [ ] Improve conversation navigation, message metadata, quoted text readability, and keyboard access to reading actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571695)
  - [ ] Provide clear attachment loading, size, download, open, and error states; request confirmation for risky actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571846)
  - [ ] Validate reply, reply-all, and forward behavior, including recipient visibility and attachment handling [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571856)
  - [ ] Prevent delayed message or attachment results from replacing the current view after selection changes or cancellation [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571865)

- **M12.06: Search, sorting, and automation**

  **Project:** [Search, sorting, and automation](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314503192)

  - [ ] Refine search, saved filters, sorting, threading, and existing rules; define refinement criteria for potential tagging and advanced automation [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571876)
  - [ ] Make account and folder search scope, active filters, and unsupported search capabilities explicit [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571890)
  - [ ] Keep sort and thread order stable as messages arrive and preserve navigation context when filters change [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571904)
  - [ ] Allow users to inspect rule order and test matches before applying potentially destructive actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571912)
  - [ ] Define how potential tags and advanced automation would interact with folders, search, and existing rules [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314571919)

- **M12.07: Security and privacy**

  **Project:** [Security and privacy](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314503352)

  - [ ] Refine credential handling, remote-content controls, attachment safety, ClamAV and spam feedback, and privacy settings [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572097)
  - [ ] Use secure credential storage and explain authentication, certificate, and account-access errors [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572104)
  - [ ] Keep remote content blocked or user-controlled and make external-link destinations visible before opening [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572110)
  - [ ] Distinguish clean, suspicious, unscanned, and failed scan states without treating unavailable scanning as success [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572118)
  - [ ] Provide understandable spam controls and recovery from misclassification without exposing mail content unnecessarily [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572127)
  - [ ] Test untrusted message content, attachment names, links, and imported data across validation, permissions, and error boundaries [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572134)
  - [ ] Check that settings, logs, temporary files, and backups do not inadvertently expose credentials or message contents [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572145)

- **M12.08: Settings and notifications**

  **Project:** [Settings and notifications](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314503553)

  - [ ] Refine settings organization, defaults, workspace preferences, notifications, status output, and background-task feedback [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572150)
  - [ ] Make global, account, and folder settings distinguishable and explain inherited defaults [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572314)
  - [ ] Keep settings searchable and validate persistence, reset behavior, and unavailable integration states [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572320)
  - [ ] Offer per-account and per-folder notification controls with privacy-conscious message previews [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572328)
  - [ ] Surface sync, rule, and background-task failures with actionable details rather than silent status changes [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572339)
  - [ ] Explain Apply, OK, and Cancel behavior, including live previews and changes committed immediately by external dialogs [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572351)
  - [ ] Validate settings inline, explain unavailable options, and report save failures or conflicting external changes [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572362)
  - [ ] Confirm page-specific or global resets and distinguish app-owned settings from shared KDE data and credentials [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572372)
  - [ ] Keep settings search, keyboard navigation, and unsaved edits responsive while loading larger account or provider lists [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572381)

- **M12.09: Desktop, services, and assets**

  **Project:** [Desktop, services, and assets](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314503752)

  - [ ] Refine KDE and Plasma integration, provider behavior, and Nextcloud asset workflows; define refinement criteria for potential Plasma Opus integration [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572521)
  - [ ] Verify Plasma theme, icons, notifications, file dialogs, and session behavior against desktop conventions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572530)
  - [ ] Explain provider authentication, capability differences, and recovery when access expires [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572534)
  - [ ] Make Nextcloud file access, shared-link visibility, upload progress, and permission failures clear before sending [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572542)
  - [ ] Define a potential Plasma Opus integration contract for context, permissions, and behavior when the other app is unavailable [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572552)
  - [ ] Review mail integration with KDE Plasma services such as notifications, KIO, file handling, session integration, account storage, and desktop-level actions [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572563)
  - [ ] Define the user-facing contract for Plasma Opus-aware mail flows: calendar handoff, notes capture, contact lookup, workspace-aware context, and graceful fallback when a companion app is unavailable [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572568)

- **M12.10: Calendar and contacts**

  **Project:** [Calendar and contacts](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314503888)

  - [ ] Define refinement criteria for potential calendar, scheduling, contacts, and address-book workflows [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572573)
  - [ ] Define criteria for readable calendar views, time zones, reminders, and event conflict feedback [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572727)
  - [ ] Define criteria for contact search, recipient lookup, duplicate handling, and account-specific sources [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572739)
  - [ ] Define privacy, permissions, and error expectations for provider-backed event and contact data [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572749)
  - [ ] Describe how potential mail-to-event and contact-to-compose workflows should preserve context [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572758)

- **M12.11: Notes, journal, and workspaces**

  **Project:** [Notes, journal, and workspaces](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314504035)

  - [ ] Define refinement criteria for potential notes, journal, the Plasma Opus editor, and cross-app workspace behavior [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572767)
  - [ ] Define editor criteria for keyboard navigation, draft recovery, formatting consistency, and accessible text entry [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572781)
  - [ ] Define note and journal criteria for search, organization, autosave feedback, and storage privacy [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572787)
  - [ ] Specify potential workspace context switching, account boundaries, and state restoration across apps [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572799)
  - [ ] Define compatibility and failure expectations for sharing editor or workspace context with Plasma Opus [Board](https://github.com/users/mikepadiernos/projects/4?pane=issue&itemId=314572957)

This milestone follows the core app and Nextcloud integration. Refine implemented features here and document criteria for potential future work.

## Notes

Update this page with real milestones, owners, dependencies, and acceptance criteria as work begins.
