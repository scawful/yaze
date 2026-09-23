# Dungeon Workbench placement and object inspector handoff

## Context

The 0.8.0 editing work connects the object browser, pending preview, and
selected-object inspector, then completes the remaining authoring domains.
The user requested substantial implementation work
from Codex and delegated extended validation to Claude. Avoid asking the user
to repeat a sequence of manual tests before continuing development.

## Pot-item position repair and branch reconciliation (2026-09-23)

The next source increment after `eb795570f` closes the legacy pot-coordinate
codec audit. `PotItem` now decodes a 64x64 tilemap byte offset: X in bits 1..6,
Y in bits 7..12, eight pixels per tile. Movement preserves bits 0 and 13..15;
`FFFF` remains a terminator, never an authored item. Placement, dragging,
inspector controls, arrow nudges, mixed movement and paste use the shared codec.
Item-only nudges use 8 pixels; selections containing sprites retain their
16-pixel movement constraint. Raw room JSON positions remain compatible.

Engine evidence: USDASM `RoomDraw_SinglePot` at `$01B3AA..$01B3B9` stores the
Y tilemap byte index in `$0540` and ORs `$2000` for the lower tilemap.
`RevealPotItem` at `$01E6DD` masks `$8000` before comparison. A row is 128 bytes.
ZScream's `Rooms/Room.cs` decoder independently matches this layout. Some
USDASM XYZ annotations omit the odd-row bit; executable addressing is authority.

Example: raw `$13CC` is pixel `(304,312)`, not `(816,304)`; `$2660` is
`(384,96)` with its layer bit retained, not Y=608. Moving `$A660` to `(304,312)`
produces `$B3CC`; Undo restores `$A660` exactly.

Validation: 138 focused tests passed, then all 1,024 tests across 50 affected
authoring suites passed with zero failures/skips. Results are recorded in
`/tmp/yaze-pot-authoring-final-tests.log`; the filter adds `PotItemPositionTest.*`
and `DungeonSaveTest.*Pot*` to the existing 1,000-test candidate selection below.
The original broader run had one stale 16-pixel nudge expectation; that test
now asserts the audited 8-pixel step. Application/unit builds passed. Scoped
clang-tidy analyzer checks passed for `item_interaction_handler.cc` and
`dungeon_selection_edit.cc`, including the codec header. This is synthetic
in-memory save/reload evidence, not native UI, ROM-file, or emulator acceptance.

**Integration boundary:** main checkout `codex/dungeon-workbench-bottom-drawer`
has the consolidated overworld changes through `06343577b`, but lacks this
branch's chest, mixed-selection, connection and room-transfer increments.
Do not claim these dungeon features are in that checkout or its installed app.
A non-mutating merge preview of those branches found 15 conflicted files,
including overworld save code, sprite metadata, widgets, Mesen integration,
CI, and protocol/status docs. The preview is `/tmp/yaze-dungeon-integration-preview.txt`.
No merge or remote synchronization occurred during this repair.

**Next:** integrate the branches in an isolated checkout, preserving both the
new overworld sprite writer and reviewed dungeon authoring paths. Resolve
ownership for non-editor conflicts explicitly, build the merged result, and run
both focused verification sets before changing the user's main source checkout.
Keep manual checks deferred: flagged pot selection, odd-row movement,
Undo/Redo, save/reopen, and layer display are added to the user's checklist.

## Current Oracle interchange repair candidate (2026-09-23)

Source: **`339cc9cdf`** on `codex/editor-parity-dungeon-authoring`, following
`30e5c4681` (documentation) / `bb185d1ae` (source). Universe task:
`task_20260923T151812Z_31987`. Worktree:
`/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`.

### Independent qualification received

Claude reported disposable vanilla/Oracle qualification of `30e5c4681` using
Preview → Apply → Undo → Redo → Save → a new file → independent-process reopen
and comparison of all 296 exported rooms. This is external evidence for that
source, not a new real-ROM run by Codex:

- All 296 unopened vanilla rooms export. Pots-only `004 → 011` saves/reopens
  with only the destination changed; the shared terminator repair holds.
- Block clone `09E → 034` correctly rejects its physical ROM overlap before
  mutation. Chest-table and object/sprite stream capacity refusals remain valid.
- Vanilla chest/torch/pot and Oracle core/sprite cases retain their earlier
  positive save/reopen results, with unchanged source input hashes.
- The exit-door finding was retracted after previewing all 81 doors in the
  25 vanilla exit rooms; none offered Create Return Door.
- The 989-test filter passed. The reported full unit run had 4,477 passes and
  three failures: the existing `RomTest.LoadFromFile` fixture issue and two
  order-dependent theme tests. Do not call that full suite green.

The remaining export rejection affected Oracle `033` (463 objects), `075`
(425 objects), `123` (21 chest records), and `124` (23 chest records).

### Repair and limits

Interchange now preserves those counts under independent resource bounds:
4,096 objects, the fixed 168-record shared chest-table capacity, and a 1 MiB
serialized document cap. These are document limits, not game-authoring limits.

Changed object replacements still use the 400-object authoring limit unless the
destination already exceeds it; an oversized destination may keep its count or
shrink. Actual save preflight still checks encoded stream capacity and policy.
Changed chest mappings still require exact correspondence and at most six
combined chest/big-key-lock event slots. Exact unchanged object/chest imports
retain legacy records and destination block identities, including JSON whose
physical slots normalize to `-1`. Unrelated selected domains can transfer.
Arbitrary cloning of a 21/23-chest mapping is **not** enabled or qualified.

### Current verification

**1,000 tests across 49 suites passed, zero failures and zero skipped.** Both
`yaze` and `yaze_test_unit` built. Seven model cases plus two lifecycle cases in
both Standalone/Workbench modes cover the reported counts, export/parse,
unrelated domains, growth rejection, exact no-op imports, dirty state/history,
block identities, and resource limits. The 11 additions use synthetic data.

```sh
cmake --build build/presets/mac-ai --target yaze yaze_test_unit --parallel 4
yaze_qualification_filter='*DungeonRoomMetadata*:*DungeonRoomEdit*:*DungeonChestEditor*:*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*:*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*:*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*:DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:DungeonSaveTest.SaveAllBlocks*:*RoomHeader*:ChestEditTest.*:DungeonSaveTest.LoadObjects*:*DungeonSelectionEdit*:*DungeonConnection*:DungeonStreamAllocatorTest.*:*DungeonRoomTransfer*:*DungeonRoomDocument*:*DungeonFixedStreamReadTest*-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_qualification_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_qualification_filter" --gtest_output=xml:/tmp/yaze-oracle-interchange-tests.xml
/opt/homebrew/bin/bash scripts/dev/editor-guardrails.sh 30e5c4681 339cc9cdf
git diff --check 30e5c4681 339cc9cdf
```

Build/selection/result logs: `/tmp/yaze-oracle-interchange-build.log`,
`/tmp/yaze-oracle-interchange-selected.log`,
`/tmp/yaze-oracle-interchange-tests.log`. Changed-line formatting and pre-commit
checks passed. No new real-ROM, emulator, manual GUI, WASM, installed-app, or
remote-CI result is claimed for `339cc9cdf`.

**Next qualification:** rerun all 296 Oracle exports on disposable copies,
roundtrip the four reported rooms through JSON, and verify unchanged import
plus unrelated-domain transfer. Confirm unsupported replacement still refuses
without mutation. Then qualify representative chests/torches/pots in game.
Preserve the separate uncommitted `validate-placement` harness.

## Prior qualification repair candidate (2026-09-23)

Source: **`bb185d1ae`**, following `2520aa6b0` / `df1cc4f2d`, on
`codex/editor-parity-dungeon-authoring`. Worktree:
`/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`. Universe task:
`task_20260923T140908Z_16625`. The prior candidate and its evidence remain below
as history. Its requested disposable-ROM requalification was subsequently
reported by Claude; see the current section above for results and remaining
checks. No merge, runtime, or release readiness is claimed.

### Qualification findings and disposition

1. **Block/torch overlap fixed.** Vanilla ROM has 99 four-byte block records
   before `kTorchData` at PC `0x2736A`; the loader's 128-record WRAM capacity is
   not a ROM allocation. Both `SaveAllBlocks` overloads validate existing and
   replacement payload spans against the torch table and its length pointer
   before writes or identity/dirty-state changes. Every active page is checked.
   A separately repointed fourth page can still support 128 records. Synthetic
   native-layout regressions cover 98 → 99 success, 99 → 100 rejection, malformed
   existing length, relocated pages, and room-transfer preview rejection in both
   viewer modes with unchanged ROM/data/history.
2. **Shared pot terminators fixed.** The strict fixed-bank reader can include
   the next room's empty `FFFF` list as the current room's terminator only at a
   complete three-byte record boundary and inside the existing storage/bank/ROM
   limits. It cannot consume the next nonempty list. Unopened-room export and
   preview regressions exercise this path; raw position flags remain intact.
   A separate read-only raw-byte survey found all 67 previously rejected vanilla
   lists use this exact pattern (296 readable under the corrected rule). This
   survey is format evidence, not an application export/save test.
3. **Overworld exit report not reproduced; not claimed fixed.** Existing guards
   reject explicit exit families and ordinary doors sharing a lane with the
   vanilla exit marker. Added model, ImGui, and lifecycle regressions verify no
   Create Return Door / Open Target Room offer for covered exits and rejection
   through preview/stale apply without mutation. The confirmed vanilla case is
   room `055`, south slot `6`; another internal door remains connectable. A room
   having an overworld-exit entry does not identify which of its doors is the
   exit. If the report persists, supply the exact ROM identity, room, slot, and
   type so the unsupported pattern can be checked. Production connection rules
   were not changed in this repair.
4. **Scratch GameData moved to the heap.** Its approximately 1.77 MB allocation
   no longer contributes to the preview stack frame. The owner outlives the
   detached editor. The macOS build passes; WASM execution has not been tested.
5. **Shared-header recovery added.** A structured shared-header failure offers
   **Preview without room properties** when at least one other domain is
   selected. Clicking it explicitly clears room properties/destination copying
   and reruns preview; it never applies automatically. A properties-only request
   does not offer an empty replacement. The header-overlap guard remains active.
6. **Residual chest records preserved for interchange.** Vanilla rooms `005`,
   `016`, `05B`, and `0B3` have records without corresponding authored chest
   objects. A raw room/layout survey and the engine's ordinal chest lookup
   support treating these as residual records, not another missing chest type.
   JSON serialization/parsing now preserves these records while retaining field
   validation. Object/chest replacement still requires strict correspondence;
   unrelated selected domains can transfer. No reward is dropped or inferred.
7. **Portable block slot normalization added.** JSON emits `block_load_order: -1`; parsing accepts bounded older v1 values and normalizes them to `-1`.
   Physical slots are save-time identities, not authored room content. Internal
   snapshots, stale checks, and undo keep exact identities. This prevents table
   repacking from appearing as unrelated authored-room changes in JSON diffs.

### Qualification repair verification

**989 tests across 49 suites, all passed, zero failures and zero skipped.**
Both `yaze` and `yaze_test_unit` built successfully. The filter below was
explicitly discovered before execution; it adds 21 cases to the prior 968-test
candidate. Full unit-suite and real-ROM application disk qualification were not
rerun in this repair pass.

```sh
cmake --build build/presets/mac-ai --target yaze yaze_test_unit --parallel 4 > /tmp/yaze-transfer-qualification-fixes-build.log 2>&1
yaze_qualification_filter='*DungeonRoomMetadata*:*DungeonRoomEdit*:*DungeonChestEditor*:*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*:*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*:*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*:DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:DungeonSaveTest.SaveAllBlocks*:*RoomHeader*:ChestEditTest.*:DungeonSaveTest.LoadObjects*:*DungeonSelectionEdit*:*DungeonConnection*:DungeonStreamAllocatorTest.*:*DungeonRoomTransfer*:*DungeonRoomDocument*:*DungeonFixedStreamReadTest*-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_qualification_filter" > /tmp/yaze-transfer-qualification-fixes-selected.log 2>&1
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_qualification_filter" --gtest_output=xml:/tmp/yaze-transfer-qualification-fixes-tests.xml > /tmp/yaze-transfer-qualification-fixes-tests.log 2>&1
/opt/homebrew/opt/llvm/bin/clang-tidy -p build/analysis/mac-ai \
  --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' \
  --header-filter='(dungeon_room_transfer|dungeon_editor_v2_room_transfer|dungeon_room_transfer_editor|dungeon_stream_allocator)\.(cc|h)$' \
  src/app/editor/dungeon/dungeon_room_transfer.cc \
  src/app/editor/dungeon/dungeon_room_transfer_json.cc \
  src/app/editor/dungeon/dungeon_editor_v2_room_transfer.cc \
  src/app/editor/dungeon/inspectors/dungeon_room_transfer_editor.cc \
  src/zelda3/dungeon/dungeon_stream_allocator.cc \
  src/zelda3/dungeon/room.cc > /tmp/yaze-transfer-qualification-fixes-analyzer.log 2>&1
/opt/homebrew/bin/bash scripts/dev/editor-guardrails.sh df1cc4f2d bb185d1ae
git diff --check df1cc4f2d bb185d1ae
```

The analysis database uses disabled precompiled headers (configuration command
in the historical section below). This six-translation-unit analyzer run passed within its
explicit header filter; eight diagnostics in excluded headers were suppressed.
After the final properties-only recovery refinement, the UI translation unit
was analyzed again successfully, with the same analyzer checks and a header
filter of `(dungeon_room_transfer|dungeon_room_transfer_editor)\.(cc|h)$`;
output: `/tmp/yaze-transfer-qualification-fixes-ui-analyzer.log`.
The earlier broader 13-unit scan's two unchanged diagnostics remain recorded
below. Do not describe the repository or full tidy policy as clean.
Changed-line formatting, editor guardrails, and pre-commit checks passed.

Local app: `build/presets/mac-ai/bin/Debug/yaze.app`. Executable SHA-256:
`e0a4136646a709c6370f3c391c667a2d38ccbc73a71e55d99b2eeb83b3d6c3ea`.
The binary was built from the source committed as `bb185d1ae` before committing,
so embedded Git metadata may name its parent. It was not installed or substituted
into Barista. Personal ROM/save files were not modified; no remote CI or emulator
run is claimed. The read-only two-MiB vanilla survey input had SHA-256
`b14aff6012f55827b67607e73ea0666269ed97f1053ce958d94296fe04854a72`.

### Independent qualification handoff

Claude reported the prior `df1cc4f2d` candidate passed the 968-test filter and
positive disposable-ROM save/reopen cases for vanilla chests `01C → 01A`,
torches `042 → 022`, pot JSON `038 → 026`, and Oracle `01B → 019` core /
`00A → 00C` sprites. Those are external results for the prior source, not new
results for this repair. The block overflow and strict pot read were reproduced
there. The 168-record chest rejection also left the live ROM unchanged.

Rerun in an isolated qualification worktree pinned to `bb185d1ae`:

1. Native block clone `09E → 034`, objects/doors/sprites/pots (`domains 15`),
   must now fail preview because 99 → 100 records overlaps torches. Assert exact
   ROM bytes, source/target data, selections/history, and torch bytes unchanged.
2. Export all 296 vanilla rooms without opening them first, including `004`,
   `00B`, `011`, `060`, and residual-chest rooms `005`, `016`, `05B`, `0B3`.
   Preserve the residual records; object-domain replacement with invalid chest
   correspondence must still fail explicitly.
3. Repeat the positive vanilla/Oracle cases above through Preview → Apply →
   Undo → Redo → editor Save → a new disposable file → independent-process
   reopen. Compare authored domains, including torch/block behavior, and assert
   source input hashes unchanged. Portable JSON normalizes physical block slots.
4. Exercise `01A` / `019` shared-header rejection and explicit recovery without
   properties. Keep the target properties unchanged. Bind the Oracle project,
   dependencies, feature flags, and protected-write policy in the same order as
   the application; its `save_dungeon_water_fill_zones=false` is significant.
5. Recheck capacity/policy refusals and obtain a specific exit-door reproducer
   if one still exists. Emulator traversal, manual GUI acceptance, packaged
   delivery, and remote CI remain separate gates.

## Prior authored-room clone/import candidate (2026-09-23)

Candidate: `2520aa6b0` on
`codex/editor-parity-dungeon-authoring`, following normal-door source
`be973563f` and documentation `6a1196218`. Worktree:
`/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`. Universe task:
`task_20260923T131011Z_25069`.

Verification: **968 tests across 49 suites; all passed, zero failures and
zero skipped**. Both `yaze` and `yaze_test_unit` built successfully. Expanded
scoped analysis examined 13 translation units and exited 1 on two existing
diagnostics, detailed below. The older 807-test connection result remains
history for its exact commit, not new transfer evidence.

The shared Room controls/dialog expose **Clone**, **Copy Room JSON**, and
**Import JSON** from text/clipboard. Use **Preview Replacement** to inspect
selected-domain counts/policy, then **Apply Replacement** for one Undo/Redo
action. Read-only views may copy JSON but cannot replace data.

1. The versioned document is `format: "yaze.room"`, `version: 1`, bounded to
   1 MiB. Seven selectable domains are objects/chest rewards, doors, sprites,
   pot items, room properties, custom collision, and water fill. Core defaults
   select the first five; collision/water require explicit inclusion.
2. Default clone preserves target destination room/plane fields. **Copy
   destination links** opts into their numeric values. It does not redirect
   incoming links or create returns. Properties include the pit destination
   plane; objects carry chest correspondence and torch/block metadata.
3. Graphics, layout, and message references stay numeric. Referenced assets,
   entrances, global pit-damage membership, palettes, project source, and runtime
   state are not packaged. Destination reserved header bits and sprite sort
   byte remain destination-owned. This is authored-room data exchange, not a
   complete portable project asset pack.
4. Preview validates detached data and serializes a scratch ROM/editor through
   the real room save path. Shared tables include current materialized/dirty
   state. Capacity/allocation, disabled-save domains, and actual changed-byte
   manifest policy are checked before live publication. Apply rejects stale
   source or target data, checks again after gesture finalization, then uses the
   existing undo manager. Preview writes no live ROM bytes, disk file, or history.
5. Cloned water uses a valid target/free SRAM bit without changing other rooms'
   assignments. Shared water-table saves merge unmaterialized saved zones.
   Preserve these regressions: a lower-numbered destination cannot take its
   source's mask, and editing one zone cannot delete an unopened saved zone.

### Review corrections included with this candidate

The external review's four blocking reports were treated as correctness work
before final verification. Recheck these paths during independent acceptance:

1. Pot-item type-only edits retain the exact encoded position word and use the
   existing mutation/undo path. Raw position flags are preserved by room
   transfer. The follow-up pot-coordinate repair above audits the engine format
   and corrects display/drag/mixed movement with shared, flag-preserving encoding.
2. Pending chest object and reward edits cannot save with split feature flags.
   Apply Room also refuses to publish another room's rewards without that
   room's pending objects. Reward-only changes remain independently saveable;
   enable both domains and save/apply the owning room for structural edits.
3. Object-specific Delete controls affect objects only. Mixed keyboard Delete
   keeps the existing mixed-selection behavior. Test both with an object,
   sprite, door, and pot item selected together.
4. New dungeon spriteset choices stop at `0x4F`, because the shared sprite table
   includes a 64-entry prefix. Preserve existing unknown values on undo or when
   excluding room properties; do not silently clamp loaded data.

The room-only preflight deliberately skips the live rollback transaction:
`Rom scratch(*rom_)` and a detached editor own every serializer mutation, and
both are discarded on any outcome. The private `SaveRoomImpl(..., true)` path
has no live caller. Actual Apply Room still uses `RunWithSaveTransaction`.
Do not reuse the room-only path for live saves or weaken that ownership boundary.

Additional transfer regressions cover header storage aliases, malformed torch
framing, water-bit ownership, and Save → Undo → Save after block-table compaction.
Block slot numbers are current serialization identities: historical slots from
another room must be reconciled, while duplicate claims within one room reject.

### Room transfer architecture

- [`dungeon_room_transfer.h`](../../../src/app/editor/dungeon/dungeon_room_transfer.h)
  and [implementation](../../../src/app/editor/dungeon/dungeon_room_transfer.cc):
  authored snapshots, comparison, domain options, pure planning, publication.
- [`dungeon_room_transfer_json.cc`](../../../src/app/editor/dungeon/dungeon_room_transfer_json.cc):
  strict version/field/count validation before mutation. Legacy template JSON
  is a different format; do not silently accept it as this document.
- [`dungeon_editor_v2_room_transfer.cc`](../../../src/app/editor/dungeon/dungeon_editor_v2_room_transfer.cc):
  complete source hydration, scratch persistence preflight, stale checks, and
  target-bound undo. Detached snapshots contain authored fields rather than
  borrowed graphics buffers. Keep scratch dependencies alive through destruction.
- [`dungeon_room_transfer_editor.cc`](../../../src/app/editor/dungeon/inspectors/dungeon_room_transfer_editor.cc):
  shared clone/import UI and preview invalidation. Context/source/options changes
  discard a preview; room navigation must not apply a draft to another room.
- [`dungeon_stream_allocator.h`](../../../src/zelda3/dungeon/dungeon_stream_allocator.h):
  strict object/sprite/pot stream reads before legacy lazy loaders. Exact shared
  starts are allowed; missing terminators cannot borrow another room's bytes.
  No allocator-policy expansion is implied by these read APIs.

### Room transfer verification commands

The final discovery selected 968 tests from 49 suites, including strict JSON
parameterized cases, transfer lifecycle/UI, all block-save cases, and existing
undo/selection/connection/room-save regressions. All 968 ran and passed with
zero skipped. The app/unit build and editor guardrails passed.

```sh
cmake --build build/presets/mac-ai --target yaze yaze_test_unit --parallel 4 > /tmp/yaze-room-transfer-final-build.log 2>&1
yaze_room_transfer_filter='*DungeonRoomMetadata*:*DungeonRoomEdit*:*DungeonChestEditor*:*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*:*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*:*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*:DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:DungeonSaveTest.SaveAllBlocks*:*RoomHeader*:ChestEditTest.*:DungeonSaveTest.LoadObjects*:*DungeonSelectionEdit*:*DungeonConnection*:DungeonStreamAllocatorTest.*:*DungeonRoomTransfer*:*DungeonRoomDocument*:*DungeonFixedStreamReadTest*-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_room_transfer_filter" > /tmp/yaze-room-transfer-final-selected-tests.log 2>&1
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_room_transfer_filter" --gtest_output=xml:/tmp/yaze-room-transfer-final-tests.xml > /tmp/yaze-room-transfer-final-tests.log 2>&1
cmake --preset mac-ai -B build/analysis/mac-ai -G Ninja \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DYAZE_ENABLE_CLANG_TIDY=OFF
/opt/homebrew/opt/llvm/bin/clang-tidy -p build/analysis/mac-ai \
  --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' \
  --header-filter='(dungeon_room_transfer|dungeon_editor_v2_room_transfer|dungeon_room_transfer_editor|dungeon_stream_allocator|dungeon_editor_v2_persistence|dungeon_room_edit|item_interaction_handler|dungeon_entity_inspector|dungeon_workbench_content|dungeon_workbench_room_inspector|object_editor_content|room)\.(cc|h)$' \
  src/app/editor/dungeon/dungeon_room_transfer.cc \
  src/app/editor/dungeon/dungeon_room_transfer_json.cc \
  src/app/editor/dungeon/dungeon_editor_v2_room_transfer.cc \
  src/app/editor/dungeon/inspectors/dungeon_room_transfer_editor.cc \
  src/zelda3/dungeon/dungeon_stream_allocator.cc \
  src/app/editor/dungeon/dungeon_editor_v2_persistence.cc \
  src/app/editor/dungeon/dungeon_room_edit.cc \
  src/app/editor/dungeon/interaction/item_interaction_handler.cc \
  src/app/editor/dungeon/inspectors/dungeon_entity_inspector.cc \
  src/app/editor/dungeon/workspace/dungeon_workbench_content.cc \
  src/app/editor/dungeon/workspace/dungeon_workbench_room_inspector.cc \
  src/app/editor/dungeon/inspectors/object_editor_content.cc \
  src/zelda3/dungeon/room.cc > /tmp/yaze-room-transfer-final-analyzer.log 2>&1
/opt/homebrew/bin/bash scripts/dev/editor-guardrails.sh 6a1196218 2520aa6b0
git diff --check 6a1196218 2520aa6b0
```

The 13-translation-unit analyzer run reports two existing diagnostics in
unchanged code (confirmed against parent `6a1196218`):

- `dungeon_workbench_content.cc:257`: `clang-analyzer-deadcode.DeadStores` on
  the final `deficit -= shrink` assignment.
- `room.h:71`: `clang-analyzer-optin.performance.Padding` on `LayerMergeType`.

The command exits 1 with warnings-as-errors. No analyzer diagnostic points to
changed code; this is not a clean full-tidy result. A later mechanical cleanup
can remove the dead store and review field-layout/ABI implications before
reordering the struct. Do not disable checks or change the tidy policy to hide
these findings. The earlier five-file analyzer pass preceded the review fixes
and is not the final candidate's full analysis result.

Source + Focused/synthetic persistence evidence does not establish the complete
application disk transaction, vanilla/Oracle game behavior, manual UX, remote
CI, installed delivery, or packaged acceptance. No native file-picker or
cross-project asset compatibility is claimed. Preserve installed apps and all
personal ROM/save files; qualification uses identified disposable copies.

The local app is `build/presets/mac-ai/bin/Debug/yaze.app`. Its executable
SHA-256 for this run is `8a81ce9280a0e67bb50aec403adc87392421c5d1dc7768418c6c250e3ce75195`. It was built from the source tree committed as
`2520aa6b0` before the commit was created, so embedded Git metadata may name
the parent. It has not been installed or substituted into the Barista launcher.
No personal ROM/save file was modified and no remote CI run is claimed.

### Next bounded assignments

1. **DA-5 qualification first.** Pin source and built artifact, discover selected
   tests, then exercise the application save-to-disk path on disposable supported
   ROM profiles. Check clone/import → undo/redo → Save → independent reopen;
   compare decoded domains and unrelated byte ranges. Include rejected capacity,
   project policy, and save-failure rollback. Record runtime room traversal,
   sprites, chests, and any included collision/water behavior separately from
   successful serialization. Stop at a concrete defect, fix it with focused
   evidence, then resume the same qualification packet.
2. **DA-4 project-file/asset compatibility follows.** Specify project-relative
   storage and resource identity/remapping before adding file import/export or
   claiming cross-project reuse. Reuse the versioned document, planner, and
   preflight; no direct clear/refill or second history system.
3. **DA-3 stairs/pits follow verified rules.** Audit each engine rule and supported
   ROM layout, preserve intentional one-way connections, and require persistence
   plus runtime evidence per family. Normal-door adjacency is not a staircase
   routing rule.

The maintainer's [coding entry guide](../../public/developer/dungeon-editor-contribution-guide.md)
provides bounded status-bar/selector exercises and agent assignment templates.
Those proposed human-owned UI tasks remain available; agents should not absorb
them during qualification or mechanical cleanup. Update this handoff and the
canonical plan instead of adding another roadmap.

## Prior reciprocal normal-door candidate (2026-09-23)

Candidate: `be973563f` on
`codex/editor-parity-dungeon-authoring`, following mixed-selection source
`eac49e2bd` and documentation `0815b405c`. Worktree:
`/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`. Universe task:
`task_20260923T053944Z_6854`.

Verification: **807 tests across 44
suites — passed with zero failures and zero skips**. App/unit builds: passed.
Scoped Clang analyzer: passed for the six selected implementation files and their explicit header scope. Retain the prior 640-case
mixed-selection evidence below as history for its exact source; do not treat
those results as fresh evidence for this increment.

1. Select an ordinary normal door on an outer wall. The shared Workbench and
   standalone door inspector draws both endpoints, offers upper/lower layer,
   and commits only on **Create Return Door** or **Update Pair**. Opening the
   destination is separate navigation. Preview itself creates no history or save
   dirtiness. "Apply" below describes the operation, not a button label.
2. Create a missing return door or change an existing normal pair's layer
   together. North/west slots `0..5` pair with south/east `6..11` by exactly six.
   Reject row/page wrap, ambiguous passage records, unsupported types, internal
   seams, invalid slots, and exhausted destination door capacity.
3. Reuse the selection transaction's batch undo. Validate both rooms before
   publishing either, retain all unrelated authored domains, and dirty only
   changed door streams. Undo targets the original rooms without navigating.
4. Keep intentional one-way links intact until the author applies a connection.
   This slice does not repair staircase/pit diagnostics, special doors, internal
   seams, or arbitrary destination mappings. DA-3 remains partial.

### Connection architecture and next implementation

- [`dungeon_connection_edit.h` / `.cc`](../../../src/app/editor/dungeon/dungeon_connection_edit.h)
  owns the pure request/plan, exact slot pairing, adjacency constraints, and
  passage-conflict checks. Normal-door adjacency is engine behavior, not an
  arbitrary room-link field.
- [`dungeon_editor_v2_connection_edits.cc`](../../../src/app/editor/dungeon/dungeon_editor_v2_connection_edits.cc)
  checks ROM and room identity, strictly reads an unopened destination stream
  before loading it, and refuses to overwrite a partially loaded dirty room.
  Clean WaterFill overlays attached by `ReloadWaterFillZones` survive loading:
  tile contents, SRAM mask, and cached tile count remain intact. Three additional
  lifecycle tests run in both viewer modes, including saving another zone and
  confirming that the target's clean zone remains in the shared table, plus
  rejection of a dirty partial WaterFill state.
  It recomputes the preview before Apply and after finishing any older gesture.
  Stale source or destination lists reject instead of overwriting newer edits.
- [`dungeon_editor_v2_selection_edits.cc`](../../../src/app/editor/dungeon/dungeon_editor_v2_selection_edits.cc)
  extends the existing selection action to a batch. `RestoreSelectionEditBatch`
  validates every endpoint before publishing any list, then refreshes existing
  views. There is no second undo manager or independent connection document.
- [`dungeon_connection_editor.cc`](../../../src/app/editor/dungeon/inspectors/dungeon_connection_editor.cc)
  provides the shared inspector and diagram. Its headless tests exercise Apply,
  rejection, read-only behavior, and context changes; these are not human visual
  acceptance or runtime traversal evidence.
- [`ReadDungeonObjectStream`](../../../src/zelda3/dungeon/dungeon_stream_allocator.h)
  reuses the strict object-stream parser for read-only lazy-load validation.
  Keep its malformed-pointer/stream tests in the connection verification lane.

At this historical checkpoint, the next implementation was DA-4 room
clone/import. The authored-room transfer increment above implements its bounded
data-exchange slice; portable asset compatibility and full qualification remain.
DA-3 stairs/pits/special-family work still requires its own engine rules.

### Connection verification commands

Run from the integration worktree. Confirm discovery is nonempty and includes
the new suites before executing this filter. These commands passed on the source
candidate above. Discovery selected all
807 cases before execution; the XML reports 44 suites, zero failures, and zero
skips. Connection-specific coverage comprises 67 planner cases, 16 ImGui cases,
and 38 lifecycle cases across both viewer modes. The lane also includes all 41
stream allocator tests, including eight new strict-reader cases.

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4 > /tmp/yaze-connection-final-build.log 2>&1
yaze_connection_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonChestEditor*'
yaze_connection_filter+=':*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*'
yaze_connection_filter+=':*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*'
yaze_connection_filter+=':*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*'
yaze_connection_filter+=':DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:*RoomHeader*'
yaze_connection_filter+=':ChestEditTest.*:DungeonSaveTest.LoadObjects*:*DungeonSelectionEdit*'
yaze_connection_filter+=':*DungeonConnection*:DungeonStreamAllocatorTest.*'
yaze_connection_filter+='-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_connection_filter" > /tmp/yaze-connection-final-selected-tests.log 2>&1
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_connection_filter" --gtest_output=xml:/tmp/yaze-connection-final-tests.xml > /tmp/yaze-connection-final-tests.log 2>&1
```

Scoped analysis passed with a PCH-free compilation database:

```sh
cmake --preset mac-ai -B build/analysis/mac-ai -G Ninja -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DYAZE_ENABLE_CLANG_TIDY=OFF
/opt/homebrew/opt/llvm/bin/clang-tidy -p build/analysis/mac-ai --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' --header-filter='(dungeon_connection_edit|dungeon_editor_v2_connection_edits|dungeon_editor_v2_selection_edits|dungeon_connection_editor|door_interaction_handler|dungeon_stream_allocator)\.(cc|h)$' src/app/editor/dungeon/dungeon_connection_edit.cc src/app/editor/dungeon/dungeon_editor_v2_connection_edits.cc src/app/editor/dungeon/dungeon_editor_v2_selection_edits.cc src/app/editor/dungeon/inspectors/dungeon_connection_editor.cc src/app/editor/dungeon/interaction/door_interaction_handler.cc src/zelda3/dungeon/dungeon_stream_allocator.cc > /tmp/yaze-connection-final-analyzer.log 2>&1
```

The six selected translation units are
`dungeon_connection_edit.cc`, `dungeon_editor_v2_connection_edits.cc`,
`dungeon_editor_v2_selection_edits.cc`, `dungeon_connection_editor.cc`,
`door_interaction_handler.cc`, and `dungeon_stream_allocator.cc`. The own-header
filter is
`(dungeon_connection_edit|dungeon_editor_v2_connection_edits|dungeon_editor_v2_selection_edits|dungeon_connection_editor|door_interaction_handler|dungeon_stream_allocator)\.(cc|h)$`.
The initial wider analyzer scope reports known `LayerMergeType` padding debt in
`room.h`; this legacy-header finding remains excluded by the explicit scope,
not fixed or covered by a blanket clean claim. This is not a full-repository
tidy result. Editor guardrails, changed-line formatting, and source pre-commit
checks passed. Guardrail output: `/tmp/yaze-connection-final-guardrails.log`.

Object-stream allocation remains Save-time for ordinary connection edits. This
candidate does not establish a vanilla/Oracle full application disk workflow,
game traversal, manual UX, remote CI, installed-app delivery, or packaged release
acceptance. Preserve the user's installed app, ROMs, and active sessions.

## Prior mixed-selection candidate (2026-09-23)

Candidate: `eac49e2bd` on `codex/editor-parity-dungeon-authoring`,
following chest source `478206247`. Worktree:
`/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`. Universe task:
`task_20260923T050800Z_19565`. Verification: **640 focused tests
across 39 suites passed with zero failures and zero skips**. App and unit
builds, editor guardrails, changed-line formatting, and pre-commit checks passed.
The new suites contain 34 planner cases and 36 lifecycle runs across both viewer
modes, plus one net new coordinator case.
Keep the earlier candidate evidence below as history for its exact source.

- Delete, duplicate, cut, copy, paste, nudge, and group drag cover objects,
  doors, sprites, and pot items together. A mutation is one undo action across
  all affected collections. Copy creates no history; duplicate preserves the
  prior clipboard. Stateful chest objects retain their reward records.
- Plan every participating domain before applying anything. Stale indices,
  invalid encodings, unsupported translations, bounds, and supported count
  limits reject the whole edit. Failed copy retains the previous clipboard;
  Cut deletes only after successful copy. Reuse the global chest planner and
  exact chest-region write-policy preflight when objects/chests participate.
- Objects use an 8-pixel shared movement grid; selections with sprites or pot
  items use 16 pixels. Preserve relative spacing; do not clamp members
  independently. Doors require an exact same-direction wall slot. Door-only
  arrow nudges advance slots; duplicate and paste without a canvas target keep
  a selection containing doors at its original anchors.
- Group drag contributes one history action at release. Returning to the start
  preserves existing redo history and save dirtiness. Save, room navigation,
  discrete commands, and undo/redo finish active gestures. A stale candidate
  cannot overwrite a single-entity preview committed while finishing a gesture.
  `BeginSaveTransaction` and `Save` finish gestures before capturing dirty state.
- Workbench, standalone inspector, canvas menus, and keyboard commands share
  these paths. No new parallel inspector or undo manager was introduced.

### Mixed-selection architecture and historical next implementation

1. [`dungeon_selection_edit.h` / `.cc`](../../../src/app/editor/dungeon/dungeon_selection_edit.h)
   owns the pure plan and clipboard types. `PlanDungeonSelectionEdit` returns
   complete before/after state plus actual changed-domain bits;
   `CopyDungeonSelection` copies authored values and paired chest rewards.
   `PlanChestObjectEdit` remains the chest correspondence authority.
2. [`DungeonObjectInteraction`](../../../src/app/editor/dungeon/dungeon_object_interaction.cc)
   owns the unified clipboard and UI commands.
   [`InteractionCoordinator::CommitSelectionEdit`](../../../src/app/editor/dungeon/interaction/interaction_coordinator.cc)
   plans the request and invokes the configured editor publication callback.
   Continuous drag preserves handler selection until the gesture finishes.
3. [`DungeonEditorV2::CommitSelectionEdit`](../../../src/app/editor/dungeon/dungeon_editor_v2_selection_edits.cc)
   preflights global chest state, closes previous history safely, verifies that
   planned source data is current, publishes all affected collections, and uses
   the existing undo manager. Restore targets the original room and retains
   another active room's selection. Sprite snapshots contain authored fields,
   not borrowed preview buffers.
4. Keep the standalone handler fallback testable, but production changes go
   through the configured editor callback. Do not reintroduce sequential
   per-domain mutation into mixed operations or add a second clipboard.

At this historical checkpoint, the next bounded implementation was reciprocal
connection authoring. The ordinary normal-door and authored-room transfer slices
are now recorded above. Preserve intentional
one-way links; a diagnostic is not permission to repair another room.
DA-1/DA-2 remain partial for remaining domains and controls; DA-5 qualification
continues independently on the exact candidate.

### Mixed-selection verification commands

Run from the integration worktree. Discover the selected suites before running
this filter; optional ROM parity fixtures remain a separate lane.

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4 > /tmp/yaze-mixed-final-build.log 2>&1
yaze_mixed_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonChestEditor*'
yaze_mixed_filter+=':*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*'
yaze_mixed_filter+=':*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*'
yaze_mixed_filter+=':*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*'
yaze_mixed_filter+=':DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:*RoomHeader*'
yaze_mixed_filter+=':ChestEditTest.*:DungeonSaveTest.LoadObjects*:*DungeonSelectionEdit*'
yaze_mixed_filter+='-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_mixed_filter" > /tmp/yaze-mixed-final-selected-tests.log 2>&1
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_mixed_filter" --gtest_output=xml:/tmp/yaze-mixed-final-tests.xml > /tmp/yaze-mixed-final-tests.log 2>&1
```

Scoped analysis uses the existing PCH-free database and the two new planner/
publication translation units:

```sh
/opt/homebrew/opt/llvm/bin/clang-tidy -p build/analysis/mac-ai --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' --header-filter='(dungeon_selection_edit|dungeon_editor_v2_selection_edits)\.(cc|h)$' src/app/editor/dungeon/dungeon_selection_edit.cc src/app/editor/dungeon/dungeon_editor_v2_selection_edits.cc > /tmp/yaze-mixed-final-analyzer.log 2>&1
```

Scoped analyzer checks passed for these two modules; findings outside that
header scope are not covered. This is not a full-repository tidy claim.
Focused model/editor lifecycle and synthetic in-memory save/reload tests
do not establish rendered UI acceptance or a complete application disk workflow.
Object-stream space/allocation remains a Save-time gate. Author-time manifest
preflight here covers chest ranges; do not claim every-domain preflight.
Vanilla/Oracle GUI-to-disk save/reopen, runtime traversal, manual UX, remote CI,
installation, and packaged Release acceptance remain pending. Preserve the
user's installed app and active ROM sessions during automated verification.

## Prior compound chest candidate (2026-09-23)

Candidate: `478206247` on
`codex/editor-parity-dungeon-authoring`, following `aeb0b1200`. Worktree:
`/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`. Universe task:
`task_20260923T034804Z_2446`. Verification: **569 tests across 37 suites passed, with zero failures and zero skips**.
App and unit builds passed. Scoped Clang analyzer checks passed for the two new mutation
modules. Preserve the earlier evidence below as history for its exact source.

- Add small chest / Add big chest starts the canvas placement tool using the
  object's canonical encoded size. A placed `F99`/`FB1` object receives a
  matching contents record, initially receipt ID `34` (1 Rupee). Preview-only
  actions do not mutate the room.
- Delete and small/big conversion change the object and contents together.
  Each operation uses one existing object undo action, now extended to include
  chest contents. Unknown reward bytes survive edits and undo.
- Ordinary object duplicate/copy/paste preserves reward bytes. Reordering
  objects and changing room-list layers remaps records without exchanging their
  rewards. Open/minigame chest graphics are not ordinary stateful chests.
- The shared inspector follows a changed canvas chest selection and can select
  its corresponding object. A manually chosen record remains selected until
  the canvas selection changes. Existing mapping mismatches retain reward-only
  editing and reject structural operations with an explanation.
- Structural edits validate six shared chest/big-key-lock (`F98`) event slots,
  with chests before locks in encoded room-list order. The existing global
  168-record planner includes unopened physical records and other rooms' dirty
  contents, and preflight applies manifest policy to its exact write ranges.
- Object loading no longer consumes persistent contents records and preserves
  unsaved contents during object-graphics reload. Undo affects the original
  room without changing another active room's selection.

Object-stream allocator capacity is still validated by Save. A successful
placement is not a guarantee that stream growth fits or that allocator-owned
space is available. Full vanilla/Oracle GUI-to-disk save/reopen, game behavior,
manual interaction acceptance, remote CI, and packaged acceptance remain
unqualified for this exact candidate. Do not install over the user's app or
interrupt an active ROM session as part of automated verification.

### Architecture and next implementation

Read this path in order to understand the transaction without tracing every
ImGui callback:

1. [`chest_edit.h` / `.cc`](../../../src/zelda3/dungeon/chest_edit.h) defines the
   pure planner. `ChestIndexForObject` maps objects through encoded list order;
   `PlanChestObjectEdit` combines candidate objects, source indices, and optional
   clipboard rewards into a candidate contents vector. It writes no ROM bytes.
2. [`TileObjectHandler::CommitCandidate`](../../../src/app/editor/dungeon/interaction/tile_object_handler.cc)
   stages the full object edit and asks the planner for contents before emitting
   mutation hooks. Failure leaves both domains and history unchanged.
3. [`DungeonEditorV2::PreflightObjectMutation`](../../../src/app/editor/dungeon/dungeon_editor_v2_chest_edits.cc)
   applies global table capacity and manifest checks. `EditChest` and
   `DeleteChest` use the same boundary; reward-only edits remain available when
   an existing object/contents mapping cannot be established.
4. [`DungeonObjectsAction`](../../../src/app/editor/dungeon/dungeon_undo_actions.h)
   stores object and contents snapshots together. The existing
   [viewer undo hooks](../../../src/app/editor/dungeon/dungeon_editor_v2_undo.cc)
   capture before publication and restore both domains on undo/redo. This
   extends the current undo manager; it does not add a second framework.
5. [`DrawDungeonChestEditor`](../../../src/app/editor/dungeon/inspectors/dungeon_chest_editor.cc)
   is the shared UI for both presentations. Keep preview state owned by the
   interaction handler, and use the model's mapping rather than raw vector
   position to identify a chest.

At this historical checkpoint, mixed-selection atomicity was the next bounded
package. The later mixed-selection increment above supersedes that assignment
and retains the chest planner and rewards. Do not restart completed chest work
or create another inspector/transaction system. DA-5 qualification remains
independent.

### Chest authoring verification commands

Run from the integration worktree. Discover the exact selected suites before
execution; optional ROM parity fixtures remain a separate qualification lane.

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4 > /tmp/yaze-chest-authoring-final-build.log 2>&1
yaze_chest_authoring_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonChestEditor*'
yaze_chest_authoring_filter+=':*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*'
yaze_chest_authoring_filter+=':*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*'
yaze_chest_authoring_filter+=':*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*'
yaze_chest_authoring_filter+=':DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:*RoomHeader*'
yaze_chest_authoring_filter+=':ChestEditTest.*:DungeonSaveTest.LoadObjects*'
yaze_chest_authoring_filter+='-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_chest_authoring_filter" > /tmp/yaze-chest-authoring-final-selected-tests.log 2>&1
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_chest_authoring_filter" --gtest_output=xml:/tmp/yaze-chest-authoring-final-tests.xml > /tmp/yaze-chest-authoring-final-tests.log 2>&1
```

Scoped static analysis uses the PCH-free database, with the scope limited to the
pure planner and editor chest transaction translation units:

```sh
cmake --preset mac-ai -B build/analysis/mac-ai -G Ninja -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DYAZE_ENABLE_CLANG_TIDY=OFF
clang-tidy -p build/analysis/mac-ai --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' --header-filter='(chest_edit|dungeon_editor_v2_chest_edits)\.(cc|h)$' src/zelda3/dungeon/chest_edit.cc src/app/editor/dungeon/dungeon_editor_v2_chest_edits.cc > /tmp/yaze-chest-authoring-final-analyzer.log 2>&1
```

Report the actual build/test/analyzer outcomes from these artifacts before
promoting the evidence. The earlier full-checker/PCH limitations below still
apply; a scoped analyzer result is not a full-repository tidy pass.

## Prior room authoring candidate (2026-09-22)

The prior integration increment extended the placement/entity work with room metadata
and existing chest-content editing at `aeb0b1200`, following `a893d0ef5`.
App/unit builds and 475 tests across 36 suites passed, with zero failures/skips.
Scoped Clang analyzer checks passed for the two new mutation modules. Details
and qualification boundaries are recorded in the
[capability plan](../plans/editor-capability-parity-plan.md#room-metadata-and-existing-chest-content-increment-2026-09-22).

- Workbench Room Properties uses named BG2/effect/collision/tag choices and
  validated deferred fields. Standalone properties and Room Tags share its undo
  boundary. Destinations include the pit and four staircase room/plane slots.
- Metadata snapshots preserve the hidden BG2 mode in dark rooms, distinguish
  room-header and object-stream-header save state, and refresh dependent views
  without switching the current room. Invalid and unchanged edits add no history.
- The shared Chest contents section edits existing rewards and normal/big
  record types. Its searchable receipt-ID labels are independent of the generic
  inventory-name table. Search remains visible above a scrolling results list.
  Unknown hack IDs and project labels are preserved. These controls do not
  create/delete chest objects or synchronize object and record types.
- Connected-view Clear stale validates the complete metadata batch before
  changing any room and contributes one undo entry for the whole operation.
- At this checkpoint, the next implementation package was compound chest creation/deletion with
  synchronized object/contents order and capacity preflight, followed by the
  remaining mixed-domain operations. Do not infer complete DA-1 or DA-2 from
  existing-record editing or the single-domain metadata batch. The current
  compound chest increment above supersedes that assignment.

Review the built candidate tomorrow when convenient. Do not install it over
`/Applications/yaze.app` or interrupt an active ROM session as part of automated
validation. Synthetic persistence checks are useful evidence; vanilla/Oracle
application save transactions, game behavior, packaging, and CI remain separate
qualification work. The older placement harness evidence below does not qualify
this new increment.

### Room authoring verification commands

Run from the integration worktree. The negative filter keeps the declared
Focused run independent of optional ROM fixtures; qualify those separately.

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4
yaze_room_authoring_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonChestEditor*'
yaze_room_authoring_filter+=':*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*'
yaze_room_authoring_filter+=':*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*'
yaze_room_authoring_filter+=':*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*'
yaze_room_authoring_filter+=':DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:*RoomHeader*'
yaze_room_authoring_filter+='-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_room_authoring_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_room_authoring_filter" --gtest_output=xml:/tmp/yaze-room-authoring-verified-tests.xml
```

Scoped static analysis uses the configured PCH-free analysis database:

```sh
cmake --preset mac-ai -B build/analysis/mac-ai -G Ninja -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DYAZE_ENABLE_CLANG_TIDY=OFF
clang-tidy -p build/analysis/mac-ai --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' --header-filter='(dungeon_room_edit|dungeon_editor_v2_room_edits)\.(cc|h)$' src/app/editor/dungeon/dungeon_room_edit.cc src/app/editor/dungeon/dungeon_editor_v2_room_edits.cc
```

This analyzer result covers two named translation units and their selected
header diagnostics. It is not a full-repository clang-tidy pass. Existing full
checker/PCH limitations remain documented in the capability plan. Relevant logs
are `/tmp/yaze-room-authoring-commit-build.log`,
`/tmp/yaze-room-authoring-verified-tests.log`,
`/tmp/yaze-room-authoring-verified-tests.xml`, and
`/tmp/yaze-room-authoring-final-analyzer.log`.

## Placement baseline

- Validated placement commit: `7ba7d76ce`, originally on
  `codex/dungeon-placement-feedback`. The integration worktree now uses
  `codex/editor-parity-dungeon-authoring`; the
  [capability plan](../plans/editor-capability-parity-plan.md) tracks later work.
- Worktree: `/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`.
- Parent commit: `7585a9e27` (macOS Shift-wheel routing and placement regressions).
- This branch includes PR #256's reviewed repair base `0b6ecdaf3`.
- The Workbench object browser now shows a live placement inspector. It exposes
  physical dimensions, stream/layer, and **Keep placing copies** (default on).
  Turning that off places once and returns to the inserted object's properties.
- Each successful placement selects the inserted object. Repeat mode keeps the
  preview and browser active; **Done placing** or Escape exposes the selection.
- **Place another** copies a selected object's ID, size, and layer into a new
  preview. Preview edits do not alter room data or create undo entries.
- Selection actions include Duplicate, Delete, To front, and To back. Delete
  uses the interaction facade to clear selection; property values are copied
  before drawing because stream changes can replace the object vector.
- Ordinary wheel grows/shrinks both encoded axes of packed area objects in
  lockstep, stopping when either reaches a bound. Shift-wheel changes width.
  Width/Height fields remain independent. Newly chosen floor regions start
  square (`0x05`); platforms with asymmetric borders retain those borders.
- Walls and trim use measured Length choices (tiles/pixels), sorted by physical
  span. Fixed and custom objects retain fixed-size or named-variant controls.
- Embedded browser suppresses the redundant placement summary and keeps a
  minimum grid height inside the scrollable inspector.

## Original focused verification

- macOS app and unit-test targets build successfully using the command below.
- 43 focused tests across 7 suites pass, with no skipped tests. They cover the
  ImGui placement-to-selection transition, repeat stamping, preview controls,
  custom/fixed semantics, uniform bounds, macOS Shift-wheel, and actual composite
  pixel changes plus texture upload after Workbench placement.
- The two new inspector tests draw and click controls at 272 px width and check
  placement policy, retained preview properties, selection, and ImGui stacks.
- An independent integration review caught and resolved the stream-change
  reference lifetime, delete-selection cleanup, and redundant global-room
  placement callback issues before delivery.
- `git diff --check` passes. No ROM files were edited during this work.

## Open Risks

- Source-level and synthetic ImGui checks do not establish ROM save/reopen or
  game-render parity. These remain candidate validation, not claimed results.
- This branch has not replaced `/Applications/yaze.app`. Barista's Zelda menu
  launches that installed app. Do not close the user's active app or overwrite
  their unsaved ROM session to test this candidate.
- Check a small inspector at normal and increased UI scale: placement controls
  should scroll while the browser grid retains a usable height.
- The legacy direct placement callback still has its existing editor-global
  room context. Coordinator mouse placement does not invoke it; its mutation
  and rendering notifications use the target room's existing handler path.
- No remote CI, merge, alert dismissal, or release claim is made for this slice.

## Remaining qualification

Claude completed the bounded validation recorded below. Qualify the current
integrated candidate through the application save transaction with project
dependencies attached; preserve source inputs and compare declared ROM domains
after an independent reopen. Retain the wheel/Shift-wheel, Escape, larger-scale
inspector, and game-runtime gaps. Block placement was not covered by the retained
round-trip harness. Keep parity-gate work independent until reviewed.

## Useful Commands

Run from the worktree above:

```sh
cmake --build --preset mac-ai --target yaze_test_unit yaze --parallel 4
```

Focused discovery and execution:

```sh
yaze_inspector_filter='TileObjectHandlerTest.*Placement*:TileObjectHandlerTest.Place*:TileObjectHandlerTest.BlockedPlaceOnce*:TileObjectHandlerTest.*Wheel*:DungeonWorkbenchPlacementUiTest.*:DungeonWorkbenchObjectSizeUiTest.*:DungeonWorkbenchSizeDescriptionTest.*:DungeonWorkbenchToolbarTest.WallControlsExposeLengthAndFootprint:DungeonObjectSelectorSizeTest.*:DungeonEditorPaletteRefreshTest.WorkbenchPlacementChangesCompositePixelsAndUploadsExistingTexture'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_inspector_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_inspector_filter" --gtest_output=xml:/tmp/yaze-workbench-inspector-tests.xml
```

Evidence files:
- `/tmp/yaze-workbench-inspector-build.log`
- `/tmp/yaze-workbench-inspector-selected-tests.txt`
- `/tmp/yaze-workbench-inspector-tests.log`
- `/tmp/yaze-workbench-inspector-tests.xml`

Use `YAZE_PREPUSH_BUILD_DIR=build/presets/mac-ai` for repository pre-push checks
if publication is authorized. Main checkout and Claude's `parity-gate` worktree
contain independent work and must remain intact.

## Independent validation review (2026-09-22)

Claude's validation worktree is `yaze-worktrees/validate-placement`, at
`335fdf446`, based on the older placement candidate `7ba7d76ce`. Its local
`test/CMakeLists.txt`, `dungeon_editor_palette_refresh_test.cc`, and
`validation_room_readback_test.cc` harness changes remain uncommitted and were
not imported into the integration branch.

The room-switch fix from `335fdf446` was reviewed and cherry-picked as
`6e9d52e8c`. It clears index-based selections when the room or store changes and
binds cross-room navigation before selecting the destination object/door. Review
also identified unfinished marquee, tile-drag, and paint-stroke state that must
end against the old room before rebinding; the capability plan records the
integration follow-up `50d6257ad` and its 289 passing focused tests, including
seven regression cases that fail with the selection-only transition.

Retained artifacts were inspected read-only in the reviewer’s temporary
validation directory. They were local evidence that may expire, not release
artifacts or a reusable checkout path. Each
`harness_vanilla.log` / `harness_oracle.log` records one passing
`DungeonEditorPaletteRefreshTest.CandidatePlacementSaveReopen` and
`EDITOR_SAVE OK`. Before/after readback records show:

- Vanilla `0x009`: four appended ordinary object records (`0x0C5`, `0x001`,
  `0xFEB`, and `0xFEB` on stored layer 1).
- Vanilla `0x00B`: one torch replaced at `(30,30)`, preserving other printed
  entries, including an existing block. This does not test placing a block.
- Oracle `0x0B9`: custom `0x031` and torch copies replace their source records,
  maintaining the total object count. This proves these selected encodings can
  survive the tested write/readback, not additional stream or torch capacity.

Current SHA-256 identities of the retained scratch inputs/outputs:

| File relative to artifact root | Bytes | SHA-256 |
|---|---:|---|
| `vanilla/harness_in.sfc` | 1048576 | `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb` |
| `vanilla/harness_out.sfc` | 2097152 | `61df19cd79f3bdde1991472d589f8c33068cae903fccc0ff9935a263e4b4fcbb` |
| `oracle/Roms/harness_in.sfc` | 2097152 | `ab518cb201e4d904706f4c1950a82e574c9618ee23fa12845a1ed6c66c59d0fe` |
| `oracle/Roms/harness_out.sfc` | 2097152 | `d97a9d14e39ff762123a6740b089343ea14a0d40b3a9b2753428178008730354` |

Qualification limits:

1. The harness calls `DungeonEditorV2::Save()` then `Rom::SaveToFile()` directly.
   It bypasses the application save orchestration and pot-item confirmation.
   Oracle custom-object context is activated, but the loaded `YazeProject` is
   not attached to editor dependencies, so manifest/write-policy integration is
   not qualified.
2. Fresh-process readback prints index, ID, position, size, layer, and torch/block
   classification. It does not compare other ROM domains. `allrooms_diff.txt`
   names only vanilla `0x009` and `0x00B`, but does not retain its invocation,
   comparison code, or successful-room count. Do not claim an independently
   verified whole-ROM/no-collateral comparison; the vanilla file also expands
   from 1 MiB to 2 MiB.
3. Capacity refusal prevented the observed output-file save. The direct editor
   save writes room data before checking the torch table, so a late failure can
   follow in-memory changes. Application rollback remains a separate test.
4. Claude reported 960 related tests, a mutation check, and a fixed-build GUI
   room-switch check. Those runs were not reproduced by this artifact audit;
   current integration tests are recorded separately in the capability plan.

Follow-ups retained from the validation report: advance stream/torch-capacity
feedback; duplicate selection behavior; the `Floor 3 ?` label; dirty indicator
after undo to the original state; and the pot-item prompt after a failed Apply
Room. Reproduce each against the current candidate before assigning a cause.
