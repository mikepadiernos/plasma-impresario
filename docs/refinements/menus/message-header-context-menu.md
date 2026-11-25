# Message header context menu

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M14 in roadmap](../../ROADMAP.md#m14-refinements)**

This proposed menu appears in the message header row or metadata region for the
currently selected message.

**Legend:** ![Exists][exists] in the current code on `main` ·
![Planned][planned] not implemented yet

## Purpose

This is the compact metadata and quick-action menu for the message header,
designed for fast, low-fuss mail processing without overwhelming the reading
pane.

## Menu tree

```text
└─ Message header context menu
   ├─ Open in new window
   ├─ Reply
   ├─ Reply all
   ├─ Forward
   ├─ View source
   ├─ Show details
   ├─ Expand / collapse headers
   ├─ Copy sender
   ├─ Copy recipients
   ├─ Copy subject
   ├─ Copy message ID
   ├─ Add sender to contacts
   ├─ Add recipient to contacts
   ├─ Move to folder
   ├─ Archive
   ├─ Mark as read
   └─ Flag
```

## Actions

| Action                    | Shortcut                                                     | Availability                                                       | Notes                                                                          |
| ------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------ | ------------------------------------------------------------------------------ |
| Open in new window        | <kbd>Ctrl</kbd>+<kbd>Return</kbd>                            | ![Exists][exists] Shortcut only, in the message list               |                                                                                |
| Reply                     | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ![Exists][exists] **Message** menu, navigation toolbar             | **Reply by Guess**                                                             |
| Reply all                 | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ![Exists][exists] **Message** menu, navigation toolbar             | **Reply to All**                                                               |
| Forward                   | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ![Exists][exists] **Message** menu, navigation toolbar             | Forwards as an attachment                                                      |
| View source               | —                                                            | ![Exists][exists] Message list context menu                        | **View Message Source…**                                                       |
| Show details              | <kbd>Ctrl</kbd>+<kbd>U</kbd>                                 | ![Exists][exists] Message list context menu, Message Details panel | **View Message Headers…**; the Message Details panel is off by default         |
| Expand / collapse headers | —                                                            | ![Planned][planned]                                                |                                                                                |
| Copy sender               | —                                                            | ![Exists][exists] Header address context menu                      | Right-click the sender: **Copy e-mail address** or **Copy text**               |
| Copy recipients           | —                                                            | ![Planned][planned]                                                | Each recipient address can be copied on its own                                |
| Copy subject              | —                                                            | ![Planned][planned]                                                |                                                                                |
| Copy message ID           | —                                                            | ![Planned][planned]                                                |                                                                                |
| Add sender to contacts    | —                                                            | ![Planned][planned]                                                | The Contacts panel offers **Open in Address Book** with an address-book plugin |
| Add recipient to contacts | —                                                            | ![Planned][planned]                                                |                                                                                |
| Move to folder            | —                                                            | ![Exists][exists] Drag and drop onto the folder tree               |                                                                                |
| Archive                   | <kbd>A</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    |                                                                                |
| Mark as read              | <kbd>M</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    |                                                                                |
| Flag                      | <kbd>S</kbd>                                                 | ![Exists][exists] Navigation toolbar, message list context menu    | **Mark as Flagged**                                                            |

> [!NOTE]
>
> Today only the addresses in the header have a context menu
> (`src/Gui/OneEnvelopeAddress.cpp`). The Message Details panel has a known
> limitation; see the
> [related messages and message details caveat](../../CAVEATS.md#related-messages-and-message-details-panels).

## Constraints

- Header actions should be a compact subset of the message list menu.
- Context should remain specific to the currently selected message and its
  metadata.
- Security-sensitive actions such as opening external links or viewing raw
  source should be explicit and clearly labeled.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
