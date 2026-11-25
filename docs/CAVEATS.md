# Caveats and limitations

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Reported bugs](BUGS.md)** · **[Edge cases](EDGE.md)** ·
**[Pending tasks](TODO.md)** · **[Backend parity](parity/backend.md)** ·
**[UI parity](parity/ui.md)**

This guide records limitations verified in the current source, including
security and privacy behavior. The [backend](parity/backend.md) and
[UI](parity/ui.md) references describe the implemented baseline alongside
recommended parity goals.

Legend: ⚠️ limitation · 🔒 security · 📋 planned · ✅ resolved

> [!NOTE]
>
> The app can be built with different optional features. Confirm the active
> branch and build configuration before applying a workaround.

## Contents

- [Current limitations](#current-limitations)
  - [Multiple accounts](#multiple-accounts)
  - [Related messages and message details panels](#related-messages-and-message-details-panels)
  - [OAuth sign-in](#oauth-sign-in)
  - [Mail operations](#mail-operations)
- [Before reporting results](#before-reporting-results)
- [Security and data protection](#security-and-data-protection)
  - [Recommendations](#recommendations)
  - [Safe operating guidance](#safe-operating-guidance)

## Current limitations

| Area                 | Current constraint                                                                                                                                                                                                 |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| ⚠️ Product identity  | The product is renamed to Plasma Impresario ([#221](https://github.com/mikepadiernos/plasma-impresario/issues/221)), but the application remains based on Trojitá and the product redesign is not complete         |
| ✅ Accounts          | One window holds several IMAP accounts, grouped by account in the folder sidebar; the limitations recorded for them are resolved ([details](#multiple-accounts))                                                   |
| ⚠️ Embedded panels   | Related messages search only loaded messages in the current folder, and message details show MIME-level security only ([details](#related-messages-and-message-details-panels))                                    |
| ⚠️ Mail operations   | Special folders need server attributes or English names, Go to Folder and related searches use loaded data only, and Undo after a move needs server COPYUID support ([details](#mail-operations))                  |
| ⚠️ OAuth sign-in     | Microsoft uses the `common` tenant only and cannot revoke grants from the app, IMAP and SMTP refresh tokens independently, and switching provider replaces a grant without revoking it ([details](#oauth-sign-in)) |
| ⚠️ Message rendering | Qt WebEngine is required for the default reader; a renderer-free build cannot display message contents                                                                                                             |
| 🔒 Credentials       | QtKeychain is optional; new profiles default to it, but profiles with saved cleartext passwords keep the cleartext plugin, which stores secrets in `QSettings`                                                     |
| 🔒 Transport         | Cleartext IMAP remains selectable; account configuration determines whether TLS is used                                                                                                                            |
| 🔒 Local data        | Persistent SQL cache stores message metadata and fetched parts; the application does not encrypt cache contents                                                                                                    |
| 📋 Integrations      | Plasma new-mail notifications, session integration, Gmail/Outlook APIs, Nextcloud, rules and cross-app workflows remain planned                                                                                    |
| ⚠️ Compatibility     | The [baseline](philosophy/compatibility.md) records an initial CachyOS target and partial build evidence; no full reader or desktop-session support matrix has been verified                                       |
| ⚠️ Quality evidence  | Large-mailbox benchmarks and broad accessibility, screen-reader, high-DPI and RTL verification are not recorded                                                                                                    |

### Multiple accounts

| Caveat issue                                                          | Board  | Recommendation                                                                                         | Verified with                                                              |
| --------------------------------------------------------------------- | ------ | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------- |
| [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262) | Merged | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/262#issuecomment-7142887185) | Disposable Dovecot, the opt-in `test_LiveImapAccounts` and offscreen tests |

All five limitations recorded in #262 are resolved.

**Current behavior:**

- ✅ Errors from background accounts appear in the account selector, on the
  account's header in the folder sidebar, and on a footer button that switches
  to the first flagged account. Each failure also raises one desktop
  notification, with an **Open Account** action, until the account connects
  again. Builds without KNotifications show it as a tray message instead, but
  only while the tray icon is visible.
- ✅ Removing an account deletes its settings, stored passwords and offline
  cache. For the account migrated from a single-account profile, whose cache
  shares the cache root, only its database and message-part directories are
  deleted; drafts and other accounts' caches are kept.
- ✅ Accepting an untrusted TLS certificate records the host name and the
  SHA-256 fingerprint of the certificate's public key for the whole profile.
  Other accounts on that host, and the host's other service (IMAP or SMTP),
  accept the same key without asking. A different key, or the same key for
  another host name, still prompts.
- ✅ A composer sends and files the message with the settings of the account it
  was opened from, even after switching accounts. If that account is removed,
  sending is refused with an error and the composer stays open.
- ✅ Each account's header in the folder sidebar has a menu with **Account
  Settings…**, **Rename…** and **Remove…**. **Account Settings…** switches to
  that account first, so the dialog and the window show the same account;
  renaming and removing don't switch.

The notification popup and its action are checked in a real Plasma session
with #315 in [M15](ROADMAP.md#m15-real-plasma-session-verification).

### Related messages and message details panels

| Caveat issue                                                          | Board  | Recommendation                                                                                         | Applies to                                                     |
| --------------------------------------------------------------------- | ------ | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------- |
| [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264) | Review | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/264#issuecomment-7143646817) | Panels off by default; enable them from View › Embedded Panels |

**Current behavior:**

- ⚠️ The related messages panel only searches messages already loaded in the
  current folder's message list, not the server or other folders. It lists at
  most 25 messages per group (conversation and same sender) and scans at most
  the 20,000 most recent rows.
- ⚠️ The message details panel derives its Security field from the top-level
  MIME type (`multipart/signed`, `multipart/encrypted` or S/MIME). It does not
  detect inline PGP or show whether a signature is valid; the reader still
  verifies and decrypts.
- ✅ Pane sizes saved by builds before the shell carry over the first time each
  layout opens: the folder tree's width, on the side it was saved on, and the
  split between the message list and the reader. Sizes the shell has saved
  since then take precedence.

**Workarounds:**

- Use the search bar or open the other folder to find related mail that is not
  loaded.
- Rely on the reader's security status for signature and decryption results.

**Proposed improvements:**

- 📋 Add an opt-in server-side search on `Message-ID`, `References` and sender,
  optionally across folders (M04,
  [#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)).
- 📋 Feed the details panel from the reader's verification result, including
  inline PGP (M04,
  [#303](https://github.com/mikepadiernos/plasma-impresario/issues/303)).

### OAuth sign-in

| Caveat issue                                                          | Board  | Recommendation                                                                                         | Applies to                                                  |
| --------------------------------------------------------------------- | ------ | ------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------- |
| [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362) | Review | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/362#issuecomment-7165448590) | Accounts set to OAuth in **Account Settings › IMAP** (#218) |

**Current behavior:**

- ✅ When an OAuth account needs to sign in, the status bar shows a **Sign In…**
  button, the account's sidebar menu has **Sign In…**, and the error
  notification offers **Sign In**. Each opens the browser for that account and
  reconnects it after a successful sign-in, without opening Account Settings.
- ✅ The optional client secret is stored with the grant in the password
  plugin, not in the settings file. A secret saved by an earlier build moves
  there the next time the account connects. The field in Account Settings
  starts empty, and saving is blocked while a typed secret has not been used to
  sign in.
- ✅ Signing out and removing an account revoke the grant at Google before
  deleting it locally. Removal keeps the grant when another account uses the
  same provider, client and user name.
- ✅ A grant made in Account Settings is revoked when the dialog is cancelled,
  or saved with a different provider or client.
- 🔒 OAuth needs a secure credential store and is refused with the cleartext
  password backend.
- 🔒 No client ID is built in unless one is set at build time with
  `IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID` or `IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID`.
- ⚠️ Microsoft has no revocation endpoint for these clients. Signing out and
  removing the account delete the local grant and point to Microsoft's access
  page.
- ⚠️ Signing out in Account Settings revokes the grant at Google even when
  another account shares the same user and client, which then needs to sign in
  again.
- ⚠️ Each account stores one grant. Signing in with another provider or client
  replaces the previous grant without revoking it, and cancelling the dialog
  afterwards leaves the account without a grant until it signs in again.
- ⚠️ Microsoft accounts use the `common` tenant only.
- ⚠️ SMTP requests a new access token for every message, and IMAP and SMTP
  refresh independently. A provider that rotates refresh tokens can make one of
  them fail until the next sign-in.
- ⚠️ The provider fields are on the IMAP page only; SMTP reuses them.
- ⚠️ A server that advertises `LOGINDISABLED` still needs STARTTLS before OAuth,
  as with passwords.
- ⚠️ Revoking a grant from a cancelled dialog is not covered by automated
  tests, because a sign-in needs the real provider endpoints.

**Workarounds:**

- Use QtKeychain, the default for new profiles, before choosing OAuth.
- After signing out of or removing a Microsoft account, remove the app's access
  at [account.live.com/consent/Manage](https://account.live.com/consent/Manage).
- Sign out before switching an account to another provider or client, so the
  old grant is revoked.
- For Microsoft accounts, register your own app in Microsoft Entra ID as
  described under
  [App registration](integrations/microsoft.md#app-registration) and enter
  its client ID. For single-tenant accounts, let the registration accept the
  `common` endpoint, or keep password sign-in where the server allows it.
- Prefer a public client without a secret.

**Proposed improvements:**

- 📋 Build a Plasma Impresario Microsoft client ID into release packages
  (M13.01, [#396](https://github.com/mikepadiernos/plasma-impresario/issues/396)).
- 📋 Add a tenant choice for Microsoft accounts (M13.01,
  [#397](https://github.com/mikepadiernos/plasma-impresario/issues/397)).
- 📋 Share one token session between IMAP and SMTP per account, and reuse the
  access token until it expires (M13.01,
  [#398](https://github.com/mikepadiernos/plasma-impresario/issues/398), for
  Microsoft accounts).
- 📋 Keep the previous grant until a dialog that signed in with another
  provider or client is saved, then revoke it (M14.03,
  [#428](https://github.com/mikepadiernos/plasma-impresario/issues/428)).
- 📋 Skip revocation when signing out in Account Settings if another account
  shares the grant (M14.03,
  [#429](https://github.com/mikepadiernos/plasma-impresario/issues/429)).
- 📋 Verify sign-in and revocation with a real provider account
  ([#317](https://github.com/mikepadiernos/plasma-impresario/issues/317), M15).

### Mail operations

| Caveat issue                                                          | Board   | Recommendation                                                                                         | Applies to                                                      |
| --------------------------------------------------------------------- | ------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------- |
| [#312](https://github.com/mikepadiernos/plasma-impresario/issues/312) | Backlog | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/312#issuecomment-7148901982) | Folder actions, message list filters and sorting, Unified Inbox |

**Current behavior:**

- ⚠️ Special folders (Sent, Drafts, Trash, Junk, Archive) are recognized from
  server SPECIAL-USE attributes or English folder names, not localized names.
  Only the first name match of each kind is used. The configured archive
  folder wins over the server's `\Archive`, and a changed name applies after
  reconnecting.
- ⚠️ Go to Folder lists only folders already loaded in the folder tree.
- ⚠️ Undo after moving to Trash or Archive appears only when the server returns
  `COPYUID` (UIDPLUS); otherwise the notification offers to open the folder.
  Copies cannot be undone. Undo and Empty Trash/Junk may briefly select another
  folder.
- ⚠️ Counts in confirmations may be incomplete before the folder is
  synchronized; the Expunge count covers loaded messages only.
- ⚠️ Renaming re-lists only the renamed folder and does not update the archive
  or Sent folder settings.
- ⚠️ Move To and Copy To share one list of recent folders. A folder is added
  when the operation starts, so a failed operation still adds it, and deleted
  folders stay in the list. The menus show full IMAP names, such as
  `INBOX.Lists.KDE`, rather than a path.
- ⚠️ Message list filters are kept per account, not per folder; the old global
  hide-read setting only seeds them. A thread is shown whole when any of its
  messages matches. When messages stop matching, only the current message stays
  visible, not the rest of a multiple selection.
- ⚠️ The Flagged and Unread filters hide messages until their flags load, so
  large folders fill in gradually. In "Showing X of Y", X also reflects the
  quick search, and Y is the server's total.
- ⚠️ Bulk actions report their result when the command is sent, not when the
  server confirms it.
- ⚠️ After messages are removed, the next selection is chosen heuristically
  when the server expunges only some of them, and rows hidden by filters or
  collapsed threads are not considered. The reader keeps the old message when
  the list becomes empty. Failures to mark a message as read from the reader
  are not reported.
- ⚠️ The `?` shortcut depends on the keyboard layout, and the shortcut overview
  may list stale shortcuts for windows not yet opened.
- ⚠️ Select Similar and Show Related use loaded envelopes only. Mailing-list
  matches use `List-Post`, because the message model exposes no `List-Id`.
- ⚠️ While the dense layout is on, its compact density and hidden panels are
  written to the normal preferences, so they persist after a restart.
- ⚠️ The default sort and threading for new folders come from the message
  list header's state; `guiMsgListShowThreading` is no longer written. Group By
  is not tested against the fake server. A folder's saved position is restored
  only after its message UIDs have loaded.
- ⚠️ Two-line message rows show no preview text, because message bodies are
  not fetched for the list, and no contact photos, only the sender's initials.
  In the Unified Inbox, Two Lines names each message's account; Columns show it
  only in the row's tooltip. All rows share one height, so date group headings
  are as tall as a two-line row.
  The column header, with its column chooser and sorting by clicking, is
  hidden in Two Lines.
- ⚠️ Check for New Mail asks each online account for the counts it already
  knew and resynchronizes the open folder; it does not list new folders or
  download messages of closed folders. Accounts are otherwise refreshed every
  five minutes, or immediately in the open folder when the server supports
  IDLE.
- ⚠️ Account headers show only the INBOX unread count, not a total across
  folders. Go to Next Unread Folder covers the active account and folders
  already listed in the tree; folders whose count has not arrived are skipped,
  so the first use after connecting may skip some.
- ⚠️ The Unified Inbox
  ([#223](https://github.com/mikepadiernos/plasma-impresario/issues/223)) lists
  the INBOX of each included account only. It has no threads, Group By, drag
  and drop, quick search or Show Related, and sorts on the client rather than
  on the server. Moving or copying messages of several accounts at once is
  refused. Notifications about different accounts replace each other. An
  account added while the Unified Inbox is shown appears after turning it off
  and on again. Offline accounts still list their cached messages, and the
  status line names them.
- ⚠️ These paths are unit-tested but not yet exercised in a window against a
  live server.

**Workarounds:**

- Enable SPECIAL-USE on the server, or set the archive folder in the account
  settings and reconnect.
- Expand the folder tree before using Go to Folder.
- Without Undo, open the destination from the notification and move the
  messages back.
- Update the account settings after renaming the archive or Sent folder.
- Open the folder to confirm the result of a bulk action.
- Turn off the dense layout before changing the density or panels you want to
  keep.
- In Two Lines, sort from View > Sorting, or switch View > Message Rows to
  Columns to choose and resize columns.
- Use Reload All Folders to pick up new folders, and expand a branch so that
  Go to Next Unread Folder can visit it.
- In the Unified Inbox, move or copy the messages of one account at a time,
  and open an account's INBOX to thread, group or search its messages.

**Proposed improvements:**

- 📋 Recognize localized names and allow assigning special folders per
  account.
- 📋 Query the server from Go to Folder and from related searches.
- 📋 Locate moved messages by `Message-ID` when COPYUID is missing.
- 📋 Run Empty and Undo on a dedicated connection.
- 📋 Follow renamed archive and Sent folders.
- 📋 Record recent folders after the operation succeeds, prune deleted ones,
  and show folder paths.
- 📋 Report bulk results after the server confirms them.
- 📋 Keep the dense layout's preferences separate from the normal ones.
- 📋 Offer an opt-in preview line from already downloaded text, contact photos
  from the address book only, and shorter date group headings.
- 📋 Let Check for New Mail re-list folders, add per-account and per-folder
  check intervals, and offer an account-wide or all-accounts unread total and
  next-unread search.
- 📋 Extend the Unified Inbox to other special folders, threads and quick
  search across accounts, and moves between accounts.
- 📋 Add live window-level tests.

## Before reporting results

- [ ] Confirm the desktop environment and display server.
- [ ] Record the selected account transport and password plugin.
- [ ] Identify the provider, server capabilities and whether WebEngine or
      OpenPGP support is enabled.
- [ ] Isolate test credentials and mail data.
- [ ] Record Qt, KF6, compiler, distribution and build options.
- [ ] Retain a reproducible mailbox size and sample message set.

## Security and data protection

> [!CAUTION]
>
> This is a source-based status summary, not a security audit. Behavior varies
> with account settings and optional build features.

| Area                 | Existing behavior                                                                                                                                                                                                                                                                                                                                  | Limitation                                                                                                                                                                                                               |
| -------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| 🔒 IMAP transport    | Supports cleartext, STARTTLS, dedicated TLS and process-backed connections. Qt certificate errors prompt the user; a previously accepted matching public key can be remembered, and an accepted key is trusted for its host by every account of the profile.                                                                                       | Cleartext is selectable; the active connection method determines whether traffic is encrypted. Certificate decisions and policy need focused regression tests.                                                           |
| 🔒 SMTP transport    | Supports cleartext, STARTTLS and dedicated TLS. Certificate errors prompt the user in the composer; an accepted public key is remembered per account and forgotten when the SMTP host changes. A key accepted for the same host by any account, over IMAP or SMTP, is accepted without asking.                                                     | Cleartext is selectable. Certificates the system trusts are accepted without remembering their key, so only the system's policy applies to them. There is no way yet to review or forget accepted keys.                  |
| 🔒 Credentials       | The password-plugin interface supports retrieval, storage and deletion. QtKeychain can provide encrypted storage when built and selected, and is the default for new profiles. Each account keeps its own stored passwords, which are deleted when the account is removed.                                                                         | QtKeychain is optional. Profiles that already have saved cleartext passwords keep the cleartext plugin, which stores IMAP/SMTP secrets in `QSettings`.                                                                   |
| 🔒 Message rendering | The reader uses a private profile per message, disables scripts, plugins and persistent storage, blocks local-file and remote requests by default, and delegates permitted external links. Remote images require explicit per-message consent. Switching messages destroys the profile, discarding loaded content and cancelling pending requests. | Renderer behavior has not been smoke-tested in a live Plasma session or across the documented distribution matrix.                                                                                                       |
| 🔒 Cached data       | Persistent SQL cache can hold message metadata and fetched parts; cache directory permissions are restricted on supported Unix systems.                                                                                                                                                                                                            | Application-level cache encryption is not present. Directory permissions do not protect mail from a compromised account or unencrypted disk. Removing an account deletes its cache files without overwriting them first. |
| 🔒 OpenPGP           | Optional GpgME++/Mimetic MIME handling and decryption tests are present.                                                                                                                                                                                                                                                                           | Availability depends on build options; complete signature-status UX and verification policy are not established.                                                                                                         |
| 🔒 Diagnostics       | Persistent protocol logging is opt-in at startup; an in-app protocol log is available.                                                                                                                                                                                                                                                             | Treat protocol logs as sensitive and review them before sharing.                                                                                                                                                         |

### Recommendations

| Priority | Focus                                                                                                                                  |
| -------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| **P0**   | [Secure account defaults](#p0--secure-account-defaults); [protect local data and diagnostics](#p0--protect-local-data-and-diagnostics) |
| **P1**   | [Reduce message-content risk](#p1--reduce-message-content-risk); [clarify cryptographic results](#p1--clarify-cryptographic-results)   |
| **P2**   | [Define release security checks](#p2--define-release-security-checks)                                                                  |

#### P0 — secure account defaults

- Default new accounts to verified TLS. Keep cleartext as an explicit, clearly
  warned compatibility option.
- Fail closed when required TLS negotiation fails; never retry authentication
  over plaintext after a TLS failure.
- Prefer QtKeychain or another supported OS credential store. Make `QSettings`
  cleartext storage an explicit opt-in and provide a safe migration path for
  existing credentials.
- Test certificate rejection and key changes, TLS negotiation failures, STARTTLS
  downgrade handling and SMTP transport policy.

#### P0 — protect local data and diagnostics

- Document cache location, retention and deletion; keep restrictive file and
  directory permissions and recommend full-disk encryption for cached mail.
- Review IMAP and SMTP logs for credentials, message content and personal data.
  Redact secrets before buffering or writing logs and test the redaction.
- Make persistent logging discoverable and temporary, with a clear way to stop
  logging and remove the resulting file.

#### P1 — reduce message-content risk

- Verify the Qt WebEngine reader in live Plasma sessions and across the
  supported distribution matrix.
- Keep scripts, plugins and remote content disabled by default; show external
  destinations before enabling requests where practical and reset consent when
  changing messages.
- Test malformed MIME, nested parts, oversized content, link schemes and failed
  attachment downloads using isolated fixtures.

#### P1 — clarify cryptographic results

- Distinguish encrypted, decrypted, signed and verified states, including
  invalid, unknown-key, expired-key and unsupported cases.
- Ensure failed or unavailable crypto never appears as successful verification;
  expand tests for modified signatures, expired and unknown keys, nested MIME
  and error recovery.

#### P2 — define release security checks

- Document protected data and trust boundaries across mail servers, message
  content, plugins, the local account and external tools.
- Add dependency review, static analysis, clean configuration builds, privacy
  review and regression tests to release checks.
- Record tested configurations and residual risks without claiming that tests
  prove the client is secure.

### Safe operating guidance

> [!TIP]
>
> These settings reduce risk with the current build; they do not replace the
> recommendations above.

- Use implicit TLS or required STARTTLS with valid certificates; reject
  certificate warnings unless independently verified with the provider.
- Select a secure credential plugin and avoid saving passwords through the
  cleartext plugin.
- Keep remote message content blocked unless needed for a trusted message.
- Avoid persistent debug logging during ordinary use; sanitize logs before
  attaching them to reports.
- Use operating-system disk encryption when retaining cached mail.
