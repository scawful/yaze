# Dungeon Editor UX Parity Matrix (ZScreamDungeon vs yaze)

**Status:** Draft
**Owner:** TBD (High model)
**Created:** 2026-02-03
**Last Reviewed:** 2026-02-03
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
| Drag modifiers | Needs capture | Needs capture | Unknown | Match axis lock/snap/duplicate modifiers; add optional improvements | TODO |
| Context menu (room) | Needs capture; includes room ops | User-reported missing actions | Missing parity actions | Audit ZScream actions; add missing actions + reorder | TODO |
| Context menu (objects) | Needs capture | Needs capture | Unknown | Align actions + add key shortcuts | TODO |
| Panel persistence | Window positions persist | User-reported positions reset/out of view | Layout restore missing | Persist window state + clamp to screen; add reset layout | TODO |
| Object palette filtering | Basic filter | Limited | Possible parity gap | Add fast filter/search + favorites | TODO |
| Custom overlays toggle | Toggle for extra overlays | User requests toolbar toggle | Missing control | Add toolbar toggle + state persistence | TODO |
| Minecart overlays | Needs capture | User-reported unreliable | Usability gap | Ensure overlay draw + selection snapping | TODO |

## Notes
- "Needs capture" means we need concrete ZScream behavior notes before finalizing.
- User reports are flagged where we haven't verified the current yaze behavior.
