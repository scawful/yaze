# Canonical editor consolidation

Canonical local candidate: `codex/combined-editor-candidate` in
`/Users/scawful/src/hobby/yaze-worktrees/overworld-paint-regression-fixes`.
Main `codex/dungeon-workbench-bottom-drawer` at `0f3855d6f` is already an ancestor.
Keep this candidate as the integration target; do not replace the dirty main tree.
No push, installed-app replacement, worktree deletion or layout reset performed.

## Included / already represented

| Packet | Evidence / result |
| --- | --- |
| Main bottom-drawer UI | Ancestor of candidate; no missing main committed UI |
| Sprite owner snapshot | 43 exact input hashes; integrated `7e2f16590`, preserving project parser validation; unrelated patrol registration excluded |
| Overworld follow-cursor properties | `ff0545893`; preserves candidate entity history and no-hover guards |
| Safe disk staging | `1677e0e7d`; retained |
| Persistent dungeon room input | `1e7c1a5ea`; retained |
| Weekly SPC700 reset | Missing source confirmed; `9557ff3bb` cherry-picked as `7b0d74012` |
| Weekly drawer badge fit | Only manager cc/h and tests copied; `b5c7f34a7`; unrelated README omitted |
| Tile16 doctor preservation | Old destructive heuristic confirmed present; `a1484bab3` cherry-picked as `e3d5c34ed` |
| Welcome polish through `0f0fd698f` | All five changed source/test files identical to candidate; unique ancestry is not missing UI |
| Theme pack / MIT notices | Assets identical; candidate retains additional distributable manifest and stronger tests |
| Sidebar, drawer navigation, selector symbology, Room Graphics, custom atlas, Oracle animated-frame fixes | `git cherry` reports equivalent patches; no duplicate application |
| Portable bundle verifier `3890a12be` | Three latest changed files identical to candidate |
| Dungeon remove-object | Command source identical; candidate tests have further changes |

## Pending / deliberately excluded

- Tile16 multiselect/destination-graphics preview and graphics-group isolation
  follow-up is now integrated at `699c86326` (13-file patch hash verified).
- Broad historical editor-safety-containment and unrelated CLI/CI/maintenance
  branches are not automatically merged. Their remaining differences require
  separate behavior review; this is a bounded UI/correctness consolidation.
- Old welcome-simplify dirty submodule pointers are not intended UI changes.
- Theme-salvage dirty deletion of MIT attribution lines is not imported.
- Main Cursor/Claude edits and all owner worktrees remain unchanged.
- Hooks/manifest consumer review remains separate; no scanner deletion or
  assembler-default change occurs in this consolidation.

## Why an app may still look old

Source comparison proves current welcome and main bottom-drawer code is present.
It does not prove which binary a user has open. Multiple builds share the same
bundle identity; prior UI automation could not reliably distinguish native picker
input. The running processes previously recorded may exit/restart; do not reuse
old PIDs as current evidence.

LayoutManager loads global/project named layouts and stored ImGui layout data;
PlatformPaths resolves `imgui.ini`, with `YAZE_APP_DATA_DIR` as an override.
Persisted layout can therefore retain an older arrangement despite updated
presets. This is a supported possibility, not a reproduced diagnosis. Do not
silently delete preferences or reset the user's layout. For an isolated review,
use a fresh app-data directory and a clearly identified app instance after
coordinating CUA ownership.

Exact app/CLI hashes and build/test evidence are kept in
`build/presets/mac-ai/bin/Debug/combined-editor-candidate.json` and
`build/presets/mac-ai/combined-editor-evidence/`.
Live sprite and vanilla-overworld results belong to their owner builds; combined
Origins/Oracle editing, save/reopen, native iPad and gameplay remain unqualified.

## Final validation

Canonical application source: `699c86326`. App, CLI, unit and quick-editor targets
built successfully with four workers. Focused results (overlapping suites, do not
sum as unique tests): 99 preview/tracking/history tests; 62 welcome/theme UI tests;
17 drawer/SPC700/doctor tests. Earlier combined sprite/project/toolbar87 and
OW/editor-save104 results remain separately recorded. Theme test contamination
was fixed by naming its synthetic UTF-8 theme uniquely; shipped assets unchanged.
Precommit checks passed. Main/owner worktrees, installed apps and ROMs preserved.
