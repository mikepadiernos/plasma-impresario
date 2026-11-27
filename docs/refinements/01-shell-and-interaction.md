# Shell and interaction refinement

**[Documentation](../README.md)** · **[Roadmap: M14](../ROADMAP.md#m14-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Cross-product quality](00-cross-product-quality.md)** ·
**[Next: Accounts, sync and storage](02-accounts-sync-and-storage.md)** ·
**[Menus](menus/README.md)** · **[Toolbars](toolbars/README.md)**

Refines navigation, panes, layouts, keyboard flow and general interaction
quality. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                             | Issue                                                               | Board               |
| -------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M14.02: Shell and interaction](../ROADMAP.md#m14-refinements) | [#21](https://github.com/mikepadiernos/plasma-impresario/issues/21) | ![Backlog][backlog] |

## Current state

The mail shell, the Compact, Wide and One At a Time layouts, embedded panels
and <kbd>F6</kbd>/<kbd>Shift</kbd>+<kbd>F6</kbd> focus cycling were built in M02.
Offscreen tests and live IMAP tests against a synthetic Dovecot cover them; a
real Plasma session has not been verified. See
[Current interface](../philosophy/designs.md#current-interface).

The [menu](menus/README.md) and [toolbar](toolbars/README.md) specifications
record which actions exist today. The optional **View › Show Header** strip is
a simple shell header, off by default; it is not the designed header planned in
[#306](https://github.com/mikepadiernos/plasma-impresario/issues/306).

## Focus

Every feature issue below is in the board's Backlog. A dash marks a refinement
recorded only in this plan.

| Refinement                                                                                                                                                                     | Issue                                                                 |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------- |
| Refine navigation, panes, toolbars, layouts, density, focus states, and keyboard interaction                                                                                   | [#149](https://github.com/mikepadiernos/plasma-impresario/issues/149) |
| Validate the [five-region shell](../philosophy/designs.md#panes-and-placement): side-column navigation, stacked or side-by-side main content, and configurable embedded panels | —                                                                     |
| Preserve pane sizes, visibility, and focus across restarts and layout changes                                                                                                  | [#150](https://github.com/mikepadiernos/plasma-impresario/issues/150) |
| Make primary actions discoverable without crowding compact layouts; allow toolbar customization                                                                                | [#151](https://github.com/mikepadiernos/plasma-impresario/issues/151) |
| Build the designed header with search, compose, the account switcher, and connection status                                                                                    | [#306](https://github.com/mikepadiernos/plasma-impresario/issues/306) |
| Add sliding and floating panel placements, and rearrange panels in the customization mode                                                                                      | [#307](https://github.com/mikepadiernos/plasma-impresario/issues/307) |
| Mark icon menus on the main side toolbar with an accent-colored corner instead of carets, in every placement                                                                   | [#486](https://github.com/mikepadiernos/plasma-impresario/issues/486) |
| Keep spacing and theme in sync with Plasma Opus, falling back to Plasma without it                                                                                             | [#487](https://github.com/mikepadiernos/plasma-impresario/issues/487) |
| Verify keyboard-only navigation, visible focus, and predictable focus restoration after dialogs and actions                                                                    | [#152](https://github.com/mikepadiernos/plasma-impresario/issues/152) |
| Validate compact, comfortable, and focus modes at narrow window sizes and different display scales                                                                             | [#153](https://github.com/mikepadiernos/plasma-impresario/issues/153) |
| Keep menu and toolbar actions synchronized with the active account, folder, and selection, including while views refresh                                                       | [#154](https://github.com/mikepadiernos/plasma-impresario/issues/154) |
| Preserve customized menus, toolbars, shortcuts, and saved layouts when defaults change or actions are added                                                                    | [#155](https://github.com/mikepadiernos/plasma-impresario/issues/155) |
| Restore windows to a usable visible position after display changes without overriding window-manager placement                                                                 | [#156](https://github.com/mikepadiernos/plasma-impresario/issues/156) |
| Keep controls usable with long translations, right-to-left layouts, and reduced-motion preferences                                                                             | [#157](https://github.com/mikepadiernos/plasma-impresario/issues/157) |

## Acceptance criteria

- The app remains understandable in compact, comfortable, and focus modes.
- Keyboard users can navigate the shell without visual ambiguity.
- Layout changes remain stable and restore cleanly after restart or reconnect.
- Menus and toolbar actions remain synchronized with the current account,
  folder, and selection.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
