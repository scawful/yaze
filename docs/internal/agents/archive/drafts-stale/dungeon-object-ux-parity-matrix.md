# Dungeon Editor UX Parity Matrix (ZScreamDungeon vs yaze)

**Status:** Draft
**Owner:** TBD (High model)
**Created:** 2026-02-03
**Last Reviewed:** 2026-02-05
**Next Review:** 2026-02-17

## Purpose
Track UX parity gaps between ZScreamDungeon and yaze for dungeon object editing,
selection, and room context actions.

## Matrix (Draft)
| Area | ZScream behavior | yaze current behavior | Gap | Proposed fix | Status |
| --- | --- | --- | --- | --- | --- |
| Selection bounds (size=0) | Uses routine-specific size rules (e.g., 32 or 26) | Size-zero overrides + min clamp to 1 tile; hit-test now uses selection offsets | Offsets still mismatch for diagonals/rails | Add per-routine offsets in hit-test/selection | PARTIAL |
| Selection priority | Code-confirmed: iterates objects from end (last added wins); filtered by active layer unless Bgallmode; collisionPoint check for most objects; Alt clears selection | Reverse-order hit test now respects layer filter | CollisionPoint vs bounds parity unverified | Validate collisionPoint selection behavior; decide if bounds-only is sufficient | CODE CONFIRMED / RUNTIME PENDING |
| Marquee select | Code-confirmed: starts only when no prior selection; intersects object bounds | Rectangle select starts only when no selection; intersects bounds; min size threshold; Alt cancels | Edge inclusion rules + additive marquee parity unverified | Validate edge inclusion + additive/toggle behavior | CODE CONFIRMED / RUNTIME PENDING |
| Drag modifiers | Code-confirmed: none (selection modifiers only; Alt clears selection) | Shift locks axis; Alt-drag duplicates selection (yaze-only) | ZScream runtime capture still needed; ensure Alt-drag doesn't conflict | Verify runtime behavior + document | CODE CONFIRMED / RUNTIME PENDING |
| Context menu (room) | Code-confirmed: insert/paste/delete/delete-all with mode-dependent labels | Canvas room menu: copy ID/name + open Room List/Matrix/Entrance List/Room Graphics | Missing ZScream parity actions + ordering | Audit ZScream actions; add missing actions + reorder | CODE CONFIRMED / RUNTIME PENDING |
| Context menu (objects) | Code-confirmed: Insert/Cut/Copy/Paste/Delete + layer/z-order; group adds Save As New Layout; single has Edit Graphics (disabled) | Insert menu (object/sprite/item/door), cut/copy/paste/duplicate/delete/delete-all + arrange/layer + Edit Graphics jump to sheet; Save As New Layout (disabled) | Ordering parity + Save As New Layout still unimplemented | Verify ordering; implement Save As New Layout later | CODE CONFIRMED / RUNTIME PENDING |
| Panel persistence | Window positions persist | Stable panel IDs + viewport clamp; Reset Layout action restores defaults | Needs cross-session verification + ensure ini save/restore stays consistent | Verify ini persistence and docking save/restore | PARTIAL |
| Object palette filtering | Basic filter | Search + category filter in object browser | Favorites + tag filters not implemented | Add favorites + per-tag filters | PARTIAL |
| Custom overlays toggle | Code-skim: unknown | Toolbar + context menu toggle (minecart) | Needs persistence + more overlays | Persist overlay state; add additional overlay toggles | PARTIAL |
| Minecart overlays | Needs capture | Track origin overlay drawn + picking highlight | Needs full ZScream parity check | Verify coordinates/picking parity and add selection snapping if needed | PARTIAL |

## Notes
- "Needs capture" means we need concrete ZScream behavior notes before finalizing.
- User reports are flagged where we haven't verified the current yaze behavior.
- Capture log lives at `docs/internal/agents/zscream-capture-log.md`.

## Capture Checklist (ZScreamDungeon)
- Selection priority: click overlapping objects in each layer order (BG1/BG2/BG3), note which object wins.
- Marquee select: drag box on partial overlaps (edge-only, corner-only, inside) and note inclusion rules.
- Drag modifiers: confirm Shift/Alt/Ctrl behaviors (axis lock, duplicate, snap, additive) and cursor feedback.
- Room context menu: capture full action list + ordering + separators + key modifiers.
- Object context menu: capture full action list + ordering + shortcuts + multi-select behavior.

## Capture Pass Status (2026-02-05)
- Runtime capture not yet performed; current notes are based on code-skim.
- Next: run ZScreamDungeon, record short clips for selection/drag/context menus.
