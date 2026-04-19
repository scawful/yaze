# Overworld Tile16 + UI Migration Handoff — 2026-04-19

Status: IN_PROGRESS (follow-up required)  
Owner: `imgui-frontend-engineer`  
Created: 2026-04-19  
Last Reviewed: 2026-04-19  
Next Review: 2026-04-26  
Universe Task: unavailable in snapshot; follow up after `scripts/agents/coord` is healthy again

## Summary

This slice documented and fixed the main Overworld Tile16 correctness issues,
then finished the active overworld `WindowContent` migration away from
`src/app/editor/overworld/panels/` onto `src/app/editor/overworld/ui/...`.

This work is not fully closed. It needs follow-up for GUI smoke validation,
ROM-backed integration coverage, and coordination snapshot repair.

## Canonical Sibling Docs

- Plan: [docs/internal/plans/overworld-tile16-editor-fix-plan-2026-04.md](../plans/overworld-tile16-editor-fix-plan-2026-04.md)
- Architecture pattern: [docs/internal/architecture/editor-ui-module-pattern.md](../architecture/editor-ui-module-pattern.md)

## What Landed

- Documented the Tile16 bugs and staged fix plan in the canonical plan doc.
- Unified Tile16 selection through one guarded overworld-owned path so selector,
  eyedropper, drag/drop, and tile cycling share staged-change protection.
- Wired Tile16 commits back into authoritative in-memory overworld Tile16 state
  and refreshed derived blockset/map presentation after commit.
- Refreshed Tile16-bound surfaces on palette changes and exposed `Redo` in the
  Tile16 action rail.
- Moved active overworld window surfaces into feature-oriented `ui/` modules:
  - `ui/shared/overworld_window_context.h`
  - `ui/canvas/overworld_canvas_view.{h,cc}`
  - `ui/tiles/{tile8_selector_view,tile16_selector_view,tile16_editor_view}.{h,cc}`
  - `ui/graphics/{area_graphics_view,gfx_groups_view}.{h,cc}`
  - `ui/properties/{map_properties_view,v3_settings_view}.{h,cc}`
  - `ui/items/overworld_item_list_view.{h,cc}`
  - `ui/workspace/scratch_space_view.{h,cc}`
  - `ui/analytics/usage_statistics_view.{h,cc}`
  - `ui/debug/debug_window_view.{h,cc}`
- Updated the module-pattern RFC so it explicitly treats the overworld `ui/`
  tree as canonical.

## Validation Completed

```bash
cmake --preset mac-ai-fast
ctest --test-dir build/presets/mac-ai-fast -R 'Tile16EditorActionStateTest|Tile16EditorShortcutsTest|MapRefreshCoordinatorTest' --output-on-failure
ctest --test-dir build/presets/mac-ai-fast -I 1465,1481,1 --output-on-failure
ninja -C build/presets/mac-ai-fast src/CMakeFiles/yaze_editor.dir/Debug/cmake_pch.hxx.pch src/CMakeFiles/yaze_editor.dir/Debug/app/editor/overworld/ui/debug/debug_window_view.cc.o
```

Observed results:

- Focused Tile16/unit coverage passed: 22/22.
- Synthetic Tile16 integration coverage passed: 2/2.
- ROM-backed `OverworldEditorTest` cases in the same integration slice still
  skip in this environment.
- The moved overworld `ui/*View` sources compile inside the `yaze_editor`
  library surface.

## Follow-Up Required

1. Run a manual GUI smoke pass on the overworld dock/card layout after the
   `ui/` migration. This has not been done yet.
2. Re-run the ROM-backed `OverworldEditorTest` Tile16 cases in an environment
   with the required fixture data so the new selection/commit refresh path is
   exercised end-to-end.
3. Revisit the remaining large-service extraction in `OverworldEditor` and its
   coordinators. The directory migration is done for active surfaces, but the
   editor is still too state-heavy.
4. Regenerate `docs/internal/agents/coordination-board.generated.md` once
   `scripts/agents/coord` / `universe-coord.sh` is healthy again. Current local
   coordination calls appear wedged, so this handoff is the human-readable
   record for now.
5. Do not treat the current full-build blocker in
   `src/app/editor/system/commands/shortcut_configurator.cc` as an overworld
   regression. The failing symbols are in the dungeon object shortcut path and
   need separate follow-up.

## Known Non-Overworld Blocker

`cmake --build --preset mac-ai-fast --target yaze_test yaze_test_integration -j4`
currently stops in
`src/app/editor/system/commands/shortcut_configurator.cc` because dungeon object
shortcuts call methods that live on `ObjectEditorContent`, not
`ObjectSelectorContent`. That failure is outside this overworld Tile16 / `ui/`
migration slice.
