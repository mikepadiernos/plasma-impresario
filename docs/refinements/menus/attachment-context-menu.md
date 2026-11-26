# Attachment context menu

**[Documentation](../../README.md)** · **[Roadmap: M14](../../ROADMAP.md#m14-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed menu appears when the user right-clicks an attachment in the
reading pane, attachment tray, or compose attachment list. It maps each entry
to the attachment actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

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

| Action                 | Shortcut | Availability                                                | Notes                                                                 |
| ---------------------- | -------- | ----------------------------------------------------------- | --------------------------------------------------------------------- |
| Open                   | —        | ![Exists][exists] Attachments panel, inline attachment menu | **Open**, or activate the item; inline **Open Directly**              |
| Open with              | —        | ![Planned][planned]                                         |                                                                       |
| Save as                | —        | ![Exists][exists] Attachments panel                         | **Save As…** in the menu, **Save Attachment As…** in the panel header |
| Save all               | —        | ![Planned][planned]                                         | The reader's **Save whole message…** saves the full message instead   |
| Download               | —        | ![Exists][exists] Inline attachment menu                    | **Download**                                                          |
| Preview                | —        | ![Exists][exists] Inline attachment menu                    | **Show Preview** for previewable types                                |
| Show in file manager   | —        | ![Planned][planned]                                         |                                                                       |
| Copy file path         | —        | ![Planned][planned]                                         |                                                                       |
| Rename                 | —        | ![Exists][exists] Composer attachment list                  | **Rename…** changes the name sent with the message                    |
| Remove                 | —        | ![Exists][exists] Composer attachment list                  | **Remove**                                                            |
| Add to notes           | —        | ![Planned][planned]                                         |                                                                       |
| Forward as attachment  | —        | ![Planned][planned]                                         | **Forward** sends the whole message as an attachment                  |
| Inspect security state | —        | ![Planned][planned]                                         |                                                                       |
| Scan with ClamAV       | —        | ![Planned][planned]                                         | No ClamAV integration exists in `src/`                                |

## Current menu

Attachment actions are split across three places today:

- **Attachments embedded panel** (`src/Gui/AttachmentPanel.cpp`): header
  buttons **Open Attachment** and **Save Attachment As…**, and a context menu
  with **Open** and **Save As…**. In WebEngine builds, the reader action bar's
  attachment count chip opens this panel.
- **Inline attachment drop-down** in the reader
  (`src/Gui/AttachmentView.cpp`): **Download**, **Open Directly**, **Show
  Preview** for previewable types, and **Show Message Source** for attached
  messages.
- **Composer attachment list** (`src/Gui/ComposerAttachmentsList.cpp`):
  **Send Inline**, **Rename…** and **Remove**.

## Design rules

- Open and save actions must respect trusted, blocked, or suspicious file
  states.
- Dangerous or executable attachment types should be clearly flagged before
  open/save actions.
- Quarantine or blocked-by-policy states should be surfaced as part of the
  attachment action flow.
- File manager integration should remain consistent with KDE desktop
  conventions.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
