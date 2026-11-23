# Navigation toolbar

**[Toolbars](README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed Impresario-specific icon toolbar is separate from the account and
folder sidebar. It can be positioned in any shell region. Within either side
column it can dock to the top, bottom, left or right edge, including vertical
placement. A separate full-app-height option places it vertically at the left or
right window edge, from the top of the header to the bottom of the status bar.
Reserve space for it rather than covering the header, content or status bar.
Other toolbars remain horizontal.

The pictured left-edge arrangement is one possible placement:

```text
┌─────────────────┐
│ Compose         │
│ Mail actions    │
│ ...             │
│                 │
│ Expanding space │
│                 │
│ Utility actions │
│ Application menu│
└─────────────────┘
```

The diagram shows grouping, not a fixed icon order or a complete action list.
The upper group can include compose and common message actions; the lower group
holds utility and application-menu entry points.

## Behavior

- Use compact theme icons, visible focus, accessible names and tooltips. Clearly
  distinguish unavailable actions from active ones.
- Indicate icons that open menus with the theme-accented corner marker described
  in [M09](../../ROADMAP.md#m09-themes-and-branding-foundation), not a selection
  marker. Preserve keyboard access to those menus.
- Keep actions scoped to the active account, folder, message or pane; update
  availability when context changes. Make destructive actions recognizable and
  confirm them where appropriate.
- Use an expanding spacer to separate the mail and utility groups, adapting
  their arrangement to the toolbar orientation. Keep actions reachable through
  accessible overflow when the window is too small.
- Preserve the toolbar's contents and placement across sessions and allow
  customization through the shared mode. The toolbar remains usable when sidebar
  panels or message panes are hidden.

The exact action set, spacing and visual treatment require validation with
representative mail workflows and Plasma themes.
