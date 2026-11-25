# Reading, composition, and attachment refinement

**[Refinements](README.md)** ·
**[M13 in roadmap](../ROADMAP.md#m13-refinements)** ·
**[Previous: Mail organization and triage](03-mail-organization-and-triage.md)**
·
**[Next: Search, sorting and automation](05-search-sorting-and-automation.md)**

Refines message rendering, conversation navigation, compose and reply flows,
drafts, attachment handling and editor usability.

**Roadmap subsection:**
[M13.05: Reading, composition, and attachments](../ROADMAP.md#m13-refinements) ·
**Issue:** [#24](https://github.com/mikepadiernos/plasma-impresario/issues/24)

> [!NOTE]
>
> The Related Messages and Message Details panels have open limitations; see
> [Caveats: related messages and message details panels](../CAVEATS.md#related-messages-and-message-details-panels)
> ([#264](https://github.com/mikepadiernos/plasma-impresario/issues/264)).

## Focus

- Refine message rendering, conversation navigation, compose and reply flows,
  drafts, attachment handling, and editor usability
- Preserve draft content and attachments across interruptions and clearly report
  send failures
- Improve conversation navigation, message metadata, quoted text readability,
  and keyboard access to reading actions
- Provide clear attachment loading, size, download, open, and error states;
  request confirmation for risky actions
- Validate reply, reply-all, and forward behavior, including recipient
  visibility and attachment handling

## Acceptance criteria

- Read and compose workflows remain usable through interruptions and retries.
- Draft data and attachments survive disruption without being silently lost.
- Message rendering remains readable and keyboard-accessible.
- Attachment actions and error states are understandable and safe.
