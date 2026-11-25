# Refinements

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[M13 in roadmap](../ROADMAP.md#m13-refinements)** ·
**[Menus](menus/README.md)** · **[Toolbars](toolbars/README.md)** ·
**[Interface design](../philosophy/designs.md)**

These plans define the release-quality pass of roadmap milestone
[M13: Refinements](../ROADMAP.md#m13-refinements), which follows core mail and
integration work. Each plan file covers one M13 subsection.

> [!NOTE]
>
> Refinement plans are proposals and acceptance criteria. They do not change
> roadmap status by themselves; the [roadmap](../ROADMAP.md) owns scope and the
> [project board](https://github.com/users/mikepadiernos/projects/4) owns
> status.

## Quality areas

File numbers 00–10 map to roadmap subsections M13.01–M13.11. The M13 parent
issue is [#15](https://github.com/mikepadiernos/plasma-impresario/issues/15); it
and every subsection issue below are open and in the board's Backlog column.

| File                                                                                 | Roadmap subsection                      | Issue                                                               | Focus                                              |
| ------------------------------------------------------------------------------------ | --------------------------------------- | ------------------------------------------------------------------- | -------------------------------------------------- |
| [00 Cross-product quality](00-cross-product-quality.md)                              | [M13.01](../ROADMAP.md#m13-refinements) | [#60](https://github.com/mikepadiernos/plasma-impresario/issues/60) | Acceptance, benchmarks and regression coverage     |
| [01 Shell and interaction](01-shell-and-interaction.md)                              | [M13.02](../ROADMAP.md#m13-refinements) | [#21](https://github.com/mikepadiernos/plasma-impresario/issues/21) | Navigation, panes, focus and keyboard flow         |
| [02 Accounts, sync and storage](02-accounts-sync-and-storage.md)                     | [M13.03](../ROADMAP.md#m13-refinements) | [#22](https://github.com/mikepadiernos/plasma-impresario/issues/22) | Setup, authentication, recovery and cache behavior |
| [03 Mail organization and triage](03-mail-organization-and-triage.md)                | [M13.04](../ROADMAP.md#m13-refinements) | [#23](https://github.com/mikepadiernos/plasma-impresario/issues/23) | Folders, lists, unread state and bulk actions      |
| [04 Reading, composition and attachments](04-reading-composition-and-attachments.md) | [M13.05](../ROADMAP.md#m13-refinements) | [#24](https://github.com/mikepadiernos/plasma-impresario/issues/24) | Reader, composer, drafts and attachments           |
| [05 Search, sorting and automation](05-search-sorting-and-automation.md)             | [M13.06](../ROADMAP.md#m13-refinements) | [#25](https://github.com/mikepadiernos/plasma-impresario/issues/25) | Search, threading, sorting and rules               |
| [06 Security and privacy](06-security-and-privacy.md)                                | [M13.07](../ROADMAP.md#m13-refinements) | [#26](https://github.com/mikepadiernos/plasma-impresario/issues/26) | Credentials, remote content and attachment safety  |
| [07 Settings and notifications](07-settings-and-notifications.md)                    | [M13.08](../ROADMAP.md#m13-refinements) | [#27](https://github.com/mikepadiernos/plasma-impresario/issues/27) | Preferences and background feedback                |
| [08 Desktop, services and assets](08-desktop-services-and-assets.md)                 | [M13.09](../ROADMAP.md#m13-refinements) | [#28](https://github.com/mikepadiernos/plasma-impresario/issues/28) | Plasma, providers and Nextcloud                    |
| [09 Calendar and contacts](09-calendar-and-contacts.md)                              | [M13.10](../ROADMAP.md#m13-refinements) | [#29](https://github.com/mikepadiernos/plasma-impresario/issues/29) | Scheduling and contact workflows                   |
| [10 Notes, journal and workspaces](10-notes-journal-and-workspaces.md)               | [M13.11](../ROADMAP.md#m13-refinements) | [#30](https://github.com/mikepadiernos/plasma-impresario/issues/30) | Notes, editor context and workspaces               |

> [!TIP]
>
> The roadmap lists M13.01–M13.11 as bold items inside the
> [M13 section](../ROADMAP.md#m13-refinements), not as headings, so links point
> to that section. The board issue titles still carry the earlier "M11" prefix;
> match issues by number.

## Interface specifications

These specifications support the M13.02 shell and interaction work. Each one
marks which actions exist in the current code and which are planned only.

| Specification                     | Scope                                                         |
| --------------------------------- | ------------------------------------------------------------- |
| 🧭 [Menus](menus/README.md)       | Application menu and context menus                            |
| 🧰 [Toolbars](toolbars/README.md) | Navigation, message list, reader, compose and attachment bars |
