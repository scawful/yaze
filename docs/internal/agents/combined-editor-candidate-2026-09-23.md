# Combined editor testing candidate

Status: combined build and focused automated checks passed. Manual acceptance deferred.

## Source scope

Branch: `codex/combined-editor-candidate`.
Merge parents: `0f3855d6f` (overworld/Tile16 consolidation) and
`804b32eea` (dungeon authoring and pot-coordinate repair).

The candidate includes immediate Tile16 editing with shared overworld history,
rectangular painting/context targeting, session-bound floating Tile16 windows,
overworld sprite persistence, dungeon chest/reward edits, mixed entity history,
reciprocal ordinary doors, room clone/JSON import, and the pot position codec.

The merge also imports the authoring branch's other committed changes. It is
not a cherry-pick containing only dungeon files. Focused checks establish the
listed behavior only; final release, CI, runtime parity and manual acceptance
remain separate gates.

## Conflict decisions

- Preserve the organized `overworld/canvas`, `entity`, `maps`, `painting`,
  `tile16` and `ui` paths while retaining new dungeon sources in CMake.
- Preserve both overworld sprite coordinate flags and zero-initialized members.
- Preserve immediate Tile16 edits/history and session floating-window behavior.
- Use the newer Mesen socket portability, send-deadline and event-idle repairs;
  retain explicit TCP target authority and the bounded-connect regression test.
- Preserve wrapped empty-state titles and bounded buttons, plus callback-free
  button activation coverage.
- Keep selected-versus-hovered map status and clear hover when leaving the canvas.
- Route personas through the actual catalog and replace obsolete status claims
  with this candidate's evidence. Tile16 documentation describes immediate edits.

## Validation

The combined source built successfully with:

```bash
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze yaze_test_unit yaze_test_integration --parallel 4
```

Tests were enumerated with `--gtest_list_tests` before running each filter.
Combined results: **1,303 passed; zero failed, three skipped**:

| Packet | Tests |
|---|---:|
| Overworld, Tile16 history/painting, sprite persistence, ROM transaction/fence | 180 |
| Dungeon authoring, chest/doors/room transfer, pot coordinates | 1,024 |
| Shared window/widget, Mesen, sprite initialization, status and session checks | 80 passed, 3 skipped |
| Synthetic Tile16 integration | 19 |

The three skipped `SpriteRenderPreviewTest` cases require optional assets:
`ExpandedRomRetainsBabasuSourceFrameAndPalette` and
`ExpandedRomRetainsStalfosStaticPoseSources` require `YAZE_TEST_ROM_EXPANDED`;
`OracleManhandlaRealAssetMatchesStaticHead` requires `YAZE_TEST_ORACLE_SPRITE_ASSETS`.
They remain unqualified.

Exact filters and reproducible commands are in
[combined-editor-test-filters-2026-09-23.json](combined-editor-test-filters-2026-09-23.json).
Use the integration binary for the `integration` entry; the unit binary for
all other entries. Append `--gtest_filter=<value>` to the selected binary.

`/opt/homebrew/bin/bash scripts/pre-commit.sh`,
`scripts/agents/protocol-audit.sh`, `scripts/agents/test-universe-coord.sh`, and
`/opt/homebrew/bin/bash scripts/dev/editor-guardrails.sh 0f3855d6f HEAD` passed.
The linker reported duplicate-library warnings; no compile/link errors.
These packets use synthetic fixtures/fake sockets. They do not establish native
manual UX, actual emulator parity, ROM-file qualification, or Windows CI.

## One manual pass

Use [the consolidated checklist](../plans/manual-editor-checklist-2026-09-23.md)
with this candidate app and disposable ROM copies. All manual boxes remain open.
The Barista launcher and installed application were not changed.

## Main checkout and follow-up

Concurrent sprite-catalog changes are being authored in the main checkout.
Preserve that dirty work. Do not force a checkout, reset, stash, or overwrite
shared CMake/project/sprite files to adopt this candidate. Once that work is
committed and reviewed, merge it into the combined branch and rerun affected
checks before replacing this testing candidate.

After combined acceptance, continue the open dungeon authoring packages in
[the capability plan](../plans/editor-capability-parity-plan.md). Keep the user
manual checklist as the single testing queue, and preserve small layout tasks
for the user's later coding/design pass.

## Native iPad follow-up

The [iPad dungeon review pass](ipad-dungeon-review-2026-09-23.md)
builds on `e5f84f999`. It improves the native room sidebar and desktop-connected
room viewer. Its source checks do not replace a linked iOS build or physical
device acceptance. The accompanying dungeon fix preserves staircase links to room 000; the desktop
candidate is rebuilt for that C++ change.
