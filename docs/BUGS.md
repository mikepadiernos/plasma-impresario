# Reported bugs

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Edge cases](EDGE.md)** ·
**[Caveats](CAVEATS.md)**

This log records confirmed user-visible bugs and reliability issues.

## Fixed issues

### [bug] Installed app missing from Plasma application menu

- **Issue:**
  [#228](https://github.com/mikepadiernos/plasma-impresario/issues/228)
- **Status:** Merged
- **Reproduction:** Install to `~/.local` and open the Plasma app menu when the
  session PATH does not include `~/.local/bin`.
- **Expected:** Plasma Impresario appears and can be launched.
- **Actual:** The installed desktop entry uses a bare `TryExec` and is hidden
  when the executable cannot be found in Plasma's session PATH.
- **Fix:** Install launchers with absolute executable paths and refresh the
  Plasma application database after upgrading.

> [!IMPORTANT]
>
> An issue is complete only when its project status is **Completed** or
> **Merged**. **Completed** does not necessarily mean merged into `main`.

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
