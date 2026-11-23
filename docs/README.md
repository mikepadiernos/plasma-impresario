# Plasma Impresario documentation

**[Application](../README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Caveats](CAVEATS.md)** · **[Contributing](../CONTRIBUTING.md)**

Find the right guide for project direction, implementation planning and
development status.

## Contents

- [Start here](#start-here)
- [Plan and track](#plan-and-track)
- [Product and integration guides](#product-and-integration-guides)
- [Historical archive](#historical-archive)

## Start here

| Guide | What it covers |
| --- | --- |
| [Project README](../README.md) | Installation, feature direction and desktop integration |
| [Compatibility baseline](compatibility.md) | M01 target environment, dependencies and verification limits |
| [Scope and objective](philosophy/scope.md) | Target workflows and product boundaries |
| [Design direction](philosophy/designs.md) | Shell structure, reading flow and interface principles |
| [Inspirations](philosophy/inspirations.md) | Trojitá, Aqua Mail, Vivaldi Mail, The Bat!, Kontact and Plasma influences |
| [Project instructions](../CLAUDE.md) | Development conventions and project workflow |

## Plan and track

| Document | What it tracks |
| --- | --- |
| [Roadmap](ROADMAP.md) | Milestones, architecture and delivery order |
| [Pending tasks](TODO.md) | Blockers, handoffs and resumable work |
| [Reported bugs](BUGS.md) | User-visible issues and resolution status |
| [Caveats and security](CAVEATS.md) | Current limitations, security posture and recommendations |
| [Edge cases](EDGE.md) | Boundary scenarios that need validation |

> [!IMPORTANT] The roadmap defines intended scope. Pending tasks are handoffs,
> caveats describe limitations, edge cases are test candidates, and bugs are
> confirmed regressions—not implementation claims.

## Product and integration guides

| Guide | What it covers |
| --- | --- |
| [Refinement plans](refinements/README.md) | Release-quality review areas and acceptance criteria |
| [Menu specifications](refinements/menus/README.md) | Application and context-menu behavior |
| [Toolbar specifications](refinements/toolbars/README.md) | Main, list, reader and compose toolbars |
| [KDE Plasma integration](integrations/kde-plasma.md) | Desktop services and KDE conventions |
| [Plasma Opus integration](integrations/plasma-opus.md) | Cross-app and workspace-aware workflows |
| [Backend parity](parity/backend.md) | Current backend baseline and reference patterns |
| [UI parity](parity/ui.md) | Current layout and target interface patterns |

## Historical archive

[Archived Trojitá documents](old/README.md) preserve earlier product
specifications, protocol notes, research and user guidance. Treat those
documents as historical context; current source, build configuration,
[backend and UI parity guides](#product-and-integration-guides) and
[caveats](CAVEATS.md) define present behavior.
