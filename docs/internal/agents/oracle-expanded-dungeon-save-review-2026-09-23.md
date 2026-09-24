# Oracle RC: expanded dungeon formats and save reliability

Initial bounded review, followed by the authorized secure-staging implementation below. No Oracle source or shared ROM edits.

## Implementation follow-up — 2026-09-23

Opening coordinator authorized only the secure-staging packet after review. Editor owner confirmed exclusive ownership of the three implementation/test files and reviewed their diff without a blocking source finding. The review sections below retain the **pre-fix** evidence and qualification limits.

Implemented in `src/rom/rom.cc`: private `WriteExclusiveRomTemp` creates a unique same-directory file using POSIX `O_CREAT|O_EXCL` or Windows `CREATE_NEW`, retains that handle for complete writes, checks native flush and close failures, and cleans up its own staging file. The staging basename is independent of the destination basename, preserving long-filename behavior. POSIX creation uses mode 0666 subject to umask, matching the previous stream creation behavior. `SaveToFile` keeps its public API, actual-destination backup logic and final platform replacement path. Native staging flush errors now fail before replacement instead of being ignored. Parent-directory fsync remains best effort; browser durable synchronization is not added.

Added six synthetic `RomTest` regressions and one `EditorManagerWriteConflictTest` case. Four alias/preservation tests failed on the old writer before implementation. The partial-write regression uses a child-only POSIX file-size limit: required backup succeeds, payload writing fails after eight bytes, destination/backup survive and staging is removed. The editor regression observes actual header serialization with a write fence, forces replacement failure, compares entire source/alias files and ROM/retry state, then successfully retries Save As. Its initial failure was a fixture backup preference stopping before replacement; only the fixture was corrected.

Final focused evidence on macOS:

```sh
cmake --build --preset mac-ai --target yaze_test_unit yaze_test_quick_unit_editor --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='RomTest.*:RomFileManagerTest.*-RomTest.LoadFromFile'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='RomTest.*:RomFileManagerTest.*-RomTest.LoadFromFile'
build/presets/mac-ai/bin/Debug/yaze_test_quick_unit_editor --gtest_list_tests --gtest_filter='EditorManagerWriteConflictTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_quick_unit_editor --gtest_filter='EditorManagerWriteConflictTest.*'
```

**42/42 ROM/save-manager tests and 28/28 coordinated editor tests passed, zero skips.** The unrelated `RomTest.LoadFromFile` 1 MiB/2 MiB expectation failure is explicitly excluded, unchanged and unresolved; this is not a claim that the full suite passes. Changed-line formatting and `git diff --check` passed. Evidence: `build/presets/mac-ai/combined-editor-evidence/secure-staging-2026-09-23/`.

Desktop app/CLI relink and candidate provenance update are handed to editor owner `01a0cb50-a603-7763-8964-a27e9b7aa74a`, avoiding concurrent builds. This task does not install, push or deploy. Native Windows replacement, Emscripten persistence and power-loss behavior remain unverified. As with the existing artifact publication path, assume the destination directory is trusted; hostile concurrent directory-entry replacement after exclusive creation is not qualified. No general capacity feature, allocator, backup manager, project format or Oracle migration change was added.

## Result and ownership

**Pre-fix P1: the disk writer could overwrite an aliased staging target and report success for an unreadable destination.** The implementation follow-up repairs that staging path. Dungeon allocation already exists; a new allocator is not justified.

Backend owner: `zelda3-hacking-expert`, universe task `task_20260924T015539Z_28947`. Editor task `01a0cb50-a603-7763-8964-a27e9b7aa74a` explicitly accepted the boundary: this review owns formats, allocation, serialization and save reliability; that owner retains UI, selection, undo, connections and templates. Opening coordinator `01a0d06d-ee1d-7893-be24-c52b3a73c09c` was consulted. No new layout, room allocation, shared Oracle edit, emulator/save access, deployment or snapshot is authorized by this packet.

Candidate: `/Users/scawful/src/hobby/yaze-worktrees/overworld-paint-regression-fixes`, branch `codex/combined-editor-candidate`, inspected at `dfb2e1c88c29f1a42d1cf84bbf14d464dad59cda`. During review the editor owner committed `bf471e33bc0abc722f65ce14601f4f9125de6674`; its only change is `test/unit/editor/dungeon_room_edits_lifecycle_test.cc`. Reviewed backend files are unchanged. Main checkout and installed nightly are not this candidate.

Recent relevant commits include `e1e2cb3b1` (combined editor), `5028dcc2d` (template files), `0f38046ba` (receipt labels), and `dfb2e1c88` (staircase qualification). This review does not requalify their UI/runtime behavior.

Read: candidate AGENTS/router/persona/ROM safety instructions, canonical `editor-capability-parity-plan.md` (DA-5 and CO-1), `oracle-daily-driver-save-stability.md`, dungeon format and custom collision documentation; Oracle AGENTS, current handoff, `RC_MASTER_PLAN.md`, `abyss_origins_review_2026-09-23.md`, and rendered `abyss_origins_2026-09-23/room05.png`. Current RC means a completable main adventure and normal saves, not content lock.

## Reproduced save defect

Source: `src/rom/rom.cc:482-530`. `SaveToFile` chooses `<destination>.tmp`, opens it with `std::ofstream(..., std::ios::trunc)`, writes bytes, then renames it. Opening follows existing links; the process does not exclusively own this staging file. `ScopedRomTransaction` only restores memory.

A standalone C++ probe linked the candidate's compiled libraries, directly called `Rom::LoadFromData`, `Rom::SaveToFile` and `ScopedRomTransaction`, and used synthetic 1 MiB buffers and newly created temporary files. It did not reimplement the writer or open a shared Oracle ROM for writing.

```text
symlink status=OK target_is_symlink=1 tmp_exists=0 target_read_bytes=0 original_preserved=0
hardlink status=OK target_is_symlink=0 tmp_exists=1 target_read_bytes=1048576 original_preserved=0
rejected-save status=INTERNAL: Failed to move temp ROM into place: Is a directory
unrelated_preserved=0 unrelated_bytes=1048576 memory_rolled_back=1
```

Case 1: `.tmp` points to the destination. Truncation writes through the link; rename replaces the destination with a link to itself. The writer returns OK. Case 2: `.tmp` is a hardlink to the destination, so the original inode is overwritten before commit. Case 3: destination is a directory and `.tmp` points to an unrelated synthetic file. Replacement fails, memory rolls back, but that unrelated file has already been overwritten. This is a concrete disk-preservation gap, independent of room parsing, allocator success or in-memory rollback.

## Supported / unverified / missing matrix

| Surface | Supported in inspected candidate | Unverified or missing |
|---|---|---|
| Room formats | Fixed 296 room slots, `$000–$127`; pointer-based headers; three object lists and doors; sprite and pot streams; ZScream object storage sections and collision data | No room-ID-count expansion. Vanilla region/version, HM/PW and other patch layouts need separate qualification; loading alone is insufficient |
| Existing room `$05` growth | Object copy-on-write into manifest-owned ranges; shared sprites detach; pot items use deterministic repack | Exact proposed geometry and application save/reopen/runtime remain unqualified; room currently has zero object-stream headroom before the next stream |
| Headers / entrances / chests | Fourteen-byte headers plus separate message IDs; 133 regular dungeon entrances and 7 spawn records; 168-record chest table and physical ordering preservation | No header or chest-table expansion allocator. Transfer rejects aliased/overlapping headers; ordinary `SaveRoomHeader` has no equivalent ownership inventory. Current Oracle headers are unique |
| Oracle custom data | Dirty-only custom collision save, alias-safe blob reuse/append, reserved WaterFill tail, raw room tags, custom-object data references | Runtime Minish/shutter/reward behavior and custom-object build preservation require exact base-to-patched qualification; size-based region availability is not patch-runtime detection |
| Save protection | Manifest gates, stream plans, write fences, whole-ROM plus editor retry-state rollback, temporary-file replacement on the normal path | Reproduced aliased staging corruption. General edit capacity diagnostics remain incomplete; transfer already performs detached serializer preflight |

## Actual formats and capacity

Addresses below are headerless PC offsets unless explicitly marked SNES. `Rom` strips recognized 512-byte copier headers on load; unchanged raw-container bytes are therefore not a general guarantee for headered input.

1. **Objects:** live 24-bit pointer-table operand at PC `$00874C`; Oracle table resolves to PC `$0F8000` (SNES `$1F8000`). Two stream-header bytes precede three encoded object lists; `$FFFF` separates lists, `$FFF0` introduces the two-byte door entries, final `$FFFF` ends them. Object records are three bytes. Save preserves source header bytes unless their dedicated dirty mask changes floor/layout fields. Relocation updates both the room pointer and the door pointer at `$0F83C0 + room*3`; unchanged in-place door pointers retain their bank-mirror bit. Five bounded ZScream sections are declared in `dungeon_rom_addresses.h:29`; the allocator never treats arbitrary zero/FF runs as free space.
2. **Sprites/pots:** sprites use live bank-$09 16-bit pointers, a sort byte, packed three-byte Y/X/ID records, optional hidden key-drop markers, and `$FF` termination. They cannot relocate into arbitrary banks. Pot entries use fixed bank-$01 16-bit pointers, three-byte position/item entries, and `$FFFF` termination. Repacking preserves untouched captured payloads, deduplicates exact encoded payloads, and updates all 296 pointers within the declared region.
3. **Headers/chests:** the live header table uses 16-bit offsets plus a bank operand. Save writes 14 bytes plus the separate two-byte message ID, preserving reserved header bits. Pit/stair destination bytes do not become wider because the ROM is larger. Chest entries are three bytes (room/flag word plus receipt); associations depend on per-room occurrence and encoded chest-object order. `BuildChestSavePlan` preserves unknown records and global physical order, substitutes dirty-room occurrences, removes surplus occurrences, and appends growth deterministically. It rejects more than 168 global records. Keeping the existing Pearl chest does not require a new slot.
4. **Oracle collision:** the patch in `Dungeons/Collision/custom_collision.asm` consumes SNES `$258090` room pointers. Yaze uses PC `$128090` for the 296 long pointers and `$128450–$12E000` for collision payloads; `$12E000–$130000` is reserved for WaterFill. Collision encoding has rectangle entries, `$F0F0` single-tile mode, `$FFFF` termination. `WriteTrackCollision` only reuses a unique complete span that fits; aliases/interior overlap/growth append after occupied data and fail at the soft end. No full collision compactor is supplied. Unedited maps remain untouched. WaterFill has up to eight entries and one-byte runtime room IDs. GUI generic Oracle preflight does not request required-room membership; that stronger check needs explicit options/Oracle smoke.
5. **Oracle meaning:** room `$05` header tag is raw `$36`; current Oracle source patches `RoomTag_Holes5` at SNES `$01CC10` to `RoomTag_MinishShutterDoor`. Preserve that byte and the current receipt `$1F`; a vanilla display label does not prove the patched reward handler. Custom object assets live at the project's `Dungeons/Objects/Data` path and are outside stream allocation. Expanded ZSCustomOverworld entrance placements are separate from dungeon room-ID expansion and from the regular dungeon entry tables.

Fresh inventory on an identified disposable copy:

| Stream | Valid slots / unique streams | Allocatable unoccupied bytes | Room `$05` |
|---|---:|---:|---|
| Objects | 296 / 285 | 32,370 | Unique 182-byte stream at `$0522E6–$05239C`; next stream starts at `$05239C` |
| Sprites | 296 / 260 | 764 | Empty two-byte stream at `$04D502`, shared by 37 rooms |
| Pot items | 296 / 158 | 9 | Empty two-byte stream at `$00DDE7`, shared by 139 rooms |

All three inventories reported zero parse issues, suffix overlaps or interior overlaps. These free-byte totals are not a promise that every replacement fits: pointer-bank boundaries, fragmentation, complete replacement size and ownership still apply. Object allocation owns PC `$148000–$150000`. Sprite allocation is PC `$04D502–$04EC9F`; pots repack PC `$00DDE7–$00E6B2`. Current chest count is **168**. Room `$05` header is unique at PC `$1102C6`; none of the 296 current header starts are exact aliases.

The room render shows an unused-looking southwest chamber, not an approved route. Extending within its existing 64×64 tile grid requires neither a new room ID nor a new entrance by itself. Net encoded object growth does require relocation because room `$05` currently has no in-place stream headroom. Adding sprites requires detaching its shared empty sprite stream. Visible pots are object records; adding pot-item contents is a separate storage decision. Reusing a different existing room slot requires proof that its data/progression are available, and connection/entrance work remains editor-owned. Increasing the room count or expanding the full chest table would require a separate runtime/format change and is outside this packet.

Capacity counters are not complete runtime guarantees: constants include 400 objects, 64 total sprites, 16 active sprites, 16 doors and 6 chests. `DungeonValidator` emits warnings for several count/state-slot conditions rather than blocking them. Successful serialization does not establish runtime load, sprite scheduling or room-event correctness.

## What preflight and rollback actually cover

**Before coordinated serialization:** target/hash/build-output policy, configured manifest presence, project-registry-triggered Oracle collision/WaterFill validation, and predicted write ranges. Dungeon Save checks unpublished drafts, chest/object coupling, entrance/spawn validity and manifest ranges, chest capacity/plan, and relevant collision/water/pot conflicts before writing those domains.

**During serialization:** per-room validation and manifest ranges; actual object/sprite encoded size, sharing and ownership inventory; source-CRC-guarded allocator plans; pot repack fit; collision append fit; fixed-table bounds and write fences. Earlier rooms/domains may already have changed memory when a later check fails. `SaveRoom`/Apply and coordinated File Save provide the transaction boundary. Low-level `Room::SaveObjects`, `SaveRoomHeader`, or collision writers are not independently whole-operation atomic: for example, header message-ID bounds are checked after header bytes are written, and object payload precedes its door-pointer write. Callers must retain the existing enclosing transaction.

**After serialization, before disk:** File Save reruns Oracle preflight and compares actual byte changes with manifest ownership (with conservative fallback ranges if disk comparison is unavailable). Editor transaction snapshots include dirty/retry state, entrances/spawns, palette state, pit-table dirtiness, block identities and water metadata. On ordinary errors, the enclosing ROM guard restores bytes, size, filename and dirty state. The editor and ROM transactions commit only after successful disk save.

**Disk:** GUI `RomFileManager` blocks configured-backup failure; low-level `backup=true` is explicitly best effort, while strict CLI callers use `require_backup=true`. Normal disk saving writes/flushes/closes a temporary file and replaces the target, with best-effort fsync. The reproduced staging-alias defect invalidates an unconditional all-or-nothing disk claim. This review did not test power-loss durability, Windows replacement or browser persistence.

**Existing preview:** `DungeonEditorV2::PreflightRoomTransfer` copies the ROM, builds detached room models, invokes `SaveRoomImpl(target, true)`, checks fixed-header ownership, water state and actual manifest conflicts. It already answers transfer feasibility through the real serializers. `dungeon-stream-plan` only inventories; it accepts no replacement payload. General edit feedback does not yet expose replacement-specific byte/range diagnostics, but that is not evidence that the allocator is missing. Hand that qualification need back without adding another writer/planner now.

## ONE implementation packet: exclusively owned ROM staging

Scope: repair the concrete `SaveToFile` disk-preservation defect while retaining its public API, Save As semantics, backups, existing transactions and platform replacement behavior.

Owned files, relative to this candidate:

- `src/rom/rom.cc`: replace predictable truncating staging creation with exclusive same-directory creation and retain the opened handle through writing. Do not reopen an owned temporary path through `ofstream` after allocation. Use OS-exclusive creation (`O_CREAT|O_EXCL` / `CREATE_NEW`) and collision retry or fail closed. Clean up only the file this call created. Check write/flush/close failures before replacement. Keep the existing backup and final replace path; no second public ROM writer or backup manager.
- `test/unit/rom/rom_test.cc`: synthetic regressions for staging symlink/hardlink aliases, unrelated pre-existing `.tmp` files, new/existing Save As targets, write/replacement failure, required-backup retention and dirty-state behavior. Use independent reopen and byte comparison, not returned status alone.
- `test/unit/editor/editor_manager_write_conflict_test.cc`: one coordinated-save failure regression showing that an already-serialized dungeon edit retains ROM bytes, filename and editor retry state when staging/commit fails. Register/run it through its actual editor test target; it was not selected by this review's unit binary.

Acceptance: (1) existing `.tmp` file/link and its target are never truncated or removed by another save; (2) successful save independently reopens to the expected complete bytes; (3) failed save preserves destination and unrelated files plus in-memory/edit retry state; (4) Save As preserves source and backs up the correct existing destination; (5) no platform silently falls back to shared/truncating staging. Reuse the exclusive-create pattern already present in `src/cli/handlers/game/dungeon_collision_commands.cc`; do not introduce a dependency from ROM core onto CLI code. Add only a private helper if needed.

Focused verification after implementation: build actual unit/editor test targets, discover exact test names first, run the new staging/transaction cases plus existing `RomTest`, `RomFileManagerTest` and coordinated save tests; run native Windows replacement coverage before claiming cross-platform closure. Do not alter the unrelated ROM-size fixture expectation as part of this fix.

## Evidence, limits and handoff

Evidence directory: `build/presets/mac-ai/combined-editor-evidence/backend-save-review-2026-09-23/`. Contains inventories, probe source/results, link recipe, source/library/binary hashes and test logs. No ROM bytes were added to the repository.

- Base SHA256: `ab518cb201e4d904706f4c1950a82e574c9618ee23fa12845a1ed6c66c59d0fe`, 2,097,152 bytes. Manifest SHA256: `d382e7b59671d9d5bfa9e3fc97770f18780925f55b64e8103191052947609d87`. Disposable source/copy identities matched after inventory. No patched test artifact was modified or used to infer base compatibility.
- Candidate z3ed SHA256: `d2c8026fce894b0f047c50124817e8b3fafaf595d7c95483434f1684a8c9b151`.
- `yaze_test_unit --gtest_filter='DungeonStreamAllocatorTest.*:DungeonSaveTest.*:RomTest.*:*CustomCollision*:*SaveAllCollision*:*OracleRomSafety*:*RomFileManager*'`: **264 selected, 263 passed, 1 failed, zero skipped**. Failure: `RomTest.LoadFromFile`, expected 2,097,152 bytes but fixture loaded 1,048,576 (`test/unit/rom/rom_test.cc:119`). Includes 136 `DungeonSaveTest` and 47 allocator cases.
- Additional discovery/run using `EditorManagerWriteConflictTest.*:DungeonRoomTransferLifecycleTest.*:DungeonStreamPlanCommandsTest.*` selected only **9 CLI inventory tests**, all passed. Editor save/transfer suites were absent from this unit binary; integration binary discovery also did not select those names. No application-orchestration pass is claimed.
- Inventory command, for each `objects`, `sprites`, `pot_items`: `build/presets/mac-ai/bin/Debug/z3ed dungeon-stream-plan --rom=<disposable>/oos168.sfc --manifest=<disposable>/hack_manifest.json --kind=<kind> --format=json`. All exited 0.

Python migration was not touched. Coordinator supplied current `Scripts/README.md` and migration handoff; inspected shim routes the client to `../mesen2-oos/tools/oos_client` (override `MESEN2_OOS_ROOT`), and handheld tooling lives in sibling `oos-rg353p`; both directories exist. No migrated command was executed, no removed tool restored, and migration completeness is not claimed.

Return these DA-5 qualification cases to the editor owner after staging repair: project-bound room `$05` object growth and independent reopen; unchanged chest occurrence/receipt, tag `$36`, doors, custom collision and other-room data; shared sprite detachment; pot-content repack only if the chosen puzzle needs it; whole-ROM changed-range audit; late save failure and retry. Use identified disposable unpatched bases and separately identified rebuilt runtime artifacts. Minish traversal, shutter behavior, Pearl pickup, Goron routes, normal game saves and platform/device acceptance remain unverified. No layout decision is implied.
