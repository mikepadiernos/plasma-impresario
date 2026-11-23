# Attachment context menu

This menu appears when the user right-clicks an attachment in the reading pane, attachment tray, or compose attachment list.

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

## Safety and quality rules
- Open and save actions must respect trusted, blocked, or suspicious file states.
- Dangerous or executable attachment types should be clearly flagged before open/save actions.
- Quarantine or blocked-by-policy states should be surfaced as part of the attachment action flow.
- File manager integration should remain consistent with KDE desktop conventions.

## Purpose

This menu is the attachment action surface. It should make safe and useful desktop file handling available while keeping the user aware of potential risk.
