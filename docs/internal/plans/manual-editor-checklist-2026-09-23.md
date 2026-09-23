# Manual editor checks and continuation queue

Updated: 2026-09-23. Owner of manual checks: scawful, when ready.
Manual checks are deferred at the user's request; continue development meanwhile.

## Test later: consolidated overworld and Tile16

Source candidate: `f5df9c7fb`. Record the actual build commit when testing a
later build. The Barista launcher has not been updated by this consolidation.
Use a copy of a ROM, keeping the working original untouched.

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

## Requested work to continue

1. **Overworld sprite persistence implemented; manual check deferred.** Both
   model and editor saves now serialize loaded sprite edits, preserve other
   lists, and reject invalid/overflow data before publication. Synthetic
   vanilla/expanded save/reload and failure tests passed. Follow-up evidence:
   [sprite persistence handoff](../agents/overworld-sprite-persistence-2026-09-23.md).
2. **Dungeon authoring for 0.8.0.** Resume completion against the release ladder:
   object identification/previews, placement/resize, room workbench save paths,
   and editable pits/blocks. Reconcile existing chest, reciprocal-door, and room
   clone/import work before adding or duplicating features.
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
