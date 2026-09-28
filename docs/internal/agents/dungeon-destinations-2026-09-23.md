# DA-3: shared destination header editing

Baseline: `5028dcc2d`, branch `codex/combined-editor-candidate`.
Implementation and focused checks passed; native/manual acceptance deferred.

## Changes

`inspectors/dungeon_destination_editor.*` is shared by the Workbench room
inspector and standalone room header. It edits pit/warp or four named stair
header slots, displays the target room label, and navigates through the existing
viewer callback. Readonly mode permits navigation but disables edits. A target
of room 000 remains valid. No reverse link or placed-object assignment is created.

`RoomMetadataField::kPitPlane` supplies the missing undoable pit arrival edit.
New typed plane edits accept 00–02. Raw 03 remains intact in loaded snapshots,
interchange and undo. The UI calls it an unmapped vanilla plane. Other metadata,
including destination rooms and reserved bits, is preserved.

## Engine evidence

USDASM source revision `d53311a`:

- `bank_01.asm` `$01B660–$01B68A`: packed pit/stair plane extraction to
  `$063C–$0640`; `$01B68E` onward loads destination room bytes.
- `bank_01.asm` `LayerOfDestination` `$01C31F`: `$0476 = {0,1,1}`;
  `$01C322`: `$EE = {0,0,1}`. Three entries, not four.
- `bank_01.asm` `$01C3CA–$01C3D9`: staircase transition selects a header slot
  through `$0462 & 3`, reads `$7EC001,X` and `$063D,X`.
- `bank_07.asm` `$0794BA`: pit room comes from `$7EC000`;
  `HandleLayerOfDestination` `$0794F1–$0794FF` uses `$063C` to index both tables.

This verifies header selection/plane semantics. It does not verify the
connected-view placement-order-to-slot assumption, custom engines, or gameplay.
The inspector therefore never labels a placed staircase as belonging to a slot.

## Verification

```bash
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonWorkbench*:*DungeonRoomTransfer*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonWorkbench*:*DungeonRoomTransfer*'
```

264 tests across 21 suites passed, zero failures/skips. Includes 260px UI,
correct slot mutation, readonly room-zero navigation, retained legacy values,
invalid-value rejection, metadata lifecycle and synthetic save/reload. Pit-plane
persistence compares every ROM-buffer byte outside the intended header bits.
Manual native rendering and real-ROM traversal remain in the existing checklist.

## Next bounded task

Audit staircase object writers and the runtime `$0462` selector before using
placed object order to drive connection editing or auto-repair. The current
connected-view diagnostics retain their earlier assumptions; no broad DA-3
completion is claimed. Preserve external sprite-editor and Oracle work.
