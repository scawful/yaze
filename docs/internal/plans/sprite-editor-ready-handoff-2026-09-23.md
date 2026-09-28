# SpriteEditor: use now and Cursor/Claude handoff

## Current build

Checkout: `/Users/scawful/src/hobby/yaze`.
Branch: `codex/dungeon-workbench-bottom-drawer`.
HEAD at handoff: `0f3855d6f2df4d8cad409ec10a23ef98880d5676`.
Sprite changes are uncommitted in this shared dirty checkout. A clean checkout of
that commit will not contain this work. Preserve other owners' edits and pre-existing
deletions. Do not reset, stash, switch branches, or overwrite the working tree.

Present app: `build/presets/mac-ai/bin/Debug/yaze.app`.
The app and unit targets were rebuilt after the bounded GUI fixes below.
Launch explicitly from this checkout, rather than an older installed application:

```sh
open /Users/scawful/src/hobby/yaze/build/presets/mac-ai/bin/Debug/yaze.app
```

App/unit builds passed during sprite implementation and after the GUI fixes. A
bounded manual GUI check passed; full editor and gameplay acceptance remain open. Other owners are working in this checkout; final combined candidate
verification belongs to general editor owner `01a0cb50-a603-7763-8964-a27e9b7aa74a`
(thread title: Review yaze progress toward 0.8.0). No competing app build is underway
from this sprite task. The earlier direct handoff attempt was blocked by tool approval policy. A later
coordination update was successfully delivered to both the opening owner
(`01a0d06d-ee1d-7893-be24-c52b3a73c09c`) and general editor owner, covering
the concrete UI observations, remaining acceptance checks, and sprite-only shared
layout/label changes.

## User workflow

1. Open your project and an isolated copy of the unpatched edit ROM for graphics.
   Select Sprite editor, then Custom Sprites > New or Open for a ZSM document.
   Vanilla Sprites > Edit preview copy starts from Yaze's static layout, not a
   decoded vanilla runtime animation or writable vanilla draw routine.
2. For Oracle assets, open Sprite Catalog. Choose this checkout's
   `assets/sprite_catalogs/oracle_f0.json` and the Oracle source root containing
   `Sprites/` and `Core/`. Select Mermaid, Maple or Librarian and Import draw copy.
3. Choose Tilesheets and Asset bindings palette rows. Edit frames/tiles in Animations,
   set ranges/timing, and use Play or frame stepping. Import does not recover runtime
   animation timing. Edits refresh the canvas; undo/redo is available.
4. Save As a ZSM asset, then save the project. These are separate saves. Keep the
   project and ZSM files together: the project owns source hashes, palette/sheet
   bindings and behavior metadata; the ZSM alone does not contain those bindings.
5. Optional: Copy draw tables (ASM) for source integration review. Behavior can author
   Oracle actions and copy candidate ASM after reviewed-source checks. Neither
   export installs code or updates ROM draw/behavior data automatically.

## Supported versus unavailable

| Available in this build | Not delivered |
|---|---|
| New/open/save ZSM; tile/frame edits, duplication, clipboard, undo/redo | Full vanilla draw-routine/animation decoding and ROM write-back |
| Editor animation preview and explicit sheet/palette bindings | Emulator live-memory editing or gameplay simulation |
| F0 variant browser and source navigation; Mermaid/Librarian literal draw imports; reviewed Maple adapter | General contextual subtype placement editing or ID allocation |
| Guarded draw-table candidate export | Automatic ASM source merge, registration or runtime installation |
| Persistent Oracle action model: animation, blocking, solicited dialogue, movement/bounce and timers | Full NPC script import, dialogue choices, patrol search runtime or vanilla behavior backend |

Source drift blocks bound candidate export while retaining local asset editing.
Do not bypass it by changing reviewed fingerprints. Broader GUI coverage, sprite
appearance in-game, and gameplay acceptance remain separate checks.

## Verification and bounded patrol status

- Sprite app/unit targets rebuilt; 70 tests across 11 suites passed without skips.
- Latest bounded placement run: 35 tests across 3 suites passed without skips.
  This includes the exact proposed `2D 2F 42` record at table 0/parent $23 and an
  isolated file export/reload with original source-ROM preservation.
- The patrol candidate is placement-only. It has no installed patrol behavior.
  Predicate/lifecycle approval, final decoder identity, RAM/art and gameplay remain
  open; no production contextual validator has been installed.
- Existing duplicate-library linker warnings remain. No claim of final combined
  app/GUI readiness is made from these focused tests alone.

Build, only when needed and coordinated with the general editor owner:

```sh
CCACHE_DIR=/tmp/yaze-sprite-catalog-ccache CLANG_MODULE_CACHE_PATH=/tmp/yaze-sprite-catalog-clang cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
```

Read [visual workflow](../../public/usage/sprite-authoring.md),
[catalog setup](../../public/usage/sprite-catalog.md),
[behavior workflow](../../public/usage/sprite-behavior.md),
[agent standards](../architecture/sprite-catalog.md), and
[patrol evidence](stalfos-patrol-binding-handoff-2026-09-23.md).

## Bounded GUI follow-up

Verified the workspace app, rather than `/Applications/yaze.app`, using a temporary
copy of `oos168.sfc`. The test project and ZSM asset are under
`/var/folders/42/b_1q5t0n1xgb_05h2067y8hh0000gn/T/yaze-sprite-ui-92lk4642/`.
This directory is disposable local evidence, not a portable fixture dependency.

Fixed and manually observed:

- Sprite panels use full-width center tabs instead of squeezing the custom editor
  into a side dock (`src/app/editor/layout/layout_presets.cc`).
- Vanilla list entries show fallback names without writing default labels into
  project metadata (`src/core/project.cc`).
- Empty custom frames start blank. Tile changes reach the preview by copying the
  bitmap data into the SDL surface before texture publication
  (`sprite_editor_internal.h`, `sprite_editor.cc`).
- Tilesheets create their display texture and are visible (`sprite_editor.cc`).
- Scrolling animation/tile controls leaves canvas and tilesheets fixed; the tile
  selection outline matches the preview's 2x scale (`sprite_editor.cc`).
- Created and saved a temporary one-tile ZSM, reopened it after an app restart,
  and duplicated frame F0 to F1. The duplicate was subsequently edited, saved, and reopened in the two-frame check below.

The new `SpriteEditorPreviewTest.CompositeStartsBlankAndPublishesChangedPixels`
checks blank initialization, pixel publication, and clearing. Exact focused run:

```sh
YAZE_ORACLE_SOURCE_ROOT=/Users/scawful/src/hobby/oracle-of-secrets build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='SpriteBehavior*:SpriteAuthoring*:SpriteCatalog*:ProjectPathsTest.*:SpriteEditorPreviewTest.*:SpriteEditActionTest.*:LayoutPresetsTest.*:*ZSprite*:*Zsm*'
```

Logs: `/tmp/yaze-sprite-ui-build.log`, `/tmp/yaze-sprite-ui-tests.log`.
`git diff --check` passed. Palette correctness, catalog import UI, project-binding reload, and export UI
remain unaccepted manually. Automated coverage is not a substitute for these checks.

## Two-frame interaction acceptance

Completed on the same temporary ZSM and isolated ROM copy:

1. Edited F1 tile X offset to 16; F0 remains at 0. The rendered tile moved.
2. Ctrl+Z restored 16 -> 1 -> 0; Ctrl+Shift+Z twice restored 16, visibly.
   Numeric typing currently produces intermediate undo entries, not one entry per
   completed field edit. Data restoration works; coalescing remains a UX follow-up.
3. Set Idle range 0..1. Previous/next controls showed the distinct positions.
   Play advanced between frame 0 and frame 1 without further stepping; Stop ended
   playback. Saved timing is 68 ticks/frame; no wall-clock timing precision claim.
4. Save/Open retained both frames, range 0..1 and timing 68. Frame 1 still rendered
   at X offset16. Reopened the same ZSM again after restarting the rebuilt app.
5. Fixed low-contrast frame status locally in `sprite_editor.cc`: white text on an
   opaque dark rounded background, independent of ROM palette. Verified visually
   in the rebuilt app after restart.

Only `src/app/editor/sprite/sprite_editor.cc` changed in this follow-up. No further
layout/shared-shell changes. General editor owner confirmed separate worktree build
ownership, and CUA input was serialized between the two app instances.

Source: `/Users/scawful/src/hobby/yaze`, HEAD
`0f3855d6f2df4d8cad409ec10a23ef98880d5676` plus uncommitted changes.
This is **not evidence of integration into combined candidate `55c181549`**.
Exact per-file dirty source hashes and executable identity:
`/tmp/yaze-sprite-two-frame-source-identity.json`.
Executable: `build/presets/mac-ai/bin/Debug/yaze.app/Contents/MacOS/yaze`.
SHA-256: `6a0cbd9eeba4536ce06408d4d6f84d8f487cd85614a539923f8472c19913da67`.

Build command remains the one above. Build log:
`/tmp/yaze-sprite-two-frame-build.log`; focused test log:
`/tmp/yaze-sprite-two-frame-tests.log` (70 tests, 11 suites, all passed).
CUA screenshots and actions are in this thread's tool transcript. No standalone
screenshot file is claimed. Project/catalog/export and in-game limitations above
remain unchanged.

## Continuation boundary

Cursor/Claude: read AGENTS.md and those agent standards before changes. Preserve
all existing work. This task is stopping at the user's budget boundary; no broad
sprite features, dungeon work, or speculative patrol implementation is pending
execution here. If the user reports a concrete SpriteEditor problem, reproduce
that one workflow and fix it with focused validation. General editor owner handles
the combined usable app; patrol and opening owners retain Oracle runtime ownership.
