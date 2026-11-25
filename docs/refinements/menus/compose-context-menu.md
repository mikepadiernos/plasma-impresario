# Compose context menu

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M13 in roadmap](../../ROADMAP.md#m13-refinements)**

This proposed menu appears when the user right-clicks in the composer editor,
recipient field, attachment list, or signature area.

**Legend:** ✅ exists in the current code on `feature/m02-shell-next` · 📋
planned only

## Purpose

The compose context menu is for writing, recipient management, and safe mail
composition. It should feel familiar to desktop users while preserving
mail-specific correctness.

## Menu tree

```text
└─ Compose context menu
   ├─ Editor actions
   │  ├─ Undo
   │  ├─ Redo
   │  ├─ Cut
   │  ├─ Copy
   │  ├─ Paste
   │  ├─ Paste without formatting
   │  ├─ Select all
   │  ├─ Find
   │  ├─ Replace
   │  ├─ Insert signature
   │  ├─ Insert quote
   │  ├─ Format text
   │  └─ Check spelling
   ├─ Recipient actions
   │  ├─ Add contact
   │  ├─ Remove recipient
   │  ├─ Validate recipients
   │  ├─ Copy address
   │  ├─ Paste recipient
   │  └─ Edit recipient details
   └─ Attachment actions
      ├─ Add attachment
      ├─ Remove attachment
      ├─ Save attachment list
      ├─ Open attachment
      └─ Inspect attachment
```

## Actions

### Editor actions

| Action                   | Shortcut                                      | Availability              | Notes                                                                           |
| ------------------------ | --------------------------------------------- | ------------------------- | ------------------------------------------------------------------------------- |
| Undo                     | <kbd>Ctrl</kbd>+<kbd>Z</kbd>                  | ✅ Editor context menu    | Standard Qt text editing                                                        |
| Redo                     | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> | ✅ Editor context menu    | Standard Qt text editing                                                        |
| Cut                      | <kbd>Ctrl</kbd>+<kbd>X</kbd>                  | ✅ Editor context menu    |                                                                                 |
| Copy                     | <kbd>Ctrl</kbd>+<kbd>C</kbd>                  | ✅ Editor context menu    |                                                                                 |
| Paste                    | <kbd>Ctrl</kbd>+<kbd>V</kbd>                  | ✅ Editor context menu    |                                                                                 |
| Paste without formatting | —                                             | 📋 Planned                |                                                                                 |
| Select all               | <kbd>Ctrl</kbd>+<kbd>A</kbd>                  | ✅ Editor context menu    |                                                                                 |
| Find                     | —                                             | 📋 Planned                |                                                                                 |
| Replace                  | —                                             | 📋 Planned                |                                                                                 |
| Insert signature         | —                                             | 📋 Planned                | The selected sender identity's signature is inserted and replaced automatically |
| Insert quote             | —                                             | ✅ Editor context menu    | **Paste as Quoted Text**                                                        |
| Format text              | —                                             | 📋 Planned                |                                                                                 |
| Check spelling           | —                                             | ✅ Optional Sonnet plugin | Highlights misspelled words as you type; no menu entry of its own               |

### Recipient actions

| Action                 | Shortcut | Availability                    | Notes                                                                              |
| ---------------------- | -------- | ------------------------------- | ---------------------------------------------------------------------------------- |
| Add contact            | —        | 📋 Planned                      |                                                                                    |
| Remove recipient       | —        | 📋 Planned                      | Clearing a recipient field removes the extra empty row                             |
| Validate recipients    | —        | ✅ Recipient fields, **Send**   | Unparsable addresses are tinted red; sending reports invalid or missing recipients |
| Copy address           | —        | ✅ Recipient field context menu | Standard line edit menu                                                            |
| Paste recipient        | —        | ✅ Recipient field context menu | Standard line edit menu                                                            |
| Edit recipient details | —        | 📋 Planned                      |                                                                                    |

### Attachment actions

| Action               | Shortcut | Availability                    | Notes               |
| -------------------- | -------- | ------------------------------- | ------------------- |
| Add attachment       | —        | ✅ Composer **Attach…** button  | Opens a file dialog |
| Remove attachment    | —        | ✅ Attachment list context menu | **Remove**          |
| Save attachment list | —        | 📋 Planned                      |                     |
| Open attachment      | —        | 📋 Planned                      |                     |
| Inspect attachment   | —        | 📋 Planned                      |                     |

## Current composer menus

- ✍️ The editor (`src/Gui/ComposerTextEdit.cpp`) uses Qt's standard text context
  menu and adds **Paste as Quoted Text**.
- 📎 The attachment list (`src/Gui/ComposerAttachmentsList.cpp`) offers **Send
  Inline**, **Rename…** and **Remove**.
- 💾 Drafts are saved automatically to the cache every 30 seconds. Closing an
  unsent message asks **Save Draft?**; attachments are not saved with the draft.

## Safety and quality rules

- Composer actions should remain consistent with standard desktop text editing
  behavior.
- The recipient field should avoid destructive actions unless the user
  explicitly confirms.
- Attachment operations should remain local, visible, and safe before sending.
