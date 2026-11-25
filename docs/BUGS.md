# Reported bugs

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Edge cases](EDGE.md)** ·
**[Caveats](CAVEATS.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

This log records confirmed user-visible bugs and reliability issues. Statuses
match the project board as of 2026-11-25.

Legend: 🐞 open bug · 🚧 fix in progress · 🧪 fixed, needs verification · ✅
fixed and Completed or Merged

> [!IMPORTANT]
>
> An issue is complete only when its project status is **Completed** or
> **Merged**. **Completed** does not necessarily mean merged into `main`.

## Status

| Status    | Issue                                                                 | Bug                                                              | Board       |
| --------- | --------------------------------------------------------------------- | ---------------------------------------------------------------- | ----------- |
| 🚧 Fixing | [#427](https://github.com/mikepadiernos/plasma-impresario/issues/427) | APPENDUID with several UIDs reads past the end of the response   | Development |
| 🧪 Fixed  | [#426](https://github.com/mikepadiernos/plasma-impresario/issues/426) | Message list view can crash while being destroyed                | Review      |
| ✅ Fixed  | [#425](https://github.com/mikepadiernos/plasma-impresario/issues/425) | Threaded quick search sends search words unquoted                | Merged      |
| ✅ Fixed  | [#376](https://github.com/mikepadiernos/plasma-impresario/issues/376) | Reply to All but Me keeps the user's address without an identity | Merged      |
| ✅ Fixed  | [#366](https://github.com/mikepadiernos/plasma-impresario/issues/366) | An untrusted SMTP certificate cannot be accepted                 | Merged      |
| ✅ Fixed  | [#364](https://github.com/mikepadiernos/plasma-impresario/issues/364) | Sending hangs when the SMTP connection fails                     | Merged      |
| ✅ Fixed  | [#241](https://github.com/mikepadiernos/plasma-impresario/issues/241) | Plasma displays a generic icon instead of the application icon   | Merged      |
| ✅ Fixed  | [#228](https://github.com/mikepadiernos/plasma-impresario/issues/228) | Installed app missing from Plasma application menu               | Merged      |

Limitations of implemented features are tracked in [caveats](CAVEATS.md), and
unconfirmed scenarios in [edge cases](EDGE.md).

> [!TIP]
>
> To report a new bug, add it here with the template below and create a matching
> issue titled `[bug] …` with the `bug` label on the project board. Link the
> issue from the entry.

## Open issues

### [bug] APPENDUID with several UIDs reads past the end of the response

| Field            | Details                                                                                                                                                                                               |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Issue**        | [#427](https://github.com/mikepadiernos/plasma-impresario/issues/427)                                                                                                                                 |
| **Status**       | 🚧 Development                                                                                                                                                                                        |
| **Reproduction** | The server answers an APPEND with a UID set, such as `A003 OK [APPENDUID 38505 3955:3957] APPEND completed`.                                                                                          |
| **Expected**     | The response is parsed into the UIDVALIDITY and the UIDs, or rejected as malformed.                                                                                                                   |
| **Actual**       | `LowLevelParser::getSequence()` expects a trailing CRLF, which the APPENDUID branch does not add, so a UID set reads past the end of the buffer. Debug builds abort on `ASSERT: "n <= d.size - pos"`. |

## Fixed issues

### [bug] Message list view can crash while being destroyed

| Field            | Details                                                                                                                                                                                                                                                |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Issue**        | [#426](https://github.com/mikepadiernos/plasma-impresario/issues/426)                                                                                                                                                                                  |
| **Status**       | 🧪 Review                                                                                                                                                                                                                                              |
| **Reproduction** | Delete a message list view while its header still reports a geometry change, for example right after a resize.                                                                                                                                         |
| **Expected**     | The view is destroyed cleanly.                                                                                                                                                                                                                         |
| **Actual**       | The header is deleted after `~MsgListView` has run, and its late `geometriesChanged` signal still calls `slotFixSize()` on the partly destroyed view. `test_MsgListViewSelection` on the M03 branch disconnects the signal by hand to avoid the crash. |
| **Fix**          | Disconnect the header's signals from the view in `~MsgListView`, before QWidget deletes the header. `test_MsgListViewTeardown` destroys a shown, resized view and emits the signals of a header that outlives its view.                                |

### [bug] Threaded quick search sends search words unquoted

| Field            | Details                                                                                                                                                                                                                                                               |
| ---------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Issue**        | [#425](https://github.com/mikepadiernos/plasma-impresario/issues/425)                                                                                                                                                                                                 |
| **Status**       | ✅ Merged                                                                                                                                                                                                                                                             |
| **Reproduction** | Turn on threading in a folder on a server with THREAD, then quick-search for two words, such as `quarterly report`.                                                                                                                                                   |
| **Expected**     | The client sends `UID THREAD REFS utf-8 SUBJECT "quarterly report"` and shows the matching threads.                                                                                                                                                                   |
| **Actual**       | Every THREAD search criterion was sent as an atom, `SUBJECT quarterly report`, so the server rejected the command or searched for other words.                                                                                                                        |
| **Fix**          | Encode THREAD search criteria as SORT and SEARCH do: a single pre-formatted criterion is sent as is, and other values are quoted when needed. Incremental threading passes its `INTHREAD` criterion pre-formatted. `test_Imap_Threading` covers a value with a space. |

### [bug] Reply to All but Me keeps the user's address without an identity

| Field            | Details                                                                                                                                                                                                                                                            |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Issue**        | [#376](https://github.com/mikepadiernos/plasma-impresario/issues/376)                                                                                                                                                                                              |
| **Status**       | ✅ Merged                                                                                                                                                                                                                                                          |
| **Reproduction** | Set an account to **No identity**, then use *Reply to All but Me* on a message addressed to the account.                                                                                                                                                           |
| **Expected**     | The account's own address is left out of the recipients.                                                                                                                                                                                                           |
| **Actual**       | With no identities there was no address to exclude, so the reply kept the account's address, like *Reply to All*.                                                                                                                                                  |
| **Fix**          | Treat the account's IMAP user name as one of its own addresses when it is a plain email address, and exclude it in the main window and in the composer, which uses the account it was opened for. `test_AccountIdentities` and `test_Composer_responses` cover it. |

### [bug] An untrusted SMTP certificate cannot be accepted

| Field            | Details                                                                                                                                                                                                                                                                                                                |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Issue**        | [#366](https://github.com/mikepadiernos/plasma-impresario/issues/366)                                                                                                                                                                                                                                                  |
| **Status**       | ✅ Merged                                                                                                                                                                                                                                                                                                              |
| **Reproduction** | Send a message through an SMTP server whose certificate the system does not trust, such as a self-signed one.                                                                                                                                                                                                          |
| **Expected**     | As with IMAP, the composer shows the certificate and asks whether to trust it, and remembers an accepted public key for the account.                                                                                                                                                                                   |
| **Actual**       | Sending failed with *Cannot send message due to an SSL/TLS error*, and the certificate could not be accepted.                                                                                                                                                                                                          |
| **Fix**          | Pause the TLS handshake on certificate errors and ask in the composer. Continue with an accepted key and remember it per account; accept that key later without asking, and forget it when the SMTP host changes. `test_SmtpClient` covers implicit TLS, STARTTLS, rejection, pinned and changed keys, and cancelling. |

### [bug] Sending hangs when the SMTP connection fails

| Field            | Details                                                                                                                                                                                                                  |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| **Issue**        | [#364](https://github.com/mikepadiernos/plasma-impresario/issues/364)                                                                                                                                                    |
| **Status**       | ✅ Merged                                                                                                                                                                                                                |
| **Reproduction** | Send a message through an SMTP server that refuses the connection, drops it, or fails the TLS handshake.                                                                                                                 |
| **Expected**     | Sending fails with the connection error, such as *Connection refused*.                                                                                                                                                   |
| **Actual**       | A refused connection reports nothing and sending never finishes; other failures report only a generic message. The SMTP client connected Qt 5's `QAbstractSocket::error` signal by name, which no longer exists in Qt 6. |
| **Fix**          | Connect `errorOccurred`, report each failure once, and ignore errors while closing after `QUIT`. `test_SmtpClient` covers delivery and the three failures.                                                               |

### [bug] Plasma displays a generic icon instead of the application icon

| Field            | Details                                                                                                                          |
| ---------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| **Issue**        | [#241](https://github.com/mikepadiernos/plasma-impresario/issues/241)                                                            |
| **Status**       | ✅ Merged                                                                                                                        |
| **Reproduction** | Launch the installed application in Plasma with an icon theme containing a generic `plasma` icon, such as Breeze.                |
| **Expected**     | The launcher and task manager show the project icon.                                                                             |
| **Actual**       | The `plasma-impresario` icon name can resolve to the generic `plasma` icon instead of the application icon in the hicolor theme. |
| **Fix**          | Use the application ID as the icon name and refresh an existing hicolor cache during installation.                               |

### [bug] Installed app missing from Plasma application menu

| Field            | Details                                                                                                                       |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| **Issue**        | [#228](https://github.com/mikepadiernos/plasma-impresario/issues/228)                                                         |
| **Status**       | ✅ Merged                                                                                                                     |
| **Reproduction** | Install to `~/.local` and open the Plasma app menu when the session PATH does not include `~/.local/bin`.                     |
| **Expected**     | Plasma Impresario appears and can be launched.                                                                                |
| **Actual**       | The installed desktop entry uses a bare `TryExec` and is hidden when the executable cannot be found in Plasma's session PATH. |
| **Fix**          | Install launchers with absolute executable paths and refresh the Plasma application database after upgrading.                 |

## Report template

### Summary

- Short description of the regression

### Steps to reproduce

1. Starting condition
2. User action
3. Observed result

### Expected behavior

- Required behavior

### Actual behavior

- Current behavior

### Notes

- Environment, logs, issue link and follow-up status
