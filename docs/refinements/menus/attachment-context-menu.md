# Attachment context menu

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M12 in roadmap](../../ROADMAP.md#m12-refinements)**

This proposed menu appears when the user right-clicks an attachment in the
reading pane, attachment tray, or compose attachment list.

**Legend:** ✅ exists in the current code on `feature/m02-shell-next` · 📋
planned only

## Purpose

This menu is the attachment action surface. It should make safe and useful
desktop file handling available while keeping the user aware of potential risk.

## Menu tree

```text
└─ Attachment context menu
   ├─ Open
   ├─ Open with
   ├─ Save as
   ├─ Save all
   ├─ Download
   ├─ Preview
   ├─ Show in file manager
   ├─ Copy file path
   ├─ Rename
   ├─ Remove
   ├─ Add to notes
   ├─ Forward as attachment
   ├─ Inspect security state
   └─ Scan with ClamAV
```

## Actions

| Action                 | Shortcut | Availability                                 | Notes                                                                 |
| ---------------------- | -------- | -------------------------------------------- | --------------------------------------------------------------------- |
| Open                   | —        | ✅ Attachments panel, inline attachment menu | **Open**, or activate the item; inline **Open Directly**              |
| Open with              | —        | 📋 Planned                                   |                                                                       |
| Save as                | —        | ✅ Attachments panel                         | **Save As…** in the menu, **Save Attachment As…** in the panel header |
| Save all               | —        | 📋 Planned                                   | The reader's **Save whole message…** saves the full message instead   |
| Download               | —        | ✅ Inline attachment menu                    | **Download**                                                          |
| Preview                | —        | ✅ Inline attachment menu                    | **Show Preview** for previewable types                                |
| Show in file manager   | —        | 📋 Planned                                   |                                                                       |
| Copy file path         | —        | 📋 Planned                                   |                                                                       |
| Rename                 | —        | ✅ Composer attachment list                  | **Rename…** changes the name sent with the message                    |
| Remove                 | —        | ✅ Composer attachment list                  | **Remove**                                                            |
| Add to notes           | —        | 📋 Planned                                   |                                                                       |
| Forward as attachment  | —        | 📋 Planned                                   | **Forward** sends the whole message as an attachment                  |
| Inspect security state | —        | 📋 Planned                                   |                                                                       |
| Scan with ClamAV       | —        | 📋 Planned                                   |                                                                       |

## Current menus

Attachment actions are split across three places today:

- 📎 **Attachments embedded panel** (`src/Gui/AttachmentPanel.cpp`): header
  buttons **Open Attachment** and **Save Attachment As…**, and a context menu
  with **Open** and **Save As…**.
- 📎 **Inline attachment drop-down** in the reader
  (`src/Gui/AttachmentView.cpp`): **Download**, **Open Directly**, **Show
  Preview** for previewable types, and **Show Message Source** for attached
  messages.
- 📎 **Composer attachment list** (`src/Gui/ComposerAttachmentsList.cpp`):
  **Send Inline**, **Rename…** and **Remove**.

## Safety and quality rules

- Open and save actions must respect trusted, blocked, or suspicious file
  states.
- Dangerous or executable attachment types should be clearly flagged before
  open/save actions.
- Quarantine or blocked-by-policy states should be surfaced as part of the
  attachment action flow.
- File manager integration should remain consistent with KDE desktop
  conventions.
