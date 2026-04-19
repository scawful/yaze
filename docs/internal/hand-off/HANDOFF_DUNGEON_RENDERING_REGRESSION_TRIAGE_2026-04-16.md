# Dungeon Rendering Regression Triage - 2026-04-16

Status: active triage, not fixed.

This is the short project-local note that complements the AFS handoff at
`/Users/scawful/src/.context/scratchpad/handoffs/yaze-dungeon-corners-runtime-sync-2026-04-16.md`.

## What is true right now

- The object-level parity push is helping, but live room rendering is still not trustworthy enough to call done.
- `0x100` corner wall objects are still not confirmed fixed in the real app/runtime path.
- There are broader room-level regressions beyond corners: some rooms lose visible walls, and some rooms show a translucent layer above the room when they should not.
- Because of that, "object routine parity is green" must not be treated as equivalent to "room build parity is green".

## Current unresolved symptom buckets

1. Custom/alias corner objects still look wrong in real rooms.
2. Some rooms hide or lose wall structure.
3. Some rooms show unwanted translucent BG2 behavior or an apparent extra upper layer.
4. The editor/runtime room path still needs validation against USDASM room-build semantics, not just per-object draw routines.

## What the current tests do prove

- `test/unit/zelda3/dungeon/object_drawing_comprehensive_test.cc` gives strong routine-level coverage for draw mappings, palette bank assumptions, pit/mask identification, and room-effect/layer-merge metadata.
- `test/unit/zelda3/dungeon/object_drawer_registry_replay_test.cc` gives strong replay parity for many routine layouts, including 4x4 and weird-corner column-major cases.
- `test/unit/zelda3/dungeon/custom_object_room_render_test.cc` proves synthetic room-buffer rendering for custom bins, missing-bin placeholders, and corner alias routing.
- `test/unit/zelda3/dungeon/room_layer_manager_test.cc` proves local compositing rules and cache invalidation behavior.

## What the current tests do not prove

- They do not prove that a real room's full build order matches `bank_01.asm`.
- They do not prove that real project-backed custom corner bins reference valid art in the active room `current_gfx16_` set.
- They do not prove that room effects, layer merging, and overlay routing stay correct for specific vanilla/OoS rooms with known problem layouts.
- They do not prove that the live editor/app canvas matches the intended composite visually; current E2E coverage is mostly interaction smoke, not screenshot or golden validation.

## Important grounding from the spec

Primary reference: `docs/internal/agents/dungeon-object-rendering-spec.md`.

- The room build contract is still: floors, layout, main object list, BG2 overlay list, BG1 overlay list, then specials.
- The primary/main stream is not the same concept as editor `LayerType`; stream-vs-buffer semantics must stay explicit.
- If walls disappear or overlay layers float above the room unexpectedly, treat that first as a room-build/compositing validation problem, not as a single-object parser problem.

Related implementation plan: `docs/internal/plans/dungeon-object-rendering-parity-2026-04.md`.

## Test infrastructure gap summary

The biggest gap is between object-level parity and room-level parity.

### Missing layer: ROM-backed room regression fixtures

We need a small fixed set of known-bad rooms, split by symptom:

- corner alias room(s)
- missing-wall room(s)
- unexpected-translucent-layer room(s)
- mask/hole/overlay room(s)

For each fixture room, the harness should record:

- room id
- project/ROM role
- expected failure category
- relevant object ids and room effect/layer-merge values
- a sparse set of per-layer pixel expectations or checksums

### Missing layer: full room-build assertions

The current suite is strong at `ObjectDrawer` and `RoomLayerManager`, but weak at asserting:

- order of layout vs primary vs BG2 overlay vs BG1 overlay passes
- room-effect-driven translucency on real rooms rather than synthetic settings
- per-room interaction between object routing and background compositing

### Missing layer: visual/golden checks

`test/e2e/dungeon_layer_rendering_test.cc` currently stops at interaction/toggle checks and explicitly notes that screenshot verification is still missing.

We need at least one narrow screenshot ROI flow for dungeon rooms before claiming room rendering parity.

## Recommended next validation ladder

1. Pick 3-5 concrete regression rooms and write them down before changing more rendering code.
2. For each room, validate the room header/effect/layer-merge values and object-stream layout against `assets/asm/usdasm/bank_01.asm` behavior.
3. Add ROM-backed tests that render the real room and inspect per-layer buffers before compositing.
4. Add one small screenshot/golden assertion for a single stable room ROI.
5. Only after those fixtures are green should we resume broader refactors in room rendering/layering.

## Suggested first-room categories

When selecting fixtures, prioritize rooms that answer one question each:

- a room where `0x100-0x103` custom corner aliases are visibly wrong
- a room where walls disappear or are over-cleared
- a room where BG2 looks translucent when it should be normal
- a room where BG2 must legitimately be translucent, so we can distinguish bug from intended effect

## Commands run during this triage

These commands are green locally, which is useful evidence that the remaining problem is above routine-level parity:

```bash
ctest --preset mac-ai-unit -R "(CustomObjectRoomRenderTest|RoomLayerManagerTest|DungeonCanvasViewerNavigationTest)" --output-on-failure
ctest --preset mac-ai-unit -R "(RoomObjectRomParityTest|CustomObjectRoomRenderTest|RoomLayerManagerTest)" --output-on-failure
ctest --preset mac-ai-unit -R "(ObjectDrawingComprehensive|ObjectDrawerRegistryReplayTest)" --output-on-failure
```

Notes:

- `RoomObjectRomParityTest` still skips if the expected ROM roles are not configured in the environment.
- Passing command output here should be read as "object and synthetic layer checks are healthy", not "room rendering is solved".

## Immediate guidance for the next pass

- Do not claim the corners are fixed yet.
- Do not add more masking/translucency heuristics until the failing rooms are compared against ASM and real room data.
- Prefer adding one real-room regression at a time over more global refactors.
- Keep the AFS handoff and this local note in sync if the known-bad room list becomes concrete.
