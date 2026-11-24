# Search, sorting, and automation refinement

**[Refinements](README.md)** ·
**[M12 in roadmap](../ROADMAP.md#m12-refinements)** ·
**[Previous: Reading, composition and attachments](04-reading-composition-and-attachments.md)**
· **[Next: Security and privacy](06-security-and-privacy.md)**

Refines search, saved filters, sorting, threading and existing rules or
automation.

**Roadmap subsection:**
[M12.06: Search, sorting, and automation](../ROADMAP.md#m12-refinements) ·
**Issue:** [#25](https://github.com/mikepadiernos/plasma-impresario/issues/25)

## Focus

- Refine search, saved filters, sorting, threading, and existing rules; define
  refinement criteria for potential tagging and advanced automation
- Make account and folder search scope, active filters, and unsupported search
  capabilities explicit
- Keep sort and thread order stable as messages arrive and preserve navigation
  context when filters change
- Allow users to inspect rule order and test matches before applying potentially
  destructive actions
- Define how potential tags and advanced automation would interact with folders,
  search, and existing rules

## Acceptance criteria

- Search and filter behavior remain predictable as folders update.
- Sorting and threading remain stable and navigable across live message changes.
- Rules are understandable, inspectable, and safe before they act on mail.
- Unsupported automation or search gaps are clearly communicated instead of
  silently failing.
