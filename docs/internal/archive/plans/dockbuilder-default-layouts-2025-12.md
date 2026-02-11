# Default DockBuilder Layouts Plan
Status: ACTIVE  
Owner: imgui-frontend-engineer  
Created: 2025-12-07  
Last Reviewed: 2025-12-07  
Next Review: 2025-12-14  
Board Link: [Coordination Board – 2025-12-05 imgui-frontend-engineer – Panel launch/log filtering UX](../agents/coordination-board.md)

## Summary
- Deliver deterministic default dock layouts per editor using ImGui DockBuilder, aligned with `PanelManager` visibility rules and `LayoutPresets`.
- Ensure session-aware window titles, validation hooks, and user-facing reset/apply commands.
- Provide a reusable dock-tree builder that only creates the splits needed by each preset, with sensible ratios.

## Goals
- Build DockBuilder layouts directly from `PanelLayoutPreset::panel_positions`, honoring default/optional visibility from `LayoutPresets`.
- Keep `PanelManager` as the single source of truth for visibility (pinned/persistent honored on editor switch).
- Make layouts robust across sessions via prefixed IDs and stable window titles.
- Add validation/logging so missing/mismatched window titles are surfaced immediately.

## Non-Goals
- Full layout serialization/import (use existing ImGui ini paths later).
- Redesign of ActivityBar or RightPanelManager.
- New panel registrations; focus is layout orchestration of existing panels.

## Constraints & Context
- Docking API is internal (`imgui_internal.h`) and brittle; prefer minimal splits and clear ordering.
- `PanelDescriptor::GetWindowTitle()` must match the actual `ImGui::Begin` title. Gaps cause DockBuilder docking failures.
- Session prefixing is required when multiple sessions exist (`PanelManager::MakePanelId`).

## Work Plan
1) **Title hygiene & validation**  
   - Require every `PanelDescriptor` to supply `window_title` (or safe icon+name fallback).  
   - Extend `PanelManager::ValidatePanels()` to assert titles resolve and optionally check `ImGui::FindWindowByName` post-dock.
2) **Reusable dock-tree builder**  
   - Build only the splits needed by the positions present (Left/Right/Top/Bottom and quadrants), with per-editor ratios in `LayoutPresets`.  
   - Keep center as the default drop zone when a split is absent.
3) **Session-aware docking**  
   - When docking, resolve both the prefixed panel ID and its title via `PanelManager::MakePanelId`/`GetPanelDescriptor`.  
   - Guard rebuilds with `DockBuilderGetNode` and re-add nodes when missing.
4) **Preset application pipeline**  
   - In `LayoutManager::InitializeEditorLayout`/`RebuildLayout`, call the dock-tree builder, then show default panels from `LayoutPresets`, then `DockBuilderFinish`.  
   - `LayoutOrchestrator` triggers this on first switch and on “Reset Layout.”
5) **Named presets & commands**  
   - Add helpers to apply named presets (Minimal/Developer/Designer/etc.) that: show defaults, hide optionals, rebuild the dock tree, and optionally load/save ImGui ini blobs.  
   - Expose commands/buttons in sidebar/menu (Reset to Default, Apply <Preset>).
6) **Post-apply validation**  
   - After docking, run the validation pass; log missing titles/panels and surface a toast for user awareness.  
   - Capture failures to telemetry/logs for later fixes.
7) **Docs/tests**  
   - Document the builder contract and add a small unit/integration check that `GetWindowTitle` is non-empty for all preset IDs.

## Code Sketches

### Dock tree builder with minimal splits
```cpp
#include "imgui/imgui_internal.h"

struct BuiltDockTree {
  ImGuiID center{}, left{}, right{}, top{}, bottom{};
  ImGuiID left_top{}, left_bottom{}, right_top{}, right_bottom{};
};

static BuiltDockTree BuildDockTree(ImGuiID dockspace_id) {
  BuiltDockTree ids{};
  ids.center = dockspace_id;

  // Primary splits
  ids.left   = ImGui::DockBuilderSplitNode(ids.center, ImGuiDir_Left,  0.22f, nullptr, &ids.center);
  ids.right  = ImGui::DockBuilderSplitNode(ids.center, ImGuiDir_Right, 0.25f, nullptr, &ids.center);
  ids.bottom = ImGui::DockBuilderSplitNode(ids.center, ImGuiDir_Down,  0.25f, nullptr, &ids.center);
  ids.top    = ImGui::DockBuilderSplitNode(ids.center, ImGuiDir_Up,    0.18f, nullptr, &ids.center);

  // Secondary splits (created only if the parent exists)
  if (ids.left) {
    ids.left_bottom = ImGui::DockBuilderSplitNode(ids.left, ImGuiDir_Down, 0.50f, nullptr, &ids.left_top);
  }
  if (ids.right) {
    ids.right_bottom = ImGui::DockBuilderSplitNode(ids.right, ImGuiDir_Down, 0.50f, nullptr, &ids.right_top);
  }
  return ids;
}
```

### Dock preset application with title resolution
```cpp
void ApplyPresetLayout(const PanelLayoutPreset& preset,
                       PanelManager& panels,
                       ImGuiID dockspace_id,
                       size_t session_id = 0) {
  if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);
  }

  ImGui::DockBuilderRemoveNodeChildNodes(dockspace_id);
  auto tree = BuildDockTree(dockspace_id);

  auto dock_for = [&](DockPosition pos) -> ImGuiID {
    switch (pos) {
      case DockPosition::Left:         return tree.left   ? tree.left   : tree.center;
      case DockPosition::Right:        return tree.right  ? tree.right  : tree.center;
      case DockPosition::Top:          return tree.top    ? tree.top    : tree.center;
      case DockPosition::Bottom:       return tree.bottom ? tree.bottom : tree.center;
      case DockPosition::LeftTop:      return tree.left_top     ? tree.left_top     : (tree.left   ? tree.left   : tree.center);
      case DockPosition::LeftBottom:   return tree.left_bottom  ? tree.left_bottom  : (tree.left   ? tree.left   : tree.center);
      case DockPosition::RightTop:     return tree.right_top    ? tree.right_top    : (tree.right  ? tree.right  : tree.center);
      case DockPosition::RightBottom:  return tree.right_bottom ? tree.right_bottom : (tree.right  ? tree.right  : tree.center);
      case DockPosition::Center:
      default:                         return tree.center;
    }
  };

  for (const auto& [panel_id, pos] : preset.panel_positions) {
    const auto* desc = panels.GetPanelDescriptor(session_id, panel_id);
    if (!desc) continue;  // unknown or unregistered panel

    const std::string window_title = desc->GetWindowTitle();
    if (window_title.empty()) continue;  // validation will flag this

    ImGui::DockBuilderDockWindow(window_title.c_str(), dock_for(pos));
  }

  ImGui::DockBuilderFinish(dockspace_id);
}
```

### Integration points
- `LayoutManager::InitializeEditorLayout`  
  ```cpp
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);
  ApplyPresetLayout(LayoutPresets::GetDefaultPreset(type), *panel_manager_, dockspace_id, active_session_);
  ShowDefaultPanelsForEditor(panel_manager_, type);  // existing helper
  ImGui::DockBuilderFinish(dockspace_id);
  ```
- `LayoutOrchestrator::ResetToDefault` calls `LayoutManager::RebuildLayout` with the current dockspace ID.

## Exit Criteria
- DockBuilder helper applied for all editor presets with sensible split ratios.
- Validation logs (and optional toast) for any missing window titles or docking failures.
- User-visible controls to reset/apply presets; defaults restored correctly after reset.
- Session-aware docking verified (no cross-session clashes when multiple sessions open).
