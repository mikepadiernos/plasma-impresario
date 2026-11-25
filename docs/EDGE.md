# Mail workflow edge cases

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Reported bugs](BUGS.md)** · **[Caveats](CAVEATS.md)** ·
**[Pending tasks](TODO.md)**

This log captures boundary and failure scenarios that need validation. Record a
reproduced and confirmed defect in [reported bugs](BUGS.md) and a known
limitation in [caveats](CAVEATS.md); keep unconfirmed scenarios here.

Legend: ✅ verified against a synthetic live IMAP server · 🧪 covered by
offscreen tests only · ⚠️ known limitation · 📋 not yet validated

> [!NOTE]
>
> No scenario below has been checked in a real Plasma session. Live checks use
> the synthetic Dovecot servers described in the
> [M02 handoff](TODO.md#m02-shell-and-account-foundation).

## Candidate scenarios

### Mailboxes and synchronization

| Area                 | Scenario                                                       | Coverage |
| -------------------- | -------------------------------------------------------------- | -------- |
| 📬 Mailboxes         | Empty folders, sparse folders and very large folders           | 📋       |
| 📬 Account structure | Special folders, provider labels and duplicate folder names    | 📋       |
| 🔄 Synchronization   | Offline retries, expired sessions and partial server responses | 📋       |
| 🔄 Background work   | Cancellation, retry, overlapping sync and process restarts     | 📋       |

### Message content and attachments

| Area               | Scenario                                                            | Coverage                                        |
| ------------------ | ------------------------------------------------------------------- | ----------------------------------------------- |
| ✉️ Message content | Malformed MIME, missing bodies, invalid dates and unusual encodings | 📋                                              |
| 📎 Attachments     | Large files, missing downloads and unsafe types                     | 📋                                              |
| 📎 Attachments     | Related parts with a `start` parameter                              | ⚠️ The attachments panel assumes the first part |
| 📎 Attachments     | Opened files kept in the temp folder                                | 📋                                              |

### Multiple accounts

| Scenario                                                               | Coverage                                                                                                                                                                         |
| ---------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Two accounts on the same server and user                               | 📋                                                                                                                                                                               |
| Two accounts on a server with an untrusted certificate                 | 🧪 `test_TrustedCertificates`: a key accepted for the host is trusted for the whole profile                                                                                      |
| A shared server presents a different key                               | 🧪 `test_TrustedCertificates`: the user is asked again                                                                                                                           |
| Removing the active account                                            | 📋 Offscreen tests remove an inactive account                                                                                                                                    |
| Removing an account deletes its offline cache                          | 🧪 `test_AccountCache` and `test_MainWindowAccounts`                                                                                                                             |
| Removing the migrated first account, whose cache shares the cache root | 🧪 `test_AccountCache`: drafts and other accounts' caches are kept                                                                                                               |
| A background account with a rejected password                          | ✅ `test_LiveImapAccounts`                                                                                                                                                       |
| A background account fails repeatedly before reconnecting              | 🧪 `test_AccountStatus`: one notification until it connects again                                                                                                                |
| The background-account notification in a Plasma session                | 📋 #315 in M15                                                                                                                                                                   |
| Switching accounts during synchronization                              | 📋                                                                                                                                                                               |
| Saved searches and folder counts after switching                       | ✅ `test_LiveImapAccounts`                                                                                                                                                       |
| Mailbox and message selection after switching                          | ✅ `test_LiveImapAccounts`                                                                                                                                                       |
| Folder expansion after switching                                       | 🧪 `test_MainWindowAccounts`                                                                                                                                                     |
| Sender identities after switching                                      | 🧪 `test_MainWindowAccounts`                                                                                                                                                     |
| An account without identities opens a composer                         | 📋                                                                                                                                                                               |
| An account set to **No identity** replies to a message                 | 🧪 `test_Composer_responses`; **Reply to All but Me** leaves out the login address, fixed in [#376](BUGS.md#bug-reply-to-all-but-me-keeps-the-users-address-without-an-identity) |
| Switching accounts with a composer open                                | 🧪 `test_MainWindowAccounts`: sends with the composer's own account ([#262](CAVEATS.md#multiple-accounts))                                                                       |
| Removing the composer's account before sending                         | 🧪 `test_MainWindowAccounts`: sending is refused and the composer stays open                                                                                                     |
| One account: the sidebar shows no account headers                      | 🧪 `test_MainWindowShell`                                                                                                                                                        |
| Collapsing an account's folders, then switching accounts               | 🧪 `test_MainWindowShell`                                                                                                                                                        |
| Removing an account while its sidebar header has focus                 | 📋                                                                                                                                                                               |
| Renaming an account while its sidebar header is shown                  | 📋                                                                                                                                                                               |
| Settings, rename and remove from an account header's menu              | 🧪 `test_MainWindowShell`                                                                                                                                                        |
| A POP3 account hides folder, move, junk and tag actions                | 🧪 `test_MainWindowShell`                                                                                                                                                        |
| An action hidden for another reason stays hidden after switching       | 🧪 `test_MainWindowShell`                                                                                                                                                        |

### OAuth sign-in

| Scenario                                                       | Coverage                                                                                                           |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| Consent denied, cancelled or timed out in the browser          | 🧪 `test_OAuth`                                                                                                    |
| No browser available to open the sign-in page                  | 🧪 `test_OAuth`                                                                                                    |
| A refresh token rotated or rejected by the provider            | 🧪 `test_OAuth`                                                                                                    |
| No secure credential store                                     | 🧪 `test_OAuth`                                                                                                    |
| Two OAuth accounts at the same provider                        | 🧪 `test_OAuth`                                                                                                    |
| SMTP rejects the first access token                            | 🧪 `test_OAuth`                                                                                                    |
| A server that does not offer the OAuth mechanism to SMTP       | 🧪 `test_OAuth`                                                                                                    |
| Signing out or removing the account                            | 🧪 `test_OAuth` revokes at the provider, reports an unreachable endpoint and always deletes the local grant        |
| A provider without a revocation endpoint (Microsoft)           | 🧪 `test_OAuth`; ⚠️ points to the provider's access page ([#362](CAVEATS.md#oauth-sign-in))                        |
| Removing an account that shares its grant with another account | ⚠️ Removal keeps the grant, but signing out in Account Settings revokes it ([#362](CAVEATS.md#oauth-sign-in))      |
| A client secret saved in the settings by an earlier build      | 🧪 `test_OAuth` moves it into the grant; 🧪 `test_AccountSettingsDialog` keeps new secrets out of the settings     |
| Signing in again after the grant is missing or revoked         | 🧪 `test_MainWindowAccounts` offers Sign In in the status bar and the account menu                                 |
| Cancelling Account Settings after signing in                   | ⚠️ The new grant is revoked, but not covered by tests; a replaced grant is lost ([#362](CAVEATS.md#oauth-sign-in)) |
| A grant revoked at the provider while the app is open          | 📋                                                                                                                 |
| Signing in against a real Google or Microsoft account          | 📋                                                                                                                 |

### Microsoft accounts and Exchange

Planned in [M13](ROADMAP.md#m13-microsoft-accounts-and-exchange); see the
[Microsoft guide](integrations/microsoft.md).

| Scenario                                                                            | Coverage |
| ----------------------------------------------------------------------------------- | -------- |
| A tenant that requires admin consent or blocks sign-in through Conditional Access   | 📋       |
| IMAP or SMTP AUTH turned off for the mailbox or tenant                              | 📋       |
| Graph throttling with `Retry-After`, and a delta token the server no longer accepts | 📋       |
| The same message changed through IMAP and Graph at once                             | 📋       |
| An Autodiscover redirect to a host outside the mail domain                          | 📋       |
| A hybrid mailbox moved between Exchange Server and Exchange Online                  | 📋       |
| A shared mailbox without Send As or Send on Behalf permission                       | 📋       |
| Protected (`.rpmsg`) messages, archive mailboxes and public folders                 | 📋       |
| An event in a Windows time zone with no single IANA match                           | 📋       |
| The same calendar reached through Akonadi and Graph                                 | 📋       |

### Sending

| Scenario                                                          | Coverage                                                                                            |
| ----------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| The SMTP server refuses the connection                            | 🧪 `test_SmtpClient`, fixed in [#364](BUGS.md#bug-sending-hangs-when-the-smtp-connection-fails)     |
| The SMTP server drops the connection during the transaction       | 🧪 `test_SmtpClient`                                                                                |
| An implicit TLS connection to a plain-text server                 | 🧪 `test_SmtpClient`                                                                                |
| An untrusted SMTP certificate, accepted or rejected               | 🧪 `test_SmtpClient`, fixed in [#366](BUGS.md#bug-an-untrusted-smtp-certificate-cannot-be-accepted) |
| An SMTP certificate whose key changed since it was accepted       | 🧪 `test_SmtpClient`                                                                                |
| Cancelling sending while the certificate prompt is open           | 🧪 `test_SmtpClient`                                                                                |
| Answering the composer's certificate prompt against a real server | 📋                                                                                                  |
| The server closes the connection right after `QUIT`               | 🧪 `test_SmtpClient`                                                                                |

### Interface and embedded panels

| Area               | Scenario                                                                              | Coverage                                                 |
| ------------------ | ------------------------------------------------------------------------------------- | -------------------------------------------------------- |
| 🖥️ Interface       | Narrow windows, display scaling, RTL locales and high-contrast modes                  | 📋                                                       |
| 🧭 Navigation      | <kbd>F6</kbd> focus cycling with hidden or collapsed panes                            | 📋                                                       |
| 🧭 Navigation      | <kbd>F6</kbd> focus cycling with the header region shown                              | 🧪 `test_MainWindowShell`                                |
| 🧩 Embedded panels | Swapping sidebar sides with panels in both columns                                    | 📋                                                       |
| 🧩 Embedded panels | One-panel layout with side-column panels                                              | 📋                                                       |
| 🧩 Embedded panels | Moving a hidden panel                                                                 | 📋                                                       |
| 🧩 Embedded panels | Empty side columns or footer                                                          | 🧪 `test_MainWindowDensity`                              |
| 🧩 Embedded panels | The message model destroyed while a panel shows a message                             | 🧪 `test_EmbeddedPanels`, `test_AttachmentPanel`         |
| 🧩 Embedded panels | A message without a Reply-To header (IMAP reports the sender as Reply-To)             | ✅ `test_LiveImapPanels`; the duplicate contact is fixed |
| 🧩 Embedded panels | A short window with several panels in one column                                      | 📋                                                       |
| 🧩 Embedded panels | A panel dragged past two thirds of its column (capped when the region is next fitted) | 📋                                                       |

## Validation checklist

- [ ] Confirm the UI remains usable with little or no content.
- [ ] Verify behavior under realistic message and attachment volumes.
- [ ] Check network, credential and authentication recovery.
- [ ] Ensure notifications and threading remain understandable.
- [ ] Isolate test data and record the exact environment for each result.
