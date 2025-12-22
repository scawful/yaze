# Save State Format (v2)

This documents the chunked save-state format introduced in `Snes::saveState/loadState` (state file version 2).

## Goals
- Forward/backward compatible: per-file header + per-chunk versioning.
- Corruption-resilient: CRC on each chunk; size caps to avoid runaway allocations.
- Explicit layout: avoid raw struct dumps to sidestep padding/endianness issues.

## File Structure
```
uint32  magic   = 'YAZE' (0x59415A45)
uint32  version = 2
repeat until EOF:
  Chunk {
    uint32 tag      // ASCII packed: 'SNES', 'CPU ', 'PPU ', 'APU '
    uint32 version  // per-chunk version; currently 1 for all
    uint32 size     // payload size in bytes (capped at 16 MiB)
    uint32 crc32    // CRC-32 of payload
    uint8  payload[size]
  }
```

## Chunk Payloads (v1)
- `SNES`: Core machine state (WRAM, timers, IRQ/NMI flags, latched ports, timers, etc.).
- `CPU `: CPU registers/flags + breakpoint list (capped to 1024 entries).
- `PPU `: PPU registers, VRAM/CGRAM/OAM, layer/window/bg structs written field-by-field.
- `APU `: APU registers, ports, timers, and 64K ARAM (capped) plus DSP/SPC700 state.

## Compatibility
- Legacy v1 flat saves (no magic) are still loadable: the loader falls back if the magic/version header is missing. They do not carry CRCs and remain best-effort only.
- Host endianness: serialization assumes little-endian hosts; load/save will fail fast otherwise.

## Validation & Errors
- Size guard: any chunk `size > 16 MiB` is rejected.
- CRC guard: mismatched CRC rejects the load to avoid partial/dirty state.
- Missing required chunks (`SNES`, `CPU `, `PPU `, `APU `) rejects the load.
- Streams are checked for `fail()` after every read/write; callers receive `absl::Status`.

## Extending
- Add a new chunk tag and bump its per-chunk `version` only. Keep `file version` stable unless the top-level format changes.
- Keep payloads explicit (no raw struct dumps). Write scalars/arrays with defined width and order.
- If you add new fields to an existing chunk, prefer:
  1. Extending the payload and bumping that chunk’s version.
  2. Keeping old fields first so older loaders can short-circuit safely.

## Conventions
- Tags use little-endian ASCII packing: `'SNES'` -> `0x53454E53`.
- CRC uses `render::CalculateCRC32`.
- Max buffer cap mirrors the largest expected subsystem payload (WRAM/ARAM).
