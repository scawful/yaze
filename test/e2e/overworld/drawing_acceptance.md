# Overworld drawing acceptance

These opt-in ImGui Test Engine tests run the actual editor and SDL renderer. They
require an operator-supplied ZSCustomOverworld v3-compatible ROM. No ROM is
included in the repository. Follow
[remote native testing](../../../docs/internal/testing/remote-native-testing.md)
before launching, including dedicated Space routing and mandatory teardown.

Build the existing GUI target:

```sh
cmake --build --preset mac-ai --target yaze_test_gui --parallel 2
```

Use the resulting executable in the task-specific verification app prepared by
the native testing policy. Set an absolute source ROM path and a new output
directory; the tests refuse to overwrite existing per-test input/output files.

```sh
export YAZE_DRAWING_ACCEPTANCE_ROM=/absolute/path/to/disposable-v3-fixture.sfc
export YAZE_DRAWING_ACCEPTANCE_DIR=/absolute/path/to/new-acceptance-directory
/path/to/Verification.app/Contents/MacOS/yaze --ui --normal OverworldAcceptance
```

Expect `Test Results: 4/4 passed`. To isolate a case, use `LargeDrawing`,
`WideDrawing`, `TallDrawing`, or `PaletteUndoAfterNavigation` as the positional
filter and a fresh output
folder. The engine matches category and test name separately; a
`OverworldAcceptance/LargeDrawing` filter does not select this test. A `0/0`
result is not acceptance.

The three drawing cases copy the source, configure area `$30` as Large, Wide, or Tall,
centers a child map, and captures a 2x3 brush through right-button mouse input.
The Large brush spans `$30/$31/$38/$39`; Wide and Tall cross their respective
horizontal and vertical boundaries. Wide selects a nonuniform patch so the
one-row paste changes data. A ROM without such a patch fails fixture setup.

Assertions verify:

- Every captured tile matches its source coordinate.
- Hover/capture leaves the world tile matrix, ROM dirty flag, and undo
  availability unchanged.
- Editor Copy/Paste writes the expected destination tiles.
- Undo restores the complete world matrix, and redo restores the painted one.
- Editor serialization into `saved.sfc` reloads with the same complete light
  world tile matrix and area size through a fresh ROM/overworld model.
- Source and copied input ROM bytes remain unchanged.

`PaletteUndoAfterNavigation` replaces one hex digit in the visible Palette field
for area `$30` (one edit, without intermediate typed values),
navigates to unrelated map `$00`, and uses editor Undo/Redo. It checks all four
shared screens, their bitmap palettes, unchanged selection, and the unrelated
map and Tile16 atlas palettes. Both input ROMs remain unchanged. This verifies
palette data supplied to the renderer, not a GPU screenshot comparison.

The tests run native canvas input and the real editor command handlers. Saving
uses the editor serializer and explicit destination API; it does not drive the
native Save As dialog. Reopen uses a fresh data model rather than a new editor
window. These tests do not assert GPU preview pixels, project raw-graphics
manifest overlays or gameplay.
