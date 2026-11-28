# Reading pane context menu

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed menu appears when the user right-clicks in the reading pane or on
content inside an opened message. It maps each entry to the reader actions that
exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Purpose

This is the safe, content-first action surface for the reading experience. It
keeps content actions and mail actions clearly separated, while supporting
desktop-like reading and linking behavior.

## Menu tree

```text
└─ Reading pane context menu
   ├─ Content actions
   │  ├─ Copy
   │  ├─ Select all
   │  ├─ Copy link address
   │  ├─ Open link in browser
   │  ├─ Save link as
   │  ├─ Open external content
   │  ├─ Load remote content
   │  ├─ Show original source
   │  ├─ View message source
   │  ├─ Print
   │  ├─ Zoom in
   │  ├─ Zoom out
   │  └─ Reset zoom
   └─ Message actions
      ├─ Reply
      ├─ Reply all
      ├─ Forward
      ├─ Mark as read
      ├─ Flag
      ├─ Archive
      ├─ Move to folder
      ├─ Delete
      ├─ Save attachment as
      └─ Open attachment
```

## Actions

### Content actions

| Action                | Shortcut                     | Availability                                                 | Notes                                                                                       |
| --------------------- | ---------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------------------------------- |
| Copy                  | <kbd>Ctrl</kbd>+<kbd>C</kbd> | ![Exists][exists] Reader context menu                        |                                                                                             |
| Select all            | <kbd>Ctrl</kbd>+<kbd>A</kbd> | ![Exists][exists] Reader context menu                        |                                                                                             |
| Copy link address     | —                            | ![Exists][exists] Reader context menu                        | On a link; a `mailto:` link offers **Copy e-mail address** instead                          |
| Open link in browser  | —                            | ![Planned][planned]                                          |                                                                                             |
| Save link as          | —                            | ![Planned][planned]                                          |                                                                                             |
| Open external content | —                            | ![Planned][planned]                                          |                                                                                             |
| Load remote content   | —                            | ![Exists][exists] Remote content bar                         | A bar in the reader offers **Load Remote Content**; remote content stays blocked until then |
| Show original source  | —                            | ![Planned][planned]                                          | See **View message source**                                                                 |
| View message source   | —                            | ![Exists][exists] Message list context menu                  | **View Message Source…**                                                                    |
| Print                 | —                            | ![Planned][planned]                                          |                                                                                             |
| Zoom in               | <kbd>Ctrl</kbd>+<kbd>+</kbd> | ![Exists][exists] Reader **Zoom** submenu, reader action bar |                                                                                             |
| Zoom out              | <kbd>Ctrl</kbd>+<kbd>-</kbd> | ![Exists][exists] Reader **Zoom** submenu, reader action bar |                                                                                             |
| Reset zoom            | —                            | ![Exists][exists] Reader **Zoom** submenu, reader action bar | **Original Size** in the menu, **Reset Zoom** on the bar                                    |

### Message actions

| Action             | Shortcut                                                     | Availability                                                                                             | Notes                                                                                          |
| ------------------ | ------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| Reply              | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ![Exists][exists] **Message** menu, navigation toolbar, reader action bar                                | **Reply by Guess**                                                                             |
| Reply all          | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ![Exists][exists] **Message** menu, navigation toolbar                                                   | **Reply to All**                                                                               |
| Forward            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ![Exists][exists] **Message** menu, navigation toolbar, reader action bar                                | Forwards as an attachment                                                                      |
| Mark as read       | <kbd>M</kbd>                                                 | ![Exists][exists] Navigation toolbar, reader action bar, message list context menu                       |                                                                                                |
| Flag               | <kbd>S</kbd>                                                 | ![Exists][exists] Navigation toolbar, reader action bar, message list context menu                       | **Mark as Flagged**                                                                            |
| Archive            | <kbd>A</kbd>                                                 | ![Exists][exists] Navigation toolbar, reader action bar, message list context menu                       |                                                                                                |
| Move to folder     | <kbd>V</kbd>                                                 | ![Exists][exists] **Message › Move to Folder…**, navigation toolbar, drag and drop from the message list |                                                                                                |
| Delete             | <kbd>Delete</kbd>                                            | ![Exists][exists] Navigation toolbar, reader action bar, message list context menu                       | **Mark as Deleted**; the reader action bar shows **Move to Trash**, **Delete** or **Undelete** |
| Save attachment as | —                                                            | ![Exists][exists] Reader context menu, Attachments panel                                                 | **Save this message part…**, **Save Attachment As…**                                           |
| Open attachment    | —                                                            | ![Exists][exists] Attachments panel, inline attachment menu                                              | **Open Attachment**, **Open Directly**                                                         |

## Current menu

`SimplePartWidget::buildContextMenu()` in `src/Gui/SimplePartWidget.cpp` builds
the reader menu:

- **Search…** (<kbd>Ctrl</kbd>+<kbd>F</kbd>) opens the find bar;
  <kbd>F3</kbd> and <kbd>Shift</kbd>+<kbd>F3</kbd> find the next and previous
  match, and <kbd>Esc</kbd> closes it.
- **Copy**, **Select All**, and on a link **Copy Link** or **Copy e-mail
  address**.
- **Save this message part…**, **Save whole message…**, and on an image
  **Download Image…**.
- **Color scheme** and **Zoom** submenus.

The reader has no message actions in its menu today. They are in the
**Message** menu, the navigation toolbar, the message list menu and, in builds
with the WebEngine reader, the
[reader action bar](../toolbars/reading-pane-toolbar.md).

## Design rules

- Opening external links must be explicit and must not silently trigger without
  user confirmation when security or privacy controls are enabled.
- Remote content and images should remain blocked by default unless the user
  opts in.
- The reading pane should never expose a destructive action without clear
  context or confirmation where required.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
