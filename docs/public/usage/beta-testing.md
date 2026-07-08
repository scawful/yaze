# Beta testing yaze

YAZE is pre-1.0. The desktop app is the best beta target; the web build is a
preview and has more file-management and rendering limitations.

## Good first test pass

1. Download the latest desktop release for your platform.
2. Open a clean, headerless ALttP ROM.
3. Try a small edit in each area you care about:
   - **Overworld Editor**: switch to Brush mode, pick/sample a Tile16, paint a few tiles, save, reopen.
   - **Dungeon Editor**: open a room, move or add a small object/sprite, save, reopen.
   - **Graphics/Palette/Message**: make a tiny reversible edit and verify it persists.
4. Try your real hack or Hyrule Magic project only after the clean-ROM smoke
   works.

## ROM compatibility expectations

- Clean US/JP ROMs should be the easiest path.
- Expanded and patched ROMs are expected to improve over time, but may still
  expose editor write-range conflicts.
- If a ROM already has IPS/ASM patches or Hyrule Magic edits, keep a clean ROM
  plus your patch sources when possible. That gives yaze a safer baseline and a
  clearer way to diagnose custom-memory conflicts.
- If a "dirty" ROM fails, it is still useful: report it with the ROM shape,
  patches applied, editor action attempted, and whether it opens in other
  editors.

## Common UX notes

- **Overworld Editor** edits playable overworld areas: maps, Tile16 painting,
  entrances, exits, items, and sprites.
- **Screen Editor > Overworld Map** edits the pause-menu world map art. It is a
  different screen editor, not the playable overworld editor.
- For overworld painting, use Brush mode and pick a Tile16 from the selector, or
  right-click / use the eyedropper shortcut to sample an existing map tile.

## Useful bug reports

Include:

- Platform and release version.
- ROM type: clean/expanded/patched/Hyrule Magic-derived.
- Exact editor and action.
- What you expected vs. what happened.
- Whether save/reopen changed the result.
- Screenshots or a short screen recording when the issue is visual or UX-related.
