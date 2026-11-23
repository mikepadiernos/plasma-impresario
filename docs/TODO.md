# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)** ·
**[Bugs](BUGS.md)** · **[Caveats](CAVEATS.md)**

This log records unfinished work and resumable handoffs. Confirm current issue
and project status before acting.

## Backlog

- [ ] Validate the [initial compatibility target](philosophy/compatibility.md) with a
  complete reader and desktop session before declaring release support
- [ ] Define the minimum viable account and mailbox acceptance scenarios
- [ ] Verify the main window, message list and preview layout in an end-to-end flow
- [ ] Add implementation-backed acceptance criteria to planned notification,
  search and mailbox-organization features

## In progress

- Review the documentation structure and keep milestone status synchronized
  with project-board decisions.
- M01 installer: package-manager selection now follows distribution identity
  and fails for unsupported or missing managers instead of selecting an
  unrelated one. The installer supports dependency-only checks, build-only
  builds and explicit renderer selection. Its mock suite and real temporary-prefix
  renderer-free/WebEngine installations passed on CachyOS. Package lists and
  real installations still need verification across supported distribution families.
- M01.02 renderer: merged and complete. The WebEngine reader is integrated with
  MIME parts, alternatives, attachments, headers, source, asynchronous quoting
  and search. Issues #33 and #65-#71 are Merged.
- M01 verification: on CachyOS x86_64 with Qt 6.12.0, CMake 4.5.1 and GCC
  16.2.1, a renderer-free build and user-prefix install succeeded; the
  installed executable's `--help`, handbook, man page, desktop entry and
  translations were present. A renderer-enabled Debug build using Qt 6.12.0 and
  Qt WebEngine 6.140.0 passed all 35 CTest cases. Installation to a temporary
  prefix, executable startup, and installer checks also passed. Account setup,
  live message reading, replies, attachments, and Plasma-session behavior
  remain unverified. The clean renderer-free Debug build was also rebuilt and
  passed all available CTest cases with `WITH_WEBENGINE=OFF`.

## Pending verification

### M02 shell and account foundation

- **Current state:** M02 #4, shell #35, skeleton #74, density #75 and
  connection status #79 are in Development. Dependency gate #77 is in Review;
  other M02 issues are Defined. The
  `feature/m02-shell-accounts` branch adds a row-density selector; the
  continuation branch adds left/right folder sidebar placement in the compact
  and wide layouts. A further continuation adds persistent connection status
  in the footer. The five-region shell, full density behavior, and per-account
  status and error recovery still need implementation.
- **Account acceptance:** #78 requires setup and recovery with an isolated live
  IMAP account. No such account or local IMAP server has been verified for
  this work. Provision an isolated server and disposable credentials, then
  verify setup, failure, recovery and credential removal without using real
  home-directory mail or configuration.
- **Resume:** Implement and test the remaining M02 shell, account,
  protocol-capability and OAuth issues. Advance each issue through Review and
  Completed before checking its roadmap item; merge the milestone only when
  all items and acceptance checks are complete. Reconcile the pending roadmap
  changes in #250 and #252 before merging.

### M01 release gate and integration

- **Deferred by milestone scope:** Live account setup belongs to M02 issue #78.
  Live message reading, replies and attachments belong to M04 issue #93.
  Plasma desktop-session startup and recovery belong to M05 issue #101.
  M01.04 is complete from automated build, installer, launch and privacy
  evidence without an isolated account or live Plasma session.
- **Pending:** Verify renderer-enabled and renderer-free installs at system
  prefixes and validate package-manager branches on distribution families other
  than CachyOS before claiming them as supported.

### Milestone feature-issue synchronization

- **M01-M12:** Each roadmap item has its own linked project card and issue.
  Items belong to their subsection issue where applicable, or otherwise to
  their milestone issue; subsection issues belong to the milestone issue.
  Issue milestone assignments and parent links were verified. Existing project
  statuses were preserved; newly created M01 issues are Defined and other
  newly created issues are Backlog.
- Keep roadmap links, issue parents, milestone assignments, and project status
  synchronized when roadmap scope changes.
