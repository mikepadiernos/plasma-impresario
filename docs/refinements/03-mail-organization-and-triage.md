# Mail organization and triage refinement

**[Documentation](../README.md)** · **[Roadmap: M13](../ROADMAP.md#m13-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Accounts, sync and storage](02-accounts-sync-and-storage.md)** ·
**[Next: Reading, composition and attachments](04-reading-composition-and-attachments.md)**

Refines inbox organization, folder navigation, selection, unread counts, quick
actions and bulk operations. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                                    | Issue                                                               | Board               |
| --------------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M13.04: Mail organization and triage](../ROADMAP.md#m13-refinements) | [#23](https://github.com/mikepadiernos/plasma-impresario/issues/23) | ![Backlog][backlog] |

## Current state

Mail operations, sorting and folder management were built in M03. The message
list offers read, flag, junk, archive, delete, move and copy actions from the
context menu, the toolbar and single-key shortcuts; see the
[message list context menu](menus/message-list-context-menu.md). After a move,
an inline bar offers **Undo** when the server reports the new message UIDs
(COPYUID), or a link to the target folder otherwise. That is not yet the
delayed, local-first undo planned in
[#383](https://github.com/mikepadiernos/plasma-impresario/issues/383). A real
Plasma session has not been verified.

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                                                                           | Issue                                                                 |
| ---------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Refine folder navigation, message lists, selection, unread counts, quick actions, and bulk operations                                                | [#163](https://github.com/mikepadiernos/plasma-impresario/issues/163) |
| Keep unread counts and message selection consistent when folders update or accounts reconnect                                                        | [#164](https://github.com/mikepadiernos/plasma-impresario/issues/164) |
| Make quick actions and keyboard shortcuts consistent across individual messages and bulk selections                                                  | [#165](https://github.com/mikepadiernos/plasma-impresario/issues/165) |
| Show progress and partial failures for bulk operations, with confirmation or recovery for destructive actions                                        | [#166](https://github.com/mikepadiernos/plasma-impresario/issues/166) |
| Improve folder hierarchy, account grouping, and dense message indicators without obscuring message state                                             | [#167](https://github.com/mikepadiernos/plasma-impresario/issues/167) |
| Keep context-menu actions bound to the messages for which the menu opened, even if the selection changes                                             | [#168](https://github.com/mikepadiernos/plasma-impresario/issues/168) |
| Distinguish cancellation from rollback and identify completed, skipped, and failed messages in batch results                                         | [#169](https://github.com/mikepadiernos/plasma-impresario/issues/169) |
| Apply moves, deletions and spam marking locally at once, send them after a short delay with Undo until then, and restore messages the server refuses | [#383](https://github.com/mikepadiernos/plasma-impresario/issues/383) |

## Acceptance criteria

- Folder and account navigation remain stable during refresh and reconnect
  cycles.
- Bulk actions expose progress, cancellation, and partial failure clearly.
- Message state remains visible even when folders or accounts refresh rapidly.
- Dense triage remains usable without overwhelming the user.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
