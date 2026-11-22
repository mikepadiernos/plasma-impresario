# Plasma Mail roadmap

This roadmap is a placeholder for planning the project.

## Status

- [ ] Define project scope and supported platforms
- [ ] Document baseline features and account integrations
- [ ] Capture mail workflow and UI goals
- [ ] Establish testing and release checkpoints

## Proposed phases

### Phase 1: Foundation

- Project structure and build flow
- Account configuration basics
- Main window and mail list layout

### Phase 2: Core workflows

- Message reading and threading
- Search, sorting, and filtering
- Folder management and synchronization

### Phase 3: Quality and polish

- Accessibility, notifications, and keyboard support
- Performance and layout tuning
- Translation and localization coverage

## Milestones

### M01: KDE / Qt6 compatibility baseline

- Confirm the project builds and runs against KDE and Qt 6 tooling
- Audit deprecated APIs, platform assumptions, and build requirements for Plasma desktop compatibility
- Define the minimum supported KDE / Qt6 environment and release gating criteria
- Establish a baseline compatibility report that all later milestones depend on

### M02: Themes and branding foundation

- Define the default Plasma Mail visual language, color palette, and desktop-native feel
- Establish branding tokens, app identity, and iconography for the mail client shell
- Set the rules for spacing, density, contrast, and readability across all mail panes
- Confirm the interface styling aligns with KDE / Plasma aesthetic expectations

### M03: Customizations and settings

- Design theme options, compact vs comfortable density modes, and personalization controls
- Define account-specific settings for layout, notifications, sorting, and message behavior
- Add configuration surfaces for appearance, mail actions, and general desktop integration preferences
- Validate that customizations remain consistent across account types and panel layouts

### M04: Integrations and account services

- Connect the app to KDE and Plasma integrations, system services, and account lifecycle flows
- Define hooks for sync, notifications, authentication, and desktop-level mail actions
- Establish the backend contract for folder synchronization, message updates, and account state
- Prepare the project for IMAP and system-service integration without locking in a single workflow

### M05: Product shell and account foundation

- Create the desktop shell: header, left navigation, main mail list, right preview, and footer regions
- Define account model, connection state, authentication flow, and basic folder synchronization hooks
- Implement the app skeleton and navigation behavior inspired by Trojitá and Kontact
- Confirm the layout supports both compact and comfortable density modes

### M06: Mail operations and folder management

- Implement inbox, sent, drafts, trash, and custom folder navigation patterns
- Add message list sorting, filtering, unread state, selection, and bulk actions
- Support folder sync behavior, account grouping, and power-user navigation patterns similar to The Bat!
- Validate the interaction model against the intended Plasma Mail layout and pacing

### M07: Reading, search, and conversation flow

- Build the reading experience for message bodies, metadata, attachments, and quick actions
- Add search, thread-aware grouping, and basic message filters for common mail workflows
- Improve the readability and action density of the preview panel using Aqua Mail-inspired clarity
- Establish keyboard navigation and focus behavior for desktop-heavy usage

### M08: Reliability, sync, and notification quality

- Harden account sync status, retry behavior, offline handling, and background task reporting
- Implement notifications for errors, auth prompts, and long-running mail operations
- Define acceptance criteria for performance, inconsistent folder states, and message update consistency
- Complete the notification and sync contract between the UI shell and backend services

### M09: Refinements and release readiness

- Review accessibility, localization, keyboard support, and final visual tuning across all mail panes
- Tune the app for Plasma polish, readability, and efficient default layout behavior
- Validate the user journey from account setup through reading, triage, and response
- Finalize release criteria, known issues, and milestone handoff documentation

## Notes

Update this page with real milestones, owners, dependencies, and acceptance criteria as work begins.
