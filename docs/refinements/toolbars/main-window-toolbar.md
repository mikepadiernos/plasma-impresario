# Navigation toolbar

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Menus](../menus/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed Impresario-specific icon toolbar is separate from the account and
folder sidebar. It maps each entry to the **Navigation** toolbar that exists
today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

| Roadmap subsection                                                | Issue                                                                 |
| ----------------------------------------------------------------- | --------------------------------------------------------------------- |
| [M13.02: Shell and interaction](../../ROADMAP.md#m13-refinements) | [#151](https://github.com/mikepadiernos/plasma-impresario/issues/151) |

## Purpose

The toolbar holds compose, common message actions and utility entry points. It
can be positioned in any shell region:

- Within either side column it can dock to the top, bottom, left or right edge,
  including vertical placement.
- A separate full-app-height option places it vertically at the left or right
  window edge, from the top of the header to the bottom of the status bar.
- It reserves space rather than covering the header, content or status bar.

Other toolbars remain horizontal.

## Toolbar tree

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

## Actions

Items appear in this order in the current toolbar. Separators follow the
account selector, **Expunge** and the expanding space.

| Action              | Shortcut                                                                                         | Availability                                     | Notes                                                                                                                                                                                                                       |
| ------------------- | ------------------------------------------------------------------------------------------------ | ------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Navigate Back       | <kbd>Alt</kbd>+<kbd>Left</kbd>                                                                   | ![Exists][exists] One At a Time layout only      | Inserted at the start of the toolbar; the shortcut is the platform Back key                                                                                                                                                 |
| Account selector    | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd>, <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd> | ![Exists][exists] Always                         | Switches the active account                                                                                                                                                                                                 |
| New Message         | <kbd>Ctrl</kbd>+<kbd>N</kbd>                                                                     | ![Exists][exists] Always                         | Drop-down adds **Edit Draft…**                                                                                                                                                                                              |
| Reply               | —                                                                                                | ![Exists][exists] Online, with a message shown   | Default follows the message: Reply to Mailing List, else Reply to All but Me, else Private Reply; the drop-down lists all four                                                                                              |
| Forward             | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                                                    | ![Exists][exists] Online, with a message shown   | Forwards as an attachment                                                                                                                                                                                                   |
| Expunge             | <kbd>Ctrl</kbd>+<kbd>E</kbd>                                                                     | ![Exists][exists] Online                         | Removes messages marked as deleted                                                                                                                                                                                          |
| Mark as Read        | <kbd>M</kbd>                                                                                     | ![Exists][exists] Online, with messages selected | Toggle                                                                                                                                                                                                                      |
| Mark as Deleted     | <kbd>Delete</kbd>                                                                                | ![Exists][exists] Online, with messages selected | Toggle                                                                                                                                                                                                                      |
| Mark as Flagged     | <kbd>S</kbd>                                                                                     | ![Exists][exists] Online, with messages selected | Toggle                                                                                                                                                                                                                      |
| Mark as Junk        | <kbd>J</kbd>                                                                                     | ![Exists][exists] Online, with messages selected | Toggle                                                                                                                                                                                                                      |
| Mark as Not Junk    | <kbd>Shift</kbd>+<kbd>J</kbd>                                                                    | ![Exists][exists] Online, with messages selected | Toggle                                                                                                                                                                                                                      |
| Archive             | <kbd>A</kbd>                                                                                     | ![Exists][exists] Online, with messages selected | Moves to the account's archive folder, `Archive` unless configured; disabled when every selected message is already there                                                                                                   |
| Move to Folder      | <kbd>V</kbd>                                                                                     | ![Exists][exists] Online, with messages selected | **Move to Folder…** opens a folder picker; the drop-down lists recent **Move To** folders                                                                                                                                   |
| Expanding space     | —                                                                                                | ![Exists][exists] Always                         | Separates mail actions from the indicators                                                                                                                                                                                  |
| Activity indicator  | —                                                                                                | ![Exists][exists] Always                         | Spinner while IMAP tasks run; the tooltip counts ongoing actions                                                                                                                                                            |
| Network indicator   | —                                                                                                | ![Exists][exists] Always                         | Opens the **Network Access** menu                                                                                                                                                                                           |
| Main menu button    | —                                                                                                | ![Exists][exists] While the menu bar is hidden   | Opens the Application, View, Mailbox, Message and Help menus and **Show Main Menu Bar**                                                                                                                                     |
| Customization mode  | —                                                                                                | ![Planned][planned]                              | See [toolbars and customization](../../philosophy/designs.md#toolbars-and-customization)                                                                                                                                    |
| Side-column docking | —                                                                                                | ![Planned][planned]                              | Docking inside a side column or at full app height                                                                                                                                                                          |
| Menu corner markers | —                                                                                                | ![Planned][planned]                              | Introduced in [M09](../../ROADMAP.md#m09-themes-and-branding-foundation), refined for every placement in [M13.02](../../ROADMAP.md#m13-refinements) ([#486](https://github.com/mikepadiernos/plasma-impresario/issues/486)) |

> [!NOTE]
>
> The availability column follows the enable rules in `src/Gui/Window.cpp`;
> "Online" means the network access policy is not **Offline**. **Archive** and
> **Move to Folder** also need an account whose server can move messages, and
> **Mark as Junk** and **Mark as Not Junk** need server-side flags.

## Current toolbar

The toolbar exists today as the Qt toolbar titled **Navigation**, built in
`src/Gui/Window.cpp`. It docks at the left window edge by default, can be moved
to another edge, and restores its position with the window state. **View › Show
Toolbar** hides or shows it.

Compared with this specification, the current toolbar has no customization
mode, cannot dock inside a side column or at full app height, and its menu
buttons are Qt split buttons rather than icons with corner markers.

## Design rules

- Use compact theme icons, visible focus, accessible names and tooltips. Clearly
  distinguish unavailable actions from active ones.
- Indicate icons that open menus with a small accent-colored corner on the
  icon instead of a caret, as described in
  [M09](../../ROADMAP.md#m09-themes-and-branding-foundation) and refined in
  [#486](https://github.com/mikepadiernos/plasma-impresario/issues/486). The corner sits in the lower trailing corner, mirrors for
  right-to-left layouts and keeps its place in vertical placements. It means
  "has a menu", not selection. Preserve keyboard access to those menus.
- Take padding and spacing from the shared spacing scale, kept in step with
  Plasma Opus ([#487](https://github.com/mikepadiernos/plasma-impresario/issues/487)).
- Keep actions scoped to the active account, folder, message or pane; update
  availability when context changes. Make destructive actions recognizable and
  confirm them where appropriate.
- Use an expanding spacer to separate the mail and utility groups, adapting
  their arrangement to the toolbar orientation. Keep actions reachable through
  accessible overflow when the window is too small.
- Preserve the toolbar's contents and placement across sessions and allow
  customization through the shared mode. The toolbar remains usable when sidebar
  panels or message panes are hidden.

> [!NOTE]
>
> The exact action set, spacing and visual treatment require validation with
> representative mail workflows and Plasma themes.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
