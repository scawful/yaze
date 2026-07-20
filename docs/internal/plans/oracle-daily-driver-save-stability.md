# Oracle Daily-Driver Save Stability

**Status:** ACTIVE - Wave 1 published; Wave 2 allocator and pot repack under review
**Owner (Agent ID):** CODEX
**Created:** 2026-07-11
**Last Reviewed:** 2026-07-15
**Next Review:** 2026-07-18
**Coordination:** AFS task `task_20260711T144152Z_18414`

## Summary

Make yaze safe enough to finish Oracle of Secrets without waiting for every
editor feature to reach general-purpose completeness. The daily-driver bar is
guarded, recoverable editing of the already-developed OOS base ROM: failed
saves must not partially mutate the ROM or lose retry state, project targets
must be unambiguous, and unsupported growth must fail closed.

This plan complements the [editor-first release ladder](release-ladder-0x-2026.md)
and the broader [Oracle integration plan](oracle-yaze-integration.md). It does
not move all remaining dungeon or overworld polish into the `0.7.2` release.

## Decisions And Constraints

- `Roms/oos168.sfc` is editable; `Roms/oos168x.sfc` is disposable build output.
- Open `Oracle-of-Secrets.yaze` so target, manifest, hash, and write policy are
  loaded together.
- Use one whole-ROM transaction plus editor-state rollback for coordinated
  saves. A failure must preserve every dirty flag needed for a retry.
- Never infer free space from runs of `00` or `FF`. Future relocation may use
  only allocator-owned ranges declared by the ROM profile/project manifest.
- Shared or over-capacity dungeon streams relocate only when the manifest
  grants an explicit copy-on-write layout; otherwise they fail without
  mutation.
- Keep OOS autosave, dungeon-map saving, and graphics-sheet saving disabled
  during the guarded beta.

## Deliverables

### Wave 1 - Containment and project safety

- [x] Preserve Save As targets through hash, pot-item, and conflict prompts.
- [x] Apply build-output policy to the requested target and refresh lifecycle
  path/hash only after a successful save.
- [x] Block saves when an explicitly configured hack manifest is missing or
  malformed.
- [x] Roll back ROM bytes and editor dirty state after late save failures.
- [x] Bound dungeon object, sprite, pot, and chest writes; reject aliases and
  unknown headroom without mutation.
- [x] Bound expanded-message reads/writes, reject expanded `[BANK]`, validate
  dictionary IDs, and prevent invalid drafts from being discarded by
  navigation.
- [x] Use ROM-profile and adjacent-table bounds for Map32 storage; preserve
  disabled custom overworld tables and their original enable encoding.
- [x] Make the OOS project file and generated manifest describe the canonical
  editable/build ROM workflow.
- [x] Finish focused automated verification and publish this wave as a
  dedicated safety PR; keep release PR #69 narrowly reviewable.

### Wave 2 - Dungeon copy-on-write allocator

1. [x] Inventory object, sprite, and pot streams by physical address, size,
   bank, aliases, and owners; reconcile candidate ranges with the OOS
   manifest.
2. [x] Build a deterministic allocator that produces and validates an
   immutable write plan before changing any byte.
3. [x] Detach and relocate shared/overflowing object streams, including object
   and door pointer updates.
4. [x] Deterministically repack all pot pointers/streams inside a declared
   region.
5. [x] Detach and relocate sprite streams inside declared bank `$09` capacity.
6. [x] Add read-only `z3ed` inventory diagnostics for aliases, overlaps, and
   manifest-owned free space.
7. [ ] Add replacement-aware immutable move/write-plan output. The current
   `dungeon-stream-plan` command accepts no replacement payloads and does not
   predict moves from inventory alone.

**Estimate:** 4-5 full-time weeks for an allocator beta; 6-8 full-time weeks
including OOS soak and emulator verification. At 10-15 hours/week, plan on
roughly 3-5 months. Proving Oracle-safe allocator ownership is the main risk.

### Wave 3 - Flexible daily-driver beta

- Exercise the remaining high-value OOS message, overworld-property, project,
  backup/restore, and multi-session workflows.
- Run repeated edit/save/reopen/build cycles on representative D6 rooms
  `A8`, `B8`, `D8`, and `DA`.
- Track strict-readiness content failures separately from editor corruption;
  the current D3 readiness gap is not itself a save-stability blocker.

## Exit Criteria

- No-edit save is semantically identical for vanilla ALTTP and OOS/ZSCustom
  v3, including expanded Map32 data.
- Every failure path restores ROM bytes and all dirty/retry state.
- Save As never modifies or backs up the source path by mistake and never
  permits the configured build output as an editable destination.
- Shared-stream edits affect only the selected room after Wave 2; before then,
  they fail closed with an actionable error.
- Exact-fit streams save; overflow either relocates through a validated plan or
  fails with zero mutation.
- Rebuilding through `Scripts/Build/build_rom.sh 168` preserves intended base-
  ROM edits and refreshes manifest hashes/ownership.
- At least 50 save/reopen cycles and one multi-hour editing session complete
  with backup restoration tested.
- Mesen boots and traverses representative edited rooms without bad doors,
  objects, sprites, pots, or room loads.

## Validation

For each save-path change, run the narrow unit/integration filter first. Before
publishing the safety PR, also require:

1. `yaze_test_unit` and quick-editor transaction/message/dungeon filters.
2. Vanilla plus temporary-OOS integration round trips with source hashes
   unchanged.
3. `z3ed project-bundle-verify`, `rom-doctor`, and non-strict
   `oracle-smoke-check` on the current OOS base ROM.
4. A strict smoke run whose remaining failures are recorded as content gaps.
5. Green macOS and Windows PR checks before promoting the release candidate.

### 2026-07-11 Wave 1 snapshot

- Unit: 2,407 passed; one optional usdasm palette fixture skipped.
- Quick editor: 619 passed.
- Vanilla + temporary-OOS integration: 279 passed; two fixture-discovery
  utilities skipped; both canonical source ROM hashes remained unchanged.
- OOS project verification: 7 passes, zero warnings/failures, including the
  configured manifest and expected ROM hash.
- OOS smoke: D4 structural checks and all four required D6 track rooms pass;
  strict readiness still records only the known D3 content gap.

### 2026-07-12 Wave 2 inventory snapshot

- PR #70 follow-up commit `dac5cccf` closes Windows path-test failures and
  clamps the previously unbounded tails of all five ZScream object sections,
  pot data at PC `0x00E6B2`, and sprite data at PC `0x04EC9F` (exclusive).
- OOS objects: 285 unique streams and one 12-room empty-stream alias group.
  ZScream section 5, PC `[0x148000, 0x150000)`, is unused and is the first
  proposed manifest-owned object COW arena (32 KiB).
- OOS sprites: 260 unique streams; 37 rooms share the empty stream. Used data
  ends at PC `0x04E99A`, leaving 773 bytes before the exclusive hard end.
  Sprite serialization now preserves hidden small/big-key drop markers during
  in-place and COW saves.
- OOS pot items: 158 unique streams; 139 rooms share the empty stream. Used
  data ends at PC `0x00E6A9`, leaving only 9 bytes (three records). Pot growth
  beyond that requires a full deterministic repack or an explicit ASM/layout
  expansion decision.
- The manifest contract separates ranges that may contain existing streams
  from ranges explicitly owned for new allocations. The planner may derive
  free intervals only inside those declared allocation ranges; it never scans
  for runs of `00` or `FF`.
- Object and sprite saves detach exact aliases plus suffix/interior overlaps,
  respect manifest data boundaries, and apply CRC-guarded plans inside an
  exact write fence before clearing dirty state.
- The read-only `z3ed dungeon-stream-plan` command inventories the current OOS
  ROM as 296/296 valid object, sprite, and pot-item entries with zero parse
  issues. It reports aliases, overlaps, and free-space capacity, but does not
  yet emit replacement-aware immutable moves. The live run left both editable
  and patched ROM hashes unchanged.
- The focused manifest, allocator, dungeon-save, sprite-relocation, CLI, and
  editor-persistence regression set passes 142/142 tests.
- At this snapshot, pot items remained `repack_all` and failed closed for
  growth; publishing the OOS manifest-generator counterpart was also pending.

### 2026-07-15 pot soak and Oracle pipeline snapshot

- The pot repacker is wired through the production `DungeonEditorV2::SaveRoom`
  path and remains constrained to its manifest-declared region.
- Direct-manifest and project fixtures each passed 50 bounded
  save/destroy/reopen cycles. Every cycle reloaded the disposable ROM and
  matched the expected pot bytes exactly; the canonical OOS source hash was
  unchanged.
- A disposable Oracle build passed project-bundle verification (7/7) and
  object, sprite, and pot stream planning (296/296 valid slots for each kind,
  with zero issues or overlaps). Non-strict Oracle smoke also passed.
- Strict readiness reported only the known D3 content gap: room `0x32` has no
  authored custom collision. Follow-up found that the passing D4 check does
  not assert water-fill table membership; the old pipeline's second build had
  dropped room `0x27` from that generated table. Oracle-of-Secrets PRs #107
  and #109 preserve the tracked two-room table and add required-room
  regression coverage. All four D6 track rooms pass.
- The 50-cycle result is a focused pot persistence soak, not the complete
  daily-driver exit gate. Multi-domain edit cycles, backup restoration, a
  multi-hour GUI session, and Mesen room traversal remain required.
