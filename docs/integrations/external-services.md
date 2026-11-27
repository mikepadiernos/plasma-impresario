# External services and message content

**[Documentation](../README.md)** ·
**[Roadmap: M06](../ROADMAP.md#m06-desktop-integration-and-provider-support)** ·
**[Caveat](../CAVEATS.md#external-services-and-outbound-calls)** ·
**[Integrations](README.md)** · **[KDE Plasma](kde-plasma.md)** ·
**[Mail providers](providers.md)**

This guide defines what Plasma Impresario may send out of the application,
where it goes, and which user action comes first. It covers the network, other
programs on the computer and desktop services. The rules were defined in
**M06.06**
([#106](https://github.com/mikepadiernos/plasma-impresario/issues/106)), and
the register below lists every source file that sends something out, as of
2026-11-27.

Legend: ![Tested][tested] · ![Needs verification][needs-verification] ·
![Limitation][limitation]

> [!IMPORTANT]
>
> Message content means bodies, attachments, subjects, the addresses of
> correspondents and the headers of received mail. It goes only to the
> account's own mail servers. Nothing else receives it without a deliberate
> action by the user.

## Contents

- [Status](#status)
- [Rules](#rules)
- [Register](#register)
- [Adding an outbound call](#adding-an-outbound-call)
- [Verification](#verification)

## Status

| Area                   | Status                                    | Today                                                                                                                                                                                                       |
| ---------------------- | ----------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Rules                  | ![Tested][tested]                         | Defined below; `test_ExternalExposure` fails when a source file sends something out without a register entry, or a register entry no longer does                                                            |
| Remote content         | ![Tested][tested]                         | Remote images load only after **Load Remote Content** for that message; scripts, stylesheets, frames, redirects and other requests never load                                                               |
| Links and list actions | ![Tested][tested]                         | Open only when clicked; deceptive links and mailing list actions ask first and name their target                                                                                                            |
| Attachments            | ![Tested][tested]                         | Handed to local programs only after **Open** or **Save**; attachments of junk mail ask first, and the optional ClamAV scan uses only the local `clamd` ([caveat](../CAVEATS.md#junk-and-attachment-safety)) |
| Account setup          | ![Tested][tested]                         | **Find Settings** sends the address's domain, and the full address only to the domain's own autoconfig server                                                                                               |
| Version details        | ![Limitation][limitation]                 | The IMAP `ID` command and the `User-Agent` header include the version and operating system unless turned off; the application name is always sent                                                           |
| Desktop services       | ![Needs verification][needs-verification] | Notifications, the task manager, Plasma Search, portals and the file manager receive only what the register lists; not checked in a real Plasma session                                                     |

## Rules

1. **Message content goes only to the account's servers.** The IMAP server
   receives what synchronizing, moving, flagging, saving drafts and copying to
   Sent need. The SMTP server, or the configured `sendmail` program, receives
   only the message being sent, after **Send**.
2. **Other network services never receive message content.** A request to any
   other host carries only what the request is for: the domain or address
   during **Find Settings**, the sign-in request during OAuth, the address of
   a link the user opened, or an image the user allowed.
3. **A message cannot contact a server by itself.** Scripts never run.
   Stylesheets, fonts, frames, forms, redirects and other remote requests stay
   blocked. Remote images load only after **Load Remote Content** for that
   message, and the consent ends with the message.
4. **Links open only on request.** A link opens in the browser only when
   clicked. A link whose text names a different site asks first, and mailing
   list actions say where they lead before they open. A `mailto:` link opens a
   new message, which the user checks before sending.
5. **Attachments go only to local programs.** An attachment leaves the
   application only after **Open**, **Save** or a drag. Opened copies are
   user-only, are never executable and are removed after a day. Opening an
   attachment of junk mail asks first. With **Scan attachments with ClamAV
   before opening them** turned on, **Open** first streams the attachment to
   the local `clamd` socket; a threat blocks opening, and a scanner that cannot
   be reached or cannot decide asks first.
6. **Desktop services receive the minimum.** The task manager receives a
   count. Notifications name the account and its error, never a message.
   Plasma Search sees subjects and senders only when the user turns it on. The
   file manager receives a folder path, and the calendar application is only
   started. None of these sends anything off the computer by itself.
7. **Version details are optional.** The application name is always sent in
   the IMAP `ID` command and the `User-Agent` header. Its version and the
   operating system are sent unless **Reveal Plasma Impresario's version and
   platform information** is turned off in the **General** settings.
8. **No hidden services.** There is no telemetry, crash upload, avatar or
   BIMI lookup, link preview, translation or cloud service. The optional
   ClamAV scan talks only to the local `clamd` socket, and moving junk mail is
   an ordinary move on the account's IMAP server.

## Register

Each row names a destination, what it receives, the action that comes first,
and the source files that send it. `test_ExternalExposure` reads the source
files from this section.

| Destination                                   | What it receives                                                                                                                                                                                                                                                           | After                                                                                                                                            | Sources                                                                                             |
| --------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------- |
| The account's IMAP server                     | Mail operations; the `ID` command with the application name, and its version and operating system unless turned off                                                                                                                                                        | Connecting the account                                                                                                                           | `src/Streams/IODeviceSocket.cpp`, `src/Streams/SocketFactory.cpp` (a configured connection command) |
| The account's POP3 server                     | The sign-in; requests for the list of messages and each new message; deletion of downloaded messages when the **After downloading** setting allows it                                                                                                                      | Opening the account or reloading its folders, and the account's automatic checks                                                                 | `src/Pop3/FetchJob.cpp`                                                                             |
| The account's JMAP server                     | The sign-in; requests for the session, folders and messages, and the account's changes to flags, folders and messages; with JMAP sending, the message being sent                                                                                                           | Synchronizing a JMAP account; **Send** for accounts that send through JMAP                                                                       | `src/Jmap/Client.cpp`                                                                               |
| `lsb_release`, on the computer                | Nothing; it reads the operating system's name for the version details                                                                                                                                                                                                      | Building the version details                                                                                                                     | `src/Imap/Model/Utils.cpp`                                                                          |
| The account's SMTP server                     | The message being sent                                                                                                                                                                                                                                                     | **Send**                                                                                                                                         | `src/MSA/SMTP.cpp`, `src/qwwsmtpclient/qwwsmtpclient.cpp`                                           |
| The configured `sendmail` program             | The message being sent                                                                                                                                                                                                                                                     | **Send**, for accounts set to use it                                                                                                             | `src/MSA/Sendmail.cpp`                                                                              |
| Message parts inside the application          | Nothing leaves; `trojita-imap:` and `cid:` requests are answered from the mail model                                                                                                                                                                                       | Showing, saving or opening a part                                                                                                                | `src/Imap/Network/MsgPartNetAccessManager.cpp`, `src/Imap/Network/FileDownloadManager.cpp`          |
| Servers of remote images                      | A request for the image, which tells its server the time and the network address, and may identify the reader                                                                                                                                                              | **Load Remote Content** for that message; everything else stays blocked                                                                          | `src/Gui/WebEngineMessageProfile.cpp`                                                               |
| The default browser or `mailto:` handler      | The link                                                                                                                                                                                                                                                                   | A click; deceptive links and mailing list actions ask first                                                                                      | `src/Gui/EmbeddedWebView.cpp`, `src/Gui/EnvelopeView.cpp`, `src/Gui/OneEnvelopeAddress.cpp`         |
| Autoconfig servers, Thunderbird's ISPDB, DNS  | The domain; the full address only to `autoconfig.` of the address's own domain; test connections without signing in                                                                                                                                                        | **Find Settings**                                                                                                                                | `src/UiUtils/ServerDiscoveryNetwork.cpp`                                                            |
| Google and Microsoft sign-in                  | The client ID, scopes and the address as a login hint in the browser; then codes and tokens                                                                                                                                                                                | **Sign In**, token renewal and signing out                                                                                                       | `src/OAuth/Session.cpp`                                                                             |
| Servers of remote files                       | A download request for the file                                                                                                                                                                                                                                            | Dropping or handing over a remote file to the composer                                                                                           | `src/Gui/RemoteAttachmentFetcher.cpp`                                                               |
| Local programs for attachments                | A private copy of the attachment                                                                                                                                                                                                                                           | **Open**; junk mail asks first                                                                                                                   | `src/Gui/AttachmentView.cpp`, `src/Gui/AttachmentPanel.cpp`                                         |
| The local ClamAV daemon, `clamd`              | The attachment, streamed through a local socket                                                                                                                                                                                                                            | **Open**, when **Scan attachments with ClamAV before opening them** is on                                                                        | `src/UiUtils/ClamdScanner.cpp`                                                                      |
| The file manager and the calendar application | A folder path; a start request; a meeting link                                                                                                                                                                                                                             | **Show in Folder**, **Find Saved Files**, **Open in Calendar**, **Join Meeting**                                                                 | `src/Gui/Window.cpp`, `src/Gui/SavedFilesSearchDialog.cpp`                                          |
| Plasma notifications or the tray              | The account name and its error                                                                                                                                                                                                                                             | A background account needing attention                                                                                                           | `src/Gui/Window.cpp`                                                                                |
| The task manager                              | The unread count of all inboxes                                                                                                                                                                                                                                            | On by default; **Show the unread count on the task manager entry** turns it off                                                                  | `src/UiUtils/DesktopFeedback.cpp`                                                                   |
| XDG portals                                   | File chooser options; a request to start after login                                                                                                                                                                                                                       | **Attach File** and other open dialogs; turning on **Start after login**                                                                         | `src/Gui/FileChooser.cpp`, `src/Gui/StartAfterLogin.cpp`                                            |
| The session bus                               | The application's own interface; Plasma Search results with the subjects and senders of indexed mail; account names, states and unread counts for the Plasma widgets, and the senders or subjects of unread indexed mail only when **Plasma widgets may show** allows them | Another instance starting; Plasma Search, which is off by default; a widget asking, with counts shared by default and **Nothing** turning it off | `src/IPC/DBusInterface.cpp`                                                                         |
| Plasma Impresario, from its Plasma widgets    | Requests for the status, and opening a message, an account, the composer or the application with an activation token                                                                                                                                                       | A widget being shown; a click or key press on it                                                                                                 | `src/PlasmaWidgets/MailStatusClient.cpp`                                                            |

## Adding an outbound call

- Follow the [rules](#rules). Send message content only to the account's own
  servers, and anything else only after an action the user can see coming.
- Add a row to the [register](#register), or the file to an existing row, in
  the same change. Say what is sent, where, and after which action.
- For a desktop service, keep the code in the integration layer; see
  [Service boundaries](kde-plasma.md#service-boundaries).
- Run `ctest -R ExternalExposure`. It names each file that sends something
  out without a register entry, and each entry that no longer does.

## Verification

- ![Tested][tested] `test_ExternalExposure` scans every `.cpp` file under
  `src/` for network requests and connections, local sockets, DNS lookups, URLs and files
  handed to other programs, started processes, D-Bus calls and exported
  objects, and notifications, and compares them with the register. Fixture
  tests confirm that it accepts a matching register, ignores comments, and
  names missing and stale entries.
- ![Tested][tested] `test_WebEngineMessageProfile` checks that scripts,
  stylesheets, frames, redirects and non-image requests stay blocked even
  after consent, and that remote images need it.
- ![Tested][tested] `test_LinkSafety`, `test_LinkTarget` and
  `test_EnvelopeView` cover the link and list action questions;
  `test_MailSafety` and `test_AttachmentPanel` cover opening attachments of
  junk mail.
- ![Tested][tested] `test_ServerDiscovery` checks that Thunderbird's ISPDB
  receives only the domain, and `test_OAuth` checks the sign-in request.
- ![Needs verification][needs-verification] What notifications, Plasma
  Search and the portals show in a real Plasma session belongs to
  [M15](../ROADMAP.md#m15-real-plasma-session-verification).
- ![Limitation][limitation] The check works per file, and libraries such as
  GnuPG follow their own settings. See
  [External services and outbound calls](../CAVEATS.md#external-services-and-outbound-calls).

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[needs-verification]: https://img.shields.io/badge/Needs_verification-0969da
[tested]: https://img.shields.io/badge/Tested-1a7f37
