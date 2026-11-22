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

### M1: KDE / Qt6 compatibility baseline

- Confirm the project builds and runs against KDE and Qt 6 tooling
- Audit deprecated APIs, platform assumptions, and build requirements for Plasma desktop compatibility
- Define the minimum supported KDE / Qt6 environment and release gating criteria
- Establish a baseline compatibility report that all later milestones depend on

### M2: Product shell and account foundation

- Create the desktop shell: header, left navigation, main mail list, right preview, and footer regions
- Define account model, connection state, authentication flow, and basic folder synchronization hooks
- Implement the app skeleton and navigation behavior inspired by Trojitá and Kontact
- Confirm the layout supports both compact and comfortable density modes

### M3: Mail operations and folder management

- Implement inbox, sent, drafts, trash, and custom folder navigation patterns
- Add message list sorting, filtering, unread state, selection, and bulk actions
- Support folder sync behavior, account grouping, and power-user navigation patterns similar to The Bat!
- Validate the interaction model against the intended Plasma Mail layout and pacing

### M4: Reading, search, and conversation flow

- Build the reading experience for message bodies, metadata, attachments, and quick actions
- Add search, thread-aware grouping, and basic message filters for common mail workflows
- Improve the readability and action density of the preview panel using Aqua Mail-inspired clarity
- Establish keyboard navigation and focus behavior for desktop-heavy usage

### M5: Reliability, sync, and notification quality

- Harden account sync status, retry behavior, offline handling, and background task reporting
- Implement notifications for errors, auth prompts, and long-running mail operations
- Define acceptance criteria for performance, inconsistent folder states, and message update consistency
- Complete the notification and sync contract between the UI shell and backend services

### M6: Quality, accessibility, and release readiness

- Review accessibility, localization, and keyboard support across all mail panes
- Tune the app for Plasma polish, readability, and efficient default layout behavior
- Validate the user journey from account setup through reading, triage, and response
- Finalize release criteria, known issues, and milestone handoff documentation

## Notes

Update this page with real milestones, owners, dependencies, and acceptance criteria as work begins.
