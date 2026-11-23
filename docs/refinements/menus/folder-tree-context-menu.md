# Folder tree context menu

This menu appears when the user right-clicks a mailbox or folder in the folder tree.

```text
└─ Folder context menu
   ├─ Open folder
   ├─ Refresh folder
   ├─ New subfolder
   ├─ Rename folder
   ├─ Delete folder
   ├─ Empty folder
   ├─ Empty trash
   ├─ Move folder
   ├─ Copy folder
   ├─ Mark all as read
   ├─ Subscribe
   ├─ Unsubscribe
   ├─ Folder properties
   ├─ Set as favorite
   ├─ Set as default folder
   └─ Show folder statistics
```

## Safety and quality rules
- Empty folder and delete folder operations should be explicit and reversible through confirmation or undo windows.
- Folder operations should preserve account scope and not leak into another account or remote mailbox.
- Favorite or special-folder status must not alter the actual folder data model in unexpected ways.

## Purpose

This menu is for mailbox administration and status. It should remain focused on the folder itself and not expose read or compose actions unless they are relevant to the selected folder header.
