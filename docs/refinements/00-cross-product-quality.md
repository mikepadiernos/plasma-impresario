# Cross-product quality refinement

**[Documentation](../README.md)** · **[Roadmap: M14](../ROADMAP.md#m14-refinements)** ·
**[Refinements](README.md)** ·
**[Next: Shell and interaction](01-shell-and-interaction.md)**

Sets the product-level quality bar and acceptance criteria that apply across all
mail workflows and companion services. Board statuses are as of 2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started

| Roadmap subsection                                             | Issue                                                               | Board               |
| -------------------------------------------------------------- | ------------------------------------------------------------------- | ------------------- |
| [M14.01: Cross-product quality](../ROADMAP.md#m14-refinements) | [#60](https://github.com/mikepadiernos/plasma-impresario/issues/60) | ![Backlog][backlog] |

## Current state

No M14.01 work has started. Today the QtTest suites run offscreen through
`ctest` with `QT_QPA_PLATFORM=offscreen`, the opt-in live IMAP tests run
against a disposable Dovecot (`tools/dev-container/live-imap-accounts.sh`), and
`tests/ubsan.supp` holds UndefinedBehaviorSanitizer suppressions for selected
tests. Nothing has been checked in a real Plasma session; that is
[M15](../ROADMAP.md#m15-real-plasma-session-verification)
([#319](https://github.com/mikepadiernos/plasma-impresario/issues/319)).

## Focus

Every feature issue below is in the board's Backlog. A dash marks a refinement
recorded only in this plan.

| Refinement                                                                                                                                              | Issue                                                                 |
| ------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Review end-to-end mail and available productivity workflows and prioritize remaining friction                                                           | [#16](https://github.com/mikepadiernos/plasma-impresario/issues/16)   |
| Refine cross-feature usability, accessibility, keyboard navigation, and visual consistency                                                              | [#17](https://github.com/mikepadiernos/plasma-impresario/issues/17)   |
| Tune performance, reliability, and resource usage across realistic workloads                                                                            | [#18](https://github.com/mikepadiernos/plasma-impresario/issues/18)   |
| Validate provider, desktop, Nextcloud, and any available Plasma Opus integration behavior together                                                      | [#19](https://github.com/mikepadiernos/plasma-impresario/issues/19)   |
| Resolve refinement findings and document remaining limitations and acceptance results                                                                   | [#20](https://github.com/mikepadiernos/plasma-impresario/issues/20)   |
| Compare supported workflows with the inspiration principles and prioritize gaps without assuming feature parity                                         | [#142](https://github.com/mikepadiernos/plasma-impresario/issues/142) |
| Record reproducible acceptance results and regression coverage for each refinement area                                                                 | [#143](https://github.com/mikepadiernos/plasma-impresario/issues/143) |
| Inventory implemented workflows and map their entry points to automated or reproducible manual checks                                                   | [#144](https://github.com/mikepadiernos/plasma-impresario/issues/144) |
| Benchmark cold and warm startup, first interactive window, and settings readiness; record the environment and compare improvements against the baseline | [#145](https://github.com/mikepadiernos/plasma-impresario/issues/145) |
| Exercise successful operations, cancellation, partial failure, recovery, and persistence with isolated test data and synthetic credentials              | [#146](https://github.com/mikepadiernos/plasma-impresario/issues/146) |
| Review accessibility, translations, display scaling, optional dependencies, and representative desktop configurations; record environments not tested   | [#147](https://github.com/mikepadiernos/plasma-impresario/issues/147) |
| Run suitable static analysis and dependency checks and document security findings and residual risks without claiming testing proves security           | [#148](https://github.com/mikepadiernos/plasma-impresario/issues/148) |
| Use C++ Core Guidelines and project-specific KDE/Qt conventions for findings                                                                            | [#148](https://github.com/mikepadiernos/plasma-impresario/issues/148) |
| Evaluate open static analyzers and PVS-Studio only where licensing and host support allow                                                               | [#148](https://github.com/mikepadiernos/plasma-impresario/issues/148) |
| Establish reproducible formatting, warning, test coverage, and dynamic-analysis commands                                                                | —                                                                     |
| Use AddressSanitizer and UndefinedBehaviorSanitizer for supported desktop configurations                                                                | —                                                                     |
| Use ThreadSanitizer for targeted concurrency investigations where practical                                                                             | —                                                                     |

## Acceptance criteria

- The product feels coherent across the entire desktop workflow, not just in
  isolated features.
- Cross-feature friction is reduced and comparable across primary mail tasks.
- Quality is measured through realistic workloads, not just compile and smoke
  test success.
- Remaining limitations are documented with clear acceptance and validation
  notes.
- The app remains honest about what has and has not been verified across
  environments.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
