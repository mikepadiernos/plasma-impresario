# Notes, journal, and workspaces refinement

**[Documentation](../README.md)** · **[Roadmap: M13](../ROADMAP.md#m13-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Calendar and contacts](09-calendar-and-contacts.md)**

Defines refinement criteria for potential notes, journal, editor and cross-app
workspace behavior. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                                      | Issue                                                               | Board               |
| ----------------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M13.11: Notes, journal, and workspaces](../ROADMAP.md#m13-refinements) | [#30](https://github.com/mikepadiernos/plasma-impresario/issues/30) | ![Backlog][backlog] |

## Current state

No notes, journal, workspace or Plasma Opus code exists in `src/` yet. This
subsection defines criteria for workflows that are only potential today.

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                                        | Issue                                                                 |
| ----------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Define refinement criteria for potential notes, journal, the Plasma Opus editor, and cross-app workspace behavior | [#209](https://github.com/mikepadiernos/plasma-impresario/issues/209) |
| Define editor criteria for keyboard navigation, draft recovery, formatting consistency, and accessible text entry | [#210](https://github.com/mikepadiernos/plasma-impresario/issues/210) |
| Define note and journal criteria for search, organization, autosave feedback, and storage privacy                 | [#211](https://github.com/mikepadiernos/plasma-impresario/issues/211) |
| Specify potential workspace context switching, account boundaries, and state restoration across apps              | [#212](https://github.com/mikepadiernos/plasma-impresario/issues/212) |
| Define compatibility and failure expectations for sharing editor or workspace context with Plasma Opus            | [#213](https://github.com/mikepadiernos/plasma-impresario/issues/213) |

## Acceptance criteria

- Companion workflows remain optional and gracefully degrade when unavailable.
- Notes and journal capture flows preserve context and recover user work
  reliably.
- Workspace context support remains understandable and privacy-aware.
- Cross-app handoff remains helpful without becoming fragile or invasive.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
