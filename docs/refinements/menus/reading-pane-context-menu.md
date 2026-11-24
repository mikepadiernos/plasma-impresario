# Reading pane context menu

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M12 in roadmap](../../ROADMAP.md#m12-refinements)**

This proposed menu appears when the user right-clicks in the reading pane or on
content inside an opened message.

**Legend:** ✅ exists in the current code on `feature/m02-shell-next` · 📋
planned only

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

| Action                | Shortcut                     | Availability                 | Notes                                                                        |
| --------------------- | ---------------------------- | ---------------------------- | ---------------------------------------------------------------------------- |
| Copy                  | <kbd>Ctrl</kbd>+<kbd>C</kbd> | ✅ Reader context menu       |                                                                              |
| Select all            | <kbd>Ctrl</kbd>+<kbd>A</kbd> | ✅ Reader context menu       |                                                                              |
| Copy link address     | —                            | ✅ Reader context menu       | On a link; a `mailto:` link offers **Copy e-mail address** instead           |
| Open link in browser  | —                            | 📋 Planned                   |                                                                              |
| Save link as          | —                            | 📋 Planned                   |                                                                              |
| Open external content | —                            | 📋 Planned                   |                                                                              |
| Load remote content   | —                            | ✅ Remote content bar        | A bar in the reader offers **Load**; remote content stays blocked until then |
| Show original source  | —                            | 📋 Planned                   | See **View message source**                                                  |
| View message source   | —                            | ✅ Message list context menu | **View Message Source…**                                                     |
| Print                 | —                            | 📋 Planned                   |                                                                              |
| Zoom in               | <kbd>Ctrl</kbd>+<kbd>+</kbd> | ✅ Reader **Zoom** submenu   |                                                                              |
| Zoom out              | <kbd>Ctrl</kbd>+<kbd>-</kbd> | ✅ Reader **Zoom** submenu   |                                                                              |
| Reset zoom            | —                            | ✅ Reader **Zoom** submenu   | **Original Size**                                                            |

### Message actions

| Action             | Shortcut                                                     | Availability                                     | Notes                                                |
| ------------------ | ------------------------------------------------------------ | ------------------------------------------------ | ---------------------------------------------------- |
| Reply              | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ✅ **Message** menu, navigation toolbar          | **Reply by Guess**                                   |
| Reply all          | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ✅ **Message** menu, navigation toolbar          | **Reply to All**                                     |
| Forward            | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ✅ **Message** menu, navigation toolbar          | Forwards as an attachment                            |
| Mark as read       | <kbd>M</kbd>                                                 | ✅ Navigation toolbar, message list context menu |                                                      |
| Flag               | <kbd>S</kbd>                                                 | ✅ Navigation toolbar, message list context menu | **Mark as Flagged**                                  |
| Archive            | <kbd>A</kbd>                                                 | ✅ Navigation toolbar, message list context menu |                                                      |
| Move to folder     | —                                                            | ✅ Drag and drop onto the folder tree            | From the message list                                |
| Delete             | <kbd>Delete</kbd>                                            | ✅ Navigation toolbar, message list context menu | **Mark as Deleted**                                  |
| Save attachment as | —                                                            | ✅ Reader context menu, Attachments panel        | **Save this message part…**, **Save Attachment As…** |
| Open attachment    | —                                                            | ✅ Attachments panel, inline attachment menu     | **Open Attachment**, **Open Directly**               |

## Current menu

`SimplePartWidget::buildContextMenu()` in `src/Gui/SimplePartWidget.cpp` builds
the reader menu:

- 🔍 **Search…** (<kbd>Ctrl</kbd>+<kbd>F</kbd>) opens the find bar;
  <kbd>F3</kbd> and <kbd>Shift</kbd>+<kbd>F3</kbd> find the next and previous
  match, and <kbd>Esc</kbd> closes it.
- 📄 **Copy**, **Select All**, and on a link **Copy Link** or **Copy e-mail
  address**.
- 💾 **Save this message part…**, **Save whole message…**, and on an image
  **Download Image…**.
- 🎨 **Color scheme** and **Zoom** submenus.

The reader has no message actions in its menu today; they are in the **Message**
menu, the navigation toolbar and the message list menu.

## Safety and quality rules

- Opening external links must be explicit and must not silently trigger without
  user confirmation when security or privacy controls are enabled.
- Remote content and images should remain blocked by default unless the user
  opts in.
- The reading pane should never expose a destructive action without clear
  context or confirmation where required.
