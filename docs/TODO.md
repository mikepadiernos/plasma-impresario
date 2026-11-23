# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/4)** ·
**[Bugs](BUGS.md)** · **[Caveats](CAVEATS.md)**

This log records unfinished work and resumable handoffs. Confirm current issue
and project status before acting.

## Backlog

- [ ] Validate the [initial compatibility target](compatibility.md) with a
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
  unrelated one. Pacman skips unavailable optional packages with a warning;
  package lists and real installations still need verification across
  supported distribution families.
- M01 renderer: an opt-in experimental WebEngine bridge loads IMAP parts and
  `cid:` images with a per-message profile, reports blocked remote images, and
  delegates permitted links instead of navigating to them. It is not connected
  to the application viewer. MIME alternatives, attachments, asynchronous
  quoting and search, and the complete navigation and privacy policies still
  require implementation and verification before the full installer and
  milestone can be completed. Revoking remote-image permission blocks new
  requests but does not cancel pending requests or remove already loaded
  images; integration must replace the page and profile to discard them.
- M01 verification: on CachyOS x86_64 with Qt 6.12.0, CMake 4.5.1 and GCC
  16.2.1, a renderer-free build and user-prefix install succeeded; the
  installed executable's `--help`, handbook, man page, desktop entry and
  translations were present. Account setup, plugin behavior, message reading,
  other distributions and protected system prefixes remain unverified. A
  full-renderer configure failed because Qt6WebKitWidgets is unavailable.

## Pending verification

### Milestone feature-issue synchronization

- **M01-M12:** Each roadmap item has its own linked project card and issue.
  Items belong to their subsection issue where applicable, or otherwise to
  their milestone issue; subsection issues belong to the milestone issue.
  Issue milestone assignments and parent links were verified. Existing project
  statuses were preserved; newly created M01 issues are Defined and other
  newly created issues are Backlog.
- Keep roadmap links, issue parents, milestone assignments, and project status
  synchronized when roadmap scope changes.
