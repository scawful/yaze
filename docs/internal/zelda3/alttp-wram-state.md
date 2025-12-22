# ALTTP Dungeon Object WRAM Usage (Initial Pass)

Heuristic scan: first 64 bytes of all dungeon object handlers (Type1/2/3) from `zelda3.sfc`, looking for absolute/long accesses to $7E:*. This is a starting point; many handlers use indirect addressing, so manual tracing is still required.

## Observed WRAM Touches (by handler)

### Handler $01:A7A3
Offset | Op | WRAM
------ | -- | ----
0x30 | AF | $7EF0CA

### Handler $01:A7B6
Offset | Op | WRAM
------ | -- | ----
0x1D | AF | $7EF0CA

### Handler $01:A7D3
Offset | Op | WRAM
------ | -- | ----
0x00 | AF | $7EF0CA

---

## Next Steps
- Manually trace key handlers (e.g., Type1 $01:8B89, $01:8AA3; Type2 $01:97ED; Type3 $01:9C3E, $01:9895) to capture indirect $7E accesses and required initialization (tilemaps, offsets, flags).
- Expand scan window beyond 64 bytes and follow subroutine calls to capture deeper WRAM dependencies.
- Cross-reference accesses with known tilemap buffers ($7E2000/$7E4000), offsets ($7E049C/$7E049E), drawing flags ($7E0476), object ptrs ($7E00B7-B9), room state ($7E00AF/$7E00A0), and any temp work areas the handlers expect.
- For future state-injection (“load me into dungeon X with items Y”): map and annotate inventory/state WRAM (sword/shield/armor bits, small keys, big key, map/compass, heart count, rupees/bombs/arrows, crystal/pendant flags), Link position ($7E22–$7E2A ranges), camera offsets, submodule/state machine bytes. Use emulator save-state comparisons to seed defaults if not present in handlers.

---

## Seeded Known Addresses (to verify/expand)
- Room/submodule: `$7E00A0` (submodule/floor), `$7E00AF` (room ID)
- Object data pointer: `$7E00B7-$7E00B9` (24-bit pointer)
- Drawing flags/context: `$7E0476`
- Tilemap offsets: `$7E049C` (X), `$7E049E` (Y)
- Tilemap buffers: `$7E2000-$7E3FFF` (BG1), `$7E4000-$7E5FFF` (BG2)
- Temp/dungeon draw scratch (from handler traces, needs confirmation):
  - `$7E7E21-$7E7E26` – frequently touched across handlers (likely temp counters/coords)
  - `$7E7E71`, `$7E7EAA`, `$7E7EAD`, `$7E7ED1` – sparse temps/state flags
  - `$7E7E22A8`, `$7E7E2BB2`, `$7E7E411E` – long addresses referenced; verify if pointer tables/accumulators
  - `$7E7EC000-$7E7EC7FF` region – heavily used; appears to be work buffers (coords, queues, staging). Needs field-by-field labeling.
  - `$7E7EF0CA`, `$7E7EF3CA` – high WRAM touched by A7xx/B3xx handlers; likely state or decompression buffers.

## Data to Capture (for emulator state injection)
- Link position and camera: positions/velocities, camera scroll, room boundaries
- Inventory & flags: sword/shield/armor upgrades, small keys, big key, map/compass, rupees/bombs/arrows, bottle contents, pendant/crystal flags, boss defeat flags
- UI/graphics state: HUD counters, palette state, tilemap/CHR load state tied to dungeon/overworld room
- State machine: submodule/module bytes that gate movement/control and loading paths

## Suggested Capture Workflow
1. Select a target handler (e.g., Type1 $01:8B89) and run a trace for 256–512 bytes including subroutine calls; log all $7E accesses (direct and via indexed/indirect).
2. In emulator, save two WRAM snapshots: before/after drawing a known object; diff to identify required fields. Repeat for a few object classes (floors, walls, special objects).
3. Populate this doc with verified addresses: name, size, purpose, default/init value, “required for preview?” flag, and “required for state-injection?” flag.
4. Once core init set is stable, script a minimal WRAM initializer for the emulator preview and for “load me into dungeon X with items Y” (inventory/state injection).

---

## Static Scan (first 256 bytes per handler)
Heuristic, direct $7E absolute/long accesses found in the first 256 bytes of each handler:

- $01:9DD9 → $7E7E22
- $01:9DE5 → $7E7E22, $7E7E23
- $01:9E30 → $7E7E22, $7E7E23
- $01:9EA3 → $7E7E22, $7E7E23
- $01:A095 → $7E7E21
- $01:A71C → $7EF0CA
- $01:A74A → $7EF0CA
- $01:A7A3 → $7EF0CA
- $01:A7B6 → $7EF0CA
- $01:A7D3 → $7EF0CA
- $01:B306 → $7EF3CA
- $01:B30B → $7EF3CA
- $01:B310 → $7EF3CA
- $01:B376 → $7EF3CA
- $01:B381 → $7EF3CA
- $01:B395 → $7EF3CA

> These are starting points only; many handlers use indexed/indirect addressing and deeper subroutines that won’t show up in this static slice. Follow-up tracing is required to confirm purpose and defaults.

---

## Focus Handlers (1KB trace + subcalls, depth 3)
Direct $7E touches seen for each representative handler. Next step: label, size, and defaults via save-state diffs.

### Type1 $01:8B89 (obj 0x000)
Touches include:
- Low scratch: `$7E7E21-$7E7E26`, `$7E7E71`, `$7E7EAA`, `$7E7EAD`, `$7E7ED1`
- Work buffers: `$7E7EC000-$7E7EC7FF` (numerous fields), `$7E7EF051`, `$7E7EF0CA`, `$7E7EF282`, `$7E7EF2BB`, `$7E7EF2C3`, `$7E7EF2F0`, `$7E7EF2FB`, `$7E7EF340-$7E7EF37B`, `$7E7EF3C5-$7E7EF4FE`

### Type1 $01:8AA3 (common handler)
Touches mirror 8B89: `$7E7E21-$7E7E26`, `$7E7E71`, `$7E7EAA`, `$7E7EAD`, `$7E7ED1`, wide `$7E7EC***` work buffers, `$7E7EF0CA`, `$7E7EF34x` block, `$7E7EF3C5+`, `$7E7EF4FE`.

### Type2 $01:97ED
Touches: `$7E7E21-$7E7E26`, `$7E7ED1`, `$7E7E3F1E`, `$7E7E9059`, many `$7E7EC000+` fields, `$7E7EC3DC/DE/F6/F8`, `$7E7EC4FA`, `$7E7EC5DA+`, `$7E7EC7F2/7F4`, `$7E7EC832/834`, `$7E7EF0CA`, `$7E7EF282`, `$7E7EF2BB`, `$7E7EF2C3`, `$7E7EF2F0`, `$7E7EF2FB`, `$7E7EF340-$7E7EF37B`, `$7E7EF3C5-$7E7EF3D3`, `$7E7EF403`, `$7E7EF4FE`.

### Type3 $01:9C3E
Touches: `$7E7E21-$7E7E26`, `$7E7ED1`, `$7E7E0709`, `$7E7E3F1E`, `$7E7EC000+` region, `$7E7EC3DC/DE/F6/F8`, `$7E7EC4FA`, `$7E7EC5DA+`, `$7E7EC7F2/7F4`, `$7E7EC832/834`, `$7E7EF0CA`, `$7E7EF282`, `$7E7EF2BB`, `$7E7EF2C3`, `$7E7EF2F0`, `$7E7EF2FB`, `$7E7EF300`, `$7E7EF340-$7E7EF379`, `$7E7EF3C5-$7E7EF3D3`, `$7E7EF403`, `$7E7EF4FE`.

### Type3 $01:9895
Touches similar to 9C3E/97ED: `$7E7E21-$7E7E26`, `$7E7ED1`, `$7E7E3F1E`, `$7E7E9059`, `$7E7EC000+`, `$7E7EC3DC/DE/F6/F8`, `$7E7EC4FA`, `$7E7EC5DA+`, `$7E7EC7F2/7F4`, `$7E7EC832/834`, `$7E7EF0CA`, `$7E7EF282`, `$7E7EF2BB`, `$7E7EF2C3`, `$7E7EF2F0`, `$7E7EF2FB`, `$7E7EF340-$7E7EF37B`, `$7E7EF3C5-$7E7EF3D3`, `$7E7EF403`, `$7E7EF4FE`.

> Labeling needed: Map each address to meaning (coord, size, flags, palette, queues, staging buffers). Use emulator WRAM diffs around object draws and known state toggles to confirm.
