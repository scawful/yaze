#include "app/editor/ui/layout_manager.h"

#include "app/editor/system/panel_manager.h"
#include "app/editor/ui/layout_presets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"

namespace yaze {
namespace editor {

namespace {

// Helper function to show default cards from LayoutPresets
void ShowDefaultPanelsForEditor(PanelManager* registry, EditorType type) {
  if (!registry) return;

  auto default_panels = LayoutPresets::GetDefaultPanels(type);
  for (const auto& panel_id : default_panels) {
    registry->ShowPanel(panel_id);
  }

  LOG_INFO("LayoutManager", "Showing %zu default panels for editor type %d",
           default_panels.size(), static_cast<int>(type));
}

}  // namespace

void LayoutManager::InitializeEditorLayout(EditorType type,
                                           ImGuiID dockspace_id) {
  // Don't reinitialize if already set up
  if (IsLayoutInitialized(type)) {
    LOG_INFO("LayoutManager",
             "Layout for editor type %d already initialized, skipping",
             static_cast<int>(type));
    return;
  }

  // Store dockspace ID and current editor type for potential rebuilds
  last_dockspace_id_ = dockspace_id;
  current_editor_type_ = type;

  LOG_INFO("LayoutManager", "Initializing layout for editor type %d",
           static_cast<int>(type));

  // Clear existing layout for this dockspace
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

  // Build layout based on editor type using generic builder
  BuildLayoutFromPreset(type, dockspace_id);

  // Show default cards from LayoutPresets (single source of truth)
  ShowDefaultPanelsForEditor(panel_manager_, type);

  // Finalize the layout
  ImGui::DockBuilderFinish(dockspace_id);

  // Mark as initialized
  MarkLayoutInitialized(type);
}

void LayoutManager::RebuildLayout(EditorType type, ImGuiID dockspace_id) {
  // Validate dockspace exists
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
  if (!node) {
    LOG_ERROR("LayoutManager",
              "Cannot rebuild layout: dockspace ID %u not found", dockspace_id);
    return;
  }

  LOG_INFO("LayoutManager", "Forcing rebuild of layout for editor type %d",
           static_cast<int>(type));

  // Store dockspace ID and current editor type
  last_dockspace_id_ = dockspace_id;
  current_editor_type_ = type;

  // Clear the layout initialization flag to force rebuild
  layouts_initialized_[type] = false;

  // Clear existing layout for this dockspace
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

  // Build layout based on editor type using generic builder
  BuildLayoutFromPreset(type, dockspace_id);

  // Show default cards from LayoutPresets (single source of truth)
  ShowDefaultPanelsForEditor(panel_manager_, type);

  // Finalize the layout
  ImGui::DockBuilderFinish(dockspace_id);

  // Mark as initialized
  MarkLayoutInitialized(type);

  LOG_INFO("LayoutManager", "Layout rebuild complete for editor type %d",
           static_cast<int>(type));
}

void LayoutManager::BuildLayoutFromPreset(EditorType type, ImGuiID dockspace_id) {
  auto preset = LayoutPresets::GetDefaultPreset(type);
  
  // Define standard splits
  ImGuiID dock_center = dockspace_id;
  ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Left, 0.20f, nullptr, &dock_center);
  ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right, 0.25f, nullptr, &dock_center);
  ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, 0.25f, nullptr, &dock_center);
  ImGuiID dock_top = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Up, 0.20f, nullptr, &dock_center);
  
  // Secondary splits for more complex layouts
  ImGuiID dock_left_top = 0;
  ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.5f, nullptr, &dock_left_top);
  
  ImGuiID dock_right_top = 0;
  ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.5f, nullptr, &dock_right_top);
  
  // Assign initial values if split failed or wasn't needed? 
  // Actually DockBuilderSplitNode handles creating new nodes.
  // We need to be careful about order of splitting to preserve the center.

  // Map DockPosition to ImGuiID
  auto get_dock_id = [&](DockPosition pos) -> ImGuiID {
    switch (pos) {
      case DockPosition::Center: return dock_center;
      case DockPosition::Left: return dock_left;
      case DockPosition::Right: return dock_right;
      case DockPosition::Bottom: return dock_bottom;
      case DockPosition::Top: return dock_top;
      case DockPosition::LeftTop: return dock_left_top ? dock_left_top : dock_left; // Fallback
      case DockPosition::LeftBottom: return dock_left_bottom;
      case DockPosition::RightTop: return dock_right_top ? dock_right_top : dock_right; // Fallback
      case DockPosition::RightBottom: return dock_right_bottom;
      default: return dock_center;
    }
  };

  // Iterate through positioned panels and dock them
  for (const auto& [panel_id, position] : preset.panel_positions) {
    std::string window_title = GetWindowTitle(panel_id);
    if (window_title.empty()) {
      // If we can't find the title, try to infer it or skip
      // For now, we skip as we need the title for DockBuilder
      continue;
    }

    ImGui::DockBuilderDockWindow(window_title.c_str(), get_dock_id(position));
  }
}

// Deprecated individual build methods - redirected to generic or kept empty
void LayoutManager::BuildOverworldLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kOverworld, dockspace_id); }
void LayoutManager::BuildDungeonLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kDungeon, dockspace_id); }
void LayoutManager::BuildGraphicsLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kGraphics, dockspace_id); }
void LayoutManager::BuildPaletteLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kPalette, dockspace_id); }
void LayoutManager::BuildScreenLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kScreen, dockspace_id); }
void LayoutManager::BuildMusicLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kMusic, dockspace_id); }
void LayoutManager::BuildSpriteLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kSprite, dockspace_id); }
void LayoutManager::BuildMessageLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kMessage, dockspace_id); }
void LayoutManager::BuildAssemblyLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kAssembly, dockspace_id); }
void LayoutManager::BuildSettingsLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kSettings, dockspace_id); }
void LayoutManager::BuildEmulatorLayout(ImGuiID dockspace_id) { BuildLayoutFromPreset(EditorType::kEmulator, dockspace_id); }

void LayoutManager::SaveCurrentLayout(const std::string& name) {
  // TODO: [EditorManagerRefactor] Implement layout saving to file
  // Use ImGui::SaveIniSettingsToMemory() and write to custom file
  LOG_INFO("LayoutManager", "Saving layout: %s", name.c_str());
}

void LayoutManager::LoadLayout(const std::string& name) {
  // TODO: [EditorManagerRefactor] Implement layout loading from file
  // Use ImGui::LoadIniSettingsFromMemory() and read from custom file
  LOG_INFO("LayoutManager", "Loading layout: %s", name.c_str());
}

void LayoutManager::ResetToDefaultLayout(EditorType type) {
  layouts_initialized_[type] = false;
  LOG_INFO("LayoutManager", "Reset layout for editor type %d",
           static_cast<int>(type));
}

bool LayoutManager::IsLayoutInitialized(EditorType type) const {
  auto it = layouts_initialized_.find(type);
  return it != layouts_initialized_.end() && it->second;
}

void LayoutManager::MarkLayoutInitialized(EditorType type) {
  layouts_initialized_[type] = true;
  LOG_INFO("LayoutManager", "Marked layout for editor type %d as initialized",
           static_cast<int>(type));
}

void LayoutManager::ClearInitializationFlags() {
  layouts_initialized_.clear();
  LOG_INFO("LayoutManager", "Cleared all layout initialization flags");
}

std::string LayoutManager::GetWindowTitle(const std::string& card_id) const {
  if (!panel_manager_) {
    return "";
  }

  // Look up the panel descriptor in the manager (session 0 by default)
  auto* info = panel_manager_->GetPanelDescriptor(0, card_id);
  if (info) {
    return info->GetWindowTitle();
  }
  return "";
}

}  // namespace editor
}  // namespace yaze
