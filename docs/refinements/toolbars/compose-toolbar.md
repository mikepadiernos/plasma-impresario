# Compose toolbar

**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M14 in roadmap](../../ROADMAP.md#m14-refinements)**

This proposed toolbar appears in the message composer and supports drafting,
recipients, and send actions.

**Legend:** ✅ exists in the current code on `main` · 📋 planned only

## Purpose

This is the writing action surface for the message composer. It should support
focused composing while keeping the send flow clear and reliable.

## Toolbar tree

```text
└─ Compose toolbar
   ├─ Left side
   │  ├─ Save draft
   │  ├─ Attach files
   │  ├─ Add recipient
   │  ├─ Insert signature
   │  ├─ Format text
   │  └─ Spellcheck
   ├─ Center
   │  ├─ Send now
   │  ├─ Send later
   │  ├─ Schedule send
   │  └─ Save as template
   └─ Right side
      ├─ Switch to plain text / rich text
      ├─ Validate recipients
      ├─ Check attachment safety
      └─ Compose settings
```

## Actions

The composer has no toolbar today. The Availability column names where an
equivalent action exists.

| Action                           | Shortcut | Availability                  | Notes                                                                              |
| -------------------------------- | -------- | ----------------------------- | ---------------------------------------------------------------------------------- |
| Save draft                       | —        | ✅ Composer close prompt      | Closing an unsent message asks **Save Draft?**; autosave runs every 30 seconds     |
| Attach files                     | —        | ✅ **Attach…** button         |                                                                                    |
| Add recipient                    | —        | ✅ Recipient rows             | An empty recipient row is always available                                         |
| Insert signature                 | —        | 📋 Planned                    | The sender identity's signature is inserted automatically                          |
| Format text                      | —        | 📋 Planned                    |                                                                                    |
| Spellcheck                       | —        | ✅ Optional Sonnet plugin     | Highlights misspelled words as you type                                            |
| Send now                         | —        | ✅ **Send** button            |                                                                                    |
| Send later                       | —        | 📋 Planned                    |                                                                                    |
| Schedule send                    | —        | 📋 Planned                    |                                                                                    |
| Save as template                 | —        | 📋 Planned                    |                                                                                    |
| Switch to plain text / rich text | —        | 📋 Planned                    |                                                                                    |
| Validate recipients              | —        | ✅ Recipient fields, **Send** | Unparsable addresses are tinted red; sending reports invalid or missing recipients |
| Check attachment safety          | —        | 📋 Planned                    |                                                                                    |
| Compose settings                 | —        | 📋 Planned                    |                                                                                    |

## Current composer controls

The composer (`src/Gui/ComposeWidget.cpp`) has a button row instead of a
toolbar:

- ✉️ **Send** and **Cancel**
- 🧵 a reply-marking button that switches between **New Thread** and
  **Threaded**
- ↩️ a reply-mode button with **Hand Picked Recipients** and the reply modes
  from the **Message** menu
- 📎 an **Attach…** button next to the attachment list

## Behavioral rules

- The send action must remain the most prominent, visible, and trustworthy
  control.
- Draft safety and attachment safety should be easily accessible.
- Formatting controls should remain compact, especially for keyboard and desktop
  users.
