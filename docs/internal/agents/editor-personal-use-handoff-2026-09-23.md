# Personal editing candidate and agent handoff

Status: ready for desktop practice-copy editing; manual interaction/gameplay
acceptance remains open. Secure-staging implementation: `1677e0e7d90fe0bbad667c00ce6a1143ac5bcaed`.
App and CLI rebuilt successfully with `cmake --build --preset mac-ai --target yaze z3ed --parallel 4`.
Exact executable hashes are recorded in `combined-editor-candidate.json`.

## One next user action

Open a separate practice copy of the editable
Oracle base ROM in the exact candidate below, go to room `005`, and try one
object move followed by Undo and Redo. This is interaction practice, not approval
of a new Origins layout. Save/reopen only the practice copy.

## Exact candidate

- Worktree: `/Users/scawful/src/hobby/yaze-worktrees/overworld-paint-regression-fixes`
- Branch: `codex/combined-editor-candidate`
- Application source: `1677e0e7d` (secure staging); includes `bf471e33b` preservation tests.
- App: `build/presets/mac-ai/bin/Debug/yaze.app` under that worktree.
- Provenance: `build/presets/mac-ai/bin/Debug/combined-editor-candidate.json`.
- The installed app and Barista launcher are separate; they were not updated.

Launch the exact desktop candidate for practice-copy editing:

```sh
open -n /Users/scawful/src/hobby/yaze-worktrees/overworld-paint-regression-fixes/build/presets/mac-ai/bin/Debug/yaze.app
```

This desktop candidate is not a newly deployed native iPad build.

## Editing workflow

1. Make a separate practice copy of `oos168.sfc`. Keep the original base and
   patched `oos168x.sfc` untouched. Oracle project/manifest configuration is needed
   to authorize stream relocation; a bare-ROM session may correctly reject growth.
   Do not bypass a manifest error or retarget the shared project silently.
2. Use **File > Open ROM / Project...**, then the Dungeon editor's room selector.
   Room IDs are hexadecimal: `005` Origins, `077` and `087` Goron. Confirm the
   active room before editing. Use single-room editing first; connected views
   contain estimated connections and do not prove gameplay traversal.
3. Select and move an object. For resizable objects, ordinary wheel changes the
   supported size; packed width/height objects change together. Shift-wheel
   changes width where supported. Fixed/custom object families may differ.
   Inspect the layer and dimensions, then Undo/Redo the change.
4. Preserve Origins' Pearl chest receipt `1F`, tag `36`, entrance `76`, and current
   Minish setup while practicing. Its empty southwest quadrant is not an approved
   puzzle layout. For Goron, inspect `077` and `087` separately: their open rail
   boundary is not a door pair; preserve the `077`/`0A8` staircase. Proposed parked
   minecart endpoints need the reviewed fixed-bend stem, not center stamping.
5. Use **File > Save As...** with a new practice filename. Reopen that saved file
   and inspect the edited room. Record unexpected behavior once in the shared
   manual checklist; do not overwrite a shared ROM to try again.

## Evidence boundaries

Implemented with automated coverage: selection/edit operations, shared undo/redo,
chest object/reward association, room templates, transactional serialization,
manifest-authorized relocation, receipt labels and guarded staircase diagnostics.
These are not a claim that every interaction has been manually accepted.

Latest editor test-only packet: 145/145 tests passed for
`*DungeonRoomEditsLifecycleTest*:*DungeonSelectionEditsLifecycleTest*:*DungeonStreamAllocatorTest*`.
The preceding staircase CLI qualification ran 52 read-only reports on exact base
and patched fixtures: six vanilla-model resolved and ten custom-collision-blocked
candidates per ROM. Neither establishes normal-input gameplay.

Still manual: native app placement/selection/resize/layers; undo after gestures;
practice-copy Save As/reopen; Oracle Minish shutter/passage behavior; Goron track
geometry and transitions; native iPad touch/layout. Keep these in the existing
`docs/internal/plans/manual-editor-checklist-2026-09-23.md`, not separate repeated
requests to the user.

Save fix evidence: 42/42 ROM/save-manager and 28/28 editor save-conflict tests
passed, zero skips. `RomTest.LoadFromFile` remains explicitly excluded for its
unrelated 2 MiB expectation versus 1 MiB fixture. Windows/WASM and power-loss
behavior are unqualified; parent-directory sync remains best effort.

## Cursor / Claude continuation

- Backend packet completed at `1677e0e7d` in `src/rom/rom.cc`, `test/unit/rom/rom_test.cc`, and one case in
  `test/unit/editor/editor_manager_write_conflict_test.cc` for exclusively owned
  save staging. Do not duplicate that implementation.
- Editor owner reviewed Save As source preservation, target backup, error/retry
  behavior and completed the desktop relink. Only one build at a time
  in this worktree. No broad refactor or new capacity planner.
- Existing room-transfer preflight already scratch-executes serializers.
- Preserve all unrelated dirty files. Never stage all files indiscriminately.
- Never overwrite shared Oracle ROMs, patched outputs, saves, or installed apps.
  No snapshot/version-manager invocation; no push/deploy without authorization.
- Do not restore automatic staircase-header clearing or infer connections from
  adjacent room IDs. Do not change tag36 to37 based on stale tag documentation.

Build from the worktree, after coordinating with the backend owner:

```sh
cmake --build --preset mac-ai --target yaze z3ed yaze_test_unit --parallel 4
```

Discover exact test suites with `--gtest_list_tests` before running them. The
backend owner must identify the actual editor test target; absent suites are not
passes. Record source commit, binary hash, selected/passed/skipped counts and any
remaining platform limits. Refresh candidate provenance only after the build.

## Navigation follow-up

The persistent dungeon navigation strip now includes a hexadecimal room field:
enter `005`, `077`, or `087` and press Enter. Grid arrows are explicitly labeled
as grid navigation, not verified connections. Candidate rebuilt; 11 toolbar tests
passed. Native end-to-end Origins acceptance remains blocked by CUA routing
ambiguity between two live same-bundle-ID apps; see existing manual checklist.
