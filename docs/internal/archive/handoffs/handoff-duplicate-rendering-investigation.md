# Handoff: Duplicate Element Rendering in Editor Cards

**Date:** 2025-11-25
**Status:** In Progress - Diagnostic Added
**Issue:** All elements inside editor cards appear twice (visually stacked)

## Problem Description

User reported that elements inside editor cards are "appearing twice on top of one another" - affecting all editors, not specific cards. This suggests a systematic issue with how card content is rendered.

## Investigation Summary

### Files Examined

| File | Issue Checked | Result |
|------|--------------|--------|
| `proposal_drawer.cc` | Duplicate Draw()/DrawContent() | `Draw()` is dead code - never called |
| `tile16_editor.cc` | Missing EndChild() | Begin/End counts balanced |
| `overworld_editor.cc` | Orphaned EndChild() | Begin/End counts balanced |
| `editor_card_registry.cc` | Dual rendering paths | Mutual exclusion via `IsTreeViewMode()` |
| `editor_manager.cc` | Double Update() calls | Only one `editor->Update()` per frame |
| `controller.cc` | Main loop issues | Single `NewFrame()`/`Update()`/`Render()` cycle |
| `editor_layout.cc` | EditorCard Begin/End | Proper ImGui pairing |

### What Was Ruled Out

1. **ProposalDrawer** - The `Draw()` method (lines 75-107) is never called. Only `DrawContent()` is used via `right_panel_manager.cc:238`

2. **ImGui Begin/End Mismatches** - Verified counts in:
   - `tile16_editor.cc`: 6 BeginChild, 6 EndChild
   - `overworld_editor.cc`: Balanced pairs with proper End() after each Begin()

3. **EditorCardRegistry Double Rendering** - `DrawSidebar()` and `DrawTreeSidebar()` use different window names (`##EditorCardSidebar` vs `##TreeSidebar`) and are mutually exclusive

4. **Multiple Update() Calls** - `EditorManager::Update()` only calls `editor->Update()` once per frame for each active editor (line 1047)

5. **Main Loop Issues** - `controller.cc` has clean frame lifecycle:
   - Line 63-65: Single NewFrame() calls
   - Line 124: Single `editor_manager_.Update()`
   - Line 134: Single `ImGui::Render()`

6. **Multi-Viewport** - `ImGuiConfigFlags_ViewportsEnable` is NOT enabled (only `DockingEnable`)

### Previous Fixes Found

Comments in codebase indicate prior duplicate rendering issues were fixed:
- `editor_manager.cc:827`: "Removed duplicate direct call - DrawProposalsPanel()"
- `editor_manager.cc:1030`: "Removed duplicate call to avoid showing welcome screen twice"

## Diagnostic Code Added

Added frame-based duplicate detection to `EditorCard` class:

### Files Modified

**`src/app/gui/app/editor_layout.h`** (lines 121-135):
```cpp
// Debug: Reset frame tracking (call once per frame from main loop)
static void ResetFrameTracking();

// Debug: Check if any card was rendered twice this frame
static bool HasDuplicateRendering();
static const std::string& GetDuplicateCardName();

private:
  static int last_frame_count_;
  static std::vector<std::string> cards_begun_this_frame_;
  static bool duplicate_detected_;
  static std::string duplicate_card_name_;
```

**`src/app/gui/app/editor_layout.cc`** (lines 17-23, 263-285):
- Static variable definitions
- Tracking logic in `Begin()` that:
  - Resets tracking on new frame
  - Checks if card was already begun this frame
  - Logs to stderr: `[EditorCard] DUPLICATE DETECTED: 'Card Name' Begin() called twice in frame N`

### How to Use

1. Build and run the application from terminal
2. If any card's `Begin()` is called twice in the same frame, stderr will show:
   ```
   [EditorCard] DUPLICATE DETECTED: 'Tile16 Selector' Begin() called twice in frame 1234
   ```
3. Query programmatically:
   ```cpp
   if (gui::EditorCard::HasDuplicateRendering()) {
     LOG_ERROR("Duplicate card: %s", gui::EditorCard::GetDuplicateCardName().c_str());
   }
   ```

## Next Steps

1. **Run with diagnostic** - Build succeeds, run app and check stderr for duplicate messages

2. **If duplicates detected** - The log will identify which card(s) are being rendered twice, then trace back to find the double call site

3. **If no duplicates detected** - The issue may be:
   - ImGui draw list being submitted twice
   - Z-ordering/layering visual artifacts
   - Something outside EditorCard (raw ImGui::Begin calls)

4. **Alternative debugging**:
   - Enable ImGui Demo Window's "Metrics" to inspect draw calls
   - Add similar tracking to raw `ImGui::Begin()` calls
   - Check for duplicate textures being drawn at same position

## Build Status

Build was in progress when handoff created. Command:
```bash
cmake --build build --target yaze -j4
```

## Related Files

- Plan file: `~/.claude/plans/nested-crafting-origami.md`
- Editor layout: `src/app/gui/app/editor_layout.h`, `editor_layout.cc`
- Main editors: `src/app/editor/overworld/overworld_editor.cc`
- Card registry: `src/app/editor/system/editor_card_registry.cc`
