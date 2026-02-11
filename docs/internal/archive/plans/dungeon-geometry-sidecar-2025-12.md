# Dungeon Geometry Sidecar Plan
Status: Draft | Owner: zelda3-hacking-expert | Created: 2025-12-08 | Next Review: 2025-12-15

## Goal
Establish a reusable geometry engine that derives object bounds directly from draw routines so selection/hitboxes/BG2 masks stay in lockstep with rendering.

## Current State
- Sidecar module added: `src/zelda3/dungeon/geometry/object_geometry.{h,cc}`.
- Uses draw routine registry to replay routines against a dummy buffer and reports extents in tiles/pixels.
- Anchor handling for upward diagonals; size=0 → 32 semantics preserved.
- Tests added: `test/unit/zelda3/dungeon/object_geometry_test.cc` (size=0 rightwards/downwards, diagonal acute upward growth).

## Next Steps (Implementation)
1) Expand routine coverage: add metadata/anchors for corner/special routines that move upward/left; add tests for corner routines (e.g., corner BothBG) and Somaria lines (size+1 growth).  
2) Add helper API on `ObjectGeometry` to return selection bounds + BG2 mask rectangle from measured extents (tile and pixel).  
3) Create parity tests against `ObjectDimensionTable::GetDimensions` for a sample set; document intentional deltas (selection vs render).  
4) Add CI guard: lightweight geometry test preset (unit) to catch routine table changes.  
5) Integration path (not committed yet):  
   - Swap `ObjectDrawer::CalculateObjectDimensions` to call geometry sidecar.  
   - Editor selection (`object_selection`, `dungeon_object_interaction`, `dungeon_canvas_viewer`) to consume geometry API.  
   - Remove duplicated size tables in `object_dimensions` after parity confirmed.

## References
- Disassembly: `~/Code/usdasm/bank_01.asm` (routine table at $018200).  
- Routine metadata source: `draw_routines/*` registry entries.
