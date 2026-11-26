# Message header context menu

**[Documentation](../../README.md)** · **[Roadmap: M14](../../ROADMAP.md#m14-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed menu appears in the message header row or metadata region for the
currently selected message. It maps each entry to the actions that exist today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

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

| Action                    | Shortcut                                                     | Availability                                                                                            | Notes                                                                          |
| ------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| Open in new window        | <kbd>Ctrl</kbd>+<kbd>Return</kbd>                            | ![Exists][exists] Shortcut only, in the message list                                                    |                                                                                |
| Reply                     | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ![Exists][exists] **Message** menu, navigation toolbar, reader action bar                               | **Reply by Guess**                                                             |
| Reply all                 | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ![Exists][exists] **Message** menu, navigation toolbar                                                  | **Reply to All**                                                               |
| Forward                   | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ![Exists][exists] **Message** menu, navigation toolbar, reader action bar                               | Forwards as an attachment                                                      |
| View source               | —                                                            | ![Exists][exists] Message list context menu                                                             | **View Message Source…**                                                       |
| Show details              | <kbd>Ctrl</kbd>+<kbd>U</kbd>                                 | ![Exists][exists] Message list context menu, Message Details panel                                      | **View Message Headers…**; the Message Details panel is off by default         |
| Expand / collapse headers | —                                                            | ![Planned][planned]                                                                                     |                                                                                |
| Copy sender               | —                                                            | ![Exists][exists] Header address context menu                                                           | Right-click the sender: **Copy e-mail address** or **Copy text**               |
| Copy recipients           | —                                                            | ![Planned][planned]                                                                                     | Each recipient address can be copied on its own                                |
| Copy subject              | —                                                            | ![Planned][planned]                                                                                     |                                                                                |
| Copy message ID           | —                                                            | ![Planned][planned]                                                                                     |                                                                                |
| Add sender to contacts    | —                                                            | ![Planned][planned]                                                                                     | The Contacts panel offers **Open in Address Book** with an address-book plugin |
| Add recipient to contacts | —                                                            | ![Planned][planned]                                                                                     |                                                                                |
| Move to folder            | <kbd>V</kbd>                                                 | ![Exists][exists] **Message › Move to Folder…**, navigation toolbar, drag and drop onto the folder tree |                                                                                |
| Archive                   | <kbd>A</kbd>                                                 | ![Exists][exists] Navigation toolbar, reader action bar, message list context menu                      |                                                                                |
| Mark as read              | <kbd>M</kbd>                                                 | ![Exists][exists] Navigation toolbar, reader action bar, message list context menu                      |                                                                                |
| Flag                      | <kbd>S</kbd>                                                 | ![Exists][exists] Navigation toolbar, reader action bar, message list context menu                      | **Mark as Flagged**                                                            |

## Current menu

Today only the addresses in the header have a context menu
(`src/Gui/OneEnvelopeAddress.cpp`), with **Copy e-mail address** and **Copy
text**. In builds with the WebEngine reader, the
[reader action bar](../toolbars/reading-pane-toolbar.md) above the message
covers several header actions. The Message Details panel shows Subject, From,
To, Cc, Date, Size, Folder, Account, Status and Security.

> [!NOTE]
>
> The Message Details panel has a known limitation; see the
> [related messages and message details caveat](../../CAVEATS.md#related-messages-and-message-details-panels)
> ([#264](https://github.com/mikepadiernos/plasma-impresario/issues/264)).

## Design rules

- Header actions should be a compact subset of the message list menu.
- Context should remain specific to the currently selected message and its
  metadata.
- Security-sensitive actions such as opening external links or viewing raw
  source should be explicit and clearly labeled.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
