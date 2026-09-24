# Shrine/Goron staircase qualification and RC workflow inventory

Baseline: `0f38046ba` plus the opt-in CLI report in this change.
Machine evidence: [qualification JSON](shrine-goron-staircase-qualification-2026-09-23.json).

## Delivered

`dungeon-describe-room --include-staircase-resolution` now invokes the existing
collision-based resolver. It loads the layout prefix and reports each room-stream
stair's identity, position, model status and candidate header destination. Default
output is unchanged. `runtime_qualified` is always false: this command does not
run the game. Custom collision blocks `slot`/`destination_room`; separate
`vanilla_slot`/`vanilla_destination_room` fields expose only the model prediction.

Candidate executable (relative to this checkout):
`build/presets/mac-ai/bin/Debug/z3ed`.
SHA-256: `d2c8026fce894b0f047c50124817e8b3fafaf595d7c95483434f1684a8c9b151`.
The installed nightly was not replaced.

```bash
build/presets/mac-ai/bin/Debug/z3ed dungeon-describe-room --rom=/path/to/read-only-copy.sfc --room=0x77 --include-staircase-resolution --format=json
```

## Exact inputs and preservation

26 rooms were inspected in each ROM: seven Shrine rooms plus the 19 Goron rooms
listed in the supplied understanding audit. Both ROMs match the supplied hashes:

- Base `oos168.sfc`: `ab518cb201e4d904706f4c1950a82e574c9618ee23fa12845a1ed6c66c59d0fe`.
- Patched `oos168x.sfc`: `115d784ad4fd774e213a86cacc9b74218ebdf764cdd80af9b7e4b2dfa6bb85ef`.

Commands used temporary copies. Original and copy hashes were checked afterward
and remained identical. No ROM mutations, emulator commands, save-state changes,
automatic metadata clearing, Oracle edits or device installs occurred.

## Concrete staircase results

Base and patched ROMs produced identical candidates. Slot numbers below are
zero-based. Each ROM contains 16 reported stairs: six vanilla-model resolved,
ten blocked by custom collision. Every vanilla trigger probe itself resolved;
the ten blocked results are not missing objects or guessed destinations.

| Room | Slot | Candidate target | Qualification |
|---|---:|---|---|
| 073 | 0 | 084 | Vanilla-model resolved |
| 074 | 0 | 075 | Custom collision blocks qualification |
| 075 | 0 | 074 | Vanilla-model resolved |
| 083 | 0 | 085 | Custom collision blocks qualification |
| 084 | 0 | 086 | Custom collision blocks qualification |
| 084 | 1 | 073 | Custom collision blocks qualification |
| 085 | 0 | 083 | Vanilla-model resolved |
| 086 | 0 | 074 | Vanilla-model resolved |
| 069 | 0 | 079 | Custom collision blocks qualification |
| 077 | 0 | 0A8 | Custom collision blocks qualification |
| 079 | 0 | 069 | Vanilla-model resolved |
| 097 | 0 | 0B8 | Custom collision blocks qualification |
| 0A8 | 0 | 077 | Custom collision blocks qualification |
| 0A9 | 0 | 0DA | Vanilla-model resolved |
| 0B8 | 0 | 097 | Custom collision blocks qualification |
| 0DA | 0 | 099 | Custom collision blocks qualification |

Goron candidate headers agree with 77↔A8,79↔69,97↔B8 and the asymmetric
A9→DA→99 chain. Room 99 has no staircase object. Shrine 84→86 does not imply a
return: 86 targets74. These statements describe model/header agreement, not
traversability or reciprocal arrival geometry.

All 38 Goron door-record comparisons matched the supplied audit. No links were
inferred from adjacency: reject98–A8,99–A9,A9–A8; retain97–87 as an open rail
boundary and A8–B8 as an unpaired candidate opening. The connected-view door
collector still checks type/direction/reciprocity without an outer-position
predicate, so this audit does not qualify its inferred graph.

## Tests

```bash
cmake --build --preset mac-ai --target z3ed yaze yaze_test_unit --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='DungeonEditCommandsTest.*:RoomCollisionTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='DungeonEditCommandsTest.*:RoomCollisionTest.*'
```

106 tests across two suites passed, zero failures/skips. New coverage checks the
opt-in report, model provenance, expected slot and unchanged synthetic ROM. The
52 real-ROM CLI calls all completed successfully. Full raw results and logs are
retained in `build/presets/mac-ai/combined-editor-evidence/`.

## Concrete remaining work

1. Qualify the ten custom-collision stairs per ROM against the actual Oracle
   override semantics or matched runtime collision captures. The vanilla model
   intentionally does not bypass that gate. Six model-resolved stairs also still
   need in-game traversal before being called gameplay-qualified.
2. Replace estimated connected-view stair links with explicit resolver results;
   separately classify outer/internal doors before inferring neighboring rooms.
   Keep automatic clearing disabled.
3. Preserve the distinction between stored header falls and reachable triggers,
   and between rail boundaries and door records. No reciprocal repair is implied.

## Secondary: existing RC/milestone mechanisms (source inspection only)

- `menu_orchestrator.cc` Save Snapshot As calls `SaveLayoutSnapshotAs`: it names
  layout state, not a ROM milestone.
- `RomFileManager::SaveRomAs` preserves the source and backs up an existing
  destination before replacing it when backup-on-save is enabled. Existing tests
  cover exact destination, session collision, build-output protection and failure
  behavior. Project settings already expose backup retention and daily policies.
- `core::VersionManager::CreateSnapshot` combines GitAddAll/GitCommit and a ROM
  artifact backup. It can report success after backup failure and treats commit
  errors as possibly no-change. Do not make it the RC gate as currently written,
  especially with concurrent uncommitted work. This path was inspected, not run.

Smallest proposed support: a named RC receipt after a successful existing Save As,
recording the chosen name, exact ROM path/hash and observed source revision. Reuse
existing save/backup primitives; do not add another backup store, auto-commit other
agents' changes or impose content locking. First make the reused path report
failure accurately. Implementation remains secondary to collision qualification.
