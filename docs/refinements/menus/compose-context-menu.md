# Compose context menu

This menu appears when the user right-clicks in the composer editor, recipient field, attachment list, or signature area.

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

## Safety and quality rules
- Composer actions should remain consistent with standard desktop text editing behavior.
- The recipient field should avoid destructive actions unless the user explicitly confirms.
- Attachment operations should remain local, visible, and safe before sending.

## Purpose

The compose context menu is for writing, recipient management, and safe mail composition. It should feel familiar to desktop users while preserving mail-specific correctness.
