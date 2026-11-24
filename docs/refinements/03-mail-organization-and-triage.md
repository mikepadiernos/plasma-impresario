# Mail organization and triage refinement

**[Refinements](README.md)** ·
**[M12 in roadmap](../ROADMAP.md#m12-refinements)** ·
**[Previous: Accounts, sync and storage](02-accounts-sync-and-storage.md)** ·
**[Next: Reading, composition and attachments](04-reading-composition-and-attachments.md)**

Refines inbox organization, folder navigation, selection, unread counts, quick
actions and bulk operations.

**Roadmap subsection:**
[M12.04: Mail organization and triage](../ROADMAP.md#m12-refinements) ·
**Issue:** [#23](https://github.com/mikepadiernos/plasma-impresario/issues/23)

## Focus

- Refine folder navigation, message lists, selection, unread counts, quick
  actions, and bulk operations
- Keep unread counts and message selection consistent when folders update or
  accounts reconnect
- Make quick actions and keyboard shortcuts consistent across individual
  messages and bulk selections
- Show progress and partial failures for bulk operations, with confirmation or
  recovery for destructive actions
- Improve folder hierarchy, account grouping, and dense message indicators
  without obscuring message state

## Acceptance criteria

- Folder and account navigation remain stable during refresh and reconnect
  cycles.
- Bulk actions expose progress, cancellation, and partial failure clearly.
- Message state remains visible even when folders or accounts refresh rapidly.
- Dense triage remains usable without overwhelming the user.
