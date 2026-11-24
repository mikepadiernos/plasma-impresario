# Navigation toolbar

**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M12 in roadmap](../../ROADMAP.md#m12-refinements)**

This proposed Impresario-specific icon toolbar is separate from the account and
folder sidebar. It can be positioned in any shell region. Within either side
column it can dock to the top, bottom, left or right edge, including vertical
placement. A separate full-app-height option places it vertically at the left or
right window edge, from the top of the header to the bottom of the status bar.
Reserve space for it rather than covering the header, content or status bar.
Other toolbars remain horizontal.

**Roadmap subsection:**
[M12.02: Shell and interaction](../../ROADMAP.md#m12-refinements) · **Issue:**
[#151](https://github.com/mikepadiernos/plasma-impresario/issues/151)

**Legend:** ✅ exists in the current code on `feature/m02-shell-next` · 📋
planned only

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

## Current toolbar

The toolbar exists today as the Qt toolbar titled **Navigation**, built in
`src/Gui/Window.cpp`. It docks at the left window edge by default, can be moved
to another edge, and restores its position with the window state. **View › Show
Toolbar** hides or shows it. Items appear in this order:

| Action              | Shortcut                                                                                         | Availability                      | Notes                                                                                                                          |
| ------------------- | ------------------------------------------------------------------------------------------------ | --------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| 🧭 Navigate Back    | <kbd>Alt</kbd>+<kbd>Left</kbd>                                                                   | ✅ One At a Time layout only      | Inserted at the start of the toolbar                                                                                           |
| 📬 Account selector | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd>, <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd> | ✅ Always                         | Switches the active account                                                                                                    |
| New Message         | <kbd>Ctrl</kbd>+<kbd>N</kbd>                                                                     | ✅ Always                         | Drop-down adds **Edit Draft…**                                                                                                 |
| Reply               | —                                                                                                | ✅ Online, with a message shown   | Default follows the message: Reply to Mailing List, else Reply to All but Me, else Private Reply; the drop-down lists all four |
| Forward             | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                                                    | ✅ Online, with a message shown   | Forwards as an attachment                                                                                                      |
| Expunge             | <kbd>Ctrl</kbd>+<kbd>E</kbd>                                                                     | ✅ Online                         | Removes messages marked as deleted                                                                                             |
| Mark as Read        | <kbd>M</kbd>                                                                                     | ✅ Online, with messages selected | Toggle                                                                                                                         |
| Mark as Deleted     | <kbd>Delete</kbd>                                                                                | ✅ Online, with messages selected | Toggle                                                                                                                         |
| Mark as Flagged     | <kbd>S</kbd>                                                                                     | ✅ Online, with messages selected | Toggle                                                                                                                         |
| Mark as Junk        | <kbd>J</kbd>                                                                                     | ✅ Online, with messages selected | Toggle                                                                                                                         |
| Mark as Not Junk    | <kbd>Shift</kbd>+<kbd>J</kbd>                                                                    | ✅ Online, with messages selected | Toggle                                                                                                                         |
| Archive             | <kbd>A</kbd>                                                                                     | ✅ Online, with messages selected | Moves to the account's archive folder, `Archive` unless configured; disabled when every selected message is already there      |
| Expanding space     | —                                                                                                | ✅ Always                         | Separates mail actions from the indicators                                                                                     |
| Activity indicator  | —                                                                                                | ✅ Always                         | Spinner while IMAP tasks run; the tooltip counts ongoing actions                                                               |
| Network indicator   | —                                                                                                | ✅ Always                         | Opens the **Network Access** menu                                                                                              |
| Main menu button    | —                                                                                                | ✅ While the menu bar is hidden   | Opens the Application, View, Mailbox and Message menus                                                                         |
| Customization mode  | —                                                                                                | 📋 Planned                        | See [toolbars and customization](../../philosophy/designs.md#toolbars-and-customization)                                       |
| Side-column docking | —                                                                                                | 📋 Planned                        | Docking inside a side column or at full app height                                                                             |
| Menu corner markers | —                                                                                                | 📋 Planned                        | Part of [M08](../../ROADMAP.md#m08-themes-and-branding-foundation)                                                             |

> [!NOTE]
>
> The availability column follows the enable rules in `src/Gui/Window.cpp`;
> "Online" means the network access policy is not **Offline**.

## Behavior

- Use compact theme icons, visible focus, accessible names and tooltips. Clearly
  distinguish unavailable actions from active ones.
- Indicate icons that open menus with the theme-accented corner marker described
  in [M08](../../ROADMAP.md#m08-themes-and-branding-foundation), not a selection
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
