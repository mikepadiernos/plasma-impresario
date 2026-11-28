# Calendar and contacts refinement

**[Documentation](../README.md)** · **[Roadmap: M13](../ROADMAP.md#m13-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Desktop, services and assets](08-desktop-services-and-assets.md)** ·
**[Next: Notes, journal and workspaces](10-notes-journal-and-workspaces.md)**

Defines refinement criteria for potential calendar, scheduling, contacts and
address-book workflows. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                             | Issue                                                               | Board               |
| -------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M13.10: Calendar and contacts](../ROADMAP.md#m13-refinements) | [#29](https://github.com/mikepadiernos/plasma-impresario/issues/29) | ![Backlog][backlog] |

## Current state

Contacts are reachable today through **Application › Address Book…**, the
address-book plugins (`src/Plugins/AbookAddressbook` and
`src/Plugins/AkonadiAddressbook`) and the contact panel's **Write Email**,
**Copy Address** and **Open in Address Book** buttons. There is no calendar
view; a `text/calendar` part is only labeled as a calendar event. This
subsection defines criteria for workflows that do not exist yet.

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                             | Issue                                                                 |
| ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------- |
| Define refinement criteria for potential calendar, scheduling, contacts, and address-book workflows    | [#204](https://github.com/mikepadiernos/plasma-impresario/issues/204) |
| Define criteria for readable calendar views, time zones, reminders, and event conflict feedback        | [#205](https://github.com/mikepadiernos/plasma-impresario/issues/205) |
| Define criteria for contact search, recipient lookup, duplicate handling, and account-specific sources | [#206](https://github.com/mikepadiernos/plasma-impresario/issues/206) |
| Define privacy, permissions, and error expectations for provider-backed event and contact data         | [#207](https://github.com/mikepadiernos/plasma-impresario/issues/207) |
| Describe how potential mail-to-event and contact-to-compose workflows should preserve context          | [#208](https://github.com/mikepadiernos/plasma-impresario/issues/208) |

## Acceptance criteria

- Calendar and contact handoff remains understandable and context-preserving.
- Provider-backed data has clear privacy and permission expectations.
- Contact lookup and recipient flow remain safe, consistent, and easy to recover
  from.
- Mail-to-calendar experiences remain lightweight and non-disruptive.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
