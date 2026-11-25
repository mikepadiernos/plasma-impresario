# Security and privacy refinement

**[Refinements](README.md)** ·
**[M14 in roadmap](../ROADMAP.md#m14-refinements)** ·
**[Previous: Search, sorting and automation](05-search-sorting-and-automation.md)**
· **[Next: Settings and notifications](07-settings-and-notifications.md)** ·
**[Security caveats](../CAVEATS.md#security-and-data-protection)**

Refines credential handling, remote-content controls, attachment safety, ClamAV
and spam feedback, and privacy settings.

**Roadmap subsection:**
[M14.07: Security and privacy](../ROADMAP.md#m14-refinements) · **Issue:**
[#26](https://github.com/mikepadiernos/plasma-impresario/issues/26)

## Focus

- Refine credential handling, remote-content controls, attachment safety, ClamAV
  and spam feedback, and privacy settings
- Use secure credential storage and explain authentication, certificate, and
  account-access errors
- Keep remote content blocked or user-controlled and make external-link
  destinations visible before opening
- Distinguish clean, suspicious, unscanned, and failed scan states without
  treating unavailable scanning as success
- Provide understandable spam controls and recovery from misclassification
  without exposing mail content unnecessarily

## Acceptance criteria

- User security and privacy choices are visible, explicit, and recoverable.
- Remote content and external links remain controlled by the user.
- Security state and scan outcomes are understandable without claiming
  successful protection that was not performed.
- Sensitive data remains protected in settings, logs, and temporary files.
