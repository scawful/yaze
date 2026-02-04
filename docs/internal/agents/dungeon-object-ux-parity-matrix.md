# Dungeon Editor UX Parity Matrix (ZScreamDungeon vs yaze)

**Status:** Draft
**Owner:** TBD (High model)
**Created:** 2026-02-03
**Last Reviewed:** 2026-02-04
**Next Review:** 2026-02-17

## Purpose
Track UX parity gaps between ZScreamDungeon and yaze for dungeon object editing,
selection, and room context actions.

## Matrix (Draft)
| Area | ZScream behavior | yaze current behavior | Gap | Proposed fix | Status |
| --- | --- | --- | --- | --- | --- |
| Selection bounds (size=0) | Uses routine-specific size rules (e.g., 32 or 26) | Size-zero overrides + min clamp to 1 tile | Offsets still mismatch for diagonals/rails | Add per-routine offsets in hit-test/selection | PARTIAL |
| Selection priority | Needs capture | Needs capture | Unknown | Capture overlap rules; match priority order | TODO |
| Marquee select | Needs capture | Needs capture | Unknown | Match box inclusion rules (edge inclusion, multi-select) | TODO |
| Drag modifiers | Needs capture | Shift locks axis; Alt clears selection | Missing capture + duplicate/snap parity | Capture ZScream drag modifiers; add duplicate/snap rules if needed | PARTIAL |
| Context menu (room) | Needs capture; includes room ops | Canvas room menu: copy ID/name + open Room List/Matrix/Entrance List/Room Graphics | Missing ZScream parity actions + ordering | Audit ZScream actions; add missing actions + reorder | PARTIAL |
| Context menu (objects) | Needs capture | Needs capture | Unknown | Align actions + add key shortcuts | TODO |
| Panel persistence | Window positions persist | Stable panel IDs + viewport clamp; Reset Layout action restores defaults | Needs cross-session verification + ensure ini save/restore stays consistent | Verify ini persistence and docking save/restore | PARTIAL |
| Object palette filtering | Basic filter | Limited | Possible parity gap | Add fast filter/search + favorites | TODO |
| Custom overlays toggle | Toggle for extra overlays | Toolbar + context menu toggle (minecart) | Needs persistence + more overlays | Persist overlay state; add additional overlay toggles | PARTIAL |
| Minecart overlays | Needs capture | Track origin overlay drawn + picking highlight | Needs full ZScream parity check | Verify coordinates/picking parity and add selection snapping if needed | PARTIAL |

## Notes
- "Needs capture" means we need concrete ZScream behavior notes before finalizing.
- User reports are flagged where we haven't verified the current yaze behavior.
