# Attachment toolbar

This toolbar appears for attachment actions in the reading pane or compose workflow.

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

## Behavioral rules
- Dangerous or blocked attachments must be clearly marked before open or save actions proceed.
- Attachment actions should remain context-aware and safe even when files are large or partially loaded.
- The toolbar should not cluster all file management tasks into one overloaded area.

## Purpose

This is the attachment action layer for either reading or composition. It gives users quick access to safe and useful attachment handling without sacrificing clarity.
