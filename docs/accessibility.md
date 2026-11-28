# Accessibility and keyboard audit

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Caveats](CAVEATS.md)** · **[Edge cases](EDGE.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

The [M11](ROADMAP.md#m11-release-readiness-and-polish) audit of accessible names,
keyboard-only use, localization and scaled displays
([#53](https://github.com/mikepadiernos/plasma-impresario/issues/53),
[#135](https://github.com/mikepadiernos/plasma-impresario/issues/135)). This
page lists what was checked, how, and the result recorded on 2026-11-28.

Legend: ![Pass][pass] passes · ![Fixed][fixed] found and fixed during the
audit · ![Limitation][limitation] known gap · ![Not covered][not-covered]
outside this audit, see the notes

> [!NOTE]
>
> Every check here runs offscreen in the headless Linux development container
> through Qt's accessibility interfaces, which are what AT-SPI exposes to Orca.
> Nothing on this page was heard through a screen reader or seen in a Plasma
> session; see [what the audit does not cover](#what-the-audit-does-not-cover).

## Contents

- [Result](#result)
- [What was checked](#what-was-checked)
- [Found and fixed](#found-and-fixed)
- [How to run](#how-to-run)
- [What the audit does not cover](#what-the-audit-does-not-cover)

## Result

| Area                         | Result                      | Evidence                                                                                                                         |
| ---------------------------- | --------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| Names in the main window     | ![Pass][pass]               | `test_AccessibilityAudit::mainWindowControlsHaveNames`                                                                           |
| Names in dialogs             | ![Fixed][fixed]             | `test_AccessibilityAudit::dialogControlsHaveNames`, 14 windows                                                                   |
| Unambiguous shortcuts        | ![Pass][pass]               | `test_AccessibilityAudit::shortcutsAreUnambiguous`, 28 shortcuts                                                                 |
| Tab reaches every control    | ![Pass][pass]               | `test_AccessibilityAudit::tabReachesVisibleControls`                                                                             |
| Region cycling and RTL order | ![Pass][pass]               | `test_MainWindowAccounts::focusCyclesThroughShellRegions`, `focusFollowsScreenOrder`                                             |
| Keyboard message headers     | ![Pass][pass]               | `test_Accessibility::expanderIsAKeyboardButton`, `addressLinksTakeTheKeyboard`                                                   |
| Scaled displays              | ![Pass][pass]               | `test_LayoutReadability`, and the audit at `QT_SCALE_FACTOR` 1.5 and 2                                                           |
| Localizable strings          | ![Fixed][fixed]             | Source sweep; one literal fixed                                                                                                  |
| Menu ellipses                | ![Limitation][limitation]   | Mixed `...` and `…`; see [caveat #472](CAVEATS.md#accessibility-and-theming)                                                     |
| Translations                 | ![Limitation][limitation]   | No `.po` catalog yet; see [caveat #472](CAVEATS.md#accessibility-and-theming)                                                    |
| Screen reader and Plasma     | ![Not covered][not-covered] | [#380](https://github.com/mikepadiernos/plasma-impresario/issues/380) and [M15](ROADMAP.md#m15-real-plasma-session-verification) |

## What was checked

**Accessible names.** The audit walks every visible control (buttons, fields,
combo boxes, lists, trees, spin boxes, sliders and text editors) and asks its
`QAccessibleInterface` for a name, as Orca would through AT-SPI. Scroll bars,
headers and the inner parts of spin boxes, fields and combo boxes are skipped
because the screen reader announces them through their owner. It covers:

- the main window as it opens, offline, with no account;
- every page of **Settings** and every tab inside those pages;
- **Add Account**, **Offline Search Index**, **Mail Rules**, **Activity**,
  **Configure Notifications**, **Find Saved Files**, **Go to Folder**,
  **Compose**, the shortcut overview, **Configure Shortcuts**, **About**, the
  outbox and the search builder.

**Shortcuts.** Every distinct key sequence of an enabled action or shortcut in
the window is pressed with focus in the message list, and each shortcut event
is recorded and swallowed so that nothing opens. None is ambiguous. 27 of the
28 reach an action from the list; the 28th, `Esc`, belongs to the reader,
where it returns focus to the list.

**Tab order.** Tab is pressed up to 300 times from the message list. Every
visible, enabled control that accepts Tab focus is reached; at startup that
is 13 controls.

**Scaled displays.** The audit passes unchanged at `QT_SCALE_FACTOR` 1.5 and 2.
`test_LayoutReadability` already checks that the wide and compact layouts keep
every pane readable at 800x600, 1366x768 and 2560x1440, with 10 and 16 pt text
and at 200% scale.

**Localization.** A sweep of `src/Gui` for literal strings passed to labels,
buttons, tooltips, titles and placeholders found one user-visible string
outside `i18n()`, `i18nc()` or `tr()`. The rest are object names, icon names and
a style-sheet example.

## Found and fixed

| Window or area      | Controls that had no name                                                                       |
| ------------------- | ----------------------------------------------------------------------------------------------- |
| Settings            | The settings group list and its search field                                                    |
| Settings, Theme     | Accent and role color buttons and their checkboxes, account color buttons and checkboxes        |
| Settings, overrides | Per-account override editors, now named after their setting                                     |
| Settings, other     | Mark-read delay, indexed accounts, notes folders, workspace list, Vim bindings, followed keymap |
| Search index        | The indexed folder list                                                                         |
| Find Saved Files    | The folder list                                                                                 |
| Compose             | Message text, attachment list, each recipient's type and address ("To address", "Cc address")   |
| Configure Shortcuts | The shortcut tree                                                                               |
| Reader placeholder  | Builds without a web engine showed an untranslated English placeholder                          |

The **Settings**, **Add Account**, **About** and **Configure Shortcuts**
actions also gained object names so that tests can find them.

## How to run

```bash
tools/dev-container/run.sh sh -c 'cmake --build build --target test_AccessibilityAudit && cd build && QT_QPA_PLATFORM=offscreen ./test_AccessibilityAudit'
tools/dev-container/run.sh sh -c 'cd build && QT_SCALE_FACTOR=2 QT_QPA_PLATFORM=offscreen ./test_AccessibilityAudit'
```

The test prints each control without a name, each ambiguous shortcut and each
control Tab cannot reach, with the path of object names leading to it.

## What the audit does not cover

- ![Not covered][not-covered] What Orca actually says, and in which order.
  Driving the installed app through AT-SPI in a headless session is
  [#380](https://github.com/mikepadiernos/plasma-impresario/issues/380); a real
  Plasma 6 session with Orca, Breeze High Contrast and fractional scaling is
  [M15](ROADMAP.md#m15-real-plasma-session-verification).
- ![Not covered][not-covered] Shortcuts of actions that are disabled at
  startup, such as those that need an open folder or a selected message. The
  shortcut configuration dialog refuses a sequence that is already taken, and
  `UiUtils::ShortcutOverview::duplicateShortcuts` is unit-tested.
- ![Not covered][not-covered] Windows that need a live account, such as
  account settings for an existing account or the reader with a message open.
  `test_LiveImapPanels` opens them against the synthetic Dovecot server but does
  not audit their names.
- ![Limitation][limitation] Menu items mix `...` (49 strings) and `…` (93).
  They are left alone so that existing translations keep matching; see
  [caveat #472](CAVEATS.md#accessibility-and-theming).

[fixed]: https://img.shields.io/badge/Fixed-2da44e
[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[not-covered]: https://img.shields.io/badge/Not_covered-6e7781
[pass]: https://img.shields.io/badge/Pass-2da44e
