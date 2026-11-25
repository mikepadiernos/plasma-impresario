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

| Status   | Issue                                                                 | Bug                                                            | Board  |
| -------- | --------------------------------------------------------------------- | -------------------------------------------------------------- | ------ |
| ✅ Fixed | [#364](https://github.com/mikepadiernos/plasma-impresario/issues/364) | Sending hangs when the SMTP connection fails                   | Merged |
| ✅ Fixed | [#241](https://github.com/mikepadiernos/plasma-impresario/issues/241) | Plasma displays a generic icon instead of the application icon | Merged |
| ✅ Fixed | [#228](https://github.com/mikepadiernos/plasma-impresario/issues/228) | Installed app missing from Plasma application menu             | Merged |

No open bugs are recorded. Limitations of implemented features are tracked in
[caveats](CAVEATS.md), and unconfirmed scenarios in [edge cases](EDGE.md).

> [!TIP]
>
> To report a new bug, add it here with the template below and create a matching
> issue titled `[bug] …` with the `bug` label on the project board. Link the
> issue from the entry.

## Fixed issues

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
