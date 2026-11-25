# Microsoft accounts and Exchange

**[Integrations](README.md)** · **[KDE Plasma](kde-plasma.md)** · **[Plasma
Opus](plasma-opus.md)** · **[Roadmap:
M13](../ROADMAP.md#m13-microsoft-accounts-and-exchange)** · **[OAuth
caveats](../CAVEATS.md#oauth-sign-in)**

This guide plans how Plasma Impresario supports Outlook.com, Microsoft 365 and
Exchange Server accounts: sign-in, mail, calendar, contacts, tasks and files.
Apart from OAuth sign-in for IMAP and SMTP, everything here is planned. The work
is tracked in [M13: Microsoft accounts and
Exchange](../ROADMAP.md#m13-microsoft-accounts-and-exchange).

Status: ![Done][done] · ![Decided][decided] · ![Recommended][recommended] ·
![Needs verification][needs-verification] · ![Planned][planned]

## Contents

- [Status](#status)
- [Account types](#account-types)
- [Interfaces](#interfaces)
- [Sign-in and permissions](#sign-in-and-permissions)
  - [App registration](#app-registration)
- [Calendar, contacts and tasks](#calendar-contacts-and-tasks)
- [KDE and Plasma Opus](#kde-and-plasma-opus)
- [Reusable code](#reusable-code)
- [Verification](#verification)
- [Decisions](#decisions)

## Status

| Area                               | Status                                    | Today                                                                                                                                              | Roadmap |
| ---------------------------------- | ----------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- | ------- |
| Sign-in and consent                | ![Needs verification][needs-verification] | OAuth for IMAP and SMTP through the `common` tenant ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)); no built-in client ID | M13.01  |
| Outlook.com and Microsoft 365 mail | ![Needs verification][needs-verification] | IMAP and SMTP with XOAUTH2; not yet verified with a real account ([#317](https://github.com/mikepadiernos/plasma-impresario/issues/317))           | M13.02  |
| Exchange Server                    | ![Planned][planned]                       | Works only where the server offers IMAP and SMTP with a supported sign-in                                                                          | M13.03  |
| Calendar and scheduling            | ![Planned][planned]                       | Not in Plasma Impresario; KDE PIM's Microsoft 365 (Graph) resource syncs it for KDE apps from KDE Gear 26.12                                       | M13.04  |
| Contacts and address book          | ![Planned][planned]                       | Not in Plasma Impresario; personal contacts sync through the same KDE PIM resource                                                                 | M13.05  |
| Tasks and files                    | ![Planned][planned]                       | Not in Plasma Impresario; KDE PIM's Graph resource syncs Microsoft To Do, and no KDE component covers OneDrive                                     | M13.06  |
| KDE, privacy and tests             | ![Planned][planned]                       | Limitations are recorded in [OAuth caveats](../CAVEATS.md#oauth-sign-in)                                                                           | M13.07  |

## Account types

| Account                         | Sign-in                                                                   | Mail                                                                 | Calendar, contacts and tasks |
| ------------------------------- | ------------------------------------------------------------------------- | -------------------------------------------------------------------- | ---------------------------- |
| Outlook.com, Hotmail and Live   | Microsoft personal account, OAuth only                                    | IMAP and SMTP with OAuth; Graph where IMAP falls short               | Microsoft Graph              |
| Microsoft 365 (Exchange Online) | Work or school account; tenant policies apply                             | IMAP and SMTP with OAuth if the tenant enables them; otherwise Graph | Microsoft Graph              |
| Exchange Server (on-premises)   | OAuth with hybrid modern authentication, Kerberos, NTLM or Basic over TLS | IMAP and SMTP if enabled; otherwise EWS                              | EWS                          |
| Hybrid deployments              | Depends on where the mailbox is                                           | As Microsoft 365 or Exchange Server, found by Autodiscover           | As for the mailbox location  |

Microsoft has turned off Basic authentication for IMAP, POP, EWS, Exchange
ActiveSync and Autodiscover in Exchange Online, and is retiring it for SMTP AUTH
(see [Microsoft's
announcement](https://learn.microsoft.com/en-us/exchange/clients-and-mobile-in-exchange-online/deprecation-of-basic-authentication-exchange-online)).
Plasma Impresario must not fall back to a password for these accounts.

## Interfaces

| Interface                    | Use                                                                                                                                                        | Notes                                                                                                                                                                                                                                                     |
| ---------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| IMAP and SMTP with OAuth     | Default for mail wherever the mailbox allows it                                                                                                            | Keeps the existing IMAP model; administrators can turn IMAP or SMTP AUTH off per mailbox or tenant                                                                                                                                                        |
| Microsoft Graph              | Mail when IMAP or SMTP AUTH is off, plus categories, Focused Inbox, mailbox settings, calendar, contacts, To Do and files in Microsoft 365 and Outlook.com | Delta queries for incremental sync; honor `Retry-After` when throttled                                                                                                                                                                                    |
| Exchange Web Services (EWS)  | On-premises Exchange Server only                                                                                                                           | Microsoft [announced](https://techcommunity.microsoft.com/blog/exchange/retirement-of-exchange-web-services-in-exchange-online/3924440) that it would start blocking EWS in Exchange Online on October 1, 2026; on-premises Exchange Server is unaffected |
| Autodiscover                 | Finding Exchange servers and hybrid mailbox locations                                                                                                      | Autodiscover v2 (JSON), POX and the `_autodiscover._tcp` SRV record, with the confirmation rules of M06.05 discovery                                                                                                                                      |
| Exchange ActiveSync and MAPI | Not planned                                                                                                                                                | Servers that offer only these are reported during setup                                                                                                                                                                                                   |

Where Graph and IMAP disagree, IMAP stays the source of truth for the folders
and flags it can represent, and Graph adds what IMAP does not expose, such as
Outlook categories with their colors. Archive mailboxes and public folders are
reported as unavailable when neither interface supports them.

> [!IMPORTANT]
>
> Many Microsoft 365 accounts can read mail over IMAP but cannot send over SMTP.
> Microsoft recommends turning SMTP AUTH off for the whole organization, and it
> is already off in every tenant with security defaults enabled (see [SMTP AUTH
> in Exchange
> Online](https://learn.microsoft.com/en-us/exchange/clients-and-mobile-in-exchange-online/authenticated-client-smtp-submission)).
> Sending through Graph is therefore the first Graph feature to ship.

## Sign-in and permissions

The current sign-in from #218 uses the `common` tenant, the
`IMAP.AccessAsUser.All` and `SMTP.Send` scopes and a `localhost` redirect. A
client ID comes from the account settings or the
`IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID` build option; none is built in yet.
M13.01 adds:

- a Plasma Impresario app registration with its client ID built into
  release packages, as described under [App registration](#app-registration);
- a choice of personal accounts, any work or school account, or a specific
  tenant;
- one token session per account shared by IMAP, SMTP and Graph, with
  incremental consent when a feature is turned on.

Each feature requests only its own delegated permissions:

| Feature                 | Permissions                                                           |
| ----------------------- | --------------------------------------------------------------------- |
| Staying signed in       | `offline_access`                                                      |
| IMAP and SMTP           | `IMAP.AccessAsUser.All`, `SMTP.Send`                                  |
| Mail through Graph      | `Mail.ReadWrite`, `Mail.Send`                                         |
| Shared mailboxes        | `Mail.ReadWrite.Shared`, `Mail.Send.Shared`                           |
| Automatic replies       | `MailboxSettings.ReadWrite`                                           |
| Calendar                | `Calendars.ReadWrite`                                                 |
| Contacts and directory  | `Contacts.ReadWrite`, `People.Read`, `User.ReadBasic.All`             |
| Microsoft To Do         | `Tasks.ReadWrite`                                                     |
| OneDrive and SharePoint | `Files.ReadWrite` (or `Files.ReadWrite.All` for SharePoint libraries) |

Tenants that restrict user consent need an administrator to approve the app,
and Conditional Access or multi-factor authentication can block sign-in. The
app explains these errors (AADSTS codes) with the next step. Where Microsoft
offers no revocation endpoint for a single app, removing an account deletes the
local grant and links to the Microsoft app-permissions page.

### App registration

Every Microsoft sign-in needs the client ID of an app registered in Microsoft
Entra ID. A desktop mail client is a public client: its client ID is not a
secret, it has no client secret, and it redirects to `localhost` on any free
port. Microsoft's rules decide who can own the registration:

- **Publisher verification needs an organization.** The publisher needs a
  verified Microsoft AI Cloud Partner Program account, the app must be
  registered in a work or school tenant rather than with a personal Microsoft
  account, and the publisher domain must be DNS-verified and not
  `*.onmicrosoft.com`. Verification is free (see [Microsoft's
  requirements](https://learn.microsoft.com/en-us/entra/identity-platform/publisher-verification-overview#requirements)).
- **Unverified apps still work, with friction.** Personal Outlook.com
  accounts can sign in. Work or school tenants that let users consent only to
  apps from verified publishers need an administrator to approve the app first.
- **Borrowed client IDs are not an option.** Using KMail's, Thunderbird's or
  Microsoft Office's client ID would show that app's name on the consent screen,
  and its owner could restrict or revoke it at any time.

#### KDE precedent

KDE PIM has used both models:

- **EWS resource, 2018:** it first borrowed Microsoft Office's client ID, then
  switched to its own registration. Its developer accepted that some
  administrators would block an unverified app and documented a configuration
  override for them
  ([announcement](https://micreabog.wordpress.com/2018/10/30/we-have-oauth-and-were-strong/)).
- **KMail Outlook IMAP and SMTP, 2024:** ships a built-in client ID for the
  `common` tenant.
- **Microsoft 365 (Graph) resource, 2026:** ships no client ID; each user
  or organization registers their own app.

The EWS override could be set only in a configuration file or over D-Bus, and
users who borrowed other apps' client IDs hit redirect and device-policy errors
([KDE bug 442180](https://bugs.kde.org/show_bug.cgi?id=442180)). KDE does not
document whether its client IDs are publisher-verified or which tenant holds
them.

#### Recommendation

Follow KMail and the EWS resource, with their lessons applied:

- The maintainer registers one Plasma Impresario app, and release builds
  ship its client ID, so most people sign in without any setup.
- The client ID, tenant and redirect settings can be replaced in the
  account settings, not only in a configuration file, and administrators can
  set system-wide defaults.
- Registering your own app is the documented fallback for organizations
  that require it, not the default.
- Publisher verification follows when KDE e.V. or another organization can
  hold the partner account. Until then, the guide shows administrators how to
  approve the unverified app.

#### Owners

| Step        | Owner                                     | Plan                                                                                                                                                                       |
| ----------- | ----------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1️⃣ Register | Project maintainer                        | One "Plasma Impresario" registration for any work or school account and personal accounts, in a work or school tenant with a project domain, with the permissions above    |
| 2️⃣ Ship     | Release builds and distributions          | Release builds set `IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID`; distributions may build with their own registration                                                             |
| 3️⃣ Override | Administrators and users                  | A per-account client ID and tenant in the account settings, plus system-wide KConfig defaults for managed desktops, for organizations that require their own registration  |
| 4️⃣ Verify   | An organization that can hold the account | Ask KDE e.V. to host or verify the app; otherwise verify it once a legal entity for the project exists. Until then, document how administrators approve the unverified app |

Registering the app needs the maintainer's own Microsoft account; see the
[M13 handoff](../TODO.md#m13-microsoft-milestone-insertion).

## Calendar, contacts and tasks

Calendar, contacts and tasks feed the M06.04 agenda and contacts panels. KDE
PIM and Akonadi come first when a resource provides the account, and Plasma
Impresario's own Graph or EWS client is the fallback. KDE PIM now has two
resources for Microsoft accounts:

- **Microsoft 365 (Graph) resource**, new in KDE Gear 26.12: mail, calendar,
  personal contacts and Microsoft To Do for Exchange Online and Outlook.com. It
  does not yet cover the organization directory, shared mailboxes or To Do
  checklist items.
- **EWS resource**: on-premises Exchange Server. It stops working with
  Exchange Online once Microsoft blocks EWS there.

Plasma Impresario's own client therefore focuses on mail and on what these
resources lack, such as directory lookup and shared mailboxes.

- Calendars start read-only and cached for offline use; Windows time zone
  names are converted to IANA time zones, and an event found through more than
  one source is shown once.
- Meeting invitations are answered from the reader, with iTIP and iMIP (RFC
  5546 and 6047) as the fallback.
- Recipient completion uses Outlook contacts and, for work or school
  accounts, the organization directory, including distribution lists and
  Microsoft 365 groups.
- Messages can become Microsoft To Do tasks that link back through `mid:`
  URLs (M05), and OneDrive or SharePoint files can be shared as links following
  the M12 Nextcloud flow.

## KDE and Plasma Opus

- Reuse Microsoft accounts already set up in KDE PIM's Graph or EWS
  resources, and keep working without them. KDE Online Accounts has no Microsoft
  provider.
- Offer to add Outlook calendars and contacts to KDE PIM so that Merkuro and
  [Plasma Opus](plasma-opus.md) see the same data.
- Use the same Plasma notifications, wallet and theme as other accounts.

## Reusable code

Plasma Impresario is licensed under the GPL, version 2 or 3. Code under the LGPL
version 2 or later can be combined with it, and copied files keep their SPDX
headers. Before copying any file, check its own header: the sampled files below
are LGPL-2.0-or-later.

| Component                                   | Source and license                                                                               | Reuse for                                                                                                                                             | How                                                                                                              | Roadmap       |
| ------------------------------------------- | ------------------------------------------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- | ------------- |
| Graph client and handlers                   | `kdepim-runtime` `resources/graph`, KDE Gear 26.12, LGPL-2.0-or-later                            | Graph requests with `Retry-After` back-off, paging and `$batch`; delta links; mail and MIME, events and To Do as KCalendarCore, contacts as KContacts | Copy into the source tree without the Akonadi parts and track upstream changes                                   | M13.02–M13.06 |
| Microsoft 365 (Graph) Akonadi resource      | Same                                                                                             | Calendars, contacts and To Do that KDE PIM already syncs                                                                                              | Read through Akonadi at run time; Plasma Impresario already links the Akonadi contact library                    | M13.04–M13.07 |
| EWS client                                  | `kdepim-runtime` `resources/ews/ewsclient`, LGPL-2.0-or-later                                    | EWS requests, folder and item sync, streaming notifications, POX Autodiscover, recurrences and attendees                                              | Copy without its WebEngine sign-in, using the #218 OAuth instead; propose an installed shared library to KDE PIM | M13.03        |
| Fake EWS server and Graph tests             | `kdepim-runtime` `resources/ews/test` and `resources/graph/autotests`                            | Offline tests                                                                                                                                         | Copy with the clients                                                                                            | M13.07        |
| `QTimeZone::windowsIdToDefaultIanaId`       | Qt                                                                                               | Windows time zone names                                                                                                                               | Use directly                                                                                                     | M13.04        |
| Akonadi iTIP handler and KCalendarCore      | KDE PIM and KDE Frameworks, LGPL                                                                 | Meeting replies with iTIP and iMIP                                                                                                                    | Use where KDE PIM is installed                                                                                   | M13.04        |
| `MailTransport::OutlookOAuthTokenRequester` | KMailTransport, LGPL-2.0-or-later, public API                                                    | Microsoft OAuth with PKCE and a loopback redirect                                                                                                     | Not adopted: #218 already covers this with SASL                                                                  | —             |
| Thunderbird `ews-rs`                        | Rust, MPL-2.0                                                                                    | EWS request shapes and server quirks                                                                                                                  | Reference only; not linked                                                                                       | M13.03        |
| Evolution EWS and its Microsoft 365 module  | C and GLib, LGPL-2.1                                                                             | Graph and EWS behavior in another Linux client                                                                                                        | Reference only; not linked                                                                                       | M13.02–M13.03 |
| `akonadi-microsoft365`                      | [Standalone repository](https://github.com/mzilinski/akonadi-microsoft365) of the Graph resource | App registration guide and live test tools                                                                                                            | Reference for the M13.07 setup guide and live tests                                                              | M13.07        |

## Verification

- Offline tests run against local fake Graph, EWS, Autodiscover and OAuth
  servers with anonymized recordings, covering throttling, consent errors and
  delta resets. The EWS tests reuse KDE PIM's fake EWS server.
- Opt-in live tests use a disposable Outlook.com account, a Microsoft 365
  test tenant and an Exchange Server Subscription Edition server. They run only
  when explicitly configured.
- Checks that need a real Plasma session belong to
  [M15](../ROADMAP.md#m15-real-plasma-session-verification).

## Decisions

| Decision         | Status                                                  | Outcome                                                                                                                                                                                                                                                                                                                              |
| ---------------- | ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| App registration | ![Decided][decided] 2026-11-25; registration is pending | The maintainer registers and owns the app, release builds ship its client ID, and users and administrators can replace it in the account settings, as KMail and KDE PIM's EWS resource do. Publisher verification follows when KDE e.V. or another organization can hold the partner account. See [Recommendation](#recommendation). |
| Graph mail first | ![Decided][decided] 2026-11-25                          | Graph mail ships in the first M13 release, right after sign-in, on the account-protocol layer from M06.05. If that layer is not ready, sending through Graph ships first for IMAP accounts whose tenant turns SMTP AUTH off, and receiving follows.                                                                                  |
| KDE code reuse   | ![Decided][decided] 2026-11-25                          | Reuse KDE PIM's EWS and Graph clients, its fake EWS server and the other components under [Reusable code](#reusable-code) rather than writing new clients, and read calendars, contacts and tasks through KDE PIM's Akonadi resources when they are installed.                                                                       |

[decided]: https://img.shields.io/badge/Decided-1a7f37
[done]: https://img.shields.io/badge/Done-1a7f37
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
[recommended]: https://img.shields.io/badge/Recommended-1b7c83
