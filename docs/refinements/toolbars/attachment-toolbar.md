# Attachment toolbar

**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M14 in roadmap](../../ROADMAP.md#m14-refinements)**

This proposed toolbar appears for attachment actions in the reading pane or
compose workflow.

**Legend:** ![Exists][exists] in the current code on `main` ·
![Planned][planned] not implemented yet

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

| Action                 | Shortcut | Availability                                            | Notes                                  |
| ---------------------- | -------- | ------------------------------------------------------- | -------------------------------------- |
| Open                   | —        | ![Exists][exists] Attachments panel header              | **Open Attachment**                    |
| Save as                | —        | ![Exists][exists] Attachments panel header              | **Save Attachment As…**                |
| Preview                | —        | ![Exists][exists] Inline attachment menu                | **Show Preview** for previewable types |
| Download               | —        | ![Exists][exists] Inline attachment menu                | **Download**                           |
| Open with              | —        | ![Planned][planned]                                     |                                        |
| Add attachment         | —        | ![Exists][exists] Composer **Attach…** button           |                                        |
| Remove attachment      | —        | ![Exists][exists] Composer attachment list context menu | **Remove**                             |
| Rename attachment      | —        | ![Exists][exists] Composer attachment list context menu | **Rename…**                            |
| Inspect security state | —        | ![Planned][planned]                                     |                                        |
| Save all attachments   | —        | ![Planned][planned]                                     |                                        |
| Share                  | —        | ![Planned][planned]                                     |                                        |
| Show in file manager   | —        | ![Planned][planned]                                     |                                        |
| Scan with ClamAV       | —        | ![Planned][planned]                                     |                                        |

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

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
