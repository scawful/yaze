# Dungeon staircase slot audit and automatic-repair guard

Baseline: `97ab68e0e`, `codex/combined-editor-candidate`.

## Finding

The connected graph increments one counter for every recognized staircase object.
Its previous comments asserted that this matches the engine. The disassembly
supports a more constrained, family-dependent model. Therefore, this estimate
cannot justify clearing destination headers. Transactional undo alone does not
make that decision correct.

## Source evidence

USDASM `d53311a`, `bank_01.asm`:

- `$01878B`, `$0187B2–$0187C7`: initialize staircase-family counters.
- Fat stairs up at `$01A41B` index `$06B0` with `$0438`; after writing, they
  advance `$0438` and propagate the endpoint to the other family counters.
- Fat stairs down at `$01A458` / `$01A486` use `$043A` and advance only the
  downstream down-family endpoints.
- Spiral stairs up upper at `$01A4B4` use `$047E`; up lower at `$01A4F5` use
  `$0482`. Down upper/lower use `$0480` / `$0484`.
- Straight stairs use `$04A2` / `$04A4` for up north/south, `$04A6` / `$04A8`
  for down north/south. Lower-layer variants share their corresponding counter.
- `$01B986–$01BB3A`: collision generation walks `$06B0` using those endpoints,
  beginning with tile type `$30`, incrementing the index, and setting the down
  group bits at `$01BA65–$01BA6D`.
- `$01C382–$01C39E`: transition reads a collision attribute into `$0462`.
  `$01C3CA–$01C3D4` masks it with 3 and reads the destination byte.

Example requiring explicit handling: a down-family object followed by an
up-family object can write the same position-table entry because the down writer
does not advance the earlier up counter. A plain count cannot model overwrite or
family grouping. This is source reasoning, not evidence of a particular vanilla
ROM containing that ordering.

## Implemented boundary

- Removed the Clear stale UI action and unused auto-fix count helper.
- The legacy repair entry point returns zero, reports why repair is unavailable,
  and never invokes the mutation callback. A regression checks destinations,
  header dirty state and callback count remain unchanged.
- Graph controls say Stair mapping unverified even when estimates report no
  anomalies. Staircase link descriptions say Estimated; unused-header and
  extra-object notices no longer assert proven runtime reachability.
- Existing undoable destination-header editing remains available. Room 000
  remains valid. The graph still uses estimated placement order for navigation;
  this pass does not claim exact staircase connectivity.

## Verification

```bash
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='*DungeonCanvasViewerConnectedGraph*:*DungeonRoomMetadata*:*DungeonWorkbench*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='*DungeonCanvasViewerConnectedGraph*:*DungeonRoomMetadata*:*DungeonWorkbench*'
```

Desktop app and unit target built. 138 tests across 18 suites passed, no skips
or failures. Existing duplicate-library warnings remain. No ROM writes, emulator
traversal, native UI acceptance, push or device deployment occurred.

## Next implementation packet

1. Verify the actual object stream order across layout and room layers, including
   serialization/reload; do not silently sort objects to fit an assumed order.
2. Implement a pure resolver that models family endpoints, position-table writes
   and final collision index assignment. Return ambiguity/unsupported states
   explicitly for overwritten entries, overflow, tile overlap and custom engines.
3. Test mixed families, reverse ordering, lower variants, more than four entries,
   room-zero targets and layer boundaries against independent source fixtures.
4. Connect resolved slots to the existing destination inspector. Leave automatic
   clearing disabled until real-ROM and runtime checks establish unused slots.

Manual acceptance remains in the single consolidated checklist. Preserve the
concurrent sprite-editor work in the main checkout and all Oracle work.
