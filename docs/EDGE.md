# Mail workflow edge cases

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Reported bugs](BUGS.md)** · **[Caveats](CAVEATS.md)** ·
**[Pending tasks](TODO.md)**

This log captures boundary and failure scenarios that need validation. Record
a reproduced and confirmed defect in [reported bugs](BUGS.md); keep
unconfirmed scenarios here.

## Candidate scenarios

| Area              | Scenario                                                                                                                                                                                                              |
| ----------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Mailboxes         | Empty folders, sparse folders and very large folders                                                                                                                                                                  |
| Synchronization   | Offline retries, expired sessions and partial server responses                                                                                                                                                        |
| Message content   | Malformed MIME, missing bodies, invalid dates and unusual encodings                                                                                                                                                   |
| Account structure | Special folders, provider labels and duplicate folder names                                                                                                                                                           |
| Multiple accounts | Two accounts on the same server and user, removing the active account, a background account with a rejected password, switching accounts during synchronization, and saved searches and folder counts after switching |
| Attachments       | Large files, missing downloads and unsupported or unsafe types                                                                                                                                                        |
| Interface         | Narrow windows, display scaling, RTL locales and high-contrast modes                                                                                                                                                  |
| Background work   | Cancellation, retry, overlapping sync and process restarts                                                                                                                                                            |

## Validation checklist

- confirm the UI remains usable with little or no content;
- verify behavior under realistic message and attachment volumes;
- check network, credential and authentication recovery;
- ensure notifications and threading remain understandable;
- isolate test data and record the exact environment for each result.
