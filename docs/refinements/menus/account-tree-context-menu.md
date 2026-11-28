# Account tree context menu

**[Documentation](../../README.md)** · **[Roadmap: M13](../../ROADMAP.md#m13-refinements)** ·
**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Toolbars](../toolbars/README.md)** · **[Interface design](../../philosophy/designs.md)**

This proposed menu appears when the user right-clicks an account in the
sidebar navigation tree. It maps each entry to the account actions that exist
today.

Legend: ![Exists][exists] in the current code on `main`, not checked in a real
Plasma session · ![Planned][planned] not implemented yet

## Purpose

This menu is about account lifecycle and maintenance. It should not contain
message actions that belong in the message list or reading pane.

## Menu tree

```text
└─ Account context menu
   ├─ Open account
   ├─ Expand / collapse account tree
   ├─ Synchronize now
   ├─ Pause or resume sync
   ├─ Edit account
   ├─ Sign in again
   ├─ Remove account
   ├─ Account settings
   ├─ Create folder
   ├─ Rename account
   ├─ Set as default account
   ├─ Show unread counts
   └─ Show account in sidebar
```

## Actions

| Action                         | Shortcut                                                                                         | Availability                                                                                | Notes                                                                                         |
| ------------------------------ | ------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| Open account                   | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd>, <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd> | ![Exists][exists] Account header in the folder sidebar, navigation toolbar account switcher | Switches the active account                                                                   |
| Expand / collapse account tree | <kbd>Return</kbd>, <kbd>Space</kbd> on a focused header                                          | ![Exists][exists] Account header in the folder sidebar                                      | Activating the active account's header collapses or expands its folders; not a menu entry yet |
| Synchronize now                | —                                                                                                | ![Exists][exists] Folder tree context menu, **Application › Debugging**                     | **Reload Everything** reloads the active account                                              |
| Pause or resume sync           | —                                                                                                | ![Exists][exists] **Application › Network Access**                                          | Also in the toolbar network indicator                                                         |
| Edit account                   | —                                                                                                | ![Exists][exists] Account header menu, **Application › Account Settings…**                  | The header menu switches to that account first; the Application menu edits the active account |
| Sign in again                  | —                                                                                                | ![Exists][exists] Account header menu                                                       | **Sign In…**, shown for OAuth accounts only                                                   |
| Remove account                 | —                                                                                                | ![Exists][exists] Account header menu, **Application › Remove Account…**                    | Asks for confirmation; disabled while only one account exists; server mail is kept            |
| Account settings               | —                                                                                                | ![Exists][exists] Account header menu, **Application › Account Settings…**                  | A dedicated dialog; application-wide options stay in **Settings…**                            |
| Create folder                  | —                                                                                                | ![Exists][exists] Folder tree context menu                                                  | **Create New Mailbox…** on empty space                                                        |
| Rename account                 | —                                                                                                | ![Exists][exists] Account header menu, **Application › Rename Account…**                    | Changes the display name of the account                                                       |
| Set as default account         | —                                                                                                | ![Planned][planned]                                                                         |                                                                                               |
| Show unread counts             | —                                                                                                | ![Planned][planned]                                                                         | Headers always show the account's Inbox unread count; there is no toggle                      |
| Show account in sidebar        | —                                                                                                | ![Planned][planned]                                                                         |                                                                                               |

## Current menu

With two or more accounts, the folder sidebar shows a header per account with
its name and Inbox unread count. The headers take keyboard focus:
<kbd>Up</kbd> and <kbd>Down</kbd> move between them, and <kbd>Return</kbd>,
<kbd>Enter</kbd> or <kbd>Space</kbd> activates one. Right-clicking a header, or
pressing the Menu key on it, opens the account header menu:

- **Account Settings…**, after switching to that account
- **Rename Account…**
- **Sign In…**, for OAuth accounts only
- **Remove Account…**, disabled while only one account exists

The account switcher in the navigation toolbar still switches accounts, and
the same account actions remain in the **Application** menu, where they act on
the active account. See the
[multiple accounts caveat](../../CAVEATS.md#multiple-accounts).

## Design rules

- Account deletion must require confirmation and show the number of folders,
  messages, and local data affected.
- Sync actions should remain visible even when the account is offline or broken.
- Account settings should open in a dedicated dialog with no ambiguous side
  effects.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
