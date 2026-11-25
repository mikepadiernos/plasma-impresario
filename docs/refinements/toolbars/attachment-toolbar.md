# Attachment toolbar

**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M12 in roadmap](../../ROADMAP.md#m12-refinements)**

This proposed toolbar appears for attachment actions in the reading pane or
compose workflow.

**Legend:** ✅ exists in the current code on `feature/m02-shell-next` · 📋
planned only

## Purpose

This is the attachment action layer for either reading or composition. It gives
users quick access to safe and useful attachment handling without sacrificing
clarity.

## Toolbar tree

```text
└─ Attachment toolbar
   ├─ Left side
   │  ├─ Open
   │  ├─ Save as
   │  ├─ Preview
   │  ├─ Download
   │  └─ Open with
   ├─ Center
   │  ├─ Add attachment
   │  ├─ Remove attachment
   │  ├─ Rename attachment
   │  └─ Inspect security state
   └─ Right side
      ├─ Save all attachments
      ├─ Share
      ├─ Show in file manager
      └─ Scan with ClamAV
```

## Actions

The closest existing toolbar is the header of the Attachments embedded panel,
which has **Open Attachment** and **Save Attachment As…** buttons. The panel is
shown by default. Other actions are listed where they exist.

| Action                 | Shortcut | Availability                             | Notes                                  |
| ---------------------- | -------- | ---------------------------------------- | -------------------------------------- |
| Open                   | —        | ✅ Attachments panel header              | **Open Attachment**                    |
| Save as                | —        | ✅ Attachments panel header              | **Save Attachment As…**                |
| Preview                | —        | ✅ Inline attachment menu                | **Show Preview** for previewable types |
| Download               | —        | ✅ Inline attachment menu                | **Download**                           |
| Open with              | —        | 📋 Planned                               |                                        |
| Add attachment         | —        | ✅ Composer **Attach…** button           |                                        |
| Remove attachment      | —        | ✅ Composer attachment list context menu | **Remove**                             |
| Rename attachment      | —        | ✅ Composer attachment list context menu | **Rename…**                            |
| Inspect security state | —        | 📋 Planned                               |                                        |
| Save all attachments   | —        | 📋 Planned                               |                                        |
| Share                  | —        | 📋 Planned                               |                                        |
| Show in file manager   | —        | 📋 Planned                               |                                        |
| Scan with ClamAV       | —        | 📋 Planned                               |                                        |

See the
[attachment context menu](../menus/attachment-context-menu.md#current-menus) for
every current attachment menu.

## Behavioral rules

- Dangerous or blocked attachments must be clearly marked before open or save
  actions proceed.
- Attachment actions should remain context-aware and safe even when files are
  large or partially loaded.
- The toolbar should not cluster all file management tasks into one overloaded
  area.
