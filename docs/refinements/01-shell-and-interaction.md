# Shell and interaction refinement

**[Refinements](README.md)** ·
**[M13 in roadmap](../ROADMAP.md#m13-refinements)** ·
**[Previous: Cross-product quality](00-cross-product-quality.md)** ·
**[Next: Accounts, sync and storage](02-accounts-sync-and-storage.md)**

Refines navigation, panes, layouts, keyboard flow and general interaction
quality.

**Roadmap subsection:**
[M13.02: Shell and interaction](../ROADMAP.md#m13-refinements) · **Issue:**
[#21](https://github.com/mikepadiernos/plasma-impresario/issues/21) · **Specs:**
[menus](menus/README.md), [toolbars](toolbars/README.md)

> [!NOTE]
>
> The mail shell, compact, wide and one-panel layouts, embedded panels and
> <kbd>F6</kbd>/<kbd>Shift</kbd>+<kbd>F6</kbd> focus cycling were built in M02.
> Offscreen tests and live IMAP tests against a synthetic Dovecot cover them; a
> real Plasma session has not been verified. See
> [Current interface](../philosophy/designs.md#current-interface).

## Focus

- Refine navigation, panes, toolbars, layouts, density, focus states, and
  keyboard interaction
- Validate the
  [five-region shell](../philosophy/designs.md#panes-and-placement): side-column
  navigation, stacked or side-by-side main content, and configurable embedded
  panels
- Preserve pane sizes, visibility, and focus across restarts and layout changes
- Make primary actions discoverable without crowding compact layouts
- Verify keyboard-only navigation, visible focus, and predictable focus
  restoration after dialogs and actions

## Acceptance criteria

- The app remains understandable in compact, comfortable, and focus modes.
- Keyboard users can navigate the shell without visual ambiguity.
- Layout changes remain stable and restore cleanly after restart or reconnect.
- Menus and toolbar actions remain synchronized with the current account,
  folder, and selection.
