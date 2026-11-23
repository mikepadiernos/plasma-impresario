# Accounts, sync, and storage refinement

This section covers refinement of account setup, authentication, provider compatibility, folder sync, offline recovery, and storage feedback.

## Focus

- Refine account setup, authentication, provider compatibility, folder sync, offline recovery, and storage feedback
- Clarify account setup errors, authentication renewal, certificate failures, and provider-specific limitations
- Show per-account sync progress, last successful sync, offline state, and actionable retry options
- Preserve local changes during interruptions and make conflicts or unsent changes visible before recovery
- Explain local cache usage and allow users to manage storage without deleting server messages unintentionally

## Acceptance criteria

- Account errors are understandable and recoverable.
- Sync state remains visible and actionable during reconnect, retry, and offline transitions.
- Local changes survive interruptions without silent loss or unsupported rollback.
- Storage and cache behavior remain explainable to users managing large accounts.
