# Dungeon parity baselines

Each file lists the differences between yaze's room tilemaps and one capture
of the real game that have been **reviewed and accepted**. The gate
(`DungeonGameParityGate` in
`test/integration/zelda3/dungeon_game_tilemap_parity_test.cc`) fails on
anything else.

The captures themselves are ROM-derived and are never committed. A baseline
stores only positions, a reason, evidence, and a 12-digit digest of each
(game word, yaze word) pair, so it detects a changed difference without
storing tile data.

## What the gate checks

1. The ROM under test is the captured ROM. Identity is the SHA-1 of the ROM
   zero-padded to at least 2 MB: the vanilla captures were made from a 2 MB
   zero-padded copy of the 1 MB ROM, so the original file and that copy both
   match, and nothing else does.
2. The capture matches the baseline's `rom_sha1`, `entrance` and `room_flags`,
   and every `required_rooms` entry is present, `ok`, the right size, and
   matches its manifest SHA-1.
3. Every difference is listed, unchanged, and in a group with
   `"reviewed": true` and non-empty `evidence`. Entries whose tile now matches
   the game are reported as stale and must be removed.

## Updating a baseline

Verification never writes a baseline. To start one:

```bash
YAZE_TEST_ROM_VANILLA=<rom> YAZE_GAME_TILEMAP_DIR=<capture> \
YAZE_PARITY_STATE=default YAZE_PARITY_CANDIDATE_OUT=<new file> \
yaze_test_integration --gtest_filter='DungeonParityBaselineCandidate.*'
```

The candidate is written only to a path that does not exist yet, and every
group starts with `"reviewed": false`. For each group:

1. Check the evidence against the game itself: the capture's WRAM, the room's
   header tags, and the routine in usdasm.
2. Correct the reason and evidence if needed.
3. Only then set `"reviewed": true`.

Groups with reason `object-difference` or `unexplained` are renderer
differences until shown otherwise; fix yaze instead of accepting them.

## Files

| File | Capture |
|---|---|
| `vanilla_us.default.json` | entrance `0x34`, no room flags (the `_full` capture) |
| `vanilla_us.all_flags.json` | each room's own entrance, `--room-flags 0xFF,0xFF` |
