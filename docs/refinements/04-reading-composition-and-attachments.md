# Reading, composition, and attachment refinement

**[Documentation](../README.md)** · **[Roadmap: M14](../ROADMAP.md#m14-refinements)** ·
**[Refinements](README.md)** ·
**[Previous: Mail organization and triage](03-mail-organization-and-triage.md)** ·
**[Next: Search, sorting and automation](05-search-sorting-and-automation.md)**

Refines message rendering, conversation navigation, compose and reply flows,
drafts, attachment handling and editor usability. Board statuses are as of
2026-11-25.

Legend: ![Backlog][backlog] on the project board, not started ·
![Review][review] implemented, awaiting review

| Roadmap subsection                                                             | Issue                                                               | Board               |
| ------------------------------------------------------------------------------ | ------------------------------------------------------------------- | ------------------- |
| [M14.05: Reading, composition, and attachments](../ROADMAP.md#m14-refinements) | [#24](https://github.com/mikepadiernos/plasma-impresario/issues/24) | ![Backlog][backlog] |

## Current state

Reading and conversation flow were built in M04, which is merged into `main`.
The Related Messages and Message Details panels still have open limitations;
see
[Caveats: related messages and message details panels](../CAVEATS.md#related-messages-and-message-details-panels)
([#264](https://github.com/mikepadiernos/plasma-impresario/issues/264),
![Review][review]). A real Plasma session has not been verified.

## Focus

Every feature issue below is in the board's Backlog.

| Refinement                                                                                                                    | Issue                                                                 |
| ----------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| Refine message rendering, conversation navigation, compose and reply flows, drafts, attachment handling, and editor usability | [#170](https://github.com/mikepadiernos/plasma-impresario/issues/170) |
| Preserve draft content and attachments across interruptions and clearly report send failures                                  | [#171](https://github.com/mikepadiernos/plasma-impresario/issues/171) |
| Improve conversation navigation, message metadata, quoted text readability, and keyboard access to reading actions            | [#172](https://github.com/mikepadiernos/plasma-impresario/issues/172) |
| Provide clear attachment loading, size, download, open, and error states; request confirmation for risky actions              | [#173](https://github.com/mikepadiernos/plasma-impresario/issues/173) |
| Validate reply, reply-all, and forward behavior, including recipient visibility and attachment handling                       | [#174](https://github.com/mikepadiernos/plasma-impresario/issues/174) |
| Prevent delayed message or attachment results from replacing the current view after selection changes or cancellation         | [#175](https://github.com/mikepadiernos/plasma-impresario/issues/175) |

## Acceptance criteria

- Read and compose workflows remain usable through interruptions and retries.
- Draft data and attachments survive disruption without being silently lost.
- Message rendering remains readable and keyboard-accessible.
- Attachment actions and error states are understandable and safe.

[backlog]: https://img.shields.io/badge/Backlog-6e7781
[review]: https://img.shields.io/badge/Review-2f81f7
