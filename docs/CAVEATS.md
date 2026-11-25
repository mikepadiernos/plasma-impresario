# Caveats and limitations

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Reported bugs](BUGS.md)** · **[Edge cases](EDGE.md)** ·
**[Pending tasks](TODO.md)** · **[Backend parity](parity/backend.md)** ·
**[UI parity](parity/ui.md)**

This guide records limitations verified in the current source, including
security and privacy behavior. The [backend](parity/backend.md) and
[UI](parity/ui.md) references describe the implemented baseline alongside
recommended parity goals.

Legend: ⚠️ limitation · 🔒 security · 📋 planned

> [!NOTE]
>
> The app can be built with different optional features. Confirm the active
> branch and build configuration before applying a workaround.

## Contents

- [Current limitations](#current-limitations)
  - [Multiple accounts](#multiple-accounts)
  - [Related messages and message details panels](#related-messages-and-message-details-panels)
  - [OAuth sign-in](#oauth-sign-in)
- [Before reporting results](#before-reporting-results)
- [Security and data protection](#security-and-data-protection)
  - [Recommendations](#recommendations)
  - [Safe operating guidance](#safe-operating-guidance)

## Current limitations

| Area                 | Current constraint                                                                                                                                                                                                                                                                                    |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ⚠️ Product identity  | The product is renamed to Plasma Impresario ([#221](https://github.com/mikepadiernos/plasma-impresario/issues/221)), but the application remains based on Trojitá and the product redesign is not complete                                                                                            |
| ⚠️ Accounts          | One window holds several IMAP accounts, grouped by account in the folder sidebar; background errors appear only in the account headers and selector, removing an account keeps its offline cache, and each account confirms the same untrusted certificate separately ([details](#multiple-accounts)) |
| ⚠️ Embedded panels   | Related messages search only loaded messages in the current folder, message details show MIME-level security only, and pane sizes reset once after upgrading ([details](#related-messages-and-message-details-panels))                                                                                |
| ⚠️ OAuth sign-in     | Signing in again is possible only from Account Settings, Microsoft uses the `common` tenant only, grants are not revoked at the provider, and an optional client secret is stored unencrypted ([details](#oauth-sign-in))                                                                             |
| ⚠️ Message rendering | Qt WebEngine is required for the default reader; a renderer-free build cannot display message contents                                                                                                                                                                                                |
| 🔒 Credentials       | QtKeychain is optional; new profiles default to it, but profiles with saved cleartext passwords keep the cleartext plugin, which stores secrets in `QSettings`                                                                                                                                        |
| 🔒 Transport         | Cleartext IMAP remains selectable; account configuration determines whether TLS is used                                                                                                                                                                                                               |
| 🔒 Local data        | Persistent SQL cache stores message metadata and fetched parts; the application does not encrypt cache contents                                                                                                                                                                                       |
| 📋 Integrations      | Plasma notifications, session integration, Gmail/Outlook APIs, Nextcloud, rules and cross-app workflows remain planned                                                                                                                                                                                |
| ⚠️ Compatibility     | The [baseline](philosophy/compatibility.md) records an initial CachyOS target and partial build evidence; no full reader or desktop-session support matrix has been verified                                                                                                                          |
| ⚠️ Quality evidence  | Large-mailbox benchmarks and broad accessibility, screen-reader, high-DPI and RTL verification are not recorded                                                                                                                                                                                       |

### Multiple accounts

| Caveat issue                                                          | Board   | Recommendation                                                                                         | Verified with                                             |
| --------------------------------------------------------------------- | ------- | ------------------------------------------------------------------------------------------------------ | --------------------------------------------------------- |
| [#262](https://github.com/mikepadiernos/plasma-impresario/issues/262) | Backlog | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/262#issuecomment-7142887185) | Disposable Dovecot and the opt-in `test_LiveImapAccounts` |

**Current behavior:**

- ⚠️ Errors from background accounts appear only as a label and tooltip in the
  account selector, and as a warning icon and tooltip on the account's header
  in the folder sidebar, not in the footer or as a desktop notification.
- ⚠️ Removing an account deletes its settings and stored passwords but keeps its
  offline message cache, the `account-N` directory in the application cache
  location.
- ⚠️ Each account asks separately about an untrusted TLS certificate, even on a
  shared server, and separately for its IMAP and SMTP servers. Prompts for
  background accounts appear at startup and name the account; SMTP prompts
  appear in the composer when sending.
- ⚠️ A composer offers the sender identities of the account that was active
  when it opened. When sending, though, it reads the Sent folder and SMTP
  options of the account that is active then. Switching accounts while a
  composer is open can therefore send or file the message with the other
  account's settings.
- ⚠️ **Account Settings…** edits only the active account. There is no way yet to
  open the settings of another account without switching to it.

**Workarounds:**

- Hover over a flagged account in the selector or the folder sidebar to read
  its error, then switch to it to recover.
- After removing an account, delete its cache directory manually.
- Prefer servers with trusted certificates.
- Switch back to the composer's account before sending, or finish the message
  before switching.
- Switch to an account before opening **Account Settings…**.

**Proposed improvements:**

- 📋 Report background errors in the footer and through KNotification.
- 📋 Delete the cache directory when an account is removed (needed before
  release).
- 📋 Share certificate decisions per host and fingerprint across accounts.
- 📋 Bind each composer to the account it was opened from, including its
  sending settings; this is planned with keeping the originating account in
  composition.
- 📋 Offer **Account Settings…** for any account from its header's context menu
  in the folder sidebar, now that accounts are grouped there (#80).

### Related messages and message details panels

| Caveat issue                                                          | Board   | Recommendation                                                                                         | Applies to                                                     |
| --------------------------------------------------------------------- | ------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------- |
| [#264](https://github.com/mikepadiernos/plasma-impresario/issues/264) | Backlog | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/264#issuecomment-7143646817) | Panels off by default; enable them from View › Embedded Panels |

**Current behavior:**

- ⚠️ The related messages panel only searches messages already loaded in the
  current folder's message list, not the server or other folders. It lists at
  most 25 messages per group (conversation and same sender) and scans at most
  the 20,000 most recent rows.
- ⚠️ The message details panel derives its Security field from the top-level
  MIME type (`multipart/signed`, `multipart/encrypted` or S/MIME). It does not
  detect inline PGP or show whether a signature is valid; the reader still
  verifies and decrypts.
- ⚠️ The shell saves pane sizes under new keys, so saved pane sizes reset once
  after upgrading from an earlier build.

**Workarounds:**

- Use the search bar or open the other folder to find related mail that is not
  loaded.
- Rely on the reader's security status for signature and decryption results.
- Resize the panes once after upgrading; the new sizes are saved.

**Proposed improvements:**

- 📋 Add an opt-in server-side search on `Message-ID`, `References` and sender,
  optionally across folders (M04,
  [#302](https://github.com/mikepadiernos/plasma-impresario/issues/302)).
- 📋 Feed the details panel from the reader's verification result, including
  inline PGP (M04,
  [#303](https://github.com/mikepadiernos/plasma-impresario/issues/303)).
- 📋 Migrating the old splitter sizes is optional before release.

### OAuth sign-in

| Caveat issue                                                          | Board   | Recommendation                                                                                         | Applies to                                                  |
| --------------------------------------------------------------------- | ------- | ------------------------------------------------------------------------------------------------------ | ----------------------------------------------------------- |
| [#362](https://github.com/mikepadiernos/plasma-impresario/issues/362) | Backlog | [Issue comment](https://github.com/mikepadiernos/plasma-impresario/issues/362#issuecomment-7165448590) | Accounts set to OAuth in **Account Settings › IMAP** (#218) |

**Current behavior:**

- ⚠️ Signing in is available only from **Account Settings › IMAP**. When the
  stored grant is missing or revoked, the account goes offline with a message;
  the browser is not reopened automatically.
- 🔒 OAuth needs a secure credential store and is refused with the cleartext
  password backend.
- ⚠️ Microsoft accounts use the `common` tenant only.
- 🔒 No client ID is built in unless one is set at build time with
  `IMPRESARIO_OAUTH_GOOGLE_CLIENT_ID` or `IMPRESARIO_OAUTH_MICROSOFT_CLIENT_ID`.
  The optional client secret is stored unencrypted in the account's settings
  file.
- 🔒 Signing out or removing the account deletes the local grant but does not
  revoke it at the provider.
- ⚠️ SMTP requests a new access token for every message, and IMAP and SMTP
  refresh independently. A provider that rotates refresh tokens can make one of
  them fail until the next sign-in.
- ⚠️ The provider fields are on the IMAP page only; SMTP reuses them.
- ⚠️ Cancelling Account Settings after signing in leaves a grant for a client ID
  that was not saved.
- ⚠️ A server that advertises `LOGINDISABLED` still needs STARTTLS before OAuth,
  as with passwords.

**Workarounds:**

- Sign in again from **Account Settings › IMAP** when an OAuth account goes
  offline with a sign-in message.
- Use QtKeychain, the default for new profiles, before choosing OAuth.
- For single-tenant Microsoft accounts, use an app registration that allows the
  `common` endpoint, or keep password sign-in where the server allows it.
- Prefer a public client without a secret; otherwise keep the settings
  directory private.
- After removing an account or signing out, revoke the app's access on the
  provider's account security page.

**Proposed improvements:**

- 📋 Offer **Sign In** from the offline message and the account's sidebar
  header.
- 📋 Add a tenant choice for Microsoft accounts (M13.01,
  [#397](https://github.com/mikepadiernos/plasma-impresario/issues/397)).
- 📋 Store the client secret through the password plugin with the grant.
- 📋 Revoke the grant at the provider when signing out or removing the account.
- 📋 Share one token session between IMAP and SMTP per account, and reuse the
  access token until it expires (M13.01,
  [#398](https://github.com/mikepadiernos/plasma-impresario/issues/398), for
  Microsoft accounts).
- 📋 Revoke a grant made in a dialog that is then cancelled.
- 📋 Verify sign-in with a real provider account
  ([#317](https://github.com/mikepadiernos/plasma-impresario/issues/317), M15).

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

| Area                 | Existing behavior                                                                                                                                                                                                                                                                                                                                  | Limitation                                                                                                                                                                                  |
| -------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 🔒 IMAP transport    | Supports cleartext, STARTTLS, dedicated TLS and process-backed connections. Qt certificate errors prompt the user; a previously accepted matching public key can be remembered.                                                                                                                                                                    | Cleartext is selectable; the active connection method determines whether traffic is encrypted. Certificate decisions and policy need focused regression tests.                              |
| 🔒 SMTP transport    | Supports cleartext, STARTTLS and dedicated TLS. Certificate errors prompt the user in the composer; an accepted public key is remembered per account and forgotten when the SMTP host changes.                                                                                                                                                     | Cleartext is selectable. Certificates the system trusts are accepted without remembering their key, so only the system's policy applies to them.                                            |
| 🔒 Credentials       | The password-plugin interface supports retrieval, storage and deletion. QtKeychain can provide encrypted storage when built and selected, and is the default for new profiles. Each account keeps its own stored passwords, which are deleted when the account is removed.                                                                         | QtKeychain is optional. Profiles that already have saved cleartext passwords keep the cleartext plugin, which stores IMAP/SMTP secrets in `QSettings`.                                      |
| 🔒 Message rendering | The reader uses a private profile per message, disables scripts, plugins and persistent storage, blocks local-file and remote requests by default, and delegates permitted external links. Remote images require explicit per-message consent. Switching messages destroys the profile, discarding loaded content and cancelling pending requests. | Renderer behavior has not been smoke-tested in a live Plasma session or across the documented distribution matrix.                                                                          |
| 🔒 Cached data       | Persistent SQL cache can hold message metadata and fetched parts; cache directory permissions are restricted on supported Unix systems.                                                                                                                                                                                                            | Application-level cache encryption is not present. Directory permissions do not protect mail from a compromised account or unencrypted disk. Removing an account does not delete its cache. |
| 🔒 OpenPGP           | Optional GpgME++/Mimetic MIME handling and decryption tests are present.                                                                                                                                                                                                                                                                           | Availability depends on build options; complete signature-status UX and verification policy are not established.                                                                            |
| 🔒 Diagnostics       | Persistent protocol logging is opt-in at startup; an in-app protocol log is available.                                                                                                                                                                                                                                                             | Treat protocol logs as sensitive and review them before sharing.                                                                                                                            |

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
