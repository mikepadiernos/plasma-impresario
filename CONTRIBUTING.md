# Contributing

**[Documentation](docs/README.md)** · **[Roadmap](docs/ROADMAP.md)** ·
**[Project instructions](CLAUDE.md)**

Contributions are welcome for application code, tests, documentation and bug
reports. Follow the documented milestone and issue workflow before changing
product behavior.

## Development workflow

1. Read the [scope](docs/philosophy/scope.md) and relevant roadmap milestone.
2. Record blockers in [pending tasks](docs/TODO.md) and confirmed defects in
   [reported bugs](docs/BUGS.md).
3. Use a focused feature or fix branch and add or update QtTest coverage.
4. Run the smallest relevant test set, then broader CTest coverage when
   practical.
5. Keep user-visible strings translatable and follow KDE/Plasma conventions.

## Building outside Plasma

Plasma Impresario targets KDE Plasma on Linux. On other hosts, such as macOS,
use the headless Fedora container in `tools/dev-container/` to build and run
the test suite:

```sh
tools/dev-container/run.sh cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
tools/dev-container/run.sh cmake --build build
tools/dev-container/run.sh ctest --test-dir build --output-on-failure
```

Run `tools/dev-container/run.sh` without arguments for a shell, or pass
`--rebuild` to rebuild the image. If a TLS-intercepting proxy is in use, set
`EXTRA_CA_CERT` to its root certificate before the first run.

The container has no Plasma session. Verify desktop integration, including
KWallet, notifications, Akonadi, icon theming, and window behavior, in a real
Plasma session.

### Live mail, screenshots and a VNC view

These scripts start a disposable, loopback-only Dovecot with synthetic
accounts and messages. Its data is deleted when the script ends. Build first.

```sh
tools/dev-container/run.sh sh tools/dev-container/live-imap-accounts.sh
tools/dev-container/run.sh sh tools/dev-container/live-imap-panels.sh
tools/dev-container/vnc.sh
```

- `live-imap-accounts.sh` connects several accounts and switches between
  them.
- `live-imap-panels.sh` seeds a conversation, a message with attachments and
  a signed message, checks the details, contact, related messages and
  attachments panels, and saves layout screenshots to
  `build/shell-screenshots/`.
- `vnc.sh` runs the application on a virtual display with the same seeded
  mailbox. Connect to `vnc://localhost:5901` with the password `synthvnc`;
  the port is published on the host's loopback interface only. Quit the
  application to stop.

The screenshots and the VNC view use plain Qt with the Fusion look, not the
Plasma theme, so spacing, fonts and icons differ from Plasma. In the VNC
view, a newly opened dialog or tooltip can stay black until it is moved; press
Return or Escape to answer a black dialog. The session trusts its own
synthetic server certificate in advance, so no certificate prompt appears.

## Submitting changes

This repository originated in KDE's Trojitá project. Upstream KDE uses merge
requests in [KDE Invent](https://invent.kde.org/pim/trojita); GitHub merge
requests are not the upstream review path.

For a quick introduction to KDE merge requests, see the
[community infrastructure guide](https://community.kde.org/Infrastructure/GitLab).
