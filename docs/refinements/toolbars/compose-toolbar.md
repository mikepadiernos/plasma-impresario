# Compose toolbar

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Toolbars](README.md)** ·
**[Menus](../menus/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed toolbar appears in the message composer and supports drafting,
recipients, and send actions. It maps each entry to the composer controls that
exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

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

| Action                           | Shortcut | Availability                                 | Notes                                                                                  |
| -------------------------------- | -------- | -------------------------------------------- | -------------------------------------------------------------------------------------- |
| Save draft                       | —        | ![Exists][exists] Composer close prompt      | Closing an edited, unsent message asks **Save Draft?**; autosave runs every 30 seconds |
| Attach files                     | —        | ![Exists][exists] **Attach…** button         | Files dropped on the message text are attached too                                     |
| Add recipient                    | —        | ![Exists][exists] Recipient rows             | An empty recipient row is always available                                             |
| Insert signature                 | —        | ![Planned][planned]                          | The sender identity's signature is inserted automatically                              |
| Format text                      | —        | ![Planned][planned]                          |                                                                                        |
| Spellcheck                       | —        | ![Exists][exists] Optional Sonnet plugin     | Highlights misspelled words as you type                                                |
| Send now                         | —        | ![Exists][exists] **Send** button            |                                                                                        |
| Send later                       | —        | ![Planned][planned]                          |                                                                                        |
| Schedule send                    | —        | ![Planned][planned]                          |                                                                                        |
| Save as template                 | —        | ![Planned][planned]                          |                                                                                        |
| Switch to plain text / rich text | —        | ![Planned][planned]                          |                                                                                        |
| Validate recipients              | —        | ![Exists][exists] Recipient fields, **Send** | Unparsable addresses are tinted red; sending reports invalid or missing recipients     |
| Check attachment safety          | —        | ![Planned][planned]                          |                                                                                        |
| Compose settings                 | —        | ![Planned][planned]                          |                                                                                        |

## Current toolbar

The composer (`src/Gui/ComposeWidget.cpp`) has a button row instead of a
toolbar:

- **Send** and **Cancel**; **Cancel** reads **Cancel...** once the message has
  been edited
- a reply-marking button that switches between **New Thread** and
  **Threaded**
- a reply-mode button with **Hand Picked Recipients** and the reply modes
  from the **Message** menu
- an **Attach…** button next to the attachment list

> [!WARNING]
>
> When the message has attachments, the **Save Draft?** prompt warns that
> attachments are not saved with the draft.

## Design rules

- The send action must remain the most prominent, visible, and trustworthy
  control.
- Draft safety and attachment safety should be easily accessible.
- Formatting controls should remain compact, especially for keyboard and desktop
  users.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
