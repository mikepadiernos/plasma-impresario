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

| Scenario                                         | Coverage                                                                           |
| ------------------------------------------------ | ---------------------------------------------------------------------------------- |
| Two accounts on the same server and user         | 📋                                                                                 |
| Removing the active account                      | 📋 Offscreen tests remove an inactive account                                      |
| A background account with a rejected password    | ✅ `test_LiveImapAccounts`                                                         |
| Switching accounts during synchronization        | 📋                                                                                 |
| Saved searches and folder counts after switching | ✅ `test_LiveImapAccounts`                                                         |
| Folder expansion after switching                 | 🧪 `test_MainWindowAccounts`                                                       |
| Sender identities after switching                | 🧪 `test_MainWindowAccounts`                                                       |
| An account without identities opens a composer   | 📋                                                                                 |
| Switching accounts with a composer open          | ⚠️ Sends with the active account's settings ([#262](CAVEATS.md#multiple-accounts)) |

### Interface and embedded panels

| Area               | Scenario                                                                              | Coverage                                                 |
| ------------------ | ------------------------------------------------------------------------------------- | -------------------------------------------------------- |
| 🖥️ Interface       | Narrow windows, display scaling, RTL locales and high-contrast modes                  | 📋                                                       |
| 🧭 Navigation      | <kbd>F6</kbd> focus cycling with hidden or collapsed panes                            | 📋                                                       |
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
