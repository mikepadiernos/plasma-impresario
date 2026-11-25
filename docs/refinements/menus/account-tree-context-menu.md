# Account tree context menu

**[Refinements](../README.md)** · **[Menus](README.md)** ·
**[Interface design](../../philosophy/designs.md)** ·
**[M14 in roadmap](../../ROADMAP.md#m14-refinements)**

This proposed menu appears when the user right-clicks an account in the
account/sidebar navigation tree.

**Legend:** ![Exists][exists] in the current code on `main` ·
![Planned][planned] not implemented yet

## Purpose

This menu is about account lifecycle and maintenance. It should not contain
message actions that belong in the message list or reading pane.

> [!NOTE]
>
> There is no account tree today. The folder tree shows the active account, and
> the account selector in the navigation toolbar switches accounts
> (<kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd> and
> <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd>). Account actions live in the
> **Application** menu and act on the active account. See the
> [multiple accounts caveat](../../CAVEATS.md#multiple-accounts).

## Menu tree

```text
└─ Account context menu
   ├─ Open account
   ├─ Expand / collapse account tree
   ├─ Synchronize now
   ├─ Pause or resume sync
   ├─ Edit account
   ├─ Remove account
   ├─ Account settings
   ├─ Create folder
   ├─ Rename account
   ├─ Set as default account
   ├─ Show unread counts
   └─ Show account in sidebar
```

## Actions

| Action                         | Shortcut                                                                                         | Availability                                                            | Notes                                                                   |
| ------------------------------ | ------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| Open account                   | <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgDown</kbd>, <kbd>Ctrl</kbd>+<kbd>Alt</kbd>+<kbd>PgUp</kbd> | ![Exists][exists] Navigation toolbar account selector                   | Switches the active account                                             |
| Expand / collapse account tree | —                                                                                                | ![Planned][planned]                                                     |                                                                         |
| Synchronize now                | —                                                                                                | ![Exists][exists] Folder tree context menu, **Application › Debugging** | **Reload Everything** reloads the active account                        |
| Pause or resume sync           | —                                                                                                | ![Exists][exists] **Application › Network Access**                      | Also in the toolbar network indicator                                   |
| Edit account                   | —                                                                                                | ![Exists][exists] **Application › Account Settings…**                   | Edits the active account; switch accounts to edit another               |
| Remove account                 | —                                                                                                | ![Exists][exists] **Application › Remove Account…**                     | Asks for confirmation; needs at least two accounts; server mail is kept |
| Account settings               | —                                                                                                | ![Exists][exists] **Application › Account Settings…**                   | A dedicated dialog; application-wide options stay in **Settings…**      |
| Create folder                  | —                                                                                                | ![Exists][exists] Folder tree context menu                              | **Create New Mailbox…** on empty space                                  |
| Rename account                 | —                                                                                                | ![Exists][exists] **Application › Rename Account…**                     | Changes the display name of the active account                          |
| Set as default account         | —                                                                                                | ![Planned][planned]                                                     |                                                                         |
| Show unread counts             | —                                                                                                | ![Planned][planned]                                                     |                                                                         |
| Show account in sidebar        | —                                                                                                | ![Planned][planned]                                                     |                                                                         |

## Safety and quality rules

- Account deletion must require confirmation and show the number of folders,
  messages, and local data affected.
- Sync actions should remain visible even when the account is offline or broken.
- Account settings should open in a dedicated dialog with no ambiguous side
  effects.

[exists]: https://img.shields.io/badge/Exists-1a7f37
[planned]: https://img.shields.io/badge/Planned-6e7781
