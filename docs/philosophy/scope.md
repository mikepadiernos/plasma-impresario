# Plasma Mail scope and objective

This page is a placeholder for product scope.

## Objective

Plasma Mail should provide a clear, reliable, desktop-first mail experience for KDE users.

## Questions to answer

- Who is the target user?
- Which account types and protocols are in scope?
- What defines the minimum viable feature set?
- How should the app integrate with Plasma and KDE services?

## Draft direction

Document the product priorities, constraints, and design boundaries here as the project matures.

## Feature directions from the inspiration set

- IMAP reliability and folder accuracy, modeled after Trojitá
- Compact triage flows and quick actions, inspired by Aqua Mail
- Powerful desktop mail management and keyboard-first habits, inspired by The Bat!
- KDE-native polish and system integration, aligned with Kontact and Plasma conventions

## Customization directions

- Theme presets: light, dark, plasma-accent, and high-contrast modes
- Layout presets: compact, comfortable, and focus modes
- Sidebar configuration: account list, folder tree, smart folders, and saved searches
- Preview pane behavior: show preview, hide preview, split layout, and message reading focus
- Message actions: reply, reply-all, forward, archive, flag, delete, and move-to-folder actions
- Notification settings: desktop alerts, summary banners, and sync progress reporting
- Mail workflow settings: threading, unread handling, conversation grouping, and default sorting
- Accessibility options: large text, increased contrast, keyboard navigation, and reduced motion

## Suggested customizations for the product layer

These customizations should feel intentionally similar to a Plasma Opus-style configuration model, but adapted to a mail client:

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
