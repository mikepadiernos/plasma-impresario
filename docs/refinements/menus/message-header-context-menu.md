# Message header context menu

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M12 in roadmap](../../ROADMAP.md#m12-refinements)**

This proposed menu appears in the message header row or metadata region for the
currently selected message.

**Legend:** ✅ exists in the current code on `feature/m02-shell-next` · 📋
planned only

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

| Action                    | Shortcut                                                     | Availability                                        | Notes                                                                          |
| ------------------------- | ------------------------------------------------------------ | --------------------------------------------------- | ------------------------------------------------------------------------------ |
| Open in new window        | <kbd>Ctrl</kbd>+<kbd>Return</kbd>                            | ✅ Shortcut only, in the message list               |                                                                                |
| Reply                     | <kbd>Ctrl</kbd>+<kbd>R</kbd>                                 | ✅ **Message** menu, navigation toolbar             | **Reply by Guess**                                                             |
| Reply all                 | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>R</kbd> | ✅ **Message** menu, navigation toolbar             | **Reply to All**                                                               |
| Forward                   | <kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>                | ✅ **Message** menu, navigation toolbar             | Forwards as an attachment                                                      |
| View source               | —                                                            | ✅ Message list context menu                        | **View Message Source…**                                                       |
| Show details              | <kbd>Ctrl</kbd>+<kbd>U</kbd>                                 | ✅ Message list context menu, Message Details panel | **View Message Headers…**; the Message Details panel is off by default         |
| Expand / collapse headers | —                                                            | 📋 Planned                                          |                                                                                |
| Copy sender               | —                                                            | ✅ Header address context menu                      | Right-click the sender: **Copy e-mail address** or **Copy text**               |
| Copy recipients           | —                                                            | 📋 Planned                                          | Each recipient address can be copied on its own                                |
| Copy subject              | —                                                            | 📋 Planned                                          |                                                                                |
| Copy message ID           | —                                                            | 📋 Planned                                          |                                                                                |
| Add sender to contacts    | —                                                            | 📋 Planned                                          | The Contacts panel offers **Open in Address Book** with an address-book plugin |
| Add recipient to contacts | —                                                            | 📋 Planned                                          |                                                                                |
| Move to folder            | —                                                            | ✅ Drag and drop onto the folder tree               |                                                                                |
| Archive                   | <kbd>A</kbd>                                                 | ✅ Navigation toolbar, message list context menu    |                                                                                |
| Mark as read              | <kbd>M</kbd>                                                 | ✅ Navigation toolbar, message list context menu    |                                                                                |
| Flag                      | <kbd>S</kbd>                                                 | ✅ Navigation toolbar, message list context menu    | **Mark as Flagged**                                                            |

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
