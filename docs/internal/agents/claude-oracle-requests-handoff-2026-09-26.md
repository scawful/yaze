# Claude handoff: Oracle RC requests (2026-09-26)

## Context

Work requested by the Oracle RC session on scawful's behalf, after
`docs/internal/agents/claude-editor-integration-handoff-2026-09-25.md`.
Everything is local. Nothing is pushed or merged, and `gh` needs
`gh auth login`.

Worktree: `~/src/hobby/yaze-worktrees/collision-pair-save`. It holds two
branches, both based on `d1519b02e` (`claude/editor-integration` plus the D6
report). They are separate and not merged with each other.

| Branch | Commits | What |
|---|---|---|
| `claude/collision-pair-save` | `6f6afaeee`, `b289a9565` | Saving the project's ROM keeps `[files] custom_collision_json` in step |
| `claude/cutscene-camera` | `2577c866f`, `0cbd2fb62`, `9509f53b2`, this doc | Cutscene Camera tool |

The D6 minecart report is `docs/internal/agents/d6-minecart-report-2026-09-26.md`
(`d1519b02e`).

## Paired ROM + custom_collision.json save

- The project key is `[files] custom_collision_json`. scawful has added it to
  `Oracle-of-Secrets.yaze`.
- It applies only when the project's own ROM (`rom_filename`) is saved, never
  on Save As or a copy opened under the project.
- Unchanged collision bytes: only the ROM is written, and the JSON is untouched.
- Changed collision: `core::PublishSourceArtifacts` writes the ROM and JSON
  together, and a failed write leaves both unchanged.
- A JSON edited outside yaze is refused, and nothing is written.

Verified: 7 `CollisionSourcePairing*` tests pass. With
`YAZE_ORACLE_ROOT=~/src/hobby/oracle-of-secrets`, the two Oracle tests copy the
project and run:
- Oracle's `validate_custom_collision_source.py` after a no-op save and after a
  collision edit, directly and through `EditorManager::SaveRom`;
- a Save As, which leaves the JSON alone.

Not verified: a hand check in the app. scawful can edit collision on the real
project and run the validator.

Note: the project's `[rom] expected_hash` (`58c9fadc…`) does not match
`Roms/oos168.sfc` (SHA-1 `3f297fd9…`). Opening and saving still worked.

## Cutscene Camera

- **Model** (`src/zelda3/cutscene/cutscene_shot.*`): a strict version-1 JSON
  shots document, stored under the `[files] cutscene_shots` project key.
- **Camera limits:** taken from the game's `Overworld_SetCameraBoundaries`.
  The right edge is at width − 256; the bottom is at height − 226.
- **Area sizes:**
  - `VanillaAreaExtent`: from `OverworldTransitionPositionX/Y`.
  - `AreaExtentFromParent`: from ZSCustomOverworld sizes.
- **Window** (`src/app/editor/cutscene/ui/window/cutscene_camera_panel.*`, "Cutscene
  Camera", Overworld category):
  - a draggable, clamped 256×224 viewport with the `$E2/$E8` readout;
  - Link and actor markers;
  - Save to the project file, or Copy JSON.
- With the Overworld editor open, it uses the ROM's area sizes and draws the
  area's screens through `OverworldEditor::AreaScreenBitmap`. That accessor is
  the only change in `src/app/editor/overworld/`, approved by scawful.
- It never writes the ROM.

Verified: 12 tests pass.
- The vanilla table matches the loaded vanilla overworld for all 128 areas.
- On an Oracle copy, `$2D` is small at 2560,2560 (camera x 2560–2816,
  y 2560–2846).
- `$23` is large at 1536,2048 (camera x 1536–2304, y 2048–2846).

Not done:
- Markers are circles and ID labels, not sprite graphics.
- Nothing has been hand-checked in the app.
- The Oracle-side generator (`shots.json` → `cutscene_shots.asm`) is
  Oracle's work.

## Test isolation issue (existing, not fixed)

`EditorManager` tests read the real `~/Documents/Yaze/settings.json`. After
scawful's settings changed on 2026-09-26, two tests failed:
- `EditorManagerRomWritePolicyTest.CleanSaveDoesNotMaterializeUnopenedOverworldEditor`
- `ScreenSaveStoplossTest.RomReplacementRefreshesLoadedScreenInPlaceBeforeEnabledGlobalSave`

Both pass with an isolated `HOME` and `YAZE_APP_DATA_DIR`. Run suites like this
until the tests use `UserSettings::SetSettingsFilePathForTesting`:

```sh
H=$(mktemp -d); mkdir -p $H/appdata
HOME=$H YAZE_APP_DATA_DIR=$H/appdata build/presets/mac-ai/bin/Debug/yaze_test_unit
```

Do not change scawful's own settings to make tests pass.

## Known local failures

- 7 Asar tests fail in this worktree only. Its first configure ran before the
  submodules were checked out and cached `ASAR_FOUND=FALSE`. To clear it:
  `cmake --preset mac-ai -U ASAR_FOUND`.
- `RomTest.LoadFromFile` (the vanilla ROM on this Mac is 1 MB) and an
  order-dependent settings test fail as before.

## Next step

Merge `claude/collision-pair-save` and `claude/cutscene-camera` into one
integration branch. Then build, and hand-check both in the app on a copy of
the Oracle project.
