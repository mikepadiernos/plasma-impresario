# Mail workflow edge cases

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Pending tasks](TODO.md)** · **[Bugs](BUGS.md)** ·
**[Caveats](CAVEATS.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)**

This log captures boundary and failure scenarios that need validation, with
their test coverage as of 2026-11-25. Record a reproduced and confirmed defect
in [bugs](BUGS.md) and a known limitation in [caveats](CAVEATS.md); keep
unconfirmed scenarios here.

Legend: ![Verified][verified] against a synthetic live IMAP server ·
![Offscreen tests][offscreen] covered only by offscreen tests ·
![Limitation][limitation] known limitation, see [caveats](CAVEATS.md) ·
![Not validated][not-validated] no automated or live check yet

> [!NOTE]
>
> No scenario below has been checked in a real Plasma session. Live checks use
> the synthetic Dovecot servers described in the
> [M02 handoff](TODO.md#m02-shell-and-account-foundation); real-session checks
> are planned in [M15](ROADMAP.md#m15-real-plasma-session-verification).

## Contents

- [Candidate scenarios](#candidate-scenarios)
  - [Mailboxes and synchronization](#mailboxes-and-synchronization)
  - [Message content and attachments](#message-content-and-attachments)
  - [Message security](#message-security)
  - [Multiple accounts](#multiple-accounts)
  - [OAuth sign-in](#oauth-sign-in)
  - [Microsoft accounts and Exchange](#microsoft-accounts-and-exchange)
  - [Sending](#sending)
  - [Mail operations](#mail-operations)
  - [Search](#search)
  - [Interface and embedded panels](#interface-and-embedded-panels)
- [Validation checklist](#validation-checklist)

## Candidate scenarios

### Mailboxes and synchronization

| Area              | Scenario                                                       | Coverage                        |
| ----------------- | -------------------------------------------------------------- | ------------------------------- |
| Mailboxes         | Empty folders, sparse folders and very large folders           | ![Not validated][not-validated] |
| Account structure | Special folders, provider labels and duplicate folder names    | ![Not validated][not-validated] |
| Synchronization   | Offline retries, expired sessions and partial server responses | ![Not validated][not-validated] |
| Background work   | Cancellation, retry, overlapping sync and process restarts     | ![Not validated][not-validated] |

### Message content and attachments

| Area             | Scenario                                                                                                                                                | Coverage                                                                                |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| Message content  | Malformed MIME, missing bodies, invalid dates and unusual encodings                                                                                     | ![Not validated][not-validated]                                                         |
| Attachments      | Large files, missing downloads and unsafe types                                                                                                         | ![Not validated][not-validated]                                                         |
| Attachments      | Related parts with a `start` parameter                                                                                                                  | ![Limitation][limitation] The attachments panel assumes the first part                  |
| Attachments      | Opened files kept in the temp folder                                                                                                                    | ![Not validated][not-validated]                                                         |
| Reader colors    | Plain text, HTML with and without its own background, and forced black on white in dark and light schemes                                               | ![Offscreen tests][offscreen] `test_WebEngineMessageProfile`; message scripts never run |
| Reader colors    | HTML that sets only a light text color on the body, or dark backgrounds only on inner tables                                                            | ![Not validated][not-validated] Shown on a white page in a dark scheme                  |
| Reader colors    | Switching the Plasma color scheme with a message open                                                                                                   | ![Not validated][not-validated] M15                                                     |
| Search highlight | A body search on a live server: the term is found in the reader, the focus stays in the list                                                            | ![Verified][verified] `test_LiveImapPanels`                                             |
| Search highlight | Sender, recipient, list and raw searches without a body key highlight nothing                                                                           | ![Offscreen tests][offscreen] `test_Relevance`                                          |
| Search highlight | The term only in an HTML part's hidden text, in a nested message, or split by markup                                                                    | ![Not validated][not-validated]                                                         |
| Reading privacy  | Link text on another site, user info, IP and punycode hosts, look-alike subdomains                                                                      | ![Offscreen tests][offscreen] `test_LinkSafety`                                         |
| Reading privacy  | Link text reaches the confirmation from an isolated script; message scripts stay disabled                                                               | ![Offscreen tests][offscreen] `test_WebEngineMessageProfile`                            |
| Reading privacy  | Blocked remote image names its server; a deceptive link asks and Cancel keeps it closed; consent ends with the message                                  | ![Verified][verified] `test_LiveImapPanels`                                             |
| Reading privacy  | Links activated from the keyboard, and links in attached messages                                                                                       | ![Not validated][not-validated]                                                         |
| Load on demand   | Retrying a part after the server refused it fetches it again                                                                                            | ![Offscreen tests][offscreen] `test_Imap_BodyParts`                                     |
| Load on demand   | A body never downloaded is reported offline and loads on its own when the network returns                                                               | ![Verified][verified] `test_LiveImapPanels`                                             |
| Load on demand   | A message whose structure cannot be fetched shows the unavailable page and Try Again                                                                    | ![Not validated][not-validated] The list always fetches the structure first             |
| Load on demand   | A server that refuses a part while online, and a connection lost while a part loads                                                                     | ![Not validated][not-validated]                                                         |
| Reader actions   | Buttons follow the marks, the Trash mode and a removed message; requests carry the button state                                                         | ![Offscreen tests][offscreen] `test_ReaderActionBar`                                    |
| Reader actions   | Flag and Read change the open message, not the list selection; the chip shows a hidden Attachments panel; zoom and Prefer Plain Text reload the message | ![Verified][verified] `test_LiveImapPanels`                                             |
| Reader actions   | Archive or Trash, and Undo, for a message opened from Related Messages in another folder                                                                | ![Not validated][not-validated] Undo uses the message's own folder                      |
| Triage           | Space at the end of a message opens the next unread one in the list                                                                                     | ![Verified][verified] `test_LiveImapPanels`                                             |
| Triage           | Space before a long body has loaded, or after the last unread message                                                                                   | ![Not validated][not-validated]                                                         |
| Readability      | Plain text wraps at about 90 characters of its font in a wide reader                                                                                    | ![Offscreen tests][offscreen] `test_WebEngineMessageProfile`                            |
| Readability      | Wide preformatted text, tables drawn with characters, and right-to-left plain text                                                                      | ![Not validated][not-validated]                                                         |

### Message security

| Scenario                                                                       | Coverage                                                                                                                           |
| ------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| Good, bad and unknown-key signatures and encrypted messages in Message Details | ![Offscreen tests][offscreen] `test_Cryptography_PGP` against GpgME, `test_MessageSecurity` without it                             |
| Verification or decryption finishing after the message is shown                | ![Offscreen tests][offscreen] `test_Cryptography_PGP`, `test_EmbeddedPanels`                                                       |
| A signed message's parts unavailable offline                                   | ![Offscreen tests][offscreen] `test_Cryptography_PGP`: shown as could not be verified or decryption failed                         |
| A signed message from a live server                                            | ![Verified][verified] `test_LiveImapPanels`: never shown as verified without a key                                                 |
| A malformed or stripped signature, such as from a mailing list                 | ![Offscreen tests][offscreen] `test_Cryptography_PGP`                                                                              |
| Inline PGP armor in loaded text, in an attachment or quoted mid-line           | ![Offscreen tests][offscreen] `test_MessageSecurity`; ![Limitation][limitation] not verified ([#442](CAVEATS.md#message-security)) |
| A signature inside an attached message or a second signed part                 | ![Limitation][limitation] only the outermost result ([#442](CAVEATS.md#message-security))                                          |
| An expired or revoked signing key, or an S/MIME certificate chain              | ![Not validated][not-validated]                                                                                                    |
| A screen reader announcing the Security row                                    | ![Not validated][not-validated] M15                                                                                                |

### Multiple accounts

| Scenario                                                               | Coverage                                                                                                                                                                                                    |
| ---------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Two accounts on the same server and user                               | ![Not validated][not-validated]                                                                                                                                                                             |
| Two accounts on a server with an untrusted certificate                 | ![Offscreen tests][offscreen] `test_TrustedCertificates`: a key accepted for the host is trusted for the whole profile                                                                                      |
| A shared server presents a different key                               | ![Offscreen tests][offscreen] `test_TrustedCertificates`: the user is asked again                                                                                                                           |
| Removing the active account                                            | ![Not validated][not-validated] Offscreen tests remove an inactive account                                                                                                                                  |
| Removing an account deletes its offline cache                          | ![Offscreen tests][offscreen] `test_AccountCache` and `test_MainWindowAccounts`                                                                                                                             |
| Removing the migrated first account, whose cache shares the cache root | ![Offscreen tests][offscreen] `test_AccountCache`: drafts and other accounts' caches are kept                                                                                                               |
| A background account with a rejected password                          | ![Verified][verified] `test_LiveImapAccounts`                                                                                                                                                               |
| A background account fails repeatedly before reconnecting              | ![Offscreen tests][offscreen] `test_AccountStatus`: one notification until it connects again                                                                                                                |
| The background-account notification in a Plasma session                | ![Not validated][not-validated] [#315](https://github.com/mikepadiernos/plasma-impresario/issues/315) in M15                                                                                                |
| Switching accounts during synchronization                              | ![Not validated][not-validated]                                                                                                                                                                             |
| Saved searches and folder counts after switching                       | ![Verified][verified] `test_LiveImapAccounts`                                                                                                                                                               |
| Mailbox and message selection after switching                          | ![Verified][verified] `test_LiveImapAccounts`                                                                                                                                                               |
| Folder expansion after switching                                       | ![Offscreen tests][offscreen] `test_MainWindowAccounts`                                                                                                                                                     |
| Sender identities after switching                                      | ![Offscreen tests][offscreen] `test_MainWindowAccounts`                                                                                                                                                     |
| An account without identities opens a composer                         | ![Not validated][not-validated]                                                                                                                                                                             |
| An account set to **No identity** replies to a message                 | ![Offscreen tests][offscreen] `test_Composer_responses`; **Reply to All but Me** leaves out the login address, fixed in [#376](BUGS.md#bug-reply-to-all-but-me-keeps-the-users-address-without-an-identity) |
| Switching accounts with a composer open                                | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: sends with the composer's own account ([#262](CAVEATS.md#multiple-accounts))                                                                       |
| Removing the composer's account before sending                         | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: sending is refused and the composer stays open                                                                                                     |
| Replying in the Unified Inbox to a background account's message        | ![Verified][verified] `test_LiveImapAccounts`: the reader names the account, and the reply is sent from it and its identity and marks the message answered                                                  |
| Switching a reply to another account before sending                    | ![Verified][verified] `test_LiveImapAccounts`: sent from the other account's identity; the original is still marked answered on its own account                                                             |
| Choosing an account without an outgoing server in the composer         | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: a warning, and sending is refused with the reason                                                                                                  |
| Typing an identity of another account into **From**                    | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: the composer warns which account it belongs to                                                                                                     |
| A failed send, then reopening its draft from another active account    | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: the composer stays open and the draft reopens with its own account                                                                                 |
| Reopening a draft whose account was removed                            | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: it opens with the active account and says so                                                                                                       |
| Renaming or removing an account while a composer is open               | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: the composer's account list follows, and it warns at once when its own account is removed                                                          |
| One account: the sidebar shows no account headers                      | ![Offscreen tests][offscreen] `test_MainWindowShell`                                                                                                                                                        |
| Collapsing an account's folders, then switching accounts               | ![Offscreen tests][offscreen] `test_MainWindowShell`                                                                                                                                                        |
| Removing an account while its sidebar header has focus                 | ![Not validated][not-validated]                                                                                                                                                                             |
| Renaming an account while its sidebar header is shown                  | ![Not validated][not-validated]                                                                                                                                                                             |
| Settings, rename and remove from an account header's menu              | ![Offscreen tests][offscreen] `test_MainWindowShell`                                                                                                                                                        |
| A POP3 account hides folder, move, junk and tag actions                | ![Offscreen tests][offscreen] `test_MainWindowShell`                                                                                                                                                        |
| An action hidden for another reason stays hidden after switching       | ![Offscreen tests][offscreen] `test_MainWindowShell`                                                                                                                                                        |

### OAuth sign-in

| Scenario                                                       | Coverage                                                                                                                                                             |
| -------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Consent denied, cancelled or timed out in the browser          | ![Offscreen tests][offscreen] `test_OAuth`                                                                                                                           |
| No browser available to open the sign-in page                  | ![Offscreen tests][offscreen] `test_OAuth`                                                                                                                           |
| A refresh token rotated or rejected by the provider            | ![Offscreen tests][offscreen] `test_OAuth`                                                                                                                           |
| No secure credential store                                     | ![Offscreen tests][offscreen] `test_OAuth`                                                                                                                           |
| Two OAuth accounts at the same provider                        | ![Offscreen tests][offscreen] `test_OAuth`                                                                                                                           |
| SMTP rejects the first access token                            | ![Offscreen tests][offscreen] `test_OAuth`                                                                                                                           |
| A server that does not offer the OAuth mechanism to SMTP       | ![Offscreen tests][offscreen] `test_OAuth`                                                                                                                           |
| Signing out or removing the account                            | ![Offscreen tests][offscreen] `test_OAuth` revokes at the provider, reports an unreachable endpoint and always deletes the local grant                               |
| A provider without a revocation endpoint (Microsoft)           | ![Offscreen tests][offscreen] `test_OAuth`; ![Limitation][limitation] points to the provider's access page ([#362](CAVEATS.md#oauth-sign-in))                        |
| Removing an account that shares its grant with another account | ![Limitation][limitation] Removal keeps the grant, but signing out in Account Settings revokes it ([#362](CAVEATS.md#oauth-sign-in))                                 |
| A client secret saved in the settings by an earlier build      | ![Offscreen tests][offscreen] `test_OAuth` moves it into the grant; ![Offscreen tests][offscreen] `test_AccountSettingsDialog` keeps new secrets out of the settings |
| Signing in again after the grant is missing or revoked         | ![Offscreen tests][offscreen] `test_MainWindowAccounts` offers Sign In in the status bar and the account menu                                                        |
| Cancelling Account Settings after signing in                   | ![Limitation][limitation] The new grant is revoked, but not covered by tests; a replaced grant is lost ([#362](CAVEATS.md#oauth-sign-in))                            |
| A grant revoked at the provider while the app is open          | ![Not validated][not-validated]                                                                                                                                      |
| Signing in against a real Google or Microsoft account          | ![Not validated][not-validated]                                                                                                                                      |

### Microsoft accounts and Exchange

Planned in [M13](ROADMAP.md#m13-microsoft-accounts-and-exchange); see the
[Microsoft guide](integrations/microsoft.md).

| Scenario                                                                            | Coverage                        |
| ----------------------------------------------------------------------------------- | ------------------------------- |
| A tenant that requires admin consent or blocks sign-in through Conditional Access   | ![Not validated][not-validated] |
| IMAP or SMTP AUTH turned off for the mailbox or tenant                              | ![Not validated][not-validated] |
| Graph throttling with `Retry-After`, and a delta token the server no longer accepts | ![Not validated][not-validated] |
| The same message changed through IMAP and Graph at once                             | ![Not validated][not-validated] |
| An Autodiscover redirect to a host outside the mail domain                          | ![Not validated][not-validated] |
| A hybrid mailbox moved between Exchange Server and Exchange Online                  | ![Not validated][not-validated] |
| A shared mailbox without Send As or Send on Behalf permission                       | ![Not validated][not-validated] |
| Protected (`.rpmsg`) messages, archive mailboxes and public folders                 | ![Not validated][not-validated] |
| An event in a Windows time zone with no single IANA match                           | ![Not validated][not-validated] |
| The same calendar reached through Akonadi and Graph                                 | ![Not validated][not-validated] |

### Sending

| Scenario                                                                                                               | Coverage                                                                                                                                                                    |
| ---------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| The SMTP server refuses the connection                                                                                 | ![Offscreen tests][offscreen] `test_SmtpClient`, fixed in [#364](BUGS.md#bug-sending-hangs-when-the-smtp-connection-fails)                                                  |
| The SMTP server drops the connection during the transaction                                                            | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                                                             |
| An implicit TLS connection to a plain-text server                                                                      | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                                                             |
| An untrusted SMTP certificate, accepted or rejected                                                                    | ![Offscreen tests][offscreen] `test_SmtpClient`, fixed in [#366](BUGS.md#bug-an-untrusted-smtp-certificate-cannot-be-accepted)                                              |
| An SMTP certificate whose key changed since it was accepted                                                            | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                                                             |
| Cancelling sending while the certificate prompt is open                                                                | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                                                             |
| Answering the composer's certificate prompt against a real server                                                      | ![Not validated][not-validated]                                                                                                                                             |
| The server closes the connection right after `QUIT`                                                                    | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                                                             |
| From the list to a response: open a message, save an attachment, reply to all but me, then forward it as an attachment | ![Verified][verified] `test_LiveImapCompose`: the answered and forwarded flags are set and each copy is filed in Sent                                                       |
| The outgoing server refuses a new message, then accepts it on Send again                                               | ![Verified][verified] `test_LiveImapCompose`: the composer and its autosaved draft are kept; each attempt files a copy in Sent ([caveat](CAVEATS.md#composing-and-sending)) |
| Sending a forward before the forwarded message has downloaded                                                          | ![Limitation][limitation] Refused until it arrives; Send again ([caveat](CAVEATS.md#composing-and-sending))                                                                 |
| Closing an unsent message, saving it as a draft, and opening the draft again                                           | ![Verified][verified] `test_LiveImapCompose`: recipients, subject and text are kept; attachments are not                                                                    |
| The application ends with a composer open                                                                              | ![Verified][verified] `test_LiveImapCompose`: the next start reopens the autosaved message, and sending it removes the autosave                                             |
| Delivery through a real SMTP server                                                                                    | ![Not validated][not-validated] The live test sends through a local sendmail script                                                                                         |

### Mail operations

Each item names an area, its coverage and the scenarios to validate. Results in
parentheses are the expected behavior.

- **Special folders:** ![Not validated][not-validated]. Localized names without
  SPECIAL-USE (e.g. Gesendet), several folders of one kind by name or attribute,
  a server that marks INBOX or a `\Noselect` container as special, a configured
  archive folder that is missing or renamed (applied on reconnect), an
  `INBOX.Sent` listed after a top-level `Sent`, Go to Folder into a collapsed or
  unlisted branch, in the one-panel layout, or with folders hidden by Show Only
  Subscribed, and switching accounts while the folder picker is open.
- **Folder operations:** ![Not validated][not-validated]. Renaming INBOX, a
  `\Noselect` container or a namespace root, a new name containing the separator
  or matching an existing folder, renaming the open folder or its parent,
  renaming the archive or Sent folder (settings keep the old name), renaming
  while offline, and emptying an empty Trash, a Junk folder with an unknown
  count or a very large folder.
- **Deleting:** ![Not validated][not-validated]. No Trash folder, deleting
  inside Trash, Move Deleted Messages to Trash turned off for one account only,
  a server without UIDPLUS for Delete Permanently (messages stay marked until
  Expunge), Delete Permanently on a selection that changes while the question is
  open, and Shift+Delete while the reader or quick search has focus.
- **Undo:** ![Not validated][not-validated]. A server without COPYUID (no Undo,
  Open Trash instead), Undo after the source folder was renamed or removed, Undo
  after switching accounts or going offline, Undo of a large move, and the open
  folder being reselected after Undo or Empty.
- **Confirmations:** ![Not validated][not-validated]. Do not ask again ticked
  and then cancelled (must still ask), the Confirmations group edited by hand,
  Expunge with flags not yet loaded (no count), and a folder or account switch
  while a confirmation is open.
- **Quick actions:** ![Not validated][not-validated]. A recent destination
  deleted or renamed on the server (dropped when a menu opens) or below an
  unloaded branch (kept), names with `&` or commas, the open folder (not
  offered), Move or Copy offline or after the selection changed while the picker
  was open, a server without COPYUID (Open Folder instead of Undo), a failed
  copy, switching accounts during a move, and a configured archive folder beside
  a server `\Archive` folder.
- **Fast processing:** ![Not validated][not-validated]. Show Related on a
  message whose envelope is not loaded yet (actions disabled), a sender or
  subject with quotes or leading spaces, a subject that is only a list tag (tag
  kept) or only prefixes, a message without a `mailto:` List-Post (no list
  action) or with only a List-Id, typed text that starts with `to:` or `from:`
  and is now a field search, a subject with spaces while the server threads with
  THREAD SEARCH, the current message not matching the filter, Select Similar
  with unloaded envelopes (skipped) or no match, Shift+N on the last unread
  message or with Unread hiding read messages, F and B on collapsed threads and
  placeholder rows, and Dense Processing Layout after changing density or panels
  while it is on, across restarts, or with all panels already hidden.
- **Message lists:** ![Not validated][not-validated]. Unread or Flagged filters
  before flags load (hidden) and With Attachments before the structure loads
  (shown), the current message stops matching (kept until another message is
  current) or the current row is hidden by a filter after reselection, a thread
  where only a reply matches, filters combined with quick search, switching
  accounts with different filters, an account without its own filters (former
  Hide Read Messages), Ctrl+A in the reader or search field, Invert Selection
  with collapsed threads, counts not yet known, and bulk marking offline or
  while the selection changes.
- **Selection:** ![Not validated][not-validated]. Removing a selection that ends
  the list (previous message), the whole list (nothing selected), a selection
  the server expunges one message at a time, an expunge by another client of the
  current message inside a multi-selection, a removed current message hidden by
  a filter or inside a collapsed thread, Shift+Page Down from the last row,
  Ctrl+Space with a single selected message, and letters typed while several
  messages are selected (no jump).
- **Grouping:** ![Not validated][not-validated]. Date groups around midnight or
  a week or month boundary while the list stays open (they move only on the next
  regroup), a message dated in the future, a message with several tags or only
  system flags, a favourite tag renamed or removed, the Loading group of
  envelopes which have not arrived, Read State grouping when reading a message
  moves it to Read, Select All or a removal with collapsed groups, a group
  emptied by an expunge, a flagged or tagged message inside a collapsed group,
  sorting by sender while grouped by date, and switching accounts between a
  grouped and a threaded account.
- **Relevance sort:** ![Not validated][not-validated]. Search words found only
  in unloaded messages or only in the body, Show Related by mailing list (no
  words to score), clearing the search while sorting by relevance (previous sort
  returns), a server without SORT (relevance disabled), relevance while
  grouping, and a message which loads after the list was ranked.
- **Folder views:** ![Not validated][not-validated]. A folder renamed or deleted
  on the server or by another client while it keeps a view or a position
  (dropped on the next folder change), a UIDVALIDITY change (position not
  restored), the current message expunged while away (nothing selected, list at
  the newest end), more than 200 folders with their own view (least recently
  used dropped), Use This View for All Folders with the question open while
  switching accounts or folders, a server without SORT or THREAD (stored sort or
  threading refused), relevance sort active when leaving (not stored), a resync
  that replaces every row, new mail at the top of a descending list while
  scrolled down, the current message inside a collapsed group or thread, and
  several accounts sharing the global default.
- **Batch feedback:** ![Not validated][not-validated]. A move or flag change of
  thousands of messages, a task that finishes just after the 400 ms delay, a
  failure after a newer result was shown (newer result stays), switching
  accounts while a busy message is shown, going offline during a batch (reported
  as a failure), and partial failure reported by the server for part of a batch.
- **Message rows:** ![Offscreen tests][offscreen] `test_MessageRows`,
  `test_MainWindowAccounts`; ![Not validated][not-validated] RTL and themes in
  Plasma. Two Lines with a very narrow list (sender, subject and date elided,
  indicators kept), a long sender without spaces, a sender that is only an
  address, symbols or a non-Latin script (initials), a right-to-left locale
  (mirrored row), threads nested deeply enough to squeeze the row, Sent and
  Drafts without recipients (sender shown), clicking the unread dot or flag of a
  message whose flags are not loaded (no toggle), a tag color on a selected row,
  switching to Columns with a saved header layout (restored), the dense
  processing layout turned off after choosing Columns meanwhile (Columns kept),
  and high-contrast or dark themes.
- **Folder sync:** ![Offscreen tests][offscreen] `test_UnreadFolders`,
  `test_MainWindowShell`, `test_MainWindowAccounts`; ![Verified][verified]
  `test_LiveImapAccounts` for background-account counts and Check for New Mail.
  Check for New Mail with every account offline (reported, nothing sent), with a
  background account whose INBOX is not open (count updated), while the open
  folder is still loading, and repeatedly in quick succession; account headers
  before the folder list arrives (name only), with an INBOX that cannot be
  selected, after an account is removed (count forgotten) and with a single
  account (headers hidden); Go to Next Unread Folder with no other unread
  folder, from a folder of another account, past `\Noselect` containers, into
  unlisted branches (skipped) and with counts still loading (skipped).
- **Unified Inbox:** ![Offscreen tests][offscreen] `test_UnifiedMsgListModel`,
  `test_MainWindowAccounts`; ![Not validated][not-validated] live server. An
  account without an INBOX or whose folders have not been listed yet (no rows
  until they are), an account added, renamed or removed while it is shown, every
  account left out but one (last one cannot be unchecked), offline and failed
  accounts (cached rows kept, named in the status line), a selection spanning
  accounts for Move To or Copy To (refused) and for flagging or deleting (split
  per account), Delete Permanently across accounts (one confirmation), Undo
  after switching accounts, sorting by a column while shown, filters set in a
  folder (applied), and restarting while it is shown.
- **Shortcuts:** ![Not validated][not-validated]. The <kbd>?</kbd> shortcut on
  keyboard layouts where it needs Shift or AltGr, shortcuts changed in the
  settings while the overview is open, actions of the standalone message window
  or composer that do not exist yet, two actions given the same shortcut, and
  searching by shortcut text in a translated interface.

### Search

| Area        | Scenario                                                                                                                                                                                                      | Coverage                                                                                                         |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| Date ranges | A custom range narrows the folder, alone and with a text search, on a live server                                                                                                                             | ![Verified][verified] `test_LiveImapAccounts`                                                                    |
| Date ranges | Presets, custom ranges with open ends, raw and `from:`-style queries, saved searches and cancelling the range dialog                                                                                          | ![Offscreen tests][offscreen] `test_SavedSearch` and `test_SearchScope`                                          |
| Date ranges | A message sent near midnight in another time zone, at a range boundary                                                                                                                                        | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |
| Date ranges | A Today or Past Week search left open past midnight moves on to the new day                                                                                                                                   | ![Not validated][not-validated] Covered only by the timer being active                                           |
| Date ranges | A message without a Date header, and servers other than Dovecot                                                                                                                                               | ![Not validated][not-validated]                                                                                  |
| Places      | The shown folder, a whole account, all accounts, chosen folders of two accounts and the Unified Inbox, with a rejected password and a missing folder, on a live server                                        | ![Verified][verified] `test_LiveImapSearch`                                                                      |
| Progress    | A slow server while the others answer, Stop with late answers afterwards, a connection lost mid-search and Retry once it is back, on a live server                                                            | ![Verified][verified] `test_LiveImapSearch`                                                                      |
| Progress    | Retry after a rejected password is corrected in the account settings, and Stop during a single-folder search                                                                                                  | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |
| Places      | Search In choices, saved places and chosen folders, clearing a search and the folder chooser                                                                                                                  | ![Offscreen tests][offscreen] `test_SavedSearch` and `test_SearchScope`                                          |
| Places      | Results labelled with account and folder, merged by date as each folder finishes                                                                                                                              | ![Offscreen tests][offscreen] `test_SearchResultsModel` and `test_UnifiedMsgListModel`                           |
| Places      | Changing folder or account while a search runs, and new mail arriving in a searched folder                                                                                                                    | ![Not validated][not-validated]                                                                                  |
| Support     | Field, raw and refused raw searches, an unadvertised `FUZZY` key and accented text, on Dovecot with and without ESEARCH                                                                                       | ![Verified][verified] `test_LiveImapSearch`                                                                      |
| Support     | Each refusal code, the capability check before sending, and searching again after a failure                                                                                                                   | ![Offscreen tests][offscreen] `test_SearchSupport` and `test_Imap_Threading`                                     |
| Support     | A server that advertises `SEARCH=FUZZY`, and servers other than Dovecot                                                                                                                                       | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |
| Places      | A folder that cannot be selected, thousands of results, and servers other than Dovecot                                                                                                                        | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |
| Query       | Fields, negation, OR, grouping, flags, a `$label1` keyword, `has:attachment`, `list:`, date and size comparisons, quoted phrases and refused queries, on Dovecot with and without ESEARCH                     | ![Verified][verified] `test_LiveImapSearch`                                                                      |
| Query       | Every operator, precedence, quoting and escapes, error positions, date and size limits, keywords sent as atoms, relevance with negated terms and saving an invalid query                                      | ![Offscreen tests][offscreen] `test_SearchQuery`, `test_Relevance`, `test_SavedSearch` and `test_Imap_Threading` |
| Query       | An attachment inside a signed, encrypted or `multipart/related` message, and a `multipart/mixed` message without one                                                                                          | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |
| Query       | A query typed in a non-English locale, a web address or `name:value` text without quotes, and very long or deeply nested queries                                                                              | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |
| Builder     | A match-any query of a sender and a size built in the dialog and run, on Dovecot                                                                                                                              | ![Verified][verified] `test_LiveImapSearch`                                                                      |
| Builder     | Every field's comparisons, values with spaces, reopening the current search, negated comparisons turned around, nested groups and raw queries explained, the disabled Search button and focus on an added row | ![Offscreen tests][offscreen] `test_SearchBuilder` and `test_SavedSearch`                                        |
| Builder     | Many rows in a small window, <kbd>Tab</kbd> order through the rows, a translated interface and screen readers announcing each row                                                                             | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |
| Clear All   | A search within the Unread filter, then a result opened among the results, then a search of the whole account, each cleared in one step with the selection back, on Dovecot                                   | ![Verified][verified] `test_LiveImapSearch`                                                                      |
| Clear All   | The summary's parts and separators, the enabled state of the action, the context menu entry and clearing filters with an empty search                                                                         | ![Offscreen tests][offscreen] `test_SavedSearch` and `test_MainWindowAccounts`                                   |
| Clear All   | Clearing a search and searching again before the server confirms the end of the first search's updates, threaded and not                                                                                      | ![Verified][verified] `test_LiveImapSearch` and `test_Imap_Threading`                                            |
| Clear All   | The Unified Inbox, filters kept from an earlier session, a result opened in another folder, and a summary longer than the list is wide                                                                        | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |
| History     | Remembering, reordering and the limit of 20, invalid queries and searches applied for other reasons left out, per-profile history across restarts, clearing it and turning it off                             | ![Offscreen tests][offscreen] `test_SearchSuggestions` and `test_SavedSearch`                                    |
| Suggestions | Recent searches, `tag:`, contact and folder suggestions for the word being typed, late contact answers, <kbd>Enter</kbd>, <kbd>Esc</kbd> and <kbd>Down</kbd>, and turning suggestions off                     | ![Offscreen tests][offscreen] `test_SearchSuggestions` and `test_SavedSearch`                                    |
| Suggestions | A real address book, folders of other accounts or not yet listed, input methods, screen readers and a real Plasma session                                                                                     | ![Not validated][not-validated] ([caveat](CAVEATS.md#search))                                                    |

### Interface and embedded panels

| Area             | Scenario                                                                                                                                 | Coverage                                                                                                                       |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| Interface        | Narrow windows, display scaling, RTL locales and high-contrast modes                                                                     | ![Not validated][not-validated]                                                                                                |
| Interface        | Upgrading with pane sizes saved before the shell, including a folder tree on the right                                                   | ![Offscreen tests][offscreen] `test_MainWindowShell` (compact and wide layouts)                                                |
| Navigation       | <kbd>F6</kbd> focus cycling with hidden or collapsed panes                                                                               | ![Not validated][not-validated]                                                                                                |
| Navigation       | <kbd>F6</kbd> focus cycling with the header region shown                                                                                 | ![Offscreen tests][offscreen] `test_MainWindowShell`                                                                           |
| Navigation       | <kbd>F6</kbd> order with the folder tree on the right and in right-to-left layouts                                                       | ![Offscreen tests][offscreen] `test_MainWindowAccounts`                                                                        |
| Navigation       | <kbd>F6</kbd> order with panels in the right column and the footer                                                                       | ![Verified][verified] `test_LiveImapPanels`                                                                                    |
| Navigation       | <kbd>Escape</kbd> from the reader or a panel back to the message list                                                                    | ![Offscreen tests][offscreen] `test_MainWindowAccounts`, ![Verified][verified] `test_LiveImapPanels`                           |
| Navigation       | The Menu key and single-key triage in the message list                                                                                   | ![Verified][verified] `test_LiveImapPanels` (the Menu key's context event is sent directly offscreen)                          |
| Navigation       | <kbd>Alt</kbd>+<kbd>Down</kbd> and <kbd>Alt</kbd>+<kbd>Up</kbd> through a collapsed thread, oldest to newest                             | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedMessages`                              |
| Navigation       | Conversation steps in a flat list, through a reply under another subject linked by Message-ID                                            | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedMessages`                              |
| Navigation       | Conversation steps in a thread whose first message is missing or in a grouped list                                                       | ![Offscreen tests][offscreen] `test_RelatedMessages`                                                                           |
| Navigation       | Conversation steps in a flat list under the Flagged filter reach and then re-hide unflagged replies                                      | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedMessages`, `test_PrettyMsgListFilters` |
| Navigation       | Turning a filter on or switching between threaded and flat lists keeps the current message current and visible                           | ![Verified][verified] `test_LiveImapPanels`                                                                                    |
| Navigation       | A quick search in a threaded list splits a thread whose replies don't match                                                              | ![Not validated][not-validated] ([caveat](CAVEATS.md#conversations))                                                           |
| Navigation       | Conversation steps in the Unified Inbox do nothing                                                                                       | ![Not validated][not-validated]                                                                                                |
| Navigation       | Two unrelated "Hello" messages without reply prefixes stay apart in threads and Related Messages                                         | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedMessages`                              |
| Navigation       | Replies without links ("AW: [team] Status", "Re[2]: Status") join the thread of their subject within 30 days                             | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedMessages`                              |
| Navigation       | A reply without links more than 30 days from the rest of its subject stays apart                                                         | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedMessages`                              |
| Navigation       | A message whose References name two conversations merges them                                                                            | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedMessages`                              |
| Navigation       | A threaded list merges a conversation once the envelopes scrolled into view load                                                         | ![Not validated][not-validated] ([caveat](CAVEATS.md#conversations))                                                           |
| Navigation       | Subject prefixes in other scripts (回复:, Отв:, ΑΠ:) and full-width colons                                                               | ![Offscreen tests][offscreen] `test_RelatedMessages`                                                                           |
| Navigation       | Vim mode keys typed in the composer, search or address fields                                                                            | ![Not validated][not-validated]                                                                                                |
| Navigation       | A Plasma Opus keymap that conflicts with the single-key mail shortcuts                                                                   | ![Not validated][not-validated]                                                                                                |
| Embedded panels  | Swapping sidebar sides with panels in both columns                                                                                       | ![Not validated][not-validated]                                                                                                |
| Embedded panels  | One-panel layout with side-column panels                                                                                                 | ![Not validated][not-validated]                                                                                                |
| Embedded panels  | Moving a hidden panel                                                                                                                    | ![Not validated][not-validated]                                                                                                |
| Embedded panels  | Empty side columns or footer                                                                                                             | ![Offscreen tests][offscreen] `test_MainWindowDensity`                                                                         |
| Related messages | Server search finds the conversation and the sender's mail in another folder, labelled with the folder, and the list stays on its folder | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedSearch`                                |
| Related messages | Server results that duplicate loaded messages, and a sender match from a different address that only contains the sender's               | ![Verified][verified] `test_LiveImapPanels`, ![Offscreen tests][offscreen] `test_RelatedSearch`                                |
| Related messages | Opening a result from another folder whose UID is also used in the shown folder                                                          | ![Verified][verified] `test_LiveImapPanels`                                                                                    |
| Related messages | Chosen folders that include a folder deleted on the server                                                                               | ![Verified][verified] `test_LiveImapPanels` (the failure is named, other results stay)                                         |
| Related messages | Changing the message while a server search runs                                                                                          | ![Verified][verified] `test_LiveImapPanels` (old results are never shown)                                                      |
| Related messages | An account with more than 20 folders or a folder with more than 50 matches                                                               | ![Not validated][not-validated] ([caveat](CAVEATS.md#related-messages-and-message-details-panels))                             |
| Related messages | A server search requested while a **Search In** search of the same account runs                                                          | ![Not validated][not-validated] The panel waits and starts when that search ends                                               |
| Embedded panels  | The message model destroyed while a panel shows a message                                                                                | ![Offscreen tests][offscreen] `test_EmbeddedPanels`, `test_AttachmentPanel`                                                    |
| Embedded panels  | A message without a Reply-To header (IMAP reports the sender as Reply-To)                                                                | ![Verified][verified] `test_LiveImapPanels`; the duplicate contact is fixed                                                    |
| Embedded panels  | A short window with several panels in one column                                                                                         | ![Not validated][not-validated]                                                                                                |
| Embedded panels  | A panel dragged past two thirds of its column (capped when the region is next fitted)                                                    | ![Not validated][not-validated]                                                                                                |

## Validation checklist

- [ ] Confirm the UI remains usable with little or no content.
- [ ] Verify behavior under realistic message and attachment volumes.
- [ ] Check network, credential and authentication recovery.
- [ ] Ensure notifications and threading remain understandable.
- [ ] Isolate test data and record the exact environment for each result.

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[not-validated]: https://img.shields.io/badge/Not_validated-6e7781
[offscreen]: https://img.shields.io/badge/Offscreen_tests-0969da
[verified]: https://img.shields.io/badge/Verified-1a7f37
