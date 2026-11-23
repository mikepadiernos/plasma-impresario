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
- M01.02 renderer: the WebEngine reader is integrated with MIME parts,
  alternatives, attachments, headers, source, asynchronous quoting and search.
  The renderer-enabled build, all 35 CTest cases, temporary-prefix installation,
  executable startup, and installer checks passed. Live message-reading, reply,
  and attachment smoke tests remain unverified; issues #33 and #65-#71 remain in
  Development and their roadmap items unchecked. A per-message profile is
  destroyed when switching messages to discard loaded remote content and
  cancel outstanding requests.
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

### M01 release gate and integration

- **Blocked:** M01.02 and M01.04 still need an isolated test account and a
  Plasma session for account setup, message reading, replies, attachments,
  desktop integration, and failure recovery. No real account has been used.
  Exercise these workflows with test mail and record the results before
  advancing issues #33, #65-#71, #13, and #34 or marking M01 complete.
- **Pending:** Verify clean renderer-enabled and renderer-free installations
  at user and system prefixes on the supported CachyOS target, including
  plugins, libraries, launchers, icons, translations, handbook and man page.
  Validate package-manager branches on other distribution families before
  claiming those environments are supported. Run and document the M01.04
  warning, formatting, coverage and sanitizer checks.
- **Integration:** Installer flags from draft PR #231 were incorporated into
  the uncommitted M01 renderer work; reconcile the PR and issue #61 against
  the final branch without losing either set of changes. The original
  renderer stash was applied but retained. The combined Debug build passed
  36 CTest cases and an isolated-prefix install and `--version` launch on the
  observed CachyOS host. Reply quoting of formatted plaintext was corrected
  and covered for nested and collapsed quotes. Inspect the changes and follow
  the board workflow before completing or merging any feature.

### Milestone feature-issue synchronization

- **M01-M12:** Each roadmap item has its own linked project card and issue.
  Items belong to their subsection issue where applicable, or otherwise to
  their milestone issue; subsection issues belong to the milestone issue.
  Issue milestone assignments and parent links were verified. Existing project
  statuses were preserved; newly created M01 issues are Defined and other
  newly created issues are Backlog.
- Keep roadmap links, issue parents, milestone assignments, and project status
  synchronized when roadmap scope changes.
