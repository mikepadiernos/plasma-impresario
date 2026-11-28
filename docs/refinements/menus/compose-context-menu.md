# Compose context menu

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed menu appears when the user right-clicks in the composer editor,
recipient field, attachment list, or signature area. It maps each entry to the
composer actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

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

| Action                   | Shortcut                                      | Availability                             | Notes                                                                           |
| ------------------------ | --------------------------------------------- | ---------------------------------------- | ------------------------------------------------------------------------------- |
| Undo                     | <kbd>Ctrl</kbd>+<kbd>Z</kbd>                  | ![Exists][exists] Editor context menu    | Standard Qt text editing                                                        |
| Redo                     | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>Z</kbd> | ![Exists][exists] Editor context menu    | Standard Qt text editing                                                        |
| Cut                      | <kbd>Ctrl</kbd>+<kbd>X</kbd>                  | ![Exists][exists] Editor context menu    |                                                                                 |
| Copy                     | <kbd>Ctrl</kbd>+<kbd>C</kbd>                  | ![Exists][exists] Editor context menu    |                                                                                 |
| Paste                    | <kbd>Ctrl</kbd>+<kbd>V</kbd>                  | ![Exists][exists] Editor context menu    |                                                                                 |
| Paste without formatting | —                                             | ![Planned][planned]                      |                                                                                 |
| Select all               | <kbd>Ctrl</kbd>+<kbd>A</kbd>                  | ![Exists][exists] Editor context menu    |                                                                                 |
| Find                     | —                                             | ![Planned][planned]                      |                                                                                 |
| Replace                  | —                                             | ![Planned][planned]                      |                                                                                 |
| Insert signature         | —                                             | ![Planned][planned]                      | The selected sender identity's signature is inserted and replaced automatically |
| Insert quote             | —                                             | ![Exists][exists] Editor context menu    | **Paste as Quoted Text**                                                        |
| Format text              | —                                             | ![Planned][planned]                      |                                                                                 |
| Check spelling           | —                                             | ![Exists][exists] Optional Sonnet plugin | Highlights misspelled words as you type; no menu entry of its own               |

### Recipient actions

| Action                 | Shortcut | Availability                                   | Notes                                                                              |
| ---------------------- | -------- | ---------------------------------------------- | ---------------------------------------------------------------------------------- |
| Add contact            | —        | ![Planned][planned]                            |                                                                                    |
| Remove recipient       | —        | ![Planned][planned]                            | Clearing a recipient field removes the extra empty row                             |
| Validate recipients    | —        | ![Exists][exists] Recipient fields, **Send**   | Unparsable addresses are tinted red; sending reports invalid or missing recipients |
| Copy address           | —        | ![Exists][exists] Recipient field context menu | Standard line edit menu                                                            |
| Paste recipient        | —        | ![Exists][exists] Recipient field context menu | Standard line edit menu                                                            |
| Edit recipient details | —        | ![Planned][planned]                            |                                                                                    |

### Attachment actions

| Action               | Shortcut | Availability                                   | Notes               |
| -------------------- | -------- | ---------------------------------------------- | ------------------- |
| Add attachment       | —        | ![Exists][exists] Composer **Attach…** button  | Opens a file dialog |
| Remove attachment    | —        | ![Exists][exists] Attachment list context menu | **Remove**          |
| Save attachment list | —        | ![Planned][planned]                            |                     |
| Open attachment      | —        | ![Planned][planned]                            |                     |
| Inspect attachment   | —        | ![Planned][planned]                            |                     |

## Current menu

- The editor (`src/Gui/ComposerTextEdit.cpp`) uses Qt's standard text context
  menu and adds **Paste as Quoted Text**.
- The attachment list (`src/Gui/ComposerAttachmentsList.cpp`) offers **Send
  Inline**, **Rename…** and **Remove**.
- Drafts are saved automatically every 30 seconds. Closing an unsent message
  asks **Save Draft?** and warns that attachments are not saved with the draft.

## Design rules

- Composer actions should remain consistent with standard desktop text editing
  behavior.
- The recipient field should avoid destructive actions unless the user
  explicitly confirms.
- Attachment operations should remain local, visible, and safe before sending.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
