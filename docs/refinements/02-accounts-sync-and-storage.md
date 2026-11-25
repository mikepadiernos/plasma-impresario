# Accounts, sync, and storage refinement

**[Refinements](README.md)** ·
**[M13 in roadmap](../ROADMAP.md#m13-refinements)** ·
**[Previous: Shell and interaction](01-shell-and-interaction.md)** ·
**[Next: Mail organization and triage](03-mail-organization-and-triage.md)**

Refines account setup, authentication, provider compatibility, folder sync,
offline recovery and storage feedback.

**Roadmap subsection:**
[M13.03: Accounts, sync, and storage](../ROADMAP.md#m13-refinements) ·
**Issue:** [#22](https://github.com/mikepadiernos/plasma-impresario/issues/22)

> [!NOTE]
>
> Multiple accounts with a toolbar selector exist on `feature/m02-shell-next`.
> Their open limitations are tracked in
> [Caveats: multiple accounts](../CAVEATS.md#multiple-accounts)
> ([#262](https://github.com/mikepadiernos/plasma-impresario/issues/262)).

## Focus

- Refine account setup, authentication, provider compatibility, folder sync,
  offline recovery, and storage feedback
- Clarify account setup errors, authentication renewal, certificate failures,
  and provider-specific limitations
- Show per-account sync progress, last successful sync, offline state, and
  actionable retry options
- Preserve local changes during interruptions and make conflicts or unsent
  changes visible before recovery
- Explain local cache usage and allow users to manage storage without deleting
  server messages unintentionally

## Acceptance criteria

- Account errors are understandable and recoverable.
- Sync state remains visible and actionable during reconnect, retry, and offline
  transitions.
- Local changes survive interruptions without silent loss or unsupported
  rollback.
- Storage and cache behavior remain explainable to users managing large
  accounts.
