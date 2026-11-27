# Shell and interaction refinement

**[Documentation](../README.md)** · **[Roadmap: M14](../ROADMAP.md#m14-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Cross-product quality](00-cross-product-quality.md)** ·
**[Next: Accounts, sync and storage](02-accounts-sync-and-storage.md)** ·
**[Menus](menus/README.md)** · **[Toolbars](toolbars/README.md)**

Refines navigation, panes, layouts, keyboard flow and general interaction
quality. Board statuses are as of 2026-11-27.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                             | Issue                                                               | Board               |
| -------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M14.02: Shell and interaction](../ROADMAP.md#m14-refinements) | [#21](https://github.com/mikepadiernos/plasma-impresario/issues/21) | ![Backlog][backlog] |

## Current state

The mail shell, the Compact, Wide and One At a Time layouts, embedded panels
and <kbd>F6</kbd>/<kbd>Shift</kbd>+<kbd>F6</kbd> focus cycling were built in M02.
Offscreen tests and live IMAP tests against a synthetic Dovecot cover them; a
real Plasma session has not been verified. See
[Current interface](../philosophy/designs.md#current-interface).

The [menu](menus/README.md) and [toolbar](toolbars/README.md) specifications
record which actions exist today. The optional **View › Show Header** strip is
a simple shell header, off by default; it is not the designed header planned in
[#306](https://github.com/mikepadiernos/plasma-impresario/issues/306).

## Focus

Every feature issue below is in the board's Backlog. A dash marks a refinement
recorded only in this plan.

| Refinement                                                                                                                                                                     | Issue                                                                 |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------- |
| Refine navigation, panes, toolbars, layouts, density, focus states, and keyboard interaction                                                                                   | [#149](https://github.com/mikepadiernos/plasma-impresario/issues/149) |
| Validate the [five-region shell](../philosophy/designs.md#panes-and-placement): side-column navigation, stacked or side-by-side main content, and configurable embedded panels | —                                                                     |
| Preserve pane sizes, visibility, and focus across restarts and layout changes                                                                                                  | [#150](https://github.com/mikepadiernos/plasma-impresario/issues/150) |
| Make primary actions discoverable without crowding compact layouts; allow toolbar customization                                                                                | [#151](https://github.com/mikepadiernos/plasma-impresario/issues/151) |
| Build the designed header with search, compose, the account switcher, and connection status                                                                                    | [#306](https://github.com/mikepadiernos/plasma-impresario/issues/306) |
| Add sliding and floating panel placements, and rearrange panels in the customization mode                                                                                      | [#307](https://github.com/mikepadiernos/plasma-impresario/issues/307) |
| Mark icon menus on the main side toolbar with an accent-colored corner instead of carets, in every placement                                                                   | [#486](https://github.com/mikepadiernos/plasma-impresario/issues/486) |
| Keep spacing and theme in sync with Plasma Opus, falling back to Plasma without it                                                                                             | [#487](https://github.com/mikepadiernos/plasma-impresario/issues/487) |
| Verify keyboard-only navigation, visible focus, and predictable focus restoration after dialogs and actions                                                                    | [#152](https://github.com/mikepadiernos/plasma-impresario/issues/152) |
| Validate compact, comfortable, and focus modes at narrow window sizes and different display scales                                                                             | [#153](https://github.com/mikepadiernos/plasma-impresario/issues/153) |
| Keep menu and toolbar actions synchronized with the active account, folder, and selection, including while views refresh                                                       | [#154](https://github.com/mikepadiernos/plasma-impresario/issues/154) |
| Preserve customized menus, toolbars, shortcuts, and saved layouts when defaults change or actions are added                                                                    | [#155](https://github.com/mikepadiernos/plasma-impresario/issues/155) |
| Restore windows to a usable visible position after display changes without overriding window-manager placement                                                                 | [#156](https://github.com/mikepadiernos/plasma-impresario/issues/156) |
| Keep controls usable with long translations, right-to-left layouts, and reduced-motion preferences                                                                             | [#157](https://github.com/mikepadiernos/plasma-impresario/issues/157) |

## Recommendations for overlapping items

The two M14.02 refinements build on M09 items that come first. These
recommendations shape the M09 work so that M14 tunes it instead of redoing it.
They are proposals, not implemented behavior.

| M09 foundation                                                                                                                                                  | M14.02 refinement                                                     | Shared concern                          |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- | --------------------------------------- |
| Shell styling and corner markers ([#123](https://github.com/mikepadiernos/plasma-impresario/issues/123))                                                        | [#486](https://github.com/mikepadiernos/plasma-impresario/issues/486) | How icon menus are marked and opened    |
| Text and spacing ([#290](https://github.com/mikepadiernos/plasma-impresario/issues/290), [#292](https://github.com/mikepadiernos/plasma-impresario/issues/292)) | [#487](https://github.com/mikepadiernos/plasma-impresario/issues/487) | One spacing scale matching Plasma Opus  |
| Theme sharing with Plasma Opus ([#268](https://github.com/mikepadiernos/plasma-impresario/issues/268))                                                          | [#487](https://github.com/mikepadiernos/plasma-impresario/issues/487) | One profile format and how to follow it |

### Icon menu corners

- **Draw it in one place.** Put the marker in an app-local proxy style, the
  same one that will scale spacing, rather than in each button. Plasma Opus
  uses this design (`AppearanceStyle`, a `QProxyStyle` installed only when an
  option differs from the widget style). Every menu button then changes at
  once, and M14 only adjusts placement.
- **Decide the interaction once, in M09.** A click runs the default action;
  a press and hold, a right click, <kbd>Alt</kbd>+<kbd>Down</kbd> or the
  <kbd>Menu</kbd> key opens the menu. Buttons without a default action, such as
  the main menu and the network indicator, open the menu on click and still
  show the corner. This replaces today's split `MenuButtonPopup` arrows.
- **Use one convention across the app.** Apply the corner to every icon-only
  menu button, including the reader bar's reply, the attachment icon and the
  composer's buttons, not only the side toolbar. #486 then covers the side
  toolbar's placements and right-to-left layouts.
- **Size and contrast.** Size the triangle from the icon, about a quarter of
  its edge and at least 4 logical pixels. Give it a one-pixel edge in the
  window background color so it stays visible on accent-colored hover and
  checked states. Use the text color in high-contrast schemes.
- **Say it, not only show it.** Add "Has a menu" to the accessible
  description and mention the menu shortcut in the tooltip.
- **Test the pixels.** Render buttons offscreen to an image and check the
  corner's color and position for each placement, direction and scale.

### Spacing

- **One setting, not two.** M09.07 plans both named spacing options (compact,
  normal, comfortable, custom) and a global scale. Make them one percent scale
  from 50 to 200, Plasma Opus's range, with named presets as shortcuts (for
  example Compact 75, Normal 100, Comfortable 125). #487 then copies a single
  value.
- **Keep row density separate.** The M02 message-list density ([#75](https://github.com/mikepadiernos/plasma-impresario/issues/75))
  stays its own choice, as Plasma Opus keeps item density apart from its
  spacing scale.
- **Scale the same metrics as Plasma Opus.** Scale the layout margins and
  spacing, toolbar item spacing and margins, tab padding and dock title
  margins (`PM_Layout*`, `PM_ToolBarItemSpacing`, `PM_ToolBarItemMargin`,
  `PM_TabBarTabHSpace`, `PM_TabBarTabVSpace`, `PM_DockWidgetTitleMargin`).
  Apply them only to the main window and leave dialogs at Plasma's spacing, as
  Plasma Opus does. The same value then gives the same padding in both apps.
- **Remove hard-coded spacing first.** As of 2026-11-27, 18 non-zero
  `setContentsMargins` and `setSpacing` calls in 13 files under `src/Gui/`
  bypass the style. They are spread across the embedded panels, the message
  list, the reader, attachments, the find bar and the shell header. Route them
  through a small spacing helper during M09, or the scale will miss those
  areas.
- **Use Plasma Opus's setting names.** Store `SpacingScale`, `CornerRadius`,
  `Dividers` and `AccentColor` with Plasma Opus's names, choices and defaults,
  so that values move between the apps without translation.

### Theme profiles

- **Read the format Plasma Opus already writes.** Its exported theme profiles
  are JSON with the format `plasma-opus-theme`, version 1, and values keyed by
  setting name. They are limited to 512 KiB, 100 profiles and 100-character
  names. Import that format with the same limits, apply the keys that have a
  counterpart here, and ignore the rest. Agree with Plasma Opus on a shared
  format name before writing one.
- **Follow a file, not the other app's settings.** For the **Follow Plasma
  Opus** option, watch an exported profile at a documented shared location with
  `KDirWatch`, and wait briefly after a change before applying it. Do not read
  Plasma Opus's own configuration file, whose names are internal to it.
- **Keep personal choices personal.** As in Plasma Opus, never import the
  high-contrast safeguard, and leave mail-only choices such as account and
  folder colors out of shared profiles.
- **Allow overrides.** Let a value be kept local while following, such as
  "follow Plasma Opus except the accent color". Show in the settings which
  values came from Plasma Opus.
- **Guard against format drift.** Keep a sample Plasma Opus version 1 profile
  in the tests, along with a newer-version and an oversized file, so that a
  change on either side fails a test instead of the theme.

## Acceptance criteria

- The app remains understandable in compact, comfortable, and focus modes.
- Keyboard users can navigate the shell without visual ambiguity.
- Layout changes remain stable and restore cleanly after restart or reconnect.
- Menus and toolbar actions remain synchronized with the current account,
  folder, and selection.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
