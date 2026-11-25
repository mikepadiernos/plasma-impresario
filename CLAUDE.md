# Plasma Impresario

Plasma Impresario is a KDE Plasma-only desktop mail client for IMAP-based workflows, built with Qt and KDE Frameworks and designed for focused reading, account management, and desktop integration. Other desktop environments are outside the supported product scope.

Apply the shared instructions in `~/Library/Projects/CLAUDE.md` or
`~/Library/.projects/CLAUDE.md` first. These project instructions supplement
them and take precedence where they conflict.

## Project documentation

- `docs/ROADMAP.md`: authoritative milestone and architecture plan.
- `docs/TODO.md`: blocked tasks and actionable handoffs.
- `docs/BUGS.md`: reported bugs and completion status.
- `docs/CAVEATS.md`: limitations of implemented features and recommendations.
- `docs/EDGE.md`: potential edge cases and test scenarios.
- `docs/philosophy/`: design direction, product scope, and inspirations.
- `docs/parity/`: reference patterns for UI and backend behavior, not guaranteed current implementation.

## Commands

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
cmake --build build --target clang-format   # format all C++ sources
./build/plasma-impresario
```

## Development

- Put non-widget logic in `src/core/` and cover it with QtTest in `tests/`.
  Put widgets in `src/app/` or the appropriate GUI subtree.
- Prefer KDE Frameworks facilities to custom implementations, including KIO,
  KConfig, KWallet-compatible patterns, mail account integration, and desktop
  service hooks where appropriate.
- Localize user-visible strings with `i18n()` or `i18nc()`.
- Use Plasma's active icon theme via `QIcon::fromTheme()`; do not hard-code a
  replacement icon theme. At 24 logical pixels or smaller, prefer `-symbolic`
  variants and use `SymbolicIcon::fromTheme()` or `SymbolicIcon::wrap()` when
  supported.
- Store settings in KConfig and session state through
  `KSharedConfig::openStateConfig()`.
- Isolate tests from the real home folder, mail data, and configuration with
  `QTemporaryDir` and `QStandardPaths::setTestModeEnabled(true)`.

## Roadmap

- Keep the detailed milestone headings, subheadings and individual feature
  bullets. Do not strike milestone headings, replace feature lists with
  completion paragraphs, or add a "Done so far" summary.
- Use checkboxes as feature completion markers. Check a roadmap item only
  after its issue moves to **Completed** or is already **Merged**. Leave
  features in **Review** or earlier unchecked, even when implemented.
- Before marking a feature complete, verify its implementation in the current
  checkout and check its issue status. Leave unimplemented or unverified
  features unchecked.
- A completed feature can have caveats. Record each outstanding caveat as
  described below and reference it at the end of the milestone; do not leave
  an otherwise completed feature unchecked solely because of a caveat.
- Keep the roadmap focused on milestones and architecture. Put blocked work and
  detailed handoffs in `docs/TODO.md` and link to them briefly from the roadmap.

## Project workflow

Use the [Plasma Impresario project board](https://github.com/users/mikepadiernos/projects/4)
for milestone parent issues and feature sub-issues. Follow **Backlog →
Defined → Development → Review → Completed → Merged**; never move an item
backward.

1. **Start a milestone:** Move its parent and not-yet-started feature issues
   to Defined unless they are further along. Record the parent's **Start
   date**.
2. **Start a feature:** Move the feature and parent to Development unless
   they are further along. Record the feature's **Start date**. Also move a
   bug or caveat issue to Development when work starts, unless it is further
   along.
3. **Finish implementation:** Move the feature to Review, set its **Target
   date** to the completion date, and check it in the parent issue. Leave its
   roadmap item unchecked.
4. **Advance the previous feature:** When the next feature in the same
   milestone enters Review, move the previous feature from Review to
   Completed. Move the final feature to Completed when it is ready.
5. **Complete an issue:** When moving an issue to Completed, close it with the
   completed reason. For a milestone feature, check its roadmap item in the
   same change.
6. **Complete a milestone:** When all feature items are checked, set the
   parent issue's **Target date** to the completion date.
7. **Merge:** When an issue's changes are merged into `main`, move it to
   Merged and keep it closed as completed. After merging a feature-complete
   milestone, close its parent and related feature issues as completed and
   move them to Merged. Leave unresolved bug and caveat issues open.

## Issues and caveats

- Prefix user-reported bug titles with `[bug] ` and apply the `bug` label.
- Add reported bugs to `docs/BUGS.md` and update their status there when it
  changes. Verify the issue and project status before recording completion.
- When a bug is added to `docs/BUGS.md`, also create the matching issue on the
  project board and link it from the report in this file.
- Prefix caveat report titles with `[caveat] ` and apply the `caveat` label.
- For each new or updated caveat:
  1. Describe the limitation and a practical recommendation in
     `docs/CAVEATS.md`. Distinguish current behavior from workarounds and
     proposed improvements.
  2. Link the caveat and its `[caveat]` issue to each other, and add the issue
     to the project board.
  3. Post the recommendation as an issue comment and link to it from the
     document. Update the comment when the recommendation changes; do not leave
     conflicting guidance.

## Branches and merging

- Work on each milestone in its `feature/<short-name>` branch. Work on bug and
  caveat issues in their own branches.
- A milestone is feature-complete only when all its roadmap feature items are
  checked. Once complete, commit changes by scope, push the branch, and
  merge it into `main`.
- For a fixed bug or caveat, likewise commit changes by scope, push its branch,
  and merge it into `main`.

## Blocked and pending tasks

- Keep every handoff and workflow note, including blockers, pending board
  actions, moved or renumbered items and resume steps, in `docs/TODO.md`
  only. Elsewhere, such as the roadmap, other docs, issue and pull request
  bodies or comments, and commit messages, reference the relevant TODO
  section in one line instead of repeating its details.
- When a task cannot be performed, record it in `docs/TODO.md` with the
  blocker, relevant milestone or issue, confirmed state, and actionable steps
  for resuming. Add a brief link to the relevant TODO section in the roadmap
  near the affected milestone, or in Tracking for cross-cutting work.
- For blocked GitHub Projects access, record the affected issues, intended
  board actions, and relevant dates in TODO. Do not check a roadmap feature before
  its issue is confirmed in Completed or Merged.
- When work resumes, verify the current state before acting. Reconcile project
  and issue states without moving items backward. Update or remove resolved
  TODO entries and remove their roadmap references when no longer needed.
  Pending tasks and their references are not completion markers.

## Unattended milestone work

When the user explicitly requests milestone work without interaction:

1. Complete and verify every feature in the current milestone, follow the
   project workflow, and merge the milestone into `main` without asking for
   routine confirmation.
2. Proceed to the next incomplete roadmap milestone in order and repeat until
   all milestones are complete and merged or the user stops the work.

Do not treat unfinished or unverified work as complete. If an essential action
cannot be resolved without user input or required approval, record the pending
work as described above and report the blocker rather than claiming completion.

## GitHub API requests

Apply these practices to GitHub requests throughout the project:

- Use authenticated requests, preferably through `gh`. Tokens for the same
  user share that user's rate limit.
- Avoid concurrent API requests and pause at least one second between
  mutations. Keep GraphQL queries focused, paginate results, and request only
  needed fields. Use REST where appropriate; its primary limit is separate
  from GraphQL's, but it does not replace every Projects operation.
- Check rate-limit response headers when needed. For a primary limit, wait
  until `x-ratelimit-reset` before retrying. For a secondary limit, honor
  `retry-after`; otherwise, if the remaining count is zero, wait until reset,
  or wait at least one minute. Increase the delay exponentially if secondary
  limits persist, stop after a bounded number of retries, and report the blocker.
  GraphQL can report rate-limit errors in a successful HTTP response, so check
  the response body as well.
- After a timeout or ambiguous mutation failure, check the current state before
  retrying to avoid duplicate changes. If Projects access remains blocked,
  record and reconcile the pending actions as described above.

See GitHub's [REST rate limits](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api)
and [GraphQL rate limits](https://docs.github.com/en/graphql/overview/rate-limits-and-query-limits-for-the-graphql-api).
