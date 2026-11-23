# Reading pane context menu

This menu appears when the user right-clicks in the reading pane or on content inside an opened message.

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

## Safety and quality rules
- Opening external links must be explicit and must not silently trigger without user confirmation when security or privacy controls are enabled.
- Remote content and images should remain blocked by default unless the user opts in.
- The reading pane should never expose a destructive action without clear context or confirmation where required.

## Purpose

This is the safe, content-first action surface for the reading experience. It keeps content actions and mail actions clearly separated, while supporting desktop-like reading and linking behavior.
