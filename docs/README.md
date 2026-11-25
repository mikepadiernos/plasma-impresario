# Plasma Impresario documentation

**[Application](../README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** · **[Caveats](CAVEATS.md)** ·
**[Contributing](../CONTRIBUTING.md)**

Find installation help, product direction, design and integration references,
and project tracking. Each topic has one source of truth; other documents link
to it instead of repeating it.

## Contents

- [Start here](#start-here)
- [Plan and track](#plan-and-track)
- [Design and product](#design-and-product)
- [Integrations](#integrations)
- [Specifications and references](#specifications-and-references)
- [Screenshots](#screenshots)
- [Historical archive](#historical-archive)

## Start here

| Guide                                         | What it covers                                                   |
| --------------------------------------------- | ---------------------------------------------------------------- |
| [Project README](../README.md)                | What works today, screenshots, quick start and product direction |
| [Project status](../README.md#project-status) | Milestone progress and the branch each milestone lives on        |
| [Installation](INSTALL.md)                    | Installer options, distribution packages and troubleshooting     |
| [Contributing](../CONTRIBUTING.md)            | Development workflow, board statuses and the headless container  |
| [Project instructions](../CLAUDE.md)          | Conventions for code, roadmap, board and branches                |

## Plan and track

| Document                                                        | Source of truth for                                       |
| --------------------------------------------------------------- | --------------------------------------------------------- |
| [Roadmap](ROADMAP.md)                                           | Milestones, scope, architecture and delivery order        |
| [Pending tasks](TODO.md)                                        | Blockers, verification records and resumable handoffs     |
| [Reported bugs](BUGS.md)                                        | User-visible bugs and their resolution status             |
| ![Limitation][limitation] [Caveats and security](CAVEATS.md)    | Current limitations, security posture and recommendations |
| ![Needs verification][needs-verification] [Edge cases](EDGE.md) | Boundary scenarios that still need tests                  |

> [!IMPORTANT]
>
> The roadmap defines intended scope. Pending tasks are handoffs, caveats
> describe limitations, edge cases are test candidates and bugs are confirmed
> defects. None of them is a completion marker: an item is done only when its
> issue is $\color{#2da44e}\textbf{Completed}$ or
> $\color{#8957e5}\textbf{Merged}$ on the [project
> board](https://github.com/users/mikepadiernos/projects/4).

## Design and product

| Guide                                                                  | What it covers                                                     |
| ---------------------------------------------------------------------- | ------------------------------------------------------------------ |
| [Interface design](philosophy/designs.md)                              | The current shell and panels, and the planned Plasma Impresario UI |
| [Scope and objective](philosophy/scope.md)                             | Target workflows and product boundaries                            |
| [Compatibility baseline](philosophy/compatibility.md)                  | Target environment, dependencies and verification limits           |
| ![Recommended][recommended] [Inspirations](philosophy/inspirations.md) | Mail-client and Plasma influences                                  |

## Integrations

| Guide                                                        | What it covers                                          |
| ------------------------------------------------------------ | ------------------------------------------------------- |
| [Integrations overview](integrations/README.md)              | Which roadmap milestones each integration guide feeds   |
| [KDE Plasma integration](integrations/kde-plasma.md)         | Desktop services, Plasma widgets and KDE conventions    |
| [Plasma Opus integration](integrations/plasma-opus.md)       | File dialogs, attachments, workspaces and shared themes |
| [Microsoft accounts and Exchange](integrations/microsoft.md) | Outlook.com, Microsoft 365 and Exchange Server planning |

## Specifications and references

| Guide                                                                  | What it covers                                                 |
| ---------------------------------------------------------------------- | -------------------------------------------------------------- |
| [Refinement plans](refinements/README.md)                              | Release-quality review areas and acceptance criteria           |
| ![Planned][planned] [Menu specifications](refinements/menus/README.md) | Application and context-menu behavior                          |
| [Toolbar specifications](refinements/toolbars/README.md)               | Main, list, reader, attachment and compose toolbars            |
| [Backend parity](parity/backend.md)                                    | Current IMAP, sync and account behavior against parity targets |
| [UI parity](parity/ui.md)                                              | Current mail interface against parity targets                  |

> [!NOTE]
>
> Trojitá is the primary parity reference and Kontact the secondary one. Rows
> marked ![Planned][planned] are targets, not current behavior; the
> [interface design](philosophy/designs.md#current-interface) guide and the
> [project README](../README.md#implemented-today) describe what works today.

## Screenshots

[`screenshots/`](screenshots/) holds layout screenshots captured by
`test_LiveImapPanels` against a synthetic mailbox in the headless dev container.
See [live mail, screenshots and a VNC
view](../CONTRIBUTING.md#live-mail-screenshots-and-a-vnc-view) to regenerate
them.

| Screenshot                                                                  | Shows                                                                         |
| --------------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| [Compact layout](screenshots/mail-shell-compact.png)                        | List stacked above the reader, Attachments panel below the reader             |
| [Wide layout with side panels](screenshots/mail-shell-wide-side-panels.png) | Message Details, Contacts and Related Messages in the right column            |
| [Panels around the reader](screenshots/mail-shell-panels-around-reader.png) | Details on the left, contacts on the right, attachments and related in footer |

## Historical archive

[Archived Trojitá documents](old/README.md) preserve earlier specifications,
protocol notes, research and user guidance. Treat them as historical context;
the current source, build configuration and the documents above define present
behavior.

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
[recommended]: https://img.shields.io/badge/Recommended-1b7c83
