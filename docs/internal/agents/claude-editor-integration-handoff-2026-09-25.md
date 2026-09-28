# Claude editor integration handoff (2026-09-25)

## Context

From 2026-09-22 to 2026-09-24, Claude on the Mac reviewed and qualified Codex's
dungeon authoring work, then took over integration because Codex usage ran low.
All of that work now lives on one local branch, `claude/editor-integration`,
in the worktree `~/src/hobby/yaze-worktrees/validate-placement`.

Nothing is pushed. `gh` returns HTTP 401, so run `gh auth login` before any push
or PR.

## Current state

`claude/editor-integration` = Codex's combined candidate `17b40b061`
(`codex/combined-editor-candidate`: dungeon authoring, sprite authoring,
overworld fixes, ROM save staging, SPC700) plus these commits, oldest first:

| Commit | Author | What |
|---|---|---|
| `aa31fc8e9` | Claude (Mac) | Parity gate, cherry-picked from PR #259 |
| `d069cf6f2` | Claude (Mac) | 9 stale CLI save-failure tests now use `Rom::SetStagingFailureForTesting` |
| `27bd73530`, `7bc030081` | Claude (cloud) | Read-only Proposal Preview window |
| `78073c0b0` | Claude (cloud) | Stay in the open editor after a ROM load; Settings > Test mode |
| `873bd4313` | Claude (Mac) | iOS link fix: 32 editor sources and `palette_debug.cc` were compiled twice (602 duplicate symbols); `build_cleaner.py` now excludes them |
| `2cf032d7b` | Audit agent | Test-mode gRPC server binds to localhost |
| `c5d2a8c33` | Claude (Mac) | Save As freeze: stacked popups reopened each other every frame; now only the newest is drawn |
| `3abe07ed0` | Claude (Mac) | Startup ROM opened before settings loaded, so Default Editor was ignored |
| `83dd40a06` | Claude (Mac) | `--startup_dashboard=hide` / Test mode applied after the startup load left an empty workspace |
| `d28e052e4` | Claude (Mac) | iPad crash: the blocking file picker's nested run loop re-entered `Application::Tick()`; re-entrant ticks now return |
| `0a4fcc9ee` | Claude (Mac) | iPad: `.yazeproj` type was both exported and imported in `Info-iOS.plist`; picker now resolves it by identifier |

The worktree is clean.

Installed builds:
- Yaze Nightly (`~/Applications/Yaze Nightly.app`) is `v0.8.0-g83dd40a06`. It
  does not include `d28e052e4` or `0a4fcc9ee`.
- The iPad "Baby Pad" runs `0a4fcc9ee` (Debug), installed and launched.

Other branches, superseded by `claude/editor-integration`:
- `claude/combined-qualification` (`b6a3afec2`): combined candidate plus parity
  gate, used for qualification.
- `claude/fix-stale-save-failure-tests` (`2d8b576fa`): same change as `d069cf6f2`.
- `claude/dungeon-room-switch-selection` (`335fdf446`): Codex cherry-picked it as
  `6e9d52e8c`.

Work by other agents, not on this branch:
- The audit agent's report is `oracle-of-secrets/Docs/Planning/Plans/yaze_audit_2026-09-24.md`.
  Its CPU fixes are on `claude/idle-cpu` in its own worktree.
- Codex's in-progress dungeon proposal overlay (3 untracked files) is in
  `yaze-worktrees/overworld-paint-regression-fixes`.
- The main checkout `~/src/hobby/yaze` has 75 uncommitted files (sprite,
  overworld, core). Do not reset or merge it wholesale.

## Verified

- Full unit suite on `c5d2a8c33`: 4733 passed. The 2 failures predate this work:
  `RomTest.LoadFromFile` (the vanilla ROM on this Mac is 1 MB, the test expects
  2 MB) and an order-dependent settings/theme test.
- Quick editor suite on `0a4fcc9ee`: 1965 passed.
- Real-ROM qualification of the combined candidate plus the parity gate (disposable
  vanilla and Oracle copies, save to a new file, reopen in a separate process,
  diff all 296 rooms):
  - All 296 rooms export on both ROMs.
  - Clone/import cases change only the destination room, and the reopened room
    equals the applied state.
  - The block overflow, chest table capacity and oversized rooms are all rejected,
    with the live ROM left unchanged.
  - Pot type and move edits keep flag bits.
  - Both parity baselines pass.
- Startup behavior, checked in isolated app instances (separate HOME and app data,
  window on another Space):
  - Test mode: no welcome screen, no picker, port 50052 listening.
  - Default Editor = Dungeon plus `--rom_file`: opens the Dungeon Workbench.
  - `--startup_dashboard=hide`: opens the Dungeon editor.
- The Save As fix has a regression test (`PopupManagerTest`). It fails without the
  fix (the confirmation reopened 11 of 11 frames).
- iPad: the build links, signs, installs and launches. The crash cause is
  confirmed from the device crash log (`yaze-2026-09-24-142618.ips`).

## Not verified

1. On the iPad, by hand: New Project > choose ROM no longer crashes, and
   `.yazeproj` is selectable in File > Open ROM / Project.
2. Save As in the real app: move an object in room `005`, Save As > Browse >
   new name > Save. The confirmation should appear, and the file should be written.
3. File > Open while the Dungeon editor is open. It should stay in Dungeon, with
   no picker.
4. Cloned rooms in the emulator (chests, torches, pots behave in game).
5. Windows, WASM and CI.

## Open risks

- The picker type fix is based on reading the plist and code. If `.yazeproj`
  still appears as a plain folder, check where the bundle lives (iCloud Drive or
  On My iPad) and whether the Mac created it as a real package.
- `Application::Tick()` now skips frames while a blocking picker is open. That is
  intended. If an agent sees a frozen frame behind a picker, this is why.
- About 20 CLI tests still assert that `<rom>.tmp` does not exist. They can no
  longer fail. Rewriting them to scan for `.yaze-rom-*.tmp` would be flaky,
  because they share the system temp directory.
- iPad signing needs an unlocked login keychain. From an agent shell,
  `codesign` fails with `errSecInternalComponent` until the user runs
  `security unlock-keychain ~/Library/Keychains/login.keychain-db`.
- Agents share worktrees. Commit only the files you name; check `git reflog`
  if another agent committed recently.

## Next step

Have the user run the two iPad checks (item 1 under Not verified). If both pass,
reinstall Yaze Nightly from `claude/editor-integration` and do the Save As hand
check (item 2).

## Useful commands

```sh
cd ~/src/hobby/yaze-worktrees/validate-placement
cmake --build --preset mac-ai --target yaze yaze_test_unit yaze_test_quick_unit_editor
build/presets/mac-ai/bin/Debug/yaze_test_quick_unit_editor
YAZE_NIGHTLY_BUILD_DIR=$HOME/.yaze/nightly/local/build-integration scripts/install-nightly-local.sh
scripts/xcodebuild-ios.sh ios-debug deploy   # needs an unlocked keychain; device defaults to "Baby Pad"
```

The qualification harness is local only and must never be committed:
- `~/src/hobby/yaze-practice/qualification/qualification-harness.patch` (also
  `stash@{0}` in this worktree) adds `QualifyRoomTransferSaveReopen`, a test with
  modes survey, apply, verify, dump, potedit, edit and doors.
- `~/src/hobby/yaze-practice/qualification/qualify.sh` runs every case. Update
  the `V=` path in it first; it pointed at a session scratchpad that no longer
  exists.
- Bind an Oracle project with `SetDependencies` before `SetGameData`. Also copy
  `project.feature_flags` into `core::FeatureFlags::get()`; otherwise saves hit the
  ASM-owned water table.
- The captures in `~/.yaze/dungeon_game_captures/` come from the ROM. Never commit
  them.
