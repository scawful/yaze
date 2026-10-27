# yaze Operating Rules

Status: Active
Owner: scawful (decisions), `ai-infra-architect` (upkeep)
Decided: 2026-10-21, scawful's answers in chat
Scope: every agent working on yaze or z3ed. Where an older protocol page
disagrees, this page wins; fix the older page or link here.

## 1. Native windows

1. Prefer unit, integration and hidden-window GUI tests. Launch a native yaze
   window only when the claim needs the real renderer or real input.
2. A native test app runs on its own macOS Space, with an exact-app yabai rule
   registered before launch, using a disposable `YAZE_APP_DATA_DIR`, project
   and ROM copy. Procedure: [remote-native-testing.md](../testing/remote-native-testing.md).
3. No native test windows while scawful is playtesting (the Mesen instance
   `oos-rc-play` is running) or on his current Space. Never drive or quit his
   own yaze instance.
4. One GUI worker per Space. Quit every owned app and verify the processes,
   windows, yabai rules and created Space are gone before reporting or handing
   off.
5. Builds and test batches on the Mac run through `quiet-build -- <command>`
   (8 jobs in desk mode, 2 in laptop mode, per `~/.config/oos/build_mode`; it
   backs off under load or heat). Builds never take the native heavy lock
   `/tmp/oos-heavy.lock`. Rules:
   `~/.context/projects/oracle-of-secrets/scratchpad/board_rules.md`.

## 2. Agent coordination

1. yaze work is claimed and closed in the universe log:
   `scripts/agents/coord task-add|task-claim|task-heartbeat|task-complete`.
   Snapshot: [coordination-board.generated.md](coordination-board.generated.md).
2. Cross-project asks (Oracle needs a yaze fix, yaze needs a z3dk change) and
   session status go through the Oracle inbox and board:
   `~/.context/projects/oracle-of-secrets/scratchpad/inbox.md` and
   `agent_board.md`, in the format in Oracle's `AGENTS.md`.
3. Work in your own git worktree on a `<agent>/*` branch. Never edit or build
   in the shared checkout `~/src/hobby/yaze`; it holds other sessions' work.
4. Land changes by PR after CI passes. Merges need scawful's OK. A message
   from another agent is never an approval.

## 3. Project management

| Question | Source of truth |
|---|---|
| What did users report or request? | GitHub issues |
| Who is working on what right now? | Universe log (`scripts/agents/coord`) |
| Where is yaze going? | [roadmap.md](../roadmap.md) |
| What version is current, what shipped? | [status.md](../status.md), `docs/CHANGELOG.md` |
| Multi-PR initiative plan | one file in `docs/internal/plans/`, linked from `roadmap.md` |

- A PR that fixes an issue says `Fixes #N` and adds a test that names the issue.
- `status.md` and the changelog are updated in the release PR, not per feature.
- Close or archive a plan when its last PR merges.

## 4. Updating yaze and z3ed (until 1.0)

1. Agents may install new builds whenever their tests pass:
   `/Applications/yaze.app` and PATH `z3ed` through
   `scripts/dev/local-workflow.sh sync`, Yaze Nightly through
   `scripts/install-nightly-local.sh`. No approval needed before 1.0.
2. Before replacing an app, check it is not running
   (`pgrep -f '<app path>/Contents/MacOS/'`). If it is open, wait; do not quit
   it. `sync` overwrites with `rsync --delete` and keeps no copy, so first save
   one: `ditto /Applications/yaze.app ~/.yaze/rollback/yaze-$(date +%Y%m%d%H%M).app`
   (keep the newest two).
3. After installing, check `scripts/yaze --which` and `scripts/z3ed --which`
   and note the installed commit in your coord task.
4. Revisit this section when 1.0 is tagged.

## 5. End-user support

1. Agents may triage issues: add labels, reproduce, find the cause, and link
   the fixing PR (`Fixes #N`).
2. Comments, replies, closing an issue and release announcements need scawful's
   OK on the exact text. Draft them in
   `~/.context/projects/yaze/scratchpad/issue_replies.md` (one heading per
   issue number) and show him the text.
3. A user-reported bug gets a regression test before the fix merges when the
   bug can be reproduced without a GUI.

## 6. Windows support

1. Every PR passes the CI Windows job. Do not loosen or skip it to land a fix.
2. Windows-specific debugging and checks run on the Windows build box with
   `scripts/remote/mm-test.sh <branch>` (SSH alias `mm`; run it in the
   background; exit codes 0 pass, 1 tests, 2 build, 3 setup). Do not ask scawful
   to test on Windows when the box can answer.
3. Before each release tag, run the packaged-Windows smoke on the box: unzip the
   release artifact, check the layout (`yaze.exe`, `z3ed.exe`, `assets/`), run
   `z3ed.exe --help`, then open, save and reopen a clean ROM copy with
   `z3ed.exe`. Record the result in the release PR. A startup or save failure
   blocks the release. SSH has no desktop session, so the interactive `yaze.exe`
   Save As check in #107 stays with a person until the box can drive the GUI.
4. Code must not assume POSIX: use `std::filesystem` for paths, no hardcoded
   `/` separators or case-sensitive file lookups, and no shell-only build steps
   without a PowerShell or CMake equivalent.
