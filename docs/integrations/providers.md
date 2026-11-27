# Mail providers

**[Documentation](../README.md)** ·
**[Roadmap: M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)** ·
**[Provider caveats](../CAVEATS.md#provider-setup-and-gmail)** ·
**[Integrations](README.md)** · **[Microsoft](microsoft.md)** ·
**[KDE Plasma](kde-plasma.md)**

This guide explains how to set up Gmail, Outlook.com, Microsoft 365 and any
other IMAP and SMTP account in Plasma Impresario: how each provider signs in,
which server features the client uses, and what does not work yet. It describes
the current build, as of 2026-11-27. Planned provider work is tracked in
[M06.05](../ROADMAP.md#m06-desktop-integration-and-provider-support)
([#43](https://github.com/mikepadiernos/plasma-impresario/issues/43)), and
Microsoft accounts beyond IMAP and SMTP in
[M13](../ROADMAP.md#m13-microsoft-accounts-and-exchange).

Legend: ![Done][done] · ![Needs verification][needs-verification] ·
![Limitation][limitation] · ![Planned][planned]

## Contents

- [Status](#status)
- [Adding an account](#adding-an-account)
- [Built-in providers](#built-in-providers)
- [Signing in](#signing-in)
  - [Passwords and app passwords](#passwords-and-app-passwords)
  - [OAuth](#oauth)
- [Gmail](#gmail)
- [Outlook.com and Microsoft 365](#outlookcom-and-microsoft-365)
- [Generic IMAP and SMTP](#generic-imap-and-smtp)
  - [Server features](#server-features)
  - [Sending](#sending)
- [POP3 and JMAP](#pop3-and-jmap)
- [Troubleshooting](#troubleshooting)
- [Verification](#verification)

## Status

| Area                           | Status                                    | Today                                                                                                                                                                                                                                                       | Roadmap                                                                                                                                      |
| ------------------------------ | ----------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| Finding server settings        | ![Needs verification][needs-verification] | **Find Settings** in **Add Account** looks up the servers from the email address ([#372](https://github.com/mikepadiernos/plasma-impresario/issues/372)); not tried against real providers                                                                  | M06.05                                                                                                                                       |
| Passwords and app passwords    | ![Done][done]                             | IMAP and SMTP sign in with the account or app password, kept by the password plugin                                                                                                                                                                         | —                                                                                                                                            |
| OAuth for Google and Microsoft | ![Needs verification][needs-verification] | Browser sign-in with PKCE; XOAUTH2 or OAUTHBEARER for IMAP and SMTP ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)); no client ID is built in                                                                                       | M13.01                                                                                                                                       |
| Gmail                          | ![Needs verification][needs-verification] | Works over IMAP and SMTP; Gmail search keys are offered. Labels show as chips on the open message, **Archive** uses **All Mail**, and sent messages are not uploaded twice. Not tried against real Gmail ([caveat](../CAVEATS.md#provider-setup-and-gmail)) | M06.05 [#43](https://github.com/mikepadiernos/plasma-impresario/issues/43)                                                                   |
| Outlook.com and Microsoft 365  | ![Needs verification][needs-verification] | IMAP and SMTP with OAuth only; see the [Microsoft guide](microsoft.md)                                                                                                                                                                                      | [M13](../ROADMAP.md#m13-microsoft-accounts-and-exchange)                                                                                     |
| Generic IMAP and SMTP          | ![Done][done]                             | Any server with IMAP4rev1 and SMTP submission; optional extensions are used when offered                                                                                                                                                                    | —                                                                                                                                            |
| POP3 and JMAP                  | ![Planned][planned]                       | The account model knows which actions they support, but they cannot connect yet                                                                                                                                                                             | [#216](https://github.com/mikepadiernos/plasma-impresario/issues/216), [#217](https://github.com/mikepadiernos/plasma-impresario/issues/217) |

## Adding an account

1. Choose **Application › Add Account…** and enter your name and email
   address.
2. Choose **Find Settings**. The client looks up the IMAP and SMTP servers
   without signing in or sending a password. It tries the providers built into
   the application first, then the mail domain's autoconfig file, Thunderbird's
   ISP database, DNS SRV records, the domain's MX hosts and common host names,
   and stops at the first that names an IMAP server.
3. Check what was found: the provider, the servers, where they came from, and
   any guidance, such as using an app password. Servers found through DNS
   replies that were not signed, outside your mail domain and the known
   providers, must be confirmed before the account can be added.
4. Add the account. **Account Settings** opens on the found servers; enter the
   password or choose OAuth on the **IMAP** page, then sign in.

If nothing is found, or the servers are wrong, add the account anyway and enter
the servers from your provider's help pages under **Account Settings › IMAP**
and **› SMTP**. The limits of the lookup are in
[Server settings discovery](../CAVEATS.md#server-settings-discovery).

## Built-in providers

These providers are known to the application, so **Find Settings** needs no
network lookup for their addresses. Gmail, Outlook.com and Microsoft 365 also
match custom domains whose MX hosts they run, such as Google Workspace and
Microsoft 365 domains.

| Provider      | IMAP                             | SMTP                                  | Sign-in                      | Before signing in                                        |
| ------------- | -------------------------------- | ------------------------------------- | ---------------------------- | -------------------------------------------------------- |
| Gmail         | `imap.gmail.com` 993, TLS        | `smtp.gmail.com` 465, TLS             | App password or Google OAuth | Create an app password, or set up OAuth with a client ID |
| Outlook.com   | `outlook.office365.com` 993, TLS | `smtp-mail.outlook.com` 587, STARTTLS | Microsoft OAuth only         | Set up OAuth with a client ID; passwords are refused     |
| Microsoft 365 | `outlook.office365.com` 993, TLS | `smtp.office365.com` 587, STARTTLS    | Microsoft OAuth only         | The tenant must allow IMAP and SMTP AUTH                 |
| Yahoo Mail    | `imap.mail.yahoo.com` 993, TLS   | `smtp.mail.yahoo.com` 465, TLS        | App password                 | Create an app password                                   |
| iCloud Mail   | `imap.mail.me.com` 993, TLS      | `smtp.mail.me.com` 587, STARTTLS      | App password                 | Create an app-specific password                          |
| AOL Mail      | `imap.aol.com` 993, TLS          | `smtp.aol.com` 465, TLS               | App password                 | Create an app password                                   |
| GMX           | `imap.gmx.net` 993, TLS          | `mail.gmx.net` 465, TLS               | Password                     | Turn on IMAP access in GMX's web settings                |
| WEB.DE        | `imap.web.de` 993, TLS           | `smtp.web.de` 465, TLS                | Password                     | Turn on IMAP access in WEB.DE's web settings             |
| Posteo        | `posteo.de` 993, TLS             | `posteo.de` 465, TLS                  | Password                     | Nothing                                                  |

The user name is the full email address, except for WEB.DE and iCloud IMAP,
which take the part before `@`. **Find Settings** fills it in either way.

## Signing in

### Passwords and app passwords

- **What:** IMAP and SMTP sign in with a user name and password. SMTP can reuse
  the IMAP credentials.
- **Where:** **Account Settings › IMAP** and **› SMTP**.
- **Storage:** Passwords are kept by the password plugin: QtKeychain, which
  uses KWallet or the Secret Service in Plasma and is the default for new
  profiles, or the cleartext plugin, which writes them to the settings file.
  See [Security and data protection](../CAVEATS.md#security-and-data-protection).
- **App passwords:** Gmail with 2-Step Verification, Yahoo, iCloud and AOL
  refuse the account password from mail applications. Create an app password
  in the provider's security settings and enter it instead.

### OAuth

- **What:** The account authorizes the application once in the system browser,
  with the authorization code flow, PKCE and a loopback redirect (RFC 8252).
  The refresh token is stored with the password plugin; access tokens stay in
  memory. IMAP and SMTP present them with XOAUTH2 or OAUTHBEARER (RFC 7628).
- **Providers:** Google, with the `https://mail.google.com/` scope, and
  Microsoft, through the `common` tenant with `IMAP.AccessAsUser.All`,
  `SMTP.Send` and `offline_access`.
- **Setup:** Under **Account Settings › IMAP**, choose OAuth, the provider and
  an OAuth client ID, then **Sign In…**. SMTP reuses the IMAP page's provider.
  When a sign-in expires, the status bar, the account's sidebar menu and the
  error notification offer **Sign In…**.
- **Client IDs:** None is built in unless the build sets
  `IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID` or
  `IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID`. Without one, register an application
  with the provider and enter its client ID. A client secret is optional and is
  stored with the grant, never in the settings file. For Microsoft, follow
  [App registration](microsoft.md#app-registration).
- **Requirements:** OAuth is refused with the cleartext password plugin.
- **Signing out:** Signing out or removing the account revokes the grant at
  Google. Microsoft has no revocation endpoint for these clients, so remove
  the access at [account.live.com/consent/Manage](https://account.live.com/consent/Manage).

The limitations are in [OAuth sign-in](../CAVEATS.md#oauth-sign-in).

## Gmail

| Topic           | Behavior                                                                                                                                                                                                                                                                       | Status                                    |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ----------------------------------------- |
| Sign-in         | An app password, with 2-Step Verification on, or Google OAuth with your own client ID                                                                                                                                                                                          | ![Done][done]                             |
| IMAP access     | Gmail accounts have IMAP on; Google Workspace administrators can turn it off for a domain                                                                                                                                                                                      | ![Done][done]                             |
| Labels          | Each label is still a folder, and a message with several labels appears in each. The open message shows its other labels as chips beside its tags, read with `X-GM-LABELS`; **Starred**, **Draft** and the current folder's label are left out. The chips cannot change labels | ![Limitation][limitation]                 |
| Special folders | **Sent Mail**, **Drafts**, **Spam**, **Trash** and **All Mail** are recognized from Gmail's special-use attributes                                                                                                                                                             | ![Done][done]                             |
| Archive         | With **Archive Folder Name** empty, the default, **Archive** moves messages to the folder Gmail marks `\All`, which is **All Mail** in any interface language; this removes the **Inbox** label without adding another. A name entered in the field still wins                 | ![Done][done]                             |
| Sent copies     | Gmail keeps a copy of every message sent through `smtp.gmail.com` or `smtp.googlemail.com`, so none is uploaded to **Sent Mail**; **Save Outgoing Mail** is shown off and disabled for these servers                                                                           | ![Done][done]                             |
| Search          | When the server offers `X-GM-EXT-1`, Gmail's own search syntax (`X-GM-RAW`), message and thread IDs, and labels can be searched                                                                                                                                                | ![Done][done]                             |
| Limits          | Gmail limits IMAP bandwidth and simultaneous connections per account; very large first syncs can be throttled                                                                                                                                                                  | ![Needs verification][needs-verification] |

**Recommended setup:**

- Leave **Archive Folder Name** under **Account Settings › IMAP** empty. An
  account saved by an earlier version may have `Archive` stored there; clear
  the field so that **Archive** uses **All Mail**.
- Leave Gmail's **Auto-Expunge** on, the default, so that deleted messages
  leave the label folder at once.

## Outlook.com and Microsoft 365

- Outlook.com, Hotmail and Live addresses, and Microsoft 365 mailboxes, use
  IMAP and SMTP with Microsoft OAuth. Microsoft refuses passwords from mail
  applications.
- Microsoft 365 administrators can turn off IMAP or authenticated SMTP for a
  tenant or a mailbox; the account then cannot connect or send.
- Only the `common` tenant is supported. Organizations that allow only their
  own tenant need an app registration that accepts it.
- Calendar, contacts, tasks, Graph mail and Exchange Server are planned in
  [M13](../ROADMAP.md#m13-microsoft-accounts-and-exchange). See the
  [Microsoft guide](microsoft.md).

## Generic IMAP and SMTP

Any server that offers IMAP4rev1 and SMTP submission works. Enter the servers
under **Account Settings**:

- **IMAP:** the host, port and encryption: TLS on port 993, or STARTTLS on port
  143. A **Local process** can run a command that connects to the server, or
  acts as one, for example over SSH.
- **SMTP:** the host, port and encryption: TLS on port 465, or STARTTLS on
  port 587. Authentication can reuse the IMAP user name and password.
- **User name:** usually the full email address; some servers want only the
  part before `@`.

### Server features

The client asks the server what it supports and uses these extensions only when
they are offered. Without them it falls back to plain IMAP4rev1, which is
slower but complete.

| Extension                   | Used for                                                                                 |
| --------------------------- | ---------------------------------------------------------------------------------------- |
| `IDLE`                      | New mail without polling; renewed every 29 minutes by default (**Timeout of IMAP IDLE**) |
| `CONDSTORE`, `QRESYNC`      | Fast resynchronization of flags and removed messages after reconnecting                  |
| `ESEARCH`, `SORT`, `THREAD` | Searching, sorting and threading on the server                                           |
| `MOVE`, `UIDPLUS`           | Moving messages in one step, and tracking appended messages                              |
| `SPECIAL-USE`               | Recognizing Sent, Drafts, Junk, Trash, Archive and All Mail by attribute                 |
| `LIST-STATUS`               | Folder counts in the same request as the folder list                                     |
| `COMPRESS=DEFLATE`          | Compressing the connection, in builds with zlib                                          |
| `BINARY`                    | Downloading attachments without transfer encoding                                        |
| `CATENATE`, `URLAUTH`       | Forwarding and sending with BURL without uploading the message again                     |
| `ID`                        | Identifying the client and showing the server's identity                                 |
| `X-GM-EXT-1`                | Gmail search keys and labels                                                             |

Servers that advertise an extension but implement it badly can be worked
around: list it under **Account Settings › IMAP › Extension Blacklist**, and the
client acts as if it were not offered.

The actions an account can take follow its protocol: actions the active account
cannot perform, such as server folders on POP3, are hidden.

### Sending

- **SMTP:** submission over TLS or STARTTLS, with a password or OAuth.
- **BURL:** with **Send with BURL** on, and IMAP and SMTP servers that support
  it, the message is saved to the IMAP server once and sent from there (RFC
  4468). The servers need extra configuration.
- **sendmail:** a local command, `sendmail -bm -oi` by default, for systems
  with a configured mail transfer agent.
- **Sent copies:** **Save Outgoing Mail** keeps a copy in the folder named by
  **Sent Folder Name**.

## POP3 and JMAP

POP3 ([#216](https://github.com/mikepadiernos/plasma-impresario/issues/216))
and JMAP ([#217](https://github.com/mikepadiernos/plasma-impresario/issues/217))
are planned in M06.05. The account model already records which actions each
protocol supports, and the window hides the rest, but neither protocol can
connect yet. Use IMAP for providers that offer both.

## Troubleshooting

| Problem                                           | Likely cause                                     | What to do                                                                               |
| ------------------------------------------------- | ------------------------------------------------ | ---------------------------------------------------------------------------------------- |
| Gmail, Yahoo, iCloud or AOL rejects the password  | The provider needs an app password               | Create an app password in the provider's security settings and enter it                  |
| Outlook.com or Microsoft 365 rejects the password | Microsoft accepts only OAuth                     | Choose OAuth under **Account Settings › IMAP**, enter a client ID and sign in            |
| **Sign In…** reports that OAuth is not configured | No client ID is built in or entered              | Enter a client ID from your own app registration                                         |
| OAuth is refused                                  | The cleartext password plugin is in use          | Switch to QtKeychain under **Settings › General › Passwords**                            |
| GMX or WEB.DE refuses to connect                  | IMAP access is off in the web settings           | Turn on IMAP access in the provider's web settings                                       |
| Microsoft 365 connects but cannot send            | The tenant turned off authenticated SMTP         | Ask the administrator to allow SMTP AUTH for the mailbox                                 |
| **Archive** in Gmail creates an `Archive` label   | An archive folder name is stored for the account | Clear **Archive Folder Name** under **Account Settings › IMAP**                          |
| The server misbehaves after connecting            | An extension is advertised but broken            | Add it to the **Extension Blacklist** and reconnect; the protocol log shows the exchange |

## Verification

- **Offscreen tests:** `test_ServerDiscovery` covers the built-in providers,
  autoconfig, ISPDB, SRV, MX and guessed servers with a fake network;
  `test_OAuth` covers the requests, token parsing, grants and error messages;
  `test_AccountCapabilities` covers the actions of each protocol. For Gmail,
  `test_SpecialFolder` covers archiving to **All Mail**,
  `test_Imap_Parser_parse` and `test_Imap_CopyAndFlagOperations` cover reading
  labels, `test_TagListWidget` covers the chips, and
  `test_AccountSettingsDialog` covers the Sent and archive settings. Dovecot
  has no `X-GM-EXT-1`, so these run against a scripted server.
- **Live tests:** the Dovecot accounts in `tools/dev-container` cover generic
  IMAP and SMTP with passwords.
- **Not yet tried:** real Gmail, Outlook.com, Microsoft 365, Yahoo, iCloud,
  AOL, GMX, WEB.DE and Posteo accounts, and OAuth against the real providers.
  That is [M15](../ROADMAP.md#m15-real-plasma-session-verification) and the
  [provider caveat](../CAVEATS.md#provider-setup-and-gmail).

[done]: https://img.shields.io/badge/Done-1a7f37
[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[planned]: https://img.shields.io/badge/Planned-6e7781
