# Microsoft accounts and Exchange

**[Integrations](README.md)** · **[KDE Plasma](kde-plasma.md)** · **[Plasma
Opus](plasma-opus.md)** · **[Roadmap:
M13](../ROADMAP.md#m13-microsoft-accounts-and-exchange)** · **[OAuth
caveats](../CAVEATS.md#oauth-sign-in)**

This guide plans how Plasma Impresario supports Outlook.com, Microsoft 365 and
Exchange Server accounts: sign-in, mail, calendar, contacts, tasks and files.
Apart from OAuth sign-in for IMAP and SMTP, everything here is planned. The
work is tracked in
[M13: Microsoft accounts and Exchange](../ROADMAP.md#m13-microsoft-accounts-and-exchange).

Status: ✅ done · 🧪 implemented, needs verification · 📋 planned

## Contents

- [Status](#status)
- [Account types](#account-types)
- [Interfaces](#interfaces)
- [Sign-in and permissions](#sign-in-and-permissions)
- [Calendar, contacts and tasks](#calendar-contacts-and-tasks)
- [KDE and Plasma Opus](#kde-and-plasma-opus)
- [Verification](#verification)
- [Decisions to validate](#decisions-to-validate)

## Status

| Area                                  | Status | Today                                                                                                                                              | Roadmap |
| ------------------------------------- | ------ | -------------------------------------------------------------------------------------------------------------------------------------------------- | ------- |
| 🔐 Sign-in and consent                | 🧪     | OAuth for IMAP and SMTP through the `common` tenant ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)); no built-in client ID | M13.01  |
| 📬 Outlook.com and Microsoft 365 mail | 🧪     | IMAP and SMTP with XOAUTH2; not yet verified with a real account ([#317](https://github.com/mikepadiernos/plasma-impresario/issues/317))           | M13.02  |
| 🏢 Exchange Server                    | 📋     | Works only where the server offers IMAP and SMTP with a supported sign-in                                                                          | M13.03  |
| 📅 Calendar and scheduling            | 📋     | Not available                                                                                                                                      | M13.04  |
| 👥 Contacts and address book          | 📋     | Not available                                                                                                                                      | M13.05  |
| ✅ Tasks and files                    | 📋     | Not available                                                                                                                                      | M13.06  |
| 🧩 KDE, privacy and tests             | 📋     | Limitations are recorded in [OAuth caveats](../CAVEATS.md#oauth-sign-in)                                                                           | M13.07  |

## Account types

| Account                            | Sign-in                                                                   | Mail                                                                 | Calendar, contacts and tasks |
| ---------------------------------- | ------------------------------------------------------------------------- | -------------------------------------------------------------------- | ---------------------------- |
| 👤 Outlook.com, Hotmail and Live   | Microsoft personal account, OAuth only                                    | IMAP and SMTP with OAuth; Graph where IMAP falls short               | Microsoft Graph              |
| 💼 Microsoft 365 (Exchange Online) | Work or school account; tenant policies apply                             | IMAP and SMTP with OAuth if the tenant enables them; otherwise Graph | Microsoft Graph              |
| 🏢 Exchange Server (on-premises)   | OAuth with hybrid modern authentication, Kerberos, NTLM or Basic over TLS | IMAP and SMTP if enabled; otherwise EWS                              | EWS                          |
| 🔀 Hybrid deployments              | Depends on where the mailbox is                                           | As Microsoft 365 or Exchange Server, found by Autodiscover           | As for the mailbox location  |

Microsoft has turned off Basic authentication for IMAP, POP, EWS, Exchange
ActiveSync and Autodiscover in Exchange Online, and is retiring it for SMTP AUTH
(see [Microsoft's
announcement](https://learn.microsoft.com/en-us/exchange/clients-and-mobile-in-exchange-online/deprecation-of-basic-authentication-exchange-online)).
Plasma Impresario must not fall back to a password for these accounts.

## Interfaces

| Interface                       | Use                                                                                                                                                        | Notes                                                                                                                                                                                                                                                     |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ✉️ IMAP and SMTP with OAuth     | Default for mail wherever the mailbox allows it                                                                                                            | Keeps the existing IMAP model; administrators can turn IMAP or SMTP AUTH off per mailbox or tenant                                                                                                                                                        |
| 🌐 Microsoft Graph              | Mail when IMAP or SMTP AUTH is off, plus categories, Focused Inbox, mailbox settings, calendar, contacts, To Do and files in Microsoft 365 and Outlook.com | Delta queries for incremental sync; honor `Retry-After` when throttled                                                                                                                                                                                    |
| 🏢 Exchange Web Services (EWS)  | On-premises Exchange Server only                                                                                                                           | Microsoft [announced](https://techcommunity.microsoft.com/blog/exchange/retirement-of-exchange-web-services-in-exchange-online/3924440) that it would start blocking EWS in Exchange Online on October 1, 2026; on-premises Exchange Server is unaffected |
| 🧭 Autodiscover                 | Finding Exchange servers and hybrid mailbox locations                                                                                                      | Autodiscover v2 (JSON), POX and the `_autodiscover._tcp` SRV record, with the confirmation rules of M06.05 discovery                                                                                                                                      |
| 🚫 Exchange ActiveSync and MAPI | Not planned                                                                                                                                                | Servers that offer only these are reported during setup                                                                                                                                                                                                   |

Where Graph and IMAP disagree, IMAP stays the source of truth for the folders
and flags it can represent, and Graph adds what IMAP does not expose, such as
Outlook categories with their colors. Archive mailboxes and public folders are
reported as unavailable when neither interface supports them.

## Sign-in and permissions

The current sign-in from #218 uses the `common` tenant, the
`IMAP.AccessAsUser.All` and `SMTP.Send` scopes and a `localhost` redirect. A
client ID comes from the account settings or the
`IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID` build option. M13.01 replaces this with:

- 🪪 a publisher-verified, multi-tenant Microsoft Entra app registration with
  a built-in client ID, which administrators can replace with their own;
- 🏷️ a choice of personal accounts, any work or school account, or a specific
  tenant;
- 🔁 one token session per account shared by IMAP, SMTP and Graph, with
  incremental consent when a feature is turned on.

Each feature requests only its own delegated permissions:

| Feature                    | Permissions                                                           |
| -------------------------- | --------------------------------------------------------------------- |
| 🔑 Staying signed in       | `offline_access`                                                      |
| ✉️ IMAP and SMTP           | `IMAP.AccessAsUser.All`, `SMTP.Send`                                  |
| 🌐 Mail through Graph      | `Mail.ReadWrite`, `Mail.Send`                                         |
| 👥 Shared mailboxes        | `Mail.ReadWrite.Shared`, `Mail.Send.Shared`                           |
| 🗓️ Automatic replies       | `MailboxSettings.ReadWrite`                                           |
| 📅 Calendar                | `Calendars.ReadWrite`                                                 |
| 📇 Contacts and directory  | `Contacts.ReadWrite`, `People.Read`, `User.ReadBasic.All`             |
| ✅ Microsoft To Do         | `Tasks.ReadWrite`                                                     |
| ☁️ OneDrive and SharePoint | `Files.ReadWrite` (or `Files.ReadWrite.All` for SharePoint libraries) |

Tenants that restrict user consent need an administrator to approve the app,
and Conditional Access or multi-factor authentication can block sign-in. The
app explains these errors (AADSTS codes) with the next step. Where Microsoft
offers no revocation endpoint for a single app, removing an account deletes the
local grant and links to the Microsoft app-permissions page.

## Calendar, contacts and tasks

Calendar, contacts and tasks feed the M06.04 agenda and contacts panels. KDE
PIM and Akonadi come first when a resource provides the account; Graph or EWS
is the fallback. The Akonadi EWS resource is affected by the EWS retirement in
Exchange Online, so Graph is needed for those accounts until a KDE resource
supports it.

- 📅 Calendars start read-only and cached for offline use; Windows time zone
  names are converted to IANA time zones, and an event found through more than
  one source is shown once.
- ✉️ Meeting invitations are answered from the reader, with iTIP and iMIP (RFC
  5546 and 6047) as the fallback.
- 👥 Recipient completion uses Outlook contacts and, for work or school
  accounts, the organization directory, including distribution lists and
  Microsoft 365 groups.
- ✅ Messages can become Microsoft To Do tasks that link back through `mid:`
  URLs (M05), and OneDrive or SharePoint files can be shared as links following
  the M12 Nextcloud flow.

## KDE and Plasma Opus

- 🔐 Reuse Microsoft accounts from KDE Online Accounts or KDE PIM where they
  provide one, and keep working without them.
- 📅 Offer to add Outlook calendars and contacts to KDE PIM so that Merkuro and
  [Plasma Opus](plasma-opus.md) see the same data.
- 🎨 Use the same Plasma notifications, wallet and theme as other accounts.

## Verification

- 🧪 Offline tests run against local fake Graph, EWS, Autodiscover and OAuth
  servers with anonymized recordings, covering throttling, consent errors and
  delta resets.
- 🌐 Opt-in live tests use a disposable Outlook.com account, a Microsoft 365
  test tenant and an Exchange Server Subscription Edition server. They run only
  when explicitly configured.
- 🖥️ Checks that need a real Plasma session belong to
  [M15](../ROADMAP.md#m15-real-plasma-session-verification).

## Decisions to validate

- 🪪 **App registration:** who owns the Microsoft Entra app, completes
  publisher verification and maintains its redirect URIs.
- 🌐 **Graph or IMAP only:** whether Graph mail ships in the first release or
  only after IMAP and SMTP with OAuth are verified.
- 🏢 **EWS code:** whether to reuse EWS code from KDE's `kdepim-runtime`, subject
  to a license check, or to write a minimal client.
