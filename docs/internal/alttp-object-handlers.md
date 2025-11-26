# ALTTP Dungeon Object Handler Tables

Source: `zelda3.sfc` (usdasm) — generated via `scripts/dump_object_handlers.py`.

## Table Summary
- Type 1 (standard objects) — SNES $01:8200 — 256 entries
- Type 2 (extended objects) — SNES $01:8470 — 64 entries
- Type 3 (special objects) — SNES $01:85F0 — 128 entries

## Type 1 (Object 0x000–0x0FF)
- Handler table: $01:8200 (PC 0x008200)
- Total objects: 256
- Unique handlers: 100
- Shared handlers: 156
- Most common handlers:
  - $01:8AA3 — used by 32 objects
  - $01:8F62 — used by 22 objects
  - $01:8FA5 — used by 19 objects
  - $01:8C58 — used by 6 objects
  - $01:8C61 — used by 6 objects

Example entries:
  - 0x000 → $01:8B89
  - 0x001 → $01:8A92
  - 0x005 → $01:8C37
  - 0x009 → $01:8C58
  - 0x00A → $01:8C61

## Type 2 (Object 0x000–0x03F)
- Handler table: $01:8470 (PC 0x008470)
- Total objects: 64
- Unique handlers: 30
- Shared handlers: 34
- Most common handlers:
  - $01:97ED — used by 12 objects
  - $01:9813 — used by 8 objects
  - $01:9895 — used by 7 objects
  - $01:9854 — used by 4 objects
  - $01:985C — used by 4 objects

Example entries:
  - 0x000 → $01:97ED
  - 0x010 → $01:9854
  - 0x01D → $01:8F30
  - 0x02D → $01:A41B
  - 0x03F → $01:9A0C

## Type 3 (Object 0x000–0x07F)
- Handler table: $01:85F0 (PC 0x0085F0)
- Total objects: 128
- Unique handlers: 59
- Shared handlers: 69
- Most common handlers:
  - $01:9895 — used by 35 objects
  - $01:9C3E — used by 10 objects
  - $01:99E6 — used by 7 objects
  - $01:9BD9 — used by 4 objects
  - $01:97ED — used by 4 objects

Example entries:
  - 0x000 → $01:9D29
  - 0x00F → $01:9C3E
  - 0x016 → $01:B493
  - 0x02A → $01:9DE5
  - 0x07F → $01:8AA3

## Handler Bytes (samples)
- Handler $01:8B89 (Type1 Obj 0x000): first 64 bytes:  
  `20 CC B0 20 95 98 C6 B2 D0 F9 60 E6 B2 E6 B4 A5 B2 85 0A BD 52 9B 97 BF 97 C2 97 C5 97 C8 97 CB 97 CE 97 D1 97 D4 98 18 69 00 01 A8 BD 52 9B 97 BF 97 C2 97 C5 97 C8 97 CB 97 CE 97 D1 97 D4 98`
- Handler $01:8A92 (Type1 shared): first 32 bytes:  
  `20 BE B0 86 0A A9 02 00 20 F0 97 A6 0A C6 B2 D0 F4 60 8A BB A8 20 AC B0 B9 52 9B 9F 00 40 7E 9F`
- Handler $01:97ED (Type2 common): first 32 bytes:  
  `A9 04 00 85 0E BD 52 9B 97 BF BD 54 9B 97 CB BD 56 9B 97 D7 BD 58 9B 97 DA 8A 18 69 08 00 AA C8`

## Notes / Next Steps
- Full per-object table (256/64/128 entries) was emitted by the script; consider importing the CSV/TSV variant if we want all rows in-doc.
- Phase 1 follow-up: for each handler, map WRAM touches (tilemaps, offsets, flags) and shared subroutines in bank $01 to build `alttp-wram-state.md`.
- Handler hotspots to debug (from prior plan): $01:3479 (reported loop); use cycle trace + WRAM init capture when emulating.
