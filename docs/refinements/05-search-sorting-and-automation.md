# Search, sorting, and automation refinement

**[Documentation](../README.md)** · **[Roadmap: M13](../ROADMAP.md#m13-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Reading, composition and attachments](04-reading-composition-and-attachments.md)** ·
**[Next: Security and privacy](06-security-and-privacy.md)**

Refines search, saved filters, sorting, threading and existing rules or
automation. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                                       | Issue                                                               | Board               |
| ------------------------------------------------------------------------ | ------------------------------------------------------------------- | ------------------- |
| [M13.06: Search, sorting, and automation](../ROADMAP.md#m13-refinements) | [#25](https://github.com/mikepadiernos/plasma-impresario/issues/25) | ![Backlog][backlog] |

## Current state

Full search and indexing is planned in
[M05](../ROADMAP.md#m05-search-and-indexing)
([#324](https://github.com/mikepadiernos/plasma-impresario/issues/324),
![Backlog][backlog]); this subsection refines it once it ships. The message
list already has a quick search field with fuzzy search, field selection,
per-account saved searches and an optional raw IMAP query, plus Unread, Flagged
and With Attachments filters, the **Mailbox › Sorting** menu and Group By. No
mail rules engine exists in `src/` yet. See the
[message list toolbar](toolbars/message-list-toolbar.md).

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                                                                     | Issue                                                                 |
| ---------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Refine search, saved filters, sorting, threading, and existing rules; define refinement criteria for potential tagging and advanced automation | [#176](https://github.com/mikepadiernos/plasma-impresario/issues/176) |
| Make account and folder search scope, active filters, and unsupported search capabilities explicit                                             | [#177](https://github.com/mikepadiernos/plasma-impresario/issues/177) |
| Keep sort and thread order stable as messages arrive and preserve navigation context when filters change                                       | [#178](https://github.com/mikepadiernos/plasma-impresario/issues/178) |
| Allow users to inspect rule order and test matches before applying potentially destructive actions                                             | [#179](https://github.com/mikepadiernos/plasma-impresario/issues/179) |
| Define how potential tags and advanced automation would interact with folders, search, and existing rules                                      | [#180](https://github.com/mikepadiernos/plasma-impresario/issues/180) |

## Acceptance criteria

- Search and filter behavior remain predictable as folders update.
- Sorting and threading remain stable and navigable across live message changes.
- Rules are understandable, inspectable, and safe before they act on mail.
- Unsupported automation or search gaps are clearly communicated instead of
  silently failing.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
