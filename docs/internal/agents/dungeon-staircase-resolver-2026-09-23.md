# Selected-stair destination lookup

Baseline: `f42e0d838`, branch `codex/combined-editor-candidate`.

## User workflow

Select one staircase, open Destinations, and choose **Find selected stair slot**.
A successful vanilla-model lookup opens the corresponding header slot. Lookup
itself changes no room data or undo history. Destination/arrival edits then use
the existing shared metadata command path. Unresolved lookup preserves the route.
The button is disabled without a ROM, with multiple/no selections, or with custom
collision. Active custom object overrides also prevent lookup.

## Implementation

`ResolveVanillaStaircaseSlots` in `zelda3/dungeon/room_collision.*` reuses the
existing collision replay. It checks the selected object's two-column edge and
index tiles in the final collision maps, requires staircase provenance, and
extracts the destination index with `attribute & 3`. It does not reimplement the
family counter state machine or assign slots from vector order.

The result identifies the input object and either a slot or an unresolved reason:
missing/overwritten trigger, overlapping stairs, out-of-room geometry, or more
than four stairs. Partial/ambiguous triggers do not yield editable slots.

The inspector prepends the loaded layout objects before replay. Vanilla layouts
are one primary-list prefix (`$018834`), followed by the room stream (`$01884A`).
`Room::EncodeObjects` groups stream objects stably into lists 0, 1, 2; collision
replay uses that same order. Selection indices are adjusted for the layout prefix.
Lookup happens on an explicit button press, not every UI frame.

This remains a **vanilla collision preview**. It uses the loaded layout snapshot
and normal initial room state. It does not prove behavior of patched engines,
unloaded/externally changed layouts, save-state-dependent behavior, or gameplay.
The connected graph still uses estimated links; automatic clearing remains off.

## Source anchors

USDASM `d53311a`, `bank_01.asm`: layout/stream order `$018834/$01884A`;
family bookkeeping `$01A41B–$01A764`; collision indices `$01B986–$01BB3A`;
header selector `$01C382–$01C3D4`. See the preceding staircase mapping audit for
the counter families and the reason naive placement order is insufficient.

## Verification

```bash
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='RoomCollisionTest.*:*DungeonRoomMetadata*:*DungeonWorkbench*:*DungeonCanvasViewerConnectedGraph*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='RoomCollisionTest.*:*DungeonRoomMetadata*:*DungeonWorkbench*:*DungeonCanvasViewerConnectedGraph*'
```

156 tests across 19 suites passed, with no failures or skips. App and unit target
builds passed; existing duplicate-library warnings remain. Tests include all 15
stair variants in all three layers, up/down indices, reversed-family overwrite,
serialization/decode order, trigger overwrite by pots, overlapping triggers,
capacity and bounds rejection. UI tests exercise selected-object lookup and a
subsequent slot edit in a 260px panel, with no mutation during lookup.

No real-ROM writes, emulator traversal, device deployment or push occurred.
Native acceptance stays in the consolidated manual checklist.

## Next steps

1. Qualify this resolver read-only on real rooms and runtime collision captures,
   including Oracle rooms 73/74/75/83/84/85/86. Identify the exact binary and ROM
   hashes. Do not infer reciprocal doors from registry adjacency.
2. Integrate resolved links into the connected graph only with explicit unresolved
   states. Keep automatic clearing disabled until runtime slot use is qualified.
3. Extend the same typed metadata path for selected-stair authoring; do not add
   another mutation path or silently reorder objects.

## Incoming CLI readback audit

A separate Oracle owner reported incorrect reward/sprite names and pot coordinates
from nightly `20260914-9a0156ff1-tested.e7BeAn`. That installed binary is not this
candidate. Current source triage confirms dungeon chest CLI output calls generic
`GetItemLabel`, while the shared chest inspector already has a distinct receipt-ID
name table. Extract that authoritative vanilla receipt table into the model and
reuse it in CLI output; retain raw IDs and distinguish ROM-hack overrides.

Do not label Oracle receipt 3A as a vanilla pendant without checking the patched
handler. The supplied report also flags sprite 80 and pot position 04CE; qualify
those against the current candidate codec before declaring the nightly defect
fixed. No Oracle files or ROMs were modified during this task.
