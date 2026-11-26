# Caveats and limitations

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Edge cases](EDGE.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

This page is the source of truth for known limitations of implemented
features, including security and privacy behavior, as verified in the current
source; caveat statuses match the project board as of 2026-11-25. The
[backend](parity/backend.md) and [UI](parity/ui.md) parity references describe
the implemented baseline alongside recommended parity goals.

Legend: ![Limitation][limitation] current limitation ·
![Security][security] security or privacy behavior ·
![Planned][planned] planned, not implemented ·
![Resolved][resolved] resolved, kept for context

> [!NOTE]
>
> The app can be built with different optional features. Confirm the active
> branch and build configuration before applying a workaround.

## Contents

- [Current limitations](#current-limitations)
  - [Multiple accounts](#multiple-accounts)
  - [Related messages and message details panels](#related-messages-and-message-details-panels)
  - [Message security](#message-security)
  - [Reading privacy](#reading-privacy)
  - [Loading on demand](#loading-on-demand)
  - [Reader actions](#reader-actions)
  - [Conversations](#conversations)
  - [OAuth sign-in](#oauth-sign-in)
  - [Mail operations](#mail-operations)
  - [Composing and sending](#composing-and-sending)
  - [Search](#search)
- [Before reporting results](#before-reporting-results)
- [Security and data protection](#security-and-data-protection)
  - [Recommendations](#recommendations)
  - [Safe operating guidance](#safe-operating-guidance)

## Current limitations

The overview lists each area once; the sections below hold the details. Each
section starts with its `[caveat]` issue, the issue's board status and the
issue comment that holds the current recommendation.

| Kind                      | Area              | Current constraint                                                                                                                                                                                                                                                   | Caveat                                                                                                                                       |
| ------------------------- | ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| ![Resolved][resolved]     | Accounts          | One window holds several IMAP accounts, grouped by account in the folder sidebar; the limitations recorded for them are resolved ([details](#multiple-accounts))                                                                                                     | [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262)                                                                        |
| ![Limitation][limitation] | Embedded panels   | Related messages search only loaded messages unless the bounded server search is turned on ([details](#related-messages-and-message-details-panels)); message details do not verify inline PGP ([details](#message-security))                                        | [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264), [#442](https://github.com/mikepadiernos/plasma-impresario/issues/442) |
| ![Limitation][limitation] | Reading privacy   | Link warnings are heuristic and remote content consent applies to the open message only ([details](#reading-privacy))                                                                                                                                                | [#443](https://github.com/mikepadiernos/plasma-impresario/issues/443)                                                                        |
| ![Limitation][limitation] | Loading on demand | Parts that failed after a server error load again only on Try Again; the unavailable-message page is not verified live ([details](#loading-on-demand))                                                                                                               | [#444](https://github.com/mikepadiernos/plasma-impresario/issues/444)                                                                        |
| ![Limitation][limitation] | Reader actions    | The action bar is in the main window only, zoom is not saved, and Archive and Trash from the bar are not verified live ([details](#reader-actions))                                                                                                                  | [#445](https://github.com/mikepadiernos/plasma-impresario/issues/445)                                                                        |
| ![Limitation][limitation] | Conversations     | A search in a threaded list splits threads, conversations cover the loaded messages of the current folder only, and the Unified Inbox has no conversation steps ([details](#conversations))                                                                          | [#449](https://github.com/mikepadiernos/plasma-impresario/issues/449)                                                                        |
| ![Limitation][limitation] | OAuth sign-in     | Microsoft uses the `common` tenant only and cannot revoke grants from the app, IMAP and SMTP refresh tokens independently, and switching provider replaces a grant without revoking it ([details](#oauth-sign-in))                                                   | [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362)                                                                        |
| ![Limitation][limitation] | Mail operations   | Special folders need server attributes or English names, Go to Folder and related searches use loaded data only, and Undo after a move needs server COPYUID support ([details](#mail-operations))                                                                    | [#312](https://github.com/mikepadiernos/plasma-impresario/issues/312)                                                                        |
| ![Limitation][limitation] | Composing         | Each refused send files another copy in Sent, a forward sent before its message downloads is refused, forwards are attachment-only and drafts are local without attachments ([details](#composing-and-sending))                                                      | [#450](https://github.com/mikepadiernos/plasma-impresario/issues/450)                                                                        |
| ![Limitation][limitation] | Search            | Date ranges follow the day in each message's Date header, in the sender's time zone, and a search of several folders selects each folder of the account in turn and loads every result's headers; `has:attachment` approximates by message type ([details](#search)) | [#454](https://github.com/mikepadiernos/plasma-impresario/issues/454)                                                                        |
| ![Limitation][limitation] | Product identity  | The product is renamed to Plasma Impresario ([#221](https://github.com/mikepadiernos/plasma-impresario/issues/221)), but the application remains based on Trojitá and the product redesign is not complete                                                           | —                                                                                                                                            |
| ![Limitation][limitation] | Message rendering | Qt WebEngine is required for the default reader; a renderer-free build cannot display message contents                                                                                                                                                               | —                                                                                                                                            |
| ![Security][security]     | Credentials       | QtKeychain is optional; new profiles default to it, but profiles with saved cleartext passwords keep the cleartext plugin, which stores secrets in `QSettings` ([details](#security-and-data-protection))                                                            | —                                                                                                                                            |
| ![Security][security]     | Transport         | Cleartext IMAP remains selectable; account configuration determines whether TLS is used ([details](#security-and-data-protection))                                                                                                                                   | —                                                                                                                                            |
| ![Security][security]     | Local data        | Persistent SQL cache stores message metadata and fetched parts; the application does not encrypt cache contents ([details](#security-and-data-protection))                                                                                                           | —                                                                                                                                            |
| ![Planned][planned]       | Integrations      | Plasma new-mail notifications, session integration, Gmail/Outlook APIs, Nextcloud, rules and cross-app workflows remain planned                                                                                                                                      | —                                                                                                                                            |
| ![Limitation][limitation] | Compatibility     | The [baseline](philosophy/compatibility.md) records an initial CachyOS target and partial build evidence; no full reader or desktop-session support matrix has been verified                                                                                         | —                                                                                                                                            |
| ![Limitation][limitation] | Quality evidence  | Large-mailbox benchmarks and broad accessibility, screen-reader, high-DPI and RTL verification are not recorded; nothing has been checked in a real Plasma session yet                                                                                               | —                                                                                                                                            |

### Multiple accounts

| Caveat issue                                                          | Board             | Recommendation                                                                                         | Verified with                                                              |
| --------------------------------------------------------------------- | ----------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------- |
| [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262) | ![Merged][merged] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/262#issuecomment-7142887185) | Disposable Dovecot, the opt-in `test_LiveImapAccounts` and offscreen tests |

All five limitations recorded in #262 are resolved, and the issue is closed.

**Current behavior:**

- ![Resolved][resolved] Errors from background accounts appear in the account
  selector, on the account's header in the folder sidebar, and on a footer
  button that switches to the first flagged account. Each failure also raises
  one desktop notification, with an **Open Account** action, until the account
  connects again. Builds without KNotifications show it as a tray message
  instead, but only while the tray icon is visible.
- ![Resolved][resolved] Removing an account deletes its settings, stored
  passwords and offline cache. For the account migrated from a single-account
  profile, whose cache shares the cache root, only its database and
  message-part directories are deleted; drafts and other accounts' caches are
  kept.
- ![Resolved][resolved] Accepting an untrusted TLS certificate records the host
  name and the SHA-256 fingerprint of the certificate's public key for the
  whole profile. Other accounts on that host, and the host's other service
  (IMAP or SMTP), accept the same key without asking. A different key, or the
  same key for another host name, still prompts.
- ![Resolved][resolved] A composer sends and files the message with the
  settings of the account it was opened from, even after switching accounts.
  If that account is removed, sending is refused with an error and the composer
  stays open.
- ![Resolved][resolved] Each account's header in the folder sidebar has a menu
  with **Account Settings…**, **Rename…** and **Remove…**. **Account
  Settings…** switches to that account first, so the dialog and the window show
  the same account; renaming and removing don't switch.

The notification popup and its action have not yet been checked in a real
Plasma session; that check is
[#315](https://github.com/mikepadiernos/plasma-impresario/issues/315) in
[M15](ROADMAP.md#m15-real-plasma-session-verification).

### Related messages and message details panels

| Caveat issue                                                          | Board             | Recommendation                                                                                         | Applies to                                                         |
| --------------------------------------------------------------------- | ----------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------ |
| [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264) | ![Review][review] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/264#issuecomment-7143646817) | Panels off by default; enable them from **View › Embedded Panels** |

**Current behavior:**

- ![Limitation][limitation] By default the related messages panel only
  searches messages already loaded in the current folder's message list. It
  lists at most 25 messages per group (conversation and same sender) and scans
  at most the 20,000 most recent rows.
- ![Resolved][resolved] **Search the Server**, in the panel's search menu, also
  asks the server for the conversation (`Message-ID`, `In-Reply-To` and
  `References`) and the sender's other mail, in the message's folder, every
  folder of the account or chosen folders. Results merge with the loaded ones,
  rows from other folders start with the folder name, and opening one shows it
  in the reader without changing the message list. The choice and the chosen
  folders are remembered
  ([#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)).
- ![Limitation][limitation] The server search is bounded: **All Folders**
  searches at most 20 folders, and only the newest 50 matches in each folder
  are loaded. The status line says when the limits left something out.
- ![Limitation][limitation] The server matches header keys as substrings, so
  it may return mail from a similar address or naming a similar Message-ID. The
  panel drops these, but their headers are still downloaded.
- ![Limitation][limitation] The server search uses the account's own
  connection and selects each folder in turn; the shown folder is selected
  again when it ends. It waits while a **Search In** search of the same account
  is running and starts once that search finishes or stops. Changing the
  message cancels it.
- ![Resolved][resolved] The message details panel's Security field shows the
  reader's verification and decryption results
  ([#303](https://github.com/mikepadiernos/plasma-impresario/issues/303)).
  Inline PGP and nested signatures remain limited; see
  [Message security](#message-security).
- ![Resolved][resolved] Pane sizes saved by builds before the shell carry over
  the first time each layout opens: the folder tree's width, on the side it was
  saved on, and the split between the message list and the reader. Sizes the
  shell has saved since then take precedence.

**Workarounds:**

- Turn on **Search the Server** with **Chosen Folders** to reach folders that
  **All Folders** leaves out, or use the search bar's **Search In** menu for an
  unbounded search.

**Proposed improvements:**

- Search the other accounts too, for mail that was forwarded between them.
- Use the server's `THREAD` answer, where offered, to find replies that only
  share a subject.

### Message security

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------- |
| [#442](https://github.com/mikepadiernos/plasma-impresario/issues/442) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/442#issuecomment-7174234978) | Messages with inline PGP or several protected parts, with Message Details shown |

**Current behavior:**

- ![Resolved][resolved] The reader verifies and decrypts PGP/MIME and S/MIME
  through GpgME, and the message details panel shows its result: in progress,
  valid and trusted, valid but untrusted, unknown key, invalid, unverifiable,
  decrypted or failed. The panel never starts verification or decryption
  itself.
- ![Limitation][limitation] The reader does not process inline PGP
  (`-----BEGIN PGP SIGNED MESSAGE-----` or `-----BEGIN PGP MESSAGE-----` in the
  body) and shows the armor as text. The details panel recognizes the armor at
  the start of a line in loaded, non-attachment `text/plain` parts and labels
  it _not verified_ or _not decrypted_. It never downloads a part to look for
  armor.
- ![Limitation][limitation] The panel reports the outermost signature and
  encryption only. Signatures inside an attached message or in a later signed
  part appear in the reader only.

**Workarounds:**

- Save an inline PGP message's text and verify or decrypt it with `gpg` or
  Kleopatra.
- Open an attached message to see its own signature in the reader.

**Proposed improvements:**

- Verify and decrypt inline PGP in the reader through GpgME, so that the
  details panel can report the result. Not yet scheduled.
- List each protected part in the Security tooltip when a message has more
  than one.

### Reading privacy

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                           |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------- |
| [#443](https://github.com/mikepadiernos/plasma-impresario/issues/443) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/443#issuecomment-7175141140) | Messages with remote content or links, in the reader |

**Current behavior:**

- ![Resolved][resolved] Remote content stays blocked until **Load Remote
  Content**; the bar names the servers it would contact. Links that look
  deceptive ask before opening
  ([#97](https://github.com/mikepadiernos/plasma-impresario/issues/97)).
- ![Limitation][limitation] Link warnings are heuristics. Sites are compared by
  name, without the public suffix list: a subdomain counts as its parent's
  site.
- ![Limitation][limitation] Links whose text is not an address, such as "Sign
  in", never warn; their target shows only in the hover tooltip.
- ![Limitation][limitation] Newsletters that route links through a tracking
  server while showing the destination as the link text warn on every link.
- ![Limitation][limitation] Consent is all or nothing for the open message and
  is not remembered; there is no per-sender allow list.
- ![Limitation][limitation] The bar lists the servers the page has tried so
  far; content requested later adds its server then.

**Workarounds:**

- Hover over a link to see its target before clicking.
- For a warning caused by a tracking server, check that the named site is the
  sender's mail service before choosing **Open Link**.
- Load remote content only for senders you trust.

**Proposed improvements:**

- Compare sites with the public suffix list.
- Remember remote-content consent per sender, with a way to review and revoke
  it (M14.07).
- Show the target in the status bar as well, for links whose text is not an
  address.

### Loading on demand

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------- |
| [#444](https://github.com/mikepadiernos/plasma-impresario/issues/444) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/444#issuecomment-7175640115) | Messages and parts the reader cannot load ([#94](https://github.com/mikepadiernos/plasma-impresario/issues/94)) |

**Current behavior:**

- ![Resolved][resolved] Parts that cannot be loaded are reported in an error
  bar above the message instead of a blank or a Chromium error page. Offline,
  they load on their own when the network returns (#94).
- ![Limitation][limitation] After a server error, a part loads again only on
  **Try Again** or when the message is reopened.
- ![Limitation][limitation] The page for a message whose structure cannot be
  fetched is covered by code review only; the message list fetches the
  structure first, so no live scenario reaches it yet.
- ![Limitation][limitation] Saving an attachment that cannot be downloaded, for
  example offline, still reports the failure in a dialog.

**Workarounds:**

- Offline, go online to load the missing parts.
- After a server error, use **Try Again** in the bar, or reopen the message.
- To read a folder offline, enable offline caching for the account and open its
  messages while online.

**Proposed improvements:**

- Retry a failed part once on its own after a short delay before asking.
- Report failed attachment downloads in the attachments panel.
- Cover the unavailable page and online refusals with a synthetic server that
  refuses FETCH.

### Reader actions

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                  |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------- |
| [#445](https://github.com/mikepadiernos/plasma-impresario/issues/445) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/445#issuecomment-7176256153) | The reader action bar ([#39](https://github.com/mikepadiernos/plasma-impresario/issues/39)) |

**Current behavior:**

- ![Resolved][resolved] The bar above the reader acts on the open message, not
  the list selection, and Undo returns a moved message to its own folder
  (#39).
- ![Limitation][limitation] Messages opened in their own window have no action
  bar.
- ![Limitation][limitation] Zoom applies to the open message and resets when
  another message opens.
- ![Limitation][limitation] **Prefer Plain Text** is the global reader setting,
  not a per-message choice.
- ![Limitation][limitation] Offline, Archive, Trash, Read and Flag are
  disabled.
- ![Limitation][limitation] Archive and Move to Trash from the bar are covered
  by offscreen tests only; the live test covers the flag, read, attachment,
  zoom and plain-text buttons.

**Workarounds:**

- In a standalone message window, use the main window's message actions.
- After opening another message, zoom again with the bar or the Zoom In and
  Zoom Out shortcuts.

**Proposed improvements:**

- Add the bar to standalone message windows.
- Remember the zoom level.
- Cover Archive and Trash from the bar, with Undo for a message opened from
  Related Messages, in the live test.

### Conversations

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                                                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#449](https://github.com/mikepadiernos/plasma-impresario/issues/449) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/449#issuecomment-7178393442) | Message list filters, quick search and the conversation steps ([#40](https://github.com/mikepadiernos/plasma-impresario/issues/40), [#96](https://github.com/mikepadiernos/plasma-impresario/issues/96), [#369](https://github.com/mikepadiernos/plasma-impresario/issues/369)) |

**Current behavior:**

- ![Resolved][resolved] In a threaded list, the Unread, Flagged and Attachments
  filters keep a matching message's whole thread. The open message stays in
  the list when a filter or the grouping changes (#96).
- ![Resolved][resolved] **Next in Conversation** and **Previous in
  Conversation** follow the thread in a threaded list. In a flat list they step
  through the conversation by date and show messages hidden by a filter (#40,
  #96).
- ![Limitation][limitation] A quick search, or **Show Related**, in a threaded
  list threads only the matching messages, because the server is asked to
  thread the search results. Replies that don't match are not shown with their
  thread.
- ![Resolved][resolved] Replies without In-Reply-To or References join their
  conversation by subject, ignoring localized prefixes and list tags, within 30
  days. Two messages without reply prefixes never join by subject, and a
  message that references two conversations merges them, in threads as in flat
  lists (#369).
- ![Limitation][limitation] A conversation is built from the messages loaded in
  the current folder. Your replies in Sent and messages in other folders are
  not included.
- ![Limitation][limitation] A threaded list merges conversations only among
  messages whose envelopes are loaded, so a merge may appear only after
  scrolling. It does not merge again during a search or after the server
  reports threads incrementally, until the folder is threaded again.
- ![Limitation][limitation] A reply that has a reply prefix but no links can
  still join an unrelated message of the same subject sent within 30 days.
- ![Limitation][limitation] The conversation steps do nothing in the Unified
  Inbox.

**Workarounds:**

- To see a whole thread that a search split, clear the search, open the
  message, and use the conversation steps.
- Open the Sent folder to read your own replies, or turn on **Search the
  Server** in the related messages panel to list them beside the message
  ([#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)).
- To see a merge that has not appeared yet, scroll through the folder, or
  switch Group By to None and back to Conversation.

**Proposed improvements:**

- Thread all messages on the server and filter by the search's results on the
  client, so that threads stay whole during a search.
- Step through conversations in the Unified Inbox within each account.

### OAuth sign-in

| Caveat issue                                                          | Board             | Recommendation                                                                                         | Applies to                                                                                                                   |
| --------------------------------------------------------------------- | ----------------- | ------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------- |
| [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362) | ![Review][review] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/362#issuecomment-7165448590) | Accounts set to OAuth in **Account Settings › IMAP** ([#218](https://github.com/mikepadiernos/plasma-impresario/issues/218)) |

**Current behavior:**

- ![Resolved][resolved] When an OAuth account needs to sign in, the status bar
  shows a **Sign In…** button, the account's sidebar menu has **Sign In…**,
  and the error notification offers **Sign In**. Each opens the browser for
  that account and reconnects it after a successful sign-in, without opening
  Account Settings.
- ![Resolved][resolved] The optional client secret is stored with the grant in
  the password plugin, not in the settings file. A secret saved by an earlier
  build moves there the next time the account connects. The field in Account
  Settings starts empty, and saving is blocked while a typed secret has not
  been used to sign in.
- ![Resolved][resolved] Signing out and removing an account revoke the grant at
  Google before deleting it locally. Removal keeps the grant when another
  account uses the same provider, client and user name.
- ![Resolved][resolved] A grant made in Account Settings is revoked when the
  dialog is cancelled, or saved with a different provider or client.
- ![Security][security] OAuth needs a secure credential store and is refused
  with the cleartext password backend.
- ![Security][security] No client ID is built in unless one is set at build
  time with `IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID` or
  `IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID`.
- ![Limitation][limitation] Microsoft has no revocation endpoint for these
  clients. Signing out and removing the account delete the local grant and
  point to Microsoft's access page.
- ![Limitation][limitation] Signing out in Account Settings revokes the grant
  at Google even when another account shares the same user and client, which
  then needs to sign in again.
- ![Limitation][limitation] Each account stores one grant. Signing in with
  another provider or client replaces the previous grant without revoking it,
  and cancelling the dialog afterwards leaves the account without a grant
  until it signs in again.
- ![Limitation][limitation] Microsoft accounts use the `common` tenant only.
- ![Limitation][limitation] SMTP requests a new access token for every
  message, and IMAP and SMTP refresh independently. A provider that rotates
  refresh tokens can make one of them fail until the next sign-in.
- ![Limitation][limitation] The provider fields are on the IMAP page only;
  SMTP reuses them.
- ![Limitation][limitation] A server that advertises `LOGINDISABLED` still
  needs STARTTLS before OAuth, as with passwords.
- ![Limitation][limitation] Revoking a grant from a cancelled dialog is not
  covered by automated tests, because a sign-in needs the real provider
  endpoints.

**Workarounds:**

- Use QtKeychain, the default for new profiles, before choosing OAuth.
- After signing out of or removing a Microsoft account, remove the app's access
  at [account.live.com/consent/Manage](https://account.live.com/consent/Manage).
- Sign out before switching an account to another provider or client, so the
  old grant is revoked.
- For Microsoft accounts, register your own app in Microsoft Entra ID as
  described under
  [App registration](integrations/microsoft.md#app-registration) and enter its
  client ID. For single-tenant accounts, let the registration accept the
  `common` endpoint, or keep password sign-in where the server allows it.
- Prefer a public client without a secret.

**Proposed improvements:**

- Build a Plasma Impresario Microsoft client ID into release packages (M13.01,
  [#396](https://github.com/mikepadiernos/plasma-impresario/issues/396)).
- Add a tenant choice for Microsoft accounts (M13.01,
  [#397](https://github.com/mikepadiernos/plasma-impresario/issues/397)).
- Share one token session between IMAP and SMTP per account, and reuse the
  access token until it expires (M13.01,
  [#398](https://github.com/mikepadiernos/plasma-impresario/issues/398), for
  Microsoft accounts).
- Keep the previous grant until a dialog that signed in with another provider
  or client is saved, then revoke it (M14.03,
  [#428](https://github.com/mikepadiernos/plasma-impresario/issues/428)).
- Skip revocation when signing out in Account Settings if another account
  shares the grant (M14.03,
  [#429](https://github.com/mikepadiernos/plasma-impresario/issues/429)).
- Verify sign-in and revocation with a real provider account (M15,
  [#317](https://github.com/mikepadiernos/plasma-impresario/issues/317)).

### Mail operations

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                      |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------- |
| [#312](https://github.com/mikepadiernos/plasma-impresario/issues/312) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/312#issuecomment-7148901982) | Folder actions, message list filters and sorting, Unified Inbox |

**Current behavior:**

- ![Limitation][limitation] Special folders (Sent, Drafts, Trash, Junk,
  Archive) are recognized from server SPECIAL-USE attributes or English folder
  names, not localized names. Only the first name match of each kind is used.
  The configured archive folder wins over the server's `\Archive`, and a
  changed name applies after reconnecting.
- ![Limitation][limitation] Go to Folder lists only folders already loaded in
  the folder tree.
- ![Limitation][limitation] Undo after moving to Trash or Archive appears only
  when the server returns `COPYUID` (UIDPLUS); otherwise the notification
  offers to open the folder. Copies cannot be undone. Undo and Empty
  Trash/Junk may briefly select another folder.
- ![Limitation][limitation] Counts in confirmations may be incomplete before
  the folder is synchronized; the Expunge count covers loaded messages only.
- ![Limitation][limitation] Renaming re-lists only the renamed folder and does
  not update the archive or Sent folder settings.
- ![Limitation][limitation] Move To and Copy To share one list of recent
  folders. A folder is added when the operation starts, so a failed operation
  still adds it, and deleted folders stay in the list. The menus show full
  IMAP names, such as `INBOX.Lists.KDE`, rather than a path.
- ![Limitation][limitation] Message list filters are kept per account, not per
  folder; the old global hide-read setting only seeds them. A thread is shown
  whole when any of its messages matches. When messages stop matching, only
  the current message stays visible, not the rest of a multiple selection.
- ![Limitation][limitation] The Flagged and Unread filters hide messages until
  their flags load, so large folders fill in gradually. In "Showing X of Y", X
  also reflects the quick search, and Y is the server's total.
- ![Limitation][limitation] Bulk actions report their result when the command
  is sent, not when the server confirms it.
- ![Limitation][limitation] After messages are removed, the next selection is
  chosen heuristically when the server expunges only some of them, and rows
  hidden by filters or collapsed threads are not considered. The reader keeps
  the old message when the list becomes empty. Failures to mark a message as
  read from the reader are not reported.
- ![Limitation][limitation] The <kbd>?</kbd> shortcut depends on the keyboard
  layout, and the shortcut overview may list stale shortcuts for windows not
  yet opened.
- ![Limitation][limitation] Select Similar and Show Related use loaded
  envelopes only. Mailing-list matches use `List-Post`, because the message
  model exposes no `List-Id`.
- ![Limitation][limitation] While the dense layout is on, its compact density
  and hidden panels are written to the normal preferences, so they persist
  after a restart.
- ![Limitation][limitation] The default sort and threading for new folders
  come from the message list header's state; `guiMsgListShowThreading` is no
  longer written. Group By is not tested against the fake server. A folder's
  saved position is restored only after its message UIDs have loaded.
- ![Limitation][limitation] Two-line message rows show no preview text,
  because message bodies are not fetched for the list, and no contact photos,
  only the sender's initials. In the Unified Inbox, Two Lines names each
  message's account; Columns show it only in the row's tooltip. All rows share
  one height, so date group headings are as tall as a two-line row. The column
  header, with its column chooser and sorting by clicking, is hidden in Two
  Lines.
- ![Limitation][limitation] Check for New Mail asks each online account for
  the counts it already knew and resynchronizes the open folder; it does not
  list new folders or download messages of closed folders. Accounts are
  otherwise refreshed every five minutes, or immediately in the open folder
  when the server supports IDLE.
- ![Limitation][limitation] Account headers show only the INBOX unread count,
  not a total across folders. Go to Next Unread Folder covers the active
  account and folders already listed in the tree; folders whose count has not
  arrived are skipped, so the first use after connecting may skip some.
- ![Limitation][limitation] The Unified Inbox
  ([#223](https://github.com/mikepadiernos/plasma-impresario/issues/223)) lists
  the INBOX of each included account only. It has no threads, Group By, drag
  and drop, quick search or Show Related, and sorts on the client rather than
  on the server. Moving or copying messages of several accounts at once is
  refused. Notifications about different accounts replace each other. An
  account added while the Unified Inbox is shown appears after turning it off
  and on again. Offline accounts still list their cached messages, and the
  status line names them.
- ![Limitation][limitation] These paths are unit-tested but not yet exercised
  in a window against a live server.

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
- In Two Lines, sort from **View › Sorting**, or switch **View › Message Rows**
  to Columns to choose and resize columns.
- Use Reload All Folders to pick up new folders, and expand a branch so that
  Go to Next Unread Folder can visit it.
- In the Unified Inbox, move or copy the messages of one account at a time,
  and open an account's INBOX to thread, group or search its messages.

**Proposed improvements:**

- Recognize localized names and allow assigning special folders per account.
- Query the server from Go to Folder and from related searches.
- Locate moved messages by `Message-ID` when COPYUID is missing.
- Run Empty and Undo on a dedicated connection.
- Follow renamed archive and Sent folders.
- Record recent folders after the operation succeeds, prune deleted ones, and
  show folder paths.
- Report bulk results after the server confirms them.
- Keep the dense layout's preferences separate from the normal ones.
- Offer an opt-in preview line from already downloaded text, contact photos
  from the address book only, and shorter date group headings.
- Let Check for New Mail re-list folders, add per-account and per-folder check
  intervals, and offer an account-wide or all-accounts unread total and
  next-unread search.
- Extend the Unified Inbox to other special folders, threads and quick search
  across accounts, and moves between accounts.
- Add live window-level tests.

### Composing and sending

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                                                                     |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#450](https://github.com/mikepadiernos/plasma-impresario/issues/450) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/450#issuecomment-7179584259) | The composer, replies and forwards, drafts and failed-send recovery ([#41](https://github.com/mikepadiernos/plasma-impresario/issues/41), [#93](https://github.com/mikepadiernos/plasma-impresario/issues/93)) |

**Current behavior:**

- ![Resolved][resolved] New messages, replies, including **Reply to All but
  Me**, and **Forward** send through the account the composer was opened from,
  mark the original as answered or forwarded, and file a copy in Sent (#41,
  #93, [#224](https://github.com/mikepadiernos/plasma-impresario/issues/224)).
- ![Resolved][resolved] A refused send keeps the composer open with the
  server's error, and the message stays in an automatic draft until it is
  sent; **Send** tries again. An automatic draft left by a crash opens again at
  the next start.
- ![Limitation][limitation] The copy for Sent is stored before the outgoing
  server is asked, so every refused attempt leaves another copy in Sent.
- ![Limitation][limitation] Sending a forward before the forwarded message has
  downloaded is refused with "some parts are not available".
- ![Limitation][limitation] **Forward** attaches the original message; there is
  no inline forward.
- ![Limitation][limitation] **Save Draft?** writes a local `.draft` file with
  the recipients, subject and text, but not the attachments.
- ![Limitation][limitation] The live test,
  `tools/dev-container/live-imap-compose.sh`, sends through a local sendmail
  script; SMTP delivery is covered by offscreen tests only.

**Workarounds:**

- After a refused send succeeds, delete the extra copies from Sent.
- If a forward is refused because parts are not available, wait a moment and
  press **Send** again.
- To forward inline, copy the text into a new message.
- Attach files again after opening a draft.

**Proposed improvements:**

- File the Sent copy after the outgoing server accepts the message, or remove
  the copy of a refused attempt.
- Wait for attachments to download when **Send** is pressed, with progress,
  instead of refusing.
- Add an inline forward mode.
- Store drafts in the account's Drafts folder on the server, attachments
  included.
- Exercise SMTP delivery in the live test with a local SMTP server.

### Search

| Caveat issue                                                          | Board               | Recommendation                                                                                         | Applies to                                                                                                                                                    |
| --------------------------------------------------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#454](https://github.com/mikepadiernos/plasma-impresario/issues/454) | ![Backlog][backlog] | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/454#issuecomment-7181065871) | The quick search's query syntax and builder, **Date** and **Search In** menus, saved searches with a date range or places, and searches the server cannot run |

**Current behavior:**

- ![Resolved][resolved] The **Date** menu in the search options limits the
  quick search to today, the past week, month or year, or custom days with an
  optional start and end. The range narrows text, `from:`-style and raw IMAP
  queries, works on its own and is stored with saved searches. Presets stay
  relative and move on when the day changes
  ([#95](https://github.com/mikepadiernos/plasma-impresario/issues/95)).
- ![Resolved][resolved] The **Search In** menu searches the current folder,
  chosen folders of any account, the whole account or all accounts. Results
  appear in one list as each folder is searched, labelled with the account and
  folder, and places that could not be searched are named in the status line
  and the notification bar ([#331](https://github.com/mikepadiernos/plasma-impresario/issues/331)).
- ![Resolved][resolved] A search of several places runs in the background.
  The status line's tooltip shows each account's progress and count, a slow or
  failing account does not hold up the others, and **Stop** ends the search
  while keeping the results found so far; answers that arrive later are not
  shown. **Retry**, in the status row and the notification bar, searches again
  only the accounts and folders that failed, and reconnects an account whose
  connection was lost ([#332](https://github.com/mikepadiernos/plasma-impresario/issues/332)).
- ![Resolved][resolved] The quick search keeps its field toggles, raw IMAP
  queries after `:=` and fuzzy matching, and uses ESEARCH where the server
  advertises it, plain SEARCH otherwise. A search the server cannot run shows
  **Search failed** and the reason in the status line instead of an empty
  list: a key that needs a capability the server does not advertise is named
  before anything is sent, and a refused search repeats the server's own
  explanation ([#333](https://github.com/mikepadiernos/plasma-impresario/issues/333)).
- ![Resolved][resolved] The quick search reads a query syntax: fields such
  as `from:`, `subject:`, `list:`, `tag:`, `is:unread` and `has:attachment`,
  `date:` and `size:` with `<`, `<=`, `>` and `>=`, quoted phrases, `-` or
  `NOT`, `OR` and parentheses. The whole query is checked before anything is
  sent; an invalid one is explained below the search field, with the faulty
  text selected, and the list keeps the previous results
  ([#334](https://github.com/mikepadiernos/plasma-impresario/issues/334)).
- ![Resolved][resolved] **Search Builder...** in the search options and the
  **Mailbox** menu (<kbd>Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd>) builds a
  query from rows of field, comparison and value, matching all or any of them.
  It opens with the current search and shows the query it will run; a query
  it cannot show is explained instead
  ([#335](https://github.com/mikepadiernos/plasma-impresario/issues/335)).
- ![Limitation][limitation] The server compares the calendar day in each
  message's Date header, in the sender's time zone, because IMAP's `SENTSINCE`
  and `SENTBEFORE` disregard the time and zone. The list shows local time, so a
  message sent near midnight in another zone can fall outside a range that its
  listed date is inside, or the other way round.
- ![Limitation][limitation] The Date header is set by the sender and can be
  wrong. Servers differ in how they treat a message without one.
- ![Limitation][limitation] Each account searches its folders one after
  another on its own connection, by selecting each folder. While a search runs,
  the account's selected folder changes; the shown folder is selected again
  when the account's search finishes or is cancelled. An account that is not
  shown stays on the last folder it searched.
- ![Limitation][limitation] The headers of every result are loaded before the
  next folder is searched, so a broad search of many large folders downloads
  many headers and can take a while.
- ![Limitation][limitation] Results show the full mailbox name, such as
  `Archive/2024`, rather than a translated special-folder name. **Selected
  Folders** lists only folders the sidebar has loaded; **Current Account** and
  **All Accounts** search every folder on the server.
- ![Limitation][limitation] **Retry** of an account whose password was rejected
  reports the failure again; it does not ask for a new password. A stopped
  search cannot be resumed, only started again.
- ![Limitation][limitation] The check before sending knows only common
  extension keys, such as `FUZZY`, `OLDER`, `YOUNGER`, `MODSEQ`, `SAVEDBEFORE`
  and `X-GM-RAW`; other keys go to the server, and its answer is shown.
  Dovecot accepts `FUZZY` without advertising `SEARCH=FUZZY`, so the fuzzy
  option stays off there and a raw `FUZZY` query is explained as unsupported.
  A server that advertises `SEARCH=FUZZY` is covered by offline tests only.
- ![Limitation][limitation] IMAP cannot ask for attachments, so
  `has:attachment` finds messages whose top-level type is `multipart/mixed`.
  It misses an attachment inside a signed or encrypted message or a
  `multipart/related` one, and finds a `multipart/mixed` message without a
  real attachment.
- ![Limitation][limitation] Words without quotes must all match, each
  anywhere in the chosen fields; `shared inbox` no longer means the phrase.
  The find bar of an opened message shows only the first subject or body term.
- ![Limitation][limitation] Text before a colon is read as a field name, so
  an unknown one such as in `https://example.org` or `ticket:42` is refused
  instead of searched; `Re:` or `Note:` followed by a space is still text.
  Field names, `is:` values and the upper-case operators `AND`, `OR` and `NOT`
  are English in every language.
- ![Limitation][limitation] The builder has one level of rows. A query with
  groups inside groups or a negated group cannot be opened in it, and a raw
  `:=` query is replaced by the built one. The query text in the dialog is read
  only, and the builder is unavailable while the quick search is disabled.
- ![Limitation][limitation] A builder value with spaces is searched as a
  phrase, unlike the same words typed without quotes in the search field.
- ![Limitation][limitation] Every query is run by the server; there is no
  local index yet (M05.04), so searches need a connection.

**Workarounds:**

- Widen a custom range by a day at either end when a message near a boundary
  is missing, then sort by date.
- To match by arrival time instead, allow raw IMAP search in the search
  options and use `SINCE` or `BEFORE`, which compare the date the server
  received the message.
- Choose a few folders rather than a whole account to keep a search short.
- Expand an account's folders in the sidebar before **Selected Folders** to pick
  subfolders.
- After a rejected password, correct it in the account settings, then press
  **Retry** or **Enter** in the search field.
- When a search is explained as unsupported, leave out the named key or use
  the field search, which every server supports.
- Put a phrase, an address with a colon or a web address in quotation marks,
  such as `"shared inbox"` or `"https://example.org"`.
- To find attachments the `has:` term misses, combine a sender or subject
  with `size>100K`.
- For a query the builder cannot open, type it in the search field, or use
  one builder row per word to match the words anywhere.

**Proposed improvements:**

- Offer a choice between the sent date and the received date.
- Ask the server for one extra day at each end and keep only the messages
  whose local date is inside the range, once their envelopes are loaded or the
  local index (M05.04) has them.
- Search on a separate connection, or with `MULTISEARCH` (RFC 7377) where the
  server offers it, so that the shown folder stays selected.
- Load the headers of results as their rows are shown.
- Offer fuzzy matching on servers that accept `FUZZY` without advertising it,
  once a test search succeeds.
- Show translated special-folder names in results, and list unloaded folders
  in **Selected Folders** on demand.
- Ask for the password again when **Retry** meets a rejected password, and
  offer **Continue** after **Stop** to search the remaining folders.
- Confirm `has:attachment` against each result's body structure, once loaded
  or in the local index.
- Highlight every searched word in the find bar, not only the first.
- Accept translated field names next to the English ones.
- Show nested groups in the builder as indented rows, and let the query text
  be edited there.

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

| Area              | Existing behavior                                                                                                                                                                                                                                                                                                                                  | Limitation                                                                                                                                                                                                               |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| IMAP transport    | Supports cleartext, STARTTLS, dedicated TLS and process-backed connections. Qt certificate errors prompt the user; a previously accepted matching public key can be remembered, and an accepted key is trusted for its host by every account of the profile.                                                                                       | Cleartext is selectable; the active connection method determines whether traffic is encrypted. Certificate decisions and policy need focused regression tests.                                                           |
| SMTP transport    | Supports cleartext, STARTTLS and dedicated TLS. Certificate errors prompt the user in the composer; an accepted public key is remembered per account and forgotten when the SMTP host changes. A key accepted for the same host by any account, over IMAP or SMTP, is accepted without asking.                                                     | Cleartext is selectable. Certificates the system trusts are accepted without remembering their key, so only the system's policy applies to them. There is no way yet to review or forget accepted keys.                  |
| Credentials       | The password-plugin interface supports retrieval, storage and deletion. QtKeychain can provide encrypted storage when built and selected, and is the default for new profiles. Each account keeps its own stored passwords, which are deleted when the account is removed.                                                                         | QtKeychain is optional. Profiles that already have saved cleartext passwords keep the cleartext plugin, which stores IMAP/SMTP secrets in `QSettings`.                                                                   |
| Message rendering | The reader uses a private profile per message, disables scripts, plugins and persistent storage, blocks local-file and remote requests by default, and delegates permitted external links. Remote images require explicit per-message consent. Switching messages destroys the profile, discarding loaded content and cancelling pending requests. | Renderer behavior has not been smoke-tested in a live Plasma session or across the documented distribution matrix.                                                                                                       |
| Cached data       | Persistent SQL cache can hold message metadata and fetched parts; cache directory permissions are restricted on supported Unix systems.                                                                                                                                                                                                            | Application-level cache encryption is not present. Directory permissions do not protect mail from a compromised account or unencrypted disk. Removing an account deletes its cache files without overwriting them first. |
| OpenPGP           | Optional GpgME++/Mimetic MIME handling and decryption tests are present.                                                                                                                                                                                                                                                                           | Availability depends on build options; complete signature-status UX and verification policy are not established.                                                                                                         |
| Diagnostics       | Persistent protocol logging is opt-in at startup; an in-app protocol log is available.                                                                                                                                                                                                                                                             | Treat protocol logs as sensitive and review them before sharing.                                                                                                                                                         |

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

[backlog]: https://img.shields.io/badge/Backlog-6e7781
[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[merged]: https://img.shields.io/badge/Merged-8250df
[planned]: https://img.shields.io/badge/Planned-6e7781
[resolved]: https://img.shields.io/badge/Resolved-1a7f37
[review]: https://img.shields.io/badge/Review-2f81f7
[security]: https://img.shields.io/badge/Security-bf3989
