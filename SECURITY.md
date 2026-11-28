# Security policy

**[Application](README.md)** · **[Documentation](docs/README.md)** ·
**[Installation](docs/INSTALL.md)** ·
**[Caveats and security](docs/CAVEATS.md#security-and-data-protection)** ·
**[Contributing](CONTRIBUTING.md)** ·
**[Report a vulnerability](https://github.com/mikepadiernos/plasma-impresario/security/advisories/new)**

Plasma Impresario reads untrusted mail, stores account credentials and keeps
copies of messages on disk, so security reports are welcome. This page says
which code receives fixes, how to report a problem privately and where the
current security posture is recorded, as of 2026-11-28.

Legend: ![Supported][supported] · ![Unsupported][unsupported] ·
![Limitation][limitation]

> [!CAUTION]
>
> Plasma Impresario is in development and has had no security audit. The
> [security summary](docs/CAVEATS.md#security-and-data-protection) is a
> source-based description of current behavior, not a guarantee.

## Contents

- [Supported versions](#supported-versions)
- [Reporting a vulnerability](#reporting-a-vulnerability)
- [Scope](#scope)
- [Security posture](#security-posture)
- [Safe operating guidance](#safe-operating-guidance)

## Supported versions

There are no Plasma Impresario releases yet. Fixes land on `main`, and users
build from source with [`install.sh`](docs/INSTALL.md).

| Version                                    | Status                      | Notes                                                                   |
| ------------------------------------------ | --------------------------- | ----------------------------------------------------------------------- |
| `main`                                     | ![Supported][supported]     | Receives every fix. Update with `git pull` and `./install.sh`.          |
| Milestone branches (`feature/…`, `fix/…`)  | ![Unsupported][unsupported] | Work in progress; fixes reach them only when they merge `main`.         |
| Trojitá releases (`v0.7` and earlier tags) | ![Unsupported][unsupported] | Inherited history. Report problems that still affect `main` here.       |
| Builds on other desktops, macOS or Windows | ![Unsupported][unsupported] | Outside the [supported platforms](docs/INSTALL.md#supported-platforms). |

## Reporting a vulnerability

> [!IMPORTANT]
>
> Do not open a public issue, pull request or discussion for a suspected
> vulnerability.

1. Open a
   [private vulnerability report](https://github.com/mikepadiernos/plasma-impresario/security/advisories/new)
   (repository **Security** tab, **Report a vulnerability**). Only the
   maintainer and people you invite can see it.
2. Describe the affected component, the commit you built (`git rev-parse
   HEAD`), the build options and the [platform](docs/INSTALL.md#supported-platforms).
3. Give the smallest reproduction you can. Use an invented message or an
   isolated test account; never send real mail, passwords, tokens or
   unredacted protocol logs.
4. Say whether the problem is already public and whether you want credit.

The maintainer works on the project alone, so response times vary. Expect an
acknowledgement in the report, a fix on `main`, then a published advisory
that credits the reporter if they agree. Problems in Qt, KDE Frameworks,
GpgME or other dependencies belong with those projects; the report can still
track the Plasma Impresario side, such as a workaround or a version
requirement.

Hardening ideas and known limitations that are not exploitable can go in a
normal issue with the `caveat` label; see
[caveats](docs/CAVEATS.md).

## Scope

| In scope                                                                                    | Out of scope                                                                                     |
| ------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ |
| Message rendering, remote content blocking and link handling in the reader                  | Vulnerabilities in Qt, QtWebEngine, KDE Frameworks or GpgME themselves (report upstream)         |
| MIME, IMAP, SMTP, POP3 and JMAP parsing of server or message data                           | A compromised local account, root access or an unencrypted disk                                  |
| TLS, STARTTLS and certificate decisions; OAuth sign-in                                      | Mail providers' servers and their policies                                                       |
| Credential storage through the password plugins                                             | The historical Trojitá handbook and man page in [`docs/old`](docs/old/README.md)                 |
| Cache, local mail, opened-attachment and log file permissions                               | Social engineering, such as convincing a user to accept a certificate warning                    |
| OpenPGP and S/MIME results shown to the user                                                | Denial of service that needs a malicious server the user chose to trust, unless it corrupts data |
| `install.sh`, the installed files and the D-Bus, Plasma Search and Plasma widget interfaces |                                                                                                  |

## Security posture

The [security and data protection](docs/CAVEATS.md#security-and-data-protection)
summary describes current behavior and limitations by area. The detailed
caveats behind it are:

| Area                        | Caveat                                                                                                                                                                                                                                                                              |
| --------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Signed and encrypted mail   | [Message security](docs/CAVEATS.md#message-security)                                                                                                                                                                                                                                |
| Remote content and tracking | [Reading privacy](docs/CAVEATS.md#reading-privacy)                                                                                                                                                                                                                                  |
| Sign-in tokens              | [OAuth sign-in](docs/CAVEATS.md#oauth-sign-in)                                                                                                                                                                                                                                      |
| Files on disk               | [Local data and opened attachments](docs/CAVEATS.md#local-data-and-opened-attachments)                                                                                                                                                                                              |
| Outgoing headers            | [Outgoing headers and Bcc](docs/CAVEATS.md#outgoing-headers-and-bcc)                                                                                                                                                                                                                |
| Junk and risky attachments  | [Junk and attachment safety](docs/CAVEATS.md#junk-and-attachment-safety)                                                                                                                                                                                                            |
| Network calls besides mail  | [External services and outbound calls](docs/CAVEATS.md#external-services-and-outbound-calls)                                                                                                                                                                                        |
| Renderer sandbox            | ![Limitation][limitation] Kept on; where Ubuntu 24.04 and later restrict user namespaces, the installer adds an [AppArmor profile](docs/INSTALL.md#renderer-sandbox-on-ubuntu). Untried on Ubuntu; checked in [#512](https://github.com/mikepadiernos/plasma-impresario/issues/512) |

[Where data is stored](docs/INSTALL.md#where-data-is-stored) lists every file
location, for backups and for removing data.

## Safe operating guidance

Follow the [safe operating guidance](docs/CAVEATS.md#safe-operating-guidance)
until the [recommendations](docs/CAVEATS.md#recommendations) are complete. In
short: use TLS with valid certificates, store passwords through QtKeychain
rather than the cleartext plugin, keep remote content blocked, log protocol
traffic only while diagnosing a problem and encrypt the disk that holds the
cache.

[limitation]: https://img.shields.io/badge/Limitation-bc4c00
[supported]: https://img.shields.io/badge/Supported-1a7f37
[unsupported]: https://img.shields.io/badge/Unsupported-6e7781
