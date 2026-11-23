# Caveats and limitations

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Reported bugs](BUGS.md)** · **[Edge cases](EDGE.md)** ·
**[Pending tasks](TODO.md)** · **[Backend parity](parity/backend.md)** ·
**[UI parity](parity/ui.md)**

This guide records limitations verified in the current source, including
security and privacy behavior. The [backend](parity/backend.md) and
[UI](parity/ui.md) references describe the implemented baseline alongside
recommended parity goals.

> [!NOTE]
>
> The app can be built with different optional features. Confirm the active
> branch and build configuration before applying a workaround.

## Current limitations

| Area              | Current constraint                                                                                                                                                           |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Product identity  | The application remains based on Trojitá; the Plasma Impresario product redesign is not complete                                                                             |
| Accounts          | The application instance has one IMAP account model; profiles are separate configurations, not simultaneous account management                                               |
| Message rendering | Qt WebEngine is required for the default reader; a renderer-free build cannot display message contents                                                                       |
| Credentials       | QtKeychain is optional; the cleartext password plugin stores secrets in `QSettings`                                                                                          |
| Transport         | Cleartext IMAP remains selectable; account configuration determines whether TLS is used                                                                                      |
| Local data        | Persistent SQL cache stores message metadata and fetched parts; the application does not encrypt cache contents                                                              |
| Integrations      | Plasma notifications, session integration, Gmail/Outlook APIs, Nextcloud, rules and cross-app workflows remain planned                                                       |
| Compatibility     | The [baseline](philosophy/compatibility.md) records an initial CachyOS target and partial build evidence; no full reader or desktop-session support matrix has been verified |
| Quality evidence  | Large-mailbox benchmarks and broad accessibility, screen-reader, high-DPI and RTL verification are not recorded                                                              |

## Before reporting results

- confirm the desktop environment and display server;
- record the selected account transport and password plugin;
- identify the provider, server capabilities and whether WebEngine or OpenPGP
  support is enabled;
- isolate test credentials and mail data;
- record Qt, KF6, compiler, distribution and build options;
- retain a reproducible mailbox size and sample message set.

## Security and data protection

> [!CAUTION]
>
> This is a source-based status summary, not a security audit. Behavior varies
> with account settings and optional build features.

| Area              | Existing behavior                                                                                                                                                                                                                                                                                                                                  | Limitation                                                                                                                                                     |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| IMAP transport    | Supports cleartext, STARTTLS, dedicated TLS and process-backed connections. Qt certificate errors prompt the user; a previously accepted matching public key can be remembered.                                                                                                                                                                    | Cleartext is selectable; the active connection method determines whether traffic is encrypted. Certificate decisions and policy need focused regression tests. |
| Credentials       | The password-plugin interface supports retrieval and storage. QtKeychain can provide encrypted storage when built and selected.                                                                                                                                                                                                                    | QtKeychain is optional. The cleartext plugin stores IMAP/SMTP secrets in `QSettings`.                                                                          |
| Message rendering | The reader uses a private profile per message, disables scripts, plugins and persistent storage, blocks local-file and remote requests by default, and delegates permitted external links. Remote images require explicit per-message consent. Switching messages destroys the profile, discarding loaded content and cancelling pending requests. | Renderer behavior has not been smoke-tested in a live Plasma session or across the documented distribution matrix.                                             |
| Cached data       | Persistent SQL cache can hold message metadata and fetched parts; cache directory permissions are restricted on supported Unix systems.                                                                                                                                                                                                            | Application-level cache encryption is not present. Directory permissions do not protect mail from a compromised account or unencrypted disk.                   |
| OpenPGP           | Optional GpgME++/Mimetic MIME handling and decryption tests are present.                                                                                                                                                                                                                                                                           | Availability depends on build options; complete signature-status UX and verification policy are not established.                                               |
| Diagnostics       | Persistent protocol logging is opt-in at startup; an in-app protocol log is available.                                                                                                                                                                                                                                                             | Treat protocol logs as sensitive and review them before sharing.                                                                                               |

### Recommendations

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

- Use implicit TLS or required STARTTLS with valid certificates; reject
  certificate warnings unless independently verified with the provider.
- Select a secure credential plugin and avoid saving passwords through the
  cleartext plugin.
- Keep remote message content blocked unless needed for a trusted message.
- Avoid persistent debug logging during ordinary use; sanitize logs before
  attaching them to reports.
- Use operating-system disk encryption when retaining cached mail.
