# Manual editor checks and continuation queue

Updated: 2026-09-23. Owner of manual checks: scawful, when ready.
Manual checks are deferred at the user's request; continue development meanwhile.

## Test later: consolidated overworld and Tile16

Combined candidate branch: `codex/combined-editor-candidate`. It integrates
`0f3855d6f` (overworld/Tile16) and `804b32eea` (dungeon authoring).
Record the final merge commit from the candidate provenance file when testing. The Barista launcher
has not been updated. Use the combined candidate app and a copy of a ROM,
keeping the working original untouched.

- [ ] Open Tile16 by double-click and by Edit. Confirm useful initial size,
  dock it narrow, then reopen it. Check layout and controls remain usable.
- [ ] Paint one tile and a rectangular selection. Check preview matches placement,
  map boundaries target the correct map, and right-click actions use the clicked tile.
- [ ] Change Tile16 graphics, palette and flips; switch definitions and maps.
  Confirm edits appear immediately without Commit/Discard prompts.
- [ ] Interleave map strokes and Tile16 edits, then Undo and Redo. Each operation
  should restore the expected tiles and previews in chronological order.
- [ ] Open two ROM sessions. Confirm opening/editing Tile16 affects the intended
  session. Save the test copy and reopen it independently to verify persistence.

Repeat relevant painting checks across Light/Dark/Special worlds and supported
Small/Large/Wide/Tall maps, including areas with different graphics and palettes.
Record build commit, ROM type, map ID, action, expected result, actual result.

## Test later: overworld sprites

- [ ] On a test ROM copy, insert, move, change the type, and delete sprites in
  supported game states. Save/reopen and check positions and types.
- [ ] Check parent-area child screens and switch ROM sessions. Save an insertion,
  undo it, save again, and reopen to confirm the sprite is removed.

These checks remain deferred. Automated tests use synthetic in-memory ROM data.

## Test later: dungeon pot-item coordinates

- [ ] Select and move pot items on odd tile rows and the lower layer. Confirm
  canvas position, hit testing, inspector coordinates, and 8-pixel arrow nudges.
- [ ] Undo/Redo, then save/reopen a test ROM copy. Check positions and item types.

## Test later: dungeon authoring in the same app

- [ ] Place, resize and inspect objects, including walls/trim and masks. Confirm
  the placed result matches the hover preview, including after Undo/Redo.
- [ ] Add a chest, choose its reward, move it, and delete it. Undo/Redo should
  restore the object and reward together. Save/reopen the test copy and verify both.
- [ ] Select objects alongside sprites, doors and pot items. Check an
  object-only Delete affects only objects; mixed edits should undo together.
- [ ] Connect ordinary doors between two rooms. Inspect both endpoints;
  Undo/Redo should remove/restore both. Save/reopen and check both rooms.
- [ ] Clone a room into a disposable destination and export/import room JSON.
  Confirm the source is unchanged, rejected imports leave the destination
  unchanged, and accepted operations survive Undo/Redo and save/reopen.
- [ ] Save a room with **Save Room File...**, then choose **Import JSON → Open
  Room File...** in another disposable destination. Loading alone must not change
  the room. Preview and apply, then Undo/Redo and save/reopen. Cancel a file dialog
  and try invalid JSON; the prior import form should remain available.
- [ ] Check supported spriteset choices and invalid/capacity errors. Rejected
  operations must preserve the room and history.

## Integration status

The two source branches are combined in an isolated checkout. Build and automated
qualification results are recorded in the combined candidate handoff. Manual
checks above remain open regardless of automated results. Concurrent sprite-catalog
work in the main checkout is separate and must not be overwritten or silently
included in this candidate.

## Requested work to continue

1. **Overworld sprite persistence implemented; manual check deferred.** Both
   model and editor saves now serialize loaded sprite edits, preserve other
   lists, and reject invalid/overflow data before publication. Synthetic
   vanilla/expanded save/reload and failure tests passed. Follow-up evidence:
   [sprite persistence handoff](../agents/overworld-sprite-persistence-2026-09-23.md).
2. **Dungeon authoring for 0.8.0.** Resume completion against the release ladder:
   object identification/previews, placement/resize, room workbench save paths,
   and editable pits/blocks. Use the integrated chest, reciprocal-door, and room
   clone/import paths rather than adding duplicate implementations.
3. **Useful UI and readable source.** Continue selector/context-menu workflows
   and bounded cleanup. Profile overworld loading before further optimization;
   keep correctness changes separate from performance changes.
4. **Contributor resources.** Keep the Hyrule Magic/ZScream capability baseline,
   current status, next-agent instructions, and ownership guidance current.
   Preserve small UI/layout tasks for the user when they resume coding.
5. **Tool-assisted maintenance.** Apply clang-format and scoped clang-tidy checks
   with focused regression tests. Avoid unrelated mass rewrites or making
   unfinished shortcut/status-bar drafts part of active tests.

The release ladder remains authoritative: 0.8.0 focuses on dungeon completion;
overworld completion is the 0.9.0 milestone. Repairing regressions does not change
that release scope.

## Evidence and handoff

See [the consolidation record](../agents/cursor-overworld-consolidation-2026-09-23.md)
and [the release ladder](release-ladder-0x-2026.md).
Automated checks passed for the consolidated candidate; manual checks above
remain unchecked and must not be reported as completed.
