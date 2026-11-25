# Mail workflow edge cases

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Reported bugs](BUGS.md)** · **[Caveats](CAVEATS.md)** ·
**[Pending tasks](TODO.md)**

This log captures boundary and failure scenarios that need validation. Record a
reproduced and confirmed defect in [reported bugs](BUGS.md) and a known
limitation in [caveats](CAVEATS.md); keep unconfirmed scenarios here.

Legend: ![Verified][verified] against a synthetic live IMAP server ·
![Offscreen tests][offscreen] covered only by offscreen tests ·
![Limitation][limitation] · ![Not validated][not-validated]

> [!NOTE]
>
> No scenario below has been checked in a real Plasma session. Live checks use
> the synthetic Dovecot servers described in the
> [M02 handoff](TODO.md#m02-shell-and-account-foundation).

## Candidate scenarios

### Mailboxes and synchronization

| Area              | Scenario                                                       | Coverage                        |
| ----------------- | -------------------------------------------------------------- | ------------------------------- |
| Mailboxes         | Empty folders, sparse folders and very large folders           | ![Not validated][not-validated] |
| Account structure | Special folders, provider labels and duplicate folder names    | ![Not validated][not-validated] |
| Synchronization   | Offline retries, expired sessions and partial server responses | ![Not validated][not-validated] |
| Background work   | Cancellation, retry, overlapping sync and process restarts     | ![Not validated][not-validated] |

### Message content and attachments

| Area            | Scenario                                                            | Coverage                                                               |
| --------------- | ------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| Message content | Malformed MIME, missing bodies, invalid dates and unusual encodings | ![Not validated][not-validated]                                        |
| Attachments     | Large files, missing downloads and unsafe types                     | ![Not validated][not-validated]                                        |
| Attachments     | Related parts with a `start` parameter                              | ![Limitation][limitation] The attachments panel assumes the first part |
| Attachments     | Opened files kept in the temp folder                                | ![Not validated][not-validated]                                        |

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
| The background-account notification in a Plasma session                | ![Not validated][not-validated] #315 in M15                                                                                                                                                                 |
| Switching accounts during synchronization                              | ![Not validated][not-validated]                                                                                                                                                                             |
| Saved searches and folder counts after switching                       | ![Verified][verified] `test_LiveImapAccounts`                                                                                                                                                               |
| Mailbox and message selection after switching                          | ![Verified][verified] `test_LiveImapAccounts`                                                                                                                                                               |
| Folder expansion after switching                                       | ![Offscreen tests][offscreen] `test_MainWindowAccounts`                                                                                                                                                     |
| Sender identities after switching                                      | ![Offscreen tests][offscreen] `test_MainWindowAccounts`                                                                                                                                                     |
| An account without identities opens a composer                         | ![Not validated][not-validated]                                                                                                                                                                             |
| An account set to **No identity** replies to a message                 | ![Offscreen tests][offscreen] `test_Composer_responses`; **Reply to All but Me** leaves out the login address, fixed in [#376](BUGS.md#bug-reply-to-all-but-me-keeps-the-users-address-without-an-identity) |
| Switching accounts with a composer open                                | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: sends with the composer's own account ([#262](CAVEATS.md#multiple-accounts))                                                                       |
| Removing the composer's account before sending                         | ![Offscreen tests][offscreen] `test_MainWindowAccounts`: sending is refused and the composer stays open                                                                                                     |
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

| Scenario                                                          | Coverage                                                                                                                       |
| ----------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| The SMTP server refuses the connection                            | ![Offscreen tests][offscreen] `test_SmtpClient`, fixed in [#364](BUGS.md#bug-sending-hangs-when-the-smtp-connection-fails)     |
| The SMTP server drops the connection during the transaction       | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                |
| An implicit TLS connection to a plain-text server                 | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                |
| An untrusted SMTP certificate, accepted or rejected               | ![Offscreen tests][offscreen] `test_SmtpClient`, fixed in [#366](BUGS.md#bug-an-untrusted-smtp-certificate-cannot-be-accepted) |
| An SMTP certificate whose key changed since it was accepted       | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                |
| Cancelling sending while the certificate prompt is open           | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                |
| Answering the composer's certificate prompt against a real server | ![Not validated][not-validated]                                                                                                |
| The server closes the connection right after `QUIT`               | ![Offscreen tests][offscreen] `test_SmtpClient`                                                                                |

### Interface and embedded panels

| Area            | Scenario                                                                               | Coverage                                                                        |
| --------------- | -------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| Interface       | Narrow windows, display scaling, RTL locales and high-contrast modes                   | ![Not validated][not-validated]                                                 |
| Interface       | Upgrading with pane sizes saved before the shell, including a folder tree on the right | ![Offscreen tests][offscreen] `test_MainWindowShell` (compact and wide layouts) |
| Navigation      | <kbd>F6</kbd> focus cycling with hidden or collapsed panes                             | ![Not validated][not-validated]                                                 |
| Navigation      | Vim mode keys typed in the composer, search or address fields                          | ![Not validated][not-validated]                                                 |
| Navigation      | A Plasma Opus keymap that conflicts with the single-key mail shortcuts                 | ![Not validated][not-validated]                                                 |
| Navigation      | <kbd>F6</kbd> focus cycling with the header region shown                               | ![Offscreen tests][offscreen] `test_MainWindowShell`                            |
| Embedded panels | Swapping sidebar sides with panels in both columns                                     | ![Not validated][not-validated]                                                 |
| Embedded panels | One-panel layout with side-column panels                                               | ![Not validated][not-validated]                                                 |
| Embedded panels | Moving a hidden panel                                                                  | ![Not validated][not-validated]                                                 |
| Embedded panels | Empty side columns or footer                                                           | ![Offscreen tests][offscreen] `test_MainWindowDensity`                          |
| Embedded panels | The message model destroyed while a panel shows a message                              | ![Offscreen tests][offscreen] `test_EmbeddedPanels`, `test_AttachmentPanel`     |
| Embedded panels | A message without a Reply-To header (IMAP reports the sender as Reply-To)              | ![Verified][verified] `test_LiveImapPanels`; the duplicate contact is fixed     |
| Embedded panels | A short window with several panels in one column                                       | ![Not validated][not-validated]                                                 |
| Embedded panels | A panel dragged past two thirds of its column (capped when the region is next fitted)  | ![Not validated][not-validated]                                                 |

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
