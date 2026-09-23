# Overworld sprite persistence — 2026-09-23

## Result

Both save paths now persist loaded overworld sprite edits. UI movement uses a
16-pixel grid even with the general free-movement modifier. No personal ROM,
installed app, or save file was changed. Manual testing remains deferred.

Implementation ownership:

- `src/zelda3/overworld/overworld_sprite_io.*`: bounded decoding, save planning,
  exact ordered stream deduplication, fenced atomic publication.
- `src/zelda3/overworld/overworld.cc`: loaded-map ownership, world-position
  encoding, preservation of coordinate flags, save integration. Repeated sprite
  loads replace vectors; truncated streams fail without appending partial data.
- `src/app/editor/overworld/overworld_editor.cc`: preflight sprite capacity and
  project manifest conflicts, apply plan on save, roll back ROM on any failure.
- `src/app/editor/overworld/entity/entity.cc`: representable sprite drag grid.
- `src/zelda3/sprite/sprite.h`: retain packed coordinate bits not exposed by UI.

## Format evidence and boundaries

Verified against local USDASM `bank_09.asm`: pointer tables at SNES `$09C881`,
`$09C901`, `$09CA21`; `Overworld_Sprites_EMPTY` starts at `$09CB41`;
dungeon sprite pointers start at `$09D62E`. Records are Y/X/type triples with
a single `$FF` terminator. Bank-$09 offsets are converted to PC offsets.

Vanilla/v1/v2 use counts 64/144/144 and payload `[0x4CB41, 0x4D62E)`.
Expanded v3 uses three 160-entry pointer tables at PC `0x141438`, `0x141578`,
`0x1416B8` and payload `[0x4C881, 0x4D62E)`. Expanded addresses also match
existing Yaze constants and ZScream's `Constants.cs`. The writer is implemented
independently and preserves sprite order and multiplicity when deduplicating.

The existing editor loader coverage (64/144/144 parent maps) is unchanged.
Unloaded entries remain intact, not inferred empty. No arbitrary relocation,
extra bank allocation, or cross-area sprite reassignment is implemented.
Unknown out-of-region pointers fail closed. Capacity failure does not publish
any ROM bytes. Existing outer write fences and project manifest policy apply.

## Verification

Build passed in the repair worktree:

```sh
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='*OverworldSpriteIoTest*:*OverworldSpritePlacementTest*' --gtest_list_tests
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='*OverworldSpriteIoTest*:*OverworldSpritePlacementTest*'
```

33 tests passed: vanilla/expanded insert/move/type/delete/reload, shared-pointer
detachment, duplicate/order/flag preservation, untouched tail entries, no-op
byte identity, capacity boundary/overflow, invalid pointers/terminators, truncated
regions, complete unrelated-byte preservation, write-fence rollback, editor
save with map saving disabled, saved insertion removal, and sprite drag grid.
Synthetic reload uses a separate in-memory Rom; no disk-save or emulator claim.

147 surrounding unit tests passed with the filter:

```text
*OverworldItemOperationsTest*:*WriteFence*:*RomTransaction*:Tile16DocumentHistoryTest.*:*TilePaintingManager*:OverworldTilePaintActionTest.*:OverworldPaintRefreshTest.*:MapRefreshCoordinatorTest.*:CanvasNavigationManagerTest.*:OverworldEditorStateTest.*:Tile16EditorActionStateTest.*:Tile16EditorShortcutsTest.*:Tile8SourceInteractionTest.*:MapPropertiesContextMenuTest.*:OverworldContextTargetTest.*:CanvasContextMenuOpenTest.*:CanvasContextMenuRoleTest.*
```

Scoped clang-tidy `-*,clang-analyzer-*` passed for `overworld_sprite_io.cc` using
an isolated Debug compilation database without the Apple compiler's binary PCH.
No general clang-tidy or whole-repository analyzer result is claimed.
Logs use `/tmp/yaze-sprite-` prefixes: `save-final-build.log`, `tests.log`,
`regression.log`, and `analysis.log`.

## Next

Return to dungeon 0.8.0 completion: reconcile existing chest, reciprocal-door,
and room clone/import implementations with the outstanding authoring baseline
before adding more features. User's native sprite save/reopen checks remain in
[the deferred checklist](../plans/manual-editor-checklist-2026-09-23.md).
