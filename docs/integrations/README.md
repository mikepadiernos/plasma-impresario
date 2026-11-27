# Integrations

**[Documentation](../README.md)** · **[Roadmap](../ROADMAP.md)** ·
**[Caveats](../CAVEATS.md)** · **[Scope](../philosophy/scope.md)** ·
**[Mail providers](providers.md)**

Integration guides describe the intended behavior, suggested approaches,
recommendations and verification scenarios for each integration. They keep
planned work separate from the current mail baseline, which is recorded in the
[UI](../parity/ui.md) and [backend](../parity/backend.md) parity guides.
Statuses are as of 2026-11-27.

| Guide                          | Purpose                                                                                                    | Current state                                                                                       | Roadmap                                                                                                                                                                                                                                       |
| ------------------------------ | ---------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [KDE Plasma](kde-plasma.md)    | Desktop services, KDE conventions and system behavior                                                      | ![Needs verification][needs-verification] Partly in place; nothing checked in a real Plasma session | M05.06 in [M05](../ROADMAP.md#m05-search-and-indexing); M06.01–M06.03 in [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)                                                                                                    |
| [Plasma Opus](plasma-opus.md)  | Cross-app, workspace, theme, keyboard and productivity workflows                                           | ![Planned][planned] Proposed; no integration contract exists                                        | [M05](../ROADMAP.md#m05-search-and-indexing), M06.03, [M08](../ROADMAP.md#m08-settings-and-workspace-configuration), [M09](../ROADMAP.md#m09-themes-and-branding-foundation), [M12](../ROADMAP.md#m12-nextcloud-and-digital-asset-management) |
| [Microsoft](microsoft.md)      | Outlook.com, Microsoft 365 and Exchange accounts: mail, calendar, contacts, tasks and files                | ![Needs verification][needs-verification] OAuth for IMAP and SMTP only; the rest is planned         | [M13](../ROADMAP.md#m13-microsoft-accounts-and-exchange)                                                                                                                                                                                      |
| [Mail providers](providers.md) | Setup, sign-in, server features and limits for Gmail, Outlook.com, Microsoft 365 and generic IMAP and SMTP | ![Needs verification][needs-verification] Documented; not tried with real provider accounts         | M06.05 in [M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)                                                                                                                                                                   |

Each guide starts with a table that separates what the client does today from
planned work.

> [!IMPORTANT]
>
> An integration is complete only when authentication, error recovery, privacy
> behavior and offline or unavailable-service handling are all defined.

[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
