#include "app/editor/layout/layout_manager.h"

#include "app/editor/layout/layout_presets.h"
#include "app/editor/system/panel_manager.h"
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

namespace {

struct DockSplitConfig {
  float left = 0.22f;
  float right = 0.25f;
  float bottom = 0.25f;
  float top = 0.18f;
  float vertical_split = 0.50f;

  // Per-editor type configuration
  static DockSplitConfig ForEditor(EditorType type) {
    DockSplitConfig cfg;
    switch (type) {
      case EditorType::kDungeon:
        // Dungeon: narrower left panel for room list, right for object editor
        cfg.left = 0.16f;        // Room selector panel (narrower)
        cfg.right = 0.22f;       // Object editor panel
        cfg.bottom = 0.20f;      // Palette editor (shorter)
        cfg.vertical_split = 0.45f;  // Room matrix / Entrances split
        break;
      case EditorType::kOverworld:
        cfg.left = 0.20f;
        cfg.right = 0.25f;
        cfg.bottom = 0.25f;
        break;
      default:
        // Use defaults
        break;
    }
    return cfg;
  }
};

struct DockNodeIds {
  ImGuiID center = 0;
  ImGuiID left = 0;
  ImGuiID right = 0;
  ImGuiID bottom = 0;
  ImGuiID top = 0;
  ImGuiID left_top = 0;
  ImGuiID left_bottom = 0;
  ImGuiID right_top = 0;
  ImGuiID right_bottom = 0;
};

struct DockSplitNeeds {
  bool left = false;
  bool right = false;
  bool bottom = false;
  bool top = false;
  bool left_top = false;
  bool left_bottom = false;
  bool right_top = false;
  bool right_bottom = false;
};

DockSplitNeeds ComputeSplitNeeds(const PanelLayoutPreset& preset) {
  DockSplitNeeds needs{};
  for (const auto& [_, pos] : preset.panel_positions) {
    switch (pos) {
      case DockPosition::Left:
        needs.left = true;
        break;
      case DockPosition::Right:
        needs.right = true;
        break;
      case DockPosition::Bottom:
        needs.bottom = true;
        break;
      case DockPosition::Top:
        needs.top = true;
        break;
      case DockPosition::LeftTop:
        needs.left = true;
        needs.left_top = true;
        break;
      case DockPosition::LeftBottom:
        needs.left = true;
        needs.left_bottom = true;
        break;
      case DockPosition::RightTop:
        needs.right = true;
        needs.right_top = true;
        break;
      case DockPosition::RightBottom:
        needs.right = true;
        needs.right_bottom = true;
        break;
      case DockPosition::Center:
      default:
        break;
    }
  }
  return needs;
}

DockNodeIds BuildDockTree(ImGuiID dockspace_id, const DockSplitNeeds& needs,
                          const DockSplitConfig& cfg) {
  DockNodeIds ids{};
  ids.center = dockspace_id;

  if (needs.left) {
    ids.left = ImGui::DockBuilderSplitNode(ids.center, ImGuiDir_Left, cfg.left,
                                           nullptr, &ids.center);
  }
  if (needs.right) {
    ids.right = ImGui::DockBuilderSplitNode(ids.center, ImGuiDir_Right,
                                            cfg.right, nullptr, &ids.center);
  }
  if (needs.bottom) {
    ids.bottom = ImGui::DockBuilderSplitNode(ids.center, ImGuiDir_Down,
                                             cfg.bottom, nullptr, &ids.center);
  }
  if (needs.top) {
    ids.top = ImGui::DockBuilderSplitNode(ids.center, ImGuiDir_Up, cfg.top,
                                          nullptr, &ids.center);
  }

  if (ids.left && (needs.left_top || needs.left_bottom)) {
    ids.left_bottom = ImGui::DockBuilderSplitNode(
        ids.left, ImGuiDir_Down, cfg.vertical_split, nullptr, &ids.left_top);
  }

  if (ids.right && (needs.right_top || needs.right_bottom)) {
    ids.right_bottom = ImGui::DockBuilderSplitNode(
        ids.right, ImGuiDir_Down, cfg.vertical_split, nullptr, &ids.right_top);
  }

  return ids;
}

}  // namespace

void LayoutManager::BuildLayoutFromPreset(EditorType type, ImGuiID dockspace_id) {
  auto preset = LayoutPresets::GetDefaultPreset(type);

  if (!panel_manager_) {
    LOG_WARN("LayoutManager",
             "PanelManager not available, skipping dock layout for type %d",
             static_cast<int>(type));
    return;
  }

  const size_t session_id =
      panel_manager_ ? panel_manager_->GetActiveSessionId() : 0;

  DockSplitNeeds needs = ComputeSplitNeeds(preset);
  DockSplitConfig cfg = DockSplitConfig::ForEditor(type);
  DockNodeIds ids = BuildDockTree(dockspace_id, needs, cfg);

  auto get_dock_id = [&](DockPosition pos) -> ImGuiID {
    switch (pos) {
      case DockPosition::Left:
        return ids.left ? ids.left : ids.center;
      case DockPosition::Right:
        return ids.right ? ids.right : ids.center;
      case DockPosition::Bottom:
        return ids.bottom ? ids.bottom : ids.center;
      case DockPosition::Top:
        return ids.top ? ids.top : ids.center;
      case DockPosition::LeftTop:
        return ids.left_top ? ids.left_top
                            : (ids.left ? ids.left : ids.center);
      case DockPosition::LeftBottom:
        return ids.left_bottom ? ids.left_bottom
                               : (ids.left ? ids.left : ids.center);
      case DockPosition::RightTop:
        return ids.right_top ? ids.right_top
                             : (ids.right ? ids.right : ids.center);
      case DockPosition::RightBottom:
        return ids.right_bottom ? ids.right_bottom
                                : (ids.right ? ids.right : ids.center);
      case DockPosition::Center:
      default:
        return ids.center;
    }
  };

  // Iterate through positioned panels and dock them
  for (const auto& [panel_id, position] : preset.panel_positions) {
    const PanelDescriptor* desc =
        panel_manager_
            ? panel_manager_->GetPanelDescriptor(session_id, panel_id)
            : nullptr;
    if (!desc) {
      LOG_WARN("LayoutManager",
               "Preset references panel '%s' that is not registered (session "
               "%zu)",
               panel_id.c_str(), session_id);
      continue;
    }

    std::string window_title = desc->GetWindowTitle();
    if (window_title.empty()) {
      LOG_WARN("LayoutManager",
               "Cannot dock panel '%s': missing window title (session %zu)",
               panel_id.c_str(), session_id);
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

  const size_t session_id = panel_manager_->GetActiveSessionId();
  // Look up the panel descriptor in the manager (session 0 by default)
  auto* info = panel_manager_->GetPanelDescriptor(session_id, card_id);
  if (info) {
    return info->GetWindowTitle();
  }
  return "";
}

}  // namespace editor
}  // namespace yaze
