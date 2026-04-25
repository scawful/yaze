#include "app/editor/layout/layout_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "absl/strings/str_cat.h"
#include "app/editor/layout/layout_designer/dock_tree_json.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/system/session/user_settings.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/core/background_renderer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include "util/json.h"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace editor {

namespace {

constexpr char kLegacyPanelsKey[] = "panels";
constexpr char kWindowsKey[] = "windows";

// Helper function to show default windows from LayoutPresets
void ShowDefaultWindowsForEditor(WorkspaceWindowManager* registry,
                                 EditorType type) {
  if (!registry)
    return;

  auto default_windows = LayoutPresets::GetDefaultWindows(type);
  for (const auto& window_id : default_windows) {
    registry->OpenWindow(window_id);
  }

  LOG_INFO("LayoutManager", "Showing %zu default windows for editor type %d",
           default_windows.size(), static_cast<int>(type));
}

yaze::Json BoolMapToJson(const std::unordered_map<std::string, bool>& map) {
  yaze::Json obj = yaze::Json::object();
  for (const auto& [key, value] : map) {
    obj[key] = value;
  }
  return obj;
}

void JsonToBoolMap(const yaze::Json& obj,
                   std::unordered_map<std::string, bool>* map) {
  if (!map || !obj.is_object()) {
    return;
  }
  map->clear();
  for (const auto& [key, value] : obj.items()) {
    if (value.is_boolean()) {
      (*map)[key] = value.get<bool>();
    }
  }
}

void JsonToWindowMap(const yaze::Json& entry,
                     std::unordered_map<std::string, bool>* windows) {
  if (!windows || !entry.is_object()) {
    return;
  }
  if (entry.contains(kWindowsKey)) {
    JsonToBoolMap(entry[kWindowsKey], windows);
    return;
  }
  if (entry.contains(kLegacyPanelsKey)) {
    JsonToBoolMap(entry[kLegacyPanelsKey], windows);
    return;
  }
  windows->clear();
}

std::filesystem::path GetLayoutsFilePath(LayoutScope scope,
                                         const std::string& project_key) {
  auto layouts_dir = util::PlatformPaths::GetAppDataSubdirectory("layouts");
  if (!layouts_dir.ok()) {
    return {};
  }

  if (scope == LayoutScope::kProject && !project_key.empty()) {
    std::filesystem::path projects_dir = *layouts_dir / "projects";
    (void)util::PlatformPaths::EnsureDirectoryExists(projects_dir);
    return projects_dir / (project_key + ".json");
  }

  if (scope == LayoutScope::kProject) {
    return {};
  }

  return *layouts_dir / "layouts.json";
}

bool TryGetNamedPreset(const std::string& preset_name,
                       PanelLayoutPreset* preset_out) {
  if (!preset_out) {
    return false;
  }

  if (preset_name == "Minimal") {
    *preset_out = LayoutPresets::GetMinimalPreset();
    return true;
  }
  if (preset_name == "Developer") {
    *preset_out = LayoutPresets::GetDeveloperPreset();
    return true;
  }
  if (preset_name == "Designer") {
    *preset_out = LayoutPresets::GetDesignerPreset();
    return true;
  }
  if (preset_name == "Modder") {
    *preset_out = LayoutPresets::GetModderPreset();
    return true;
  }
  if (preset_name == "Overworld Expert") {
    *preset_out = LayoutPresets::GetOverworldArtistPreset();
    return true;
  }
  if (preset_name == "Dungeon Expert") {
    *preset_out = LayoutPresets::GetDungeonMasterPreset();
    return true;
  }
  if (preset_name == "Testing") {
    *preset_out = LayoutPresets::GetLogicDebuggerPreset();
    return true;
  }
  if (preset_name == "Audio") {
    *preset_out = LayoutPresets::GetAudioEngineerPreset();
    return true;
  }

  return false;
}

std::string ResolveProfilePresetName(const std::string& profile_id,
                                     EditorType editor_type) {
  if (profile_id == "mapping") {
    if (editor_type == EditorType::kDungeon) {
      return "Dungeon Expert";
    }
    return "Overworld Expert";
  }
  if (profile_id == "code") {
    return "Minimal";
  }
  if (profile_id == "debug") {
    return "Developer";
  }
  if (profile_id == "chat") {
    return "Modder";
  }
  return "";
}

}  // namespace

void LayoutManager::InitializeEditorLayout(EditorType type,
                                           ImGuiID dockspace_id) {
  // Phase 8.2 review (2026-04-25): one-shot protection set by
  // MaybeReapplyStartupLayout. The very first lazy preset init after a
  // successful startup-reapply must NOT rebuild the dockspace —
  // doing so would silently overwrite the user's saved layout on
  // their first editor activation. Consume the flag, mark the type
  // initialized so subsequent activations behave normally, and bail.
  if (startup_reapply_pending_protection_) {
    startup_reapply_pending_protection_ = false;
    last_dockspace_id_ = dockspace_id;
    current_editor_type_ = type;
    MarkLayoutInitialized(type);
    LOG_INFO("LayoutManager",
             "Suppressed lazy preset init for editor type %d to preserve "
             "startup-reapplied custom layout",
             static_cast<int>(type));
    return;
  }

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

  ImVec2 dockspace_size = ImVec2(1280, 720);  // Safe default
  if (auto* viewport = ImGui::GetMainViewport()) {
    dockspace_size = viewport->WorkSize;
  }

  const ImVec2 last_size = gui::DockSpaceRenderer::GetLastDockspaceSize();
  if (last_size.x > 0.0f && last_size.y > 0.0f) {
    dockspace_size = last_size;
  }
  ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);

  // Build layout based on editor type using generic builder
  BuildLayoutFromPreset(type, dockspace_id);

  // Show default windows from LayoutPresets (single source of truth)
  ShowDefaultWindowsForEditor(window_manager_, type);

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
  ImVec2 dockspace_size = ImGui::GetMainViewport()->WorkSize;
  const ImVec2 last_size = gui::DockSpaceRenderer::GetLastDockspaceSize();
  if (last_size.x > 0.0f && last_size.y > 0.0f) {
    dockspace_size = last_size;
  }
  ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);

  // Build layout based on editor type using generic builder
  BuildLayoutFromPreset(type, dockspace_id);

  // Show default cards from LayoutPresets (single source of truth)
  ShowDefaultWindowsForEditor(window_manager_, type);

  // Finalize the layout
  ImGui::DockBuilderFinish(dockspace_id);

  // Mark as initialized
  MarkLayoutInitialized(type);

  LOG_INFO("LayoutManager", "Layout rebuild complete for editor type %d",
           static_cast<int>(type));
}

namespace {

struct DockSplitConfig {
  float left = 0.17f;
  float right = 0.24f;
  float bottom = 0.22f;
  float top = 0.12f;
  float vertical_split = 0.52f;

  // Per-editor type configuration
  static DockSplitConfig ForEditor(EditorType type) {
    DockSplitConfig cfg;
    switch (type) {
      case EditorType::kDungeon:
        // Dungeon: reserve more right-side space for object/property workflows.
        cfg.left = 0.16f;
        cfg.right = 0.26f;
        cfg.bottom = 0.20f;
        cfg.vertical_split = 0.50f;
        break;
      case EditorType::kOverworld:
        cfg.left = 0.22f;
        cfg.right = 0.26f;
        cfg.bottom = 0.23f;
        cfg.vertical_split = 0.42f;
        break;
      case EditorType::kGraphics:
        cfg.left = 0.16f;
        cfg.right = 0.24f;
        cfg.bottom = 0.20f;
        break;
      case EditorType::kPalette:
        cfg.left = 0.16f;
        cfg.right = 0.22f;
        cfg.bottom = 0.20f;
        break;
      case EditorType::kSprite:
        cfg.left = 0.18f;
        cfg.right = 0.24f;
        cfg.bottom = 0.20f;
        break;
      case EditorType::kScreen:
        cfg.left = 0.16f;
        cfg.right = 0.22f;
        cfg.bottom = 0.20f;
        break;
      case EditorType::kMessage:
        cfg.left = 0.20f;
        cfg.right = 0.26f;
        cfg.bottom = 0.18f;
        break;
      case EditorType::kAssembly:
        cfg.left = 0.24f;
        cfg.right = 0.16f;
        cfg.bottom = 0.20f;
        break;
      case EditorType::kEmulator:
        cfg.left = 0.14f;
        cfg.right = 0.28f;
        cfg.bottom = 0.22f;
        break;
      case EditorType::kAgent:
        cfg.left = 0.16f;
        cfg.right = 0.30f;
        cfg.bottom = 0.24f;
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

bool ShouldDockPanelInDefaultLayout(const PanelLayoutPreset& preset,
                                    const std::string& panel_id) {
  if (!preset.dock_only_default_visible_panels) {
    return true;
  }
  return std::find(preset.default_visible_panels.begin(),
                   preset.default_visible_panels.end(),
                   panel_id) != preset.default_visible_panels.end();
}

std::vector<std::pair<std::string, DockPosition>> CollectDockedPanels(
    const PanelLayoutPreset& preset) {
  std::vector<std::pair<std::string, DockPosition>> docked_panels;
  docked_panels.reserve(preset.panel_positions.size());

  std::unordered_set<std::string> seen_panels;
  seen_panels.reserve(preset.panel_positions.size());

  auto append_ordered = [&](const std::vector<std::string>& ordered_ids) {
    for (const auto& panel_id : ordered_ids) {
      if (seen_panels.contains(panel_id) ||
          !ShouldDockPanelInDefaultLayout(preset, panel_id)) {
        continue;
      }
      auto it = preset.panel_positions.find(panel_id);
      if (it == preset.panel_positions.end()) {
        continue;
      }
      docked_panels.emplace_back(it->first, it->second);
      seen_panels.insert(panel_id);
    }
  };

  append_ordered(preset.default_visible_panels);
  append_ordered(preset.optional_panels);

  std::vector<std::pair<std::string, DockPosition>> remaining_panels;
  remaining_panels.reserve(preset.panel_positions.size());
  for (const auto& [panel_id, position] : preset.panel_positions) {
    if (seen_panels.contains(panel_id) ||
        !ShouldDockPanelInDefaultLayout(preset, panel_id)) {
      continue;
    }
    remaining_panels.emplace_back(panel_id, position);
  }

  std::sort(remaining_panels.begin(), remaining_panels.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.second != rhs.second) {
                return static_cast<int>(lhs.second) <
                       static_cast<int>(rhs.second);
              }
              return lhs.first < rhs.first;
            });
  docked_panels.insert(docked_panels.end(), remaining_panels.begin(),
                       remaining_panels.end());

  return docked_panels;
}

DockSplitNeeds ComputeSplitNeeds(
    const std::vector<std::pair<std::string, DockPosition>>& docked_panels) {
  DockSplitNeeds needs{};
  for (const auto& [_, pos] : docked_panels) {
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

bool IsRightDockPosition(DockPosition pos) {
  return pos == DockPosition::Right || pos == DockPosition::RightTop ||
         pos == DockPosition::RightBottom;
}

bool IsLeftDockPosition(DockPosition pos) {
  return pos == DockPosition::Left || pos == DockPosition::LeftTop ||
         pos == DockPosition::LeftBottom;
}

float ResolvePreferredRegionWidth(
    WorkspaceWindowManager* window_manager,
    const std::vector<std::pair<std::string, DockPosition>>& docked_panels,
    bool (*matches_region)(DockPosition)) {
  if (!window_manager) {
    return 0.0f;
  }

  float preferred_width = 0.0f;
  for (const auto& [panel_id, position] : docked_panels) {
    if (!matches_region(position)) {
      continue;
    }
    if (WindowContent* panel = window_manager->GetWindowContent(panel_id)) {
      preferred_width = std::max(preferred_width, panel->GetPreferredWidth());
    }
  }
  return preferred_width;
}

void ApplyPreferredSplitWidths(
    DockSplitConfig* cfg, const DockSplitNeeds& needs, float viewport_width,
    WorkspaceWindowManager* window_manager,
    const std::vector<std::pair<std::string, DockPosition>>& docked_panels) {
  if (!cfg || !window_manager || viewport_width <= 0.0f) {
    return;
  }

  if (needs.left) {
    const float preferred_left = ResolvePreferredRegionWidth(
        window_manager, docked_panels, IsLeftDockPosition);
    if (preferred_left > 0.0f) {
      cfg->left = std::clamp(preferred_left / viewport_width, 0.14f, 0.36f);
    }
  }

  if (needs.right) {
    const float preferred_right = ResolvePreferredRegionWidth(
        window_manager, docked_panels, IsRightDockPosition);
    if (preferred_right > 0.0f) {
      cfg->right = std::clamp(preferred_right / viewport_width, 0.18f, 0.42f);
    }
  }
}

DockNodeIds BuildDockTree(ImGuiID dockspace_id, const DockSplitNeeds& needs,
                          const DockSplitConfig& cfg) {
  DockNodeIds ids{};
  ids.center = dockspace_id;

  // Split major regions
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

  // Sub-split Left region
  if (ids.left && (needs.left_top || needs.left_bottom)) {
    // If we only need one, the split still happens but we use the result accordingly
    ids.left_bottom = ImGui::DockBuilderSplitNode(
        ids.left, ImGuiDir_Down, cfg.vertical_split, nullptr, &ids.left_top);

    // If one isn't needed, we technically don't have to split, but for a stable tree,
    // we do it and get_dock_id will map to the leaf.
  }

  // Sub-split Right region
  if (ids.right && (needs.right_top || needs.right_bottom)) {
    ids.right_bottom = ImGui::DockBuilderSplitNode(
        ids.right, ImGuiDir_Down, cfg.vertical_split, nullptr, &ids.right_top);
  }

  return ids;
}

}  // namespace

void LayoutManager::BuildLayoutFromPreset(EditorType type,
                                          ImGuiID dockspace_id) {
  auto preset = LayoutPresets::GetDefaultPreset(type);

  if (!window_manager_) {
    LOG_WARN("LayoutManager",
             "WorkspaceWindowManager not available, skipping dock layout for "
             "type %d",
             static_cast<int>(type));
    return;
  }

  const size_t session_id =
      window_manager_ ? window_manager_->GetActiveSessionId() : 0;
  const auto docked_panels = CollectDockedPanels(preset);

  // On compact/touch layouts, collapse all panels into center tabs instead of
  // splitting into left/right/bottom regions. This gives each panel full
  // screen width and makes tab-switching more natural on touch.
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float viewport_width = viewport ? viewport->WorkSize.x : 0.0f;
  const bool is_compact =
#if defined(__APPLE__) && TARGET_OS_IOS == 1
      [&]() {
        static bool compact_mode = true;
        constexpr float kEnterCompactWidth = 900.0f;
        constexpr float kExitCompactWidth = 940.0f;
        if (viewport_width <= 0.0f) {
          return compact_mode;
        }
        compact_mode = compact_mode ? (viewport_width < kExitCompactWidth)
                                    : (viewport_width < kEnterCompactWidth);
        return compact_mode;
      }();
#else
      (viewport_width > 0.0f && viewport_width < 900.0f);
#endif

  DockSplitNeeds needs{};
  DockSplitConfig cfg{};
  if (!is_compact) {
    needs = ComputeSplitNeeds(docked_panels);
    cfg = DockSplitConfig::ForEditor(type);
    ApplyPreferredSplitWidths(&cfg, needs, viewport_width, window_manager_,
                              docked_panels);
  }
  // When compact, needs is all-false → BuildDockTree produces center-only.
  DockNodeIds ids = BuildDockTree(dockspace_id, needs, cfg);

  auto get_dock_id = [&](DockPosition pos) -> ImGuiID {
    switch (pos) {
      case DockPosition::Left:
        if (ids.left_top || ids.left_bottom) {
          // If sub-nodes exist, default "Left" to Top to avoid stacking in parent
          return ids.left_top ? ids.left_top : ids.left_bottom;
        }
        return ids.left ? ids.left : ids.center;
      case DockPosition::Right:
        if (ids.right_top || ids.right_bottom) {
          return ids.right_top ? ids.right_top : ids.right_bottom;
        }
        return ids.right ? ids.right : ids.center;
      case DockPosition::Bottom:
        return ids.bottom ? ids.bottom : ids.center;
      case DockPosition::Top:
        return ids.top ? ids.top : ids.center;
      case DockPosition::LeftTop:
        return ids.left_top ? ids.left_top : (ids.left ? ids.left : ids.center);
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

  std::vector<ImGuiID> auto_hide_nodes;

  // Iterate through positioned windows and dock them
  for (const auto& [panel_id, position] : docked_panels) {
    const WindowDescriptor* desc =
        window_manager_
            ? window_manager_->GetWindowDescriptor(session_id, panel_id)
            : nullptr;
    if (!desc) {
      LOG_WARN("LayoutManager",
               "Preset references window '%s' that is not registered (session "
               "%zu)",
               panel_id.c_str(), session_id);
      continue;
    }

    std::string window_title = window_manager_->GetWorkspaceWindowName(*desc);
    if (window_title.empty()) {
      LOG_WARN("LayoutManager",
               "Cannot dock window '%s': missing window name (session %zu)",
               panel_id.c_str(), session_id);
      continue;
    }

    const ImGuiID dock_id = get_dock_id(position);
    ImGui::DockBuilderDockWindow(window_title.c_str(), dock_id);

    if (WindowContent* panel = window_manager_->GetWindowContent(panel_id);
        panel && panel->PreferAutoHideTabBar() &&
        std::find(auto_hide_nodes.begin(), auto_hide_nodes.end(), dock_id) ==
            auto_hide_nodes.end()) {
      if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id)) {
        node->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
        auto_hide_nodes.push_back(dock_id);
      }
    }
  }
}

// Deprecated individual build methods - redirected to generic or kept empty
void LayoutManager::BuildOverworldLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kOverworld, dockspace_id);
}
void LayoutManager::BuildDungeonLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kDungeon, dockspace_id);
}
void LayoutManager::BuildGraphicsLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kGraphics, dockspace_id);
}
void LayoutManager::BuildPaletteLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kPalette, dockspace_id);
}
void LayoutManager::BuildScreenLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kScreen, dockspace_id);
}
void LayoutManager::BuildMusicLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kMusic, dockspace_id);
}
void LayoutManager::BuildSpriteLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kSprite, dockspace_id);
}
void LayoutManager::BuildMessageLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kMessage, dockspace_id);
}
void LayoutManager::BuildAssemblyLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kAssembly, dockspace_id);
}
void LayoutManager::BuildSettingsLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kSettings, dockspace_id);
}
void LayoutManager::BuildEmulatorLayout(ImGuiID dockspace_id) {
  BuildLayoutFromPreset(EditorType::kEmulator, dockspace_id);
}

void LayoutManager::SaveCurrentLayout(const std::string& name, bool persist) {
  if (!window_manager_) {
    LOG_WARN("LayoutManager",
             "Cannot save layout '%s': WorkspaceWindowManager not available",
             name.c_str());
    return;
  }

  const LayoutScope scope = GetActiveScope();

  // Serialize current window visibility state
  size_t session_id = window_manager_->GetActiveSessionId();
  auto visibility_state = window_manager_->SerializeVisibilityState(session_id);

  // Store in saved_layouts_ for later persistence
  saved_layouts_[name] = visibility_state;
  saved_pinned_layouts_[name] = window_manager_->SerializePinnedState();
  layout_scopes_[name] = scope;

  // Also save ImGui docking layout to memory
  size_t ini_size = 0;
  const char* ini_data = ImGui::SaveIniSettingsToMemory(&ini_size);
  if (ini_data && ini_size > 0) {
    saved_imgui_layouts_[name] = std::string(ini_data, ini_size);
  }

  if (persist) {
    SaveLayoutsToDisk(scope);
  }

  LOG_INFO("LayoutManager", "Saved layout '%s' with %zu window states%s",
           name.c_str(), visibility_state.size(),
           persist ? "" : " (session-only)");
}

void LayoutManager::LoadLayout(const std::string& name) {
  if (!window_manager_) {
    LOG_WARN("LayoutManager",
             "Cannot load layout '%s': WorkspaceWindowManager not available",
             name.c_str());
    return;
  }

  // Find saved layout
  auto layout_it = saved_layouts_.find(name);
  if (layout_it == saved_layouts_.end()) {
    LOG_WARN("LayoutManager", "Layout '%s' not found", name.c_str());
    return;
  }

  // Restore window visibility
  size_t session_id = window_manager_->GetActiveSessionId();
  window_manager_->RestoreVisibilityState(session_id, layout_it->second,
                                          /*publish_events=*/true);

  auto pinned_it = saved_pinned_layouts_.find(name);
  if (pinned_it != saved_pinned_layouts_.end()) {
    window_manager_->RestorePinnedState(pinned_it->second);
  }

  // Restore ImGui docking layout if available
  auto imgui_it = saved_imgui_layouts_.find(name);
  if (imgui_it != saved_imgui_layouts_.end() && !imgui_it->second.empty()) {
    ImGui::LoadIniSettingsFromMemory(imgui_it->second.c_str(),
                                     imgui_it->second.size());
  }

  LOG_INFO("LayoutManager", "Loaded layout '%s'", name.c_str());
}

void LayoutManager::CaptureTemporarySessionLayout(size_t session_id) {
  if (!window_manager_) {
    return;
  }

  temp_session_id_ = session_id;
  temp_session_visibility_ =
      window_manager_->SerializeVisibilityState(session_id);
  temp_session_pinned_ = window_manager_->SerializePinnedState();

  size_t ini_size = 0;
  const char* ini_data = ImGui::SaveIniSettingsToMemory(&ini_size);
  if (ini_data && ini_size > 0) {
    temp_session_imgui_layout_ = std::string(ini_data, ini_size);
  } else {
    temp_session_imgui_layout_.clear();
  }

  has_temp_session_layout_ = true;
  LOG_INFO(
      "LayoutManager",
      "Captured temporary session layout for session %zu (%zu panel states)",
      session_id, temp_session_visibility_.size());
}

bool LayoutManager::RestoreTemporarySessionLayout(size_t session_id,
                                                  bool clear_after_restore) {
  if (!window_manager_ || !has_temp_session_layout_) {
    return false;
  }

  if (session_id != temp_session_id_) {
    LOG_WARN("LayoutManager",
             "Session layout snapshot belongs to session %zu, requested %zu",
             temp_session_id_, session_id);
    return false;
  }

  window_manager_->RestoreVisibilityState(session_id, temp_session_visibility_,
                                          /*publish_events=*/true);
  window_manager_->RestorePinnedState(temp_session_pinned_);

  if (!temp_session_imgui_layout_.empty()) {
    ImGui::LoadIniSettingsFromMemory(temp_session_imgui_layout_.c_str(),
                                     temp_session_imgui_layout_.size());
  }

  if (clear_after_restore) {
    ClearTemporarySessionLayout();
  }

  LOG_INFO("LayoutManager", "Restored temporary session layout for session %zu",
           session_id);
  return true;
}

void LayoutManager::ClearTemporarySessionLayout() {
  has_temp_session_layout_ = false;
  temp_session_id_ = 0;
  temp_session_visibility_.clear();
  temp_session_pinned_.clear();
  temp_session_imgui_layout_.clear();
}

bool LayoutManager::SaveNamedSnapshot(const std::string& name,
                                      size_t session_id) {
  if (!window_manager_ || name.empty()) {
    return false;
  }
  SessionSnapshot snapshot;
  snapshot.session_id = session_id;
  snapshot.visibility = window_manager_->SerializeVisibilityState(session_id);
  snapshot.pinned = window_manager_->SerializePinnedState();
  size_t ini_size = 0;
  const char* ini_data = ImGui::SaveIniSettingsToMemory(&ini_size);
  if (ini_data && ini_size > 0) {
    snapshot.imgui_layout.assign(ini_data, ini_size);
  }
  named_snapshots_[name] = std::move(snapshot);
  LOG_INFO("LayoutManager", "Saved named snapshot '%s' for session %zu",
           name.c_str(), session_id);
  return true;
}

bool LayoutManager::RestoreNamedSnapshot(const std::string& name,
                                         size_t session_id,
                                         bool remove_after_restore) {
  if (!window_manager_) {
    return false;
  }
  auto it = named_snapshots_.find(name);
  if (it == named_snapshots_.end()) {
    return false;
  }
  const SessionSnapshot& snapshot = it->second;
  if (snapshot.session_id != session_id) {
    return false;
  }
  window_manager_->RestoreVisibilityState(session_id, snapshot.visibility,
                                          /*publish_events=*/true);
  window_manager_->RestorePinnedState(snapshot.pinned);
  if (!snapshot.imgui_layout.empty()) {
    ImGui::LoadIniSettingsFromMemory(snapshot.imgui_layout.c_str(),
                                     snapshot.imgui_layout.size());
  }
  if (remove_after_restore) {
    named_snapshots_.erase(it);
  }
  return true;
}

bool LayoutManager::DeleteNamedSnapshot(const std::string& name) {
  auto it = named_snapshots_.find(name);
  if (it == named_snapshots_.end()) {
    return false;
  }
  named_snapshots_.erase(it);
  return true;
}

std::vector<std::string> LayoutManager::ListNamedSnapshots(
    size_t session_id) const {
  std::vector<std::string> names;
  names.reserve(named_snapshots_.size());
  for (const auto& [name, snapshot] : named_snapshots_) {
    if (snapshot.session_id == session_id) {
      names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

bool LayoutManager::HasNamedSnapshot(const std::string& name) const {
  return named_snapshots_.find(name) != named_snapshots_.end();
}

bool LayoutManager::DeleteLayout(const std::string& name) {
  auto layout_it = saved_layouts_.find(name);
  if (layout_it == saved_layouts_.end()) {
    LOG_WARN("LayoutManager", "Cannot delete layout '%s': not found",
             name.c_str());
    return false;
  }

  LayoutScope scope = GetActiveScope();
  auto scope_it = layout_scopes_.find(name);
  if (scope_it != layout_scopes_.end()) {
    scope = scope_it->second;
  }

  saved_layouts_.erase(layout_it);
  saved_imgui_layouts_.erase(name);
  saved_pinned_layouts_.erase(name);
  layout_scopes_.erase(name);

  SaveLayoutsToDisk(scope);

  LOG_INFO("LayoutManager", "Deleted layout '%s'", name.c_str());
  return true;
}

std::vector<LayoutProfile> LayoutManager::GetBuiltInProfiles() {
  return {
      {.id = "code",
       .label = "Code",
       .description = "Focused editing workspace with minimal panel noise",
       .preset_name = "Minimal",
       .open_agent_chat = false},
      {.id = "debug",
       .label = "Debug",
       .description = "Debugger-first workspace for tracing and memory tools",
       .preset_name = "Developer",
       .open_agent_chat = false},
      {.id = "mapping",
       .label = "Mapping",
       .description = "Map-centric layout for overworld or dungeon workflows",
       .preset_name = "Overworld Expert",
       .open_agent_chat = false},
      {.id = "chat",
       .label = "Chat",
       .description = "Collaboration-heavy layout with agent-centric tooling",
       .preset_name = "Modder",
       .open_agent_chat = true},
  };
}

bool LayoutManager::ApplyBuiltInProfile(const std::string& profile_id,
                                        size_t session_id,
                                        EditorType editor_type,
                                        LayoutProfile* out_profile) {
  if (!window_manager_) {
    LOG_WARN("LayoutManager",
             "Cannot apply profile '%s': WorkspaceWindowManager not available",
             profile_id.c_str());
    return false;
  }

  LayoutProfile matched_profile;
  bool found = false;
  for (const auto& profile : GetBuiltInProfiles()) {
    if (profile.id == profile_id) {
      matched_profile = profile;
      found = true;
      break;
    }
  }

  if (!found) {
    LOG_WARN("LayoutManager", "Unknown layout profile id: %s",
             profile_id.c_str());
    return false;
  }

  const std::string resolved_preset =
      ResolveProfilePresetName(profile_id, editor_type);
  if (!resolved_preset.empty()) {
    matched_profile.preset_name = resolved_preset;
  }

  PanelLayoutPreset preset;
  if (!TryGetNamedPreset(matched_profile.preset_name, &preset)) {
    LOG_WARN("LayoutManager", "Unable to resolve preset '%s' for profile '%s'",
             matched_profile.preset_name.c_str(), profile_id.c_str());
    return false;
  }

  window_manager_->HideAllWindowsInSession(session_id);
  for (const auto& panel_id : preset.default_visible_panels) {
    window_manager_->OpenWindow(session_id, panel_id);
  }

  RequestRebuild();

  if (out_profile) {
    *out_profile = matched_profile;
  }

  LOG_INFO("LayoutManager", "Applied profile '%s' via preset '%s'",
           profile_id.c_str(), matched_profile.preset_name.c_str());
  return true;
}

std::vector<std::string> LayoutManager::GetSavedLayoutNames() const {
  std::vector<std::string> names;
  names.reserve(saved_layouts_.size());
  for (const auto& [name, _] : saved_layouts_) {
    names.push_back(name);
  }
  return names;
}

bool LayoutManager::HasLayout(const std::string& name) const {
  return saved_layouts_.find(name) != saved_layouts_.end();
}

void LayoutManager::LoadLayoutsFromDisk() {
  saved_layouts_.clear();
  saved_imgui_layouts_.clear();
  saved_pinned_layouts_.clear();
  layout_scopes_.clear();

  LoadLayoutsFromDiskInternal(LayoutScope::kGlobal, /*merge=*/false);
  if (!project_layout_key_.empty()) {
    LoadLayoutsFromDiskInternal(LayoutScope::kProject, /*merge=*/true);
  }
}

void LayoutManager::SetProjectLayoutKey(const std::string& key) {
  if (key.empty()) {
    UseGlobalLayouts();
    return;
  }
  project_layout_key_ = key;
  LoadLayoutsFromDisk();
}

void LayoutManager::UseGlobalLayouts() {
  project_layout_key_.clear();
  LoadLayoutsFromDisk();
}

LayoutScope LayoutManager::GetActiveScope() const {
  return project_layout_key_.empty() ? LayoutScope::kGlobal
                                     : LayoutScope::kProject;
}

void LayoutManager::LoadLayoutsFromDiskInternal(LayoutScope scope, bool merge) {
  std::filesystem::path layout_path =
      GetLayoutsFilePath(scope, project_layout_key_);
  if (layout_path.empty()) {
    return;
  }

  if (!std::filesystem::exists(layout_path)) {
    if (!merge) {
      LOG_INFO("LayoutManager", "No layouts file at %s",
               layout_path.string().c_str());
    }
    return;
  }

  try {
    std::ifstream file(layout_path);
    if (!file.is_open()) {
      LOG_WARN("LayoutManager", "Failed to open layouts file: %s",
               layout_path.string().c_str());
      return;
    }

    yaze::Json root;
    file >> root;

    if (!root.contains("layouts") || !root["layouts"].is_object()) {
      LOG_WARN("LayoutManager", "Layouts file missing 'layouts' object: %s",
               layout_path.string().c_str());
      return;
    }

    for (auto& [name, entry] : root["layouts"].items()) {
      if (!entry.is_object()) {
        continue;
      }

      std::unordered_map<std::string, bool> windows;
      std::unordered_map<std::string, bool> pinned;

      JsonToWindowMap(entry, &windows);
      if (entry.contains("pinned")) {
        JsonToBoolMap(entry["pinned"], &pinned);
      }

      saved_layouts_[name] = std::move(windows);
      saved_pinned_layouts_[name] = std::move(pinned);
      layout_scopes_[name] = scope;

      if (entry.contains("imgui_ini") && entry["imgui_ini"].is_string()) {
        saved_imgui_layouts_[name] = entry["imgui_ini"].get<std::string>();
      } else {
        saved_imgui_layouts_.erase(name);
      }
    }

    LOG_INFO("LayoutManager", "Loaded layouts from %s",
             layout_path.string().c_str());
  } catch (const std::exception& e) {
    LOG_WARN("LayoutManager", "Failed to load layouts: %s", e.what());
  }
}

void LayoutManager::SaveLayoutsToDisk(LayoutScope scope) const {
  std::filesystem::path layout_path =
      GetLayoutsFilePath(scope, project_layout_key_);
  if (layout_path.empty()) {
    LOG_WARN("LayoutManager", "No layout path resolved for scope");
    return;
  }

  auto status =
      util::PlatformPaths::EnsureDirectoryExists(layout_path.parent_path());
  if (!status.ok()) {
    LOG_WARN("LayoutManager", "Failed to create layout directory: %s",
             status.ToString().c_str());
    return;
  }

  try {
    yaze::Json root;
    root["version"] = 2;
    root["layouts"] = yaze::Json::object();

    for (const auto& [name, windows] : saved_layouts_) {
      auto scope_it = layout_scopes_.find(name);
      if (scope_it != layout_scopes_.end() && scope_it->second != scope) {
        continue;
      }

      yaze::Json entry;
      entry[kWindowsKey] = BoolMapToJson(windows);

      auto pinned_it = saved_pinned_layouts_.find(name);
      if (pinned_it != saved_pinned_layouts_.end()) {
        entry["pinned"] = BoolMapToJson(pinned_it->second);
      }

      auto imgui_it = saved_imgui_layouts_.find(name);
      if (imgui_it != saved_imgui_layouts_.end()) {
        entry["imgui_ini"] = imgui_it->second;
      }

      root["layouts"][name] = entry;
    }

    std::ofstream file(layout_path);
    if (!file.is_open()) {
      LOG_WARN("LayoutManager", "Failed to open layouts file for write: %s",
               layout_path.string().c_str());
      return;
    }
    file << root.dump(2);
    file.close();
  } catch (const std::exception& e) {
    LOG_WARN("LayoutManager", "Failed to save layouts: %s", e.what());
  }
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
  if (!window_manager_) {
    return "";
  }

  const size_t session_id = window_manager_->GetActiveSessionId();
  return window_manager_->GetWorkspaceWindowName(session_id, card_id);
}

namespace {

ImGuiDir SplitDirectionToImGuiDir(layout_designer::SplitDirection d) {
  using SD = layout_designer::SplitDirection;
  switch (d) {
    case SD::kLeft:
      return ImGuiDir_Left;
    case SD::kRight:
      return ImGuiDir_Right;
    case SD::kUp:
      return ImGuiDir_Up;
    case SD::kDown:
      return ImGuiDir_Down;
  }
  return ImGuiDir_Left;
}

std::string ResolveDockWindowTitle(const WorkspaceWindowManager* wm,
                                   size_t session_id,
                                   const layout_designer::PanelEntry& panel) {
  if (wm) {
    if (const auto* desc =
            wm->GetWindowDescriptor(session_id, panel.panel_id)) {
      return wm->GetWorkspaceWindowName(*desc);
    }
  }
  // Fallback: reconstruct the ImGui window name using the same formula as
  // WindowDescriptor::GetImGuiWindowName so layouts referencing panels
  // that register late still find their windows when they come up.
  std::string label;
  if (!panel.icon.empty() && !panel.display_name.empty()) {
    label = panel.icon + " " + panel.display_name;
  } else if (!panel.display_name.empty()) {
    label = panel.display_name;
  } else if (!panel.icon.empty()) {
    label = panel.icon;
  }
  if (panel.panel_id.empty()) {
    return label;
  }
  return label.empty() ? panel.panel_id : (label + "##" + panel.panel_id);
}

void ApplyDockNodeRecursive(WorkspaceWindowManager* wm, size_t session_id,
                            const layout_designer::DockNode& node,
                            ImGuiID target_id) {
  using NodeType = layout_designer::DockNode::Type;
  if (node.type == NodeType::kLeaf) {
    for (const auto& panel : node.panels) {
      const std::string title = ResolveDockWindowTitle(wm, session_id, panel);
      if (!title.empty()) {
        ImGui::DockBuilderDockWindow(title.c_str(), target_id);
      }
    }
    return;
  }

  // Split node.
  const ImGuiDir dir = SplitDirectionToImGuiDir(node.split_direction);
  const float ratio = std::clamp(node.split_ratio, 0.05f, 0.95f);
  ImGuiID id_other = 0;
  const ImGuiID id_at_dir =
      ImGui::DockBuilderSplitNode(target_id, dir, ratio, nullptr, &id_other);
  if (node.child_a) {
    ApplyDockNodeRecursive(wm, session_id, *node.child_a, id_at_dir);
  }
  if (node.child_b) {
    ApplyDockNodeRecursive(wm, session_id, *node.child_b, id_other);
  }
}

struct PanelLookupEntry {
  std::string panel_id;
  std::string display_name;
  std::string icon;
};

std::unique_ptr<layout_designer::DockNode> CaptureDockNodeRecursive(
    const ImGuiDockNode* node,
    const std::unordered_map<std::string, PanelLookupEntry>& by_window_name) {
  if (!node) {
    return layout_designer::DockNode::MakeLeaf({});
  }

  if (node->IsSplitNode()) {
    auto child_a =
        CaptureDockNodeRecursive(node->ChildNodes[0], by_window_name);
    auto child_b =
        CaptureDockNodeRecursive(node->ChildNodes[1], by_window_name);

    const bool vertical = node->SplitAxis == ImGuiAxis_Y;
    const layout_designer::SplitDirection dir =
        vertical ? layout_designer::SplitDirection::kUp
                 : layout_designer::SplitDirection::kLeft;

    float ratio = 0.5f;
    if (node->ChildNodes[0] && node->ChildNodes[1]) {
      if (vertical && node->Size.y > 0.0f) {
        ratio = node->ChildNodes[0]->Size.y / node->Size.y;
      } else if (!vertical && node->Size.x > 0.0f) {
        ratio = node->ChildNodes[0]->Size.x / node->Size.x;
      }
    }
    ratio = std::clamp(ratio, 0.05f, 0.95f);
    return layout_designer::DockNode::MakeSplit(dir, ratio, std::move(child_a),
                                                std::move(child_b));
  }

  // Leaf.
  std::vector<layout_designer::PanelEntry> panels;
  panels.reserve(static_cast<size_t>(node->Windows.Size));
  int selected_index = -1;
  for (int i = 0; i < node->Windows.Size; ++i) {
    const ImGuiWindow* w = node->Windows[i];
    if (!w || !w->Name)
      continue;
    auto it = by_window_name.find(std::string(w->Name));
    if (it == by_window_name.end())
      continue;
    if (node->SelectedTabId != 0 && w->ID == node->SelectedTabId) {
      selected_index = static_cast<int>(panels.size());
    }
    panels.push_back(
        {it->second.panel_id, it->second.display_name, it->second.icon});
  }
  auto leaf = layout_designer::DockNode::MakeLeaf(std::move(panels));
  if (selected_index >= 0 &&
      selected_index < static_cast<int>(leaf->panels.size())) {
    leaf->active_tab_index = selected_index;
  }
  return leaf;
}

// Recursively walk a DockTree and append every leaf's panel_ids into
// `out`. Used by ApplyDockTree to drive the visibility-open pass.
void CollectPanelIdsInSubtree(const layout_designer::DockNode& node,
                              std::vector<std::string>* out) {
  if (node.type == layout_designer::DockNode::Type::kLeaf) {
    for (const auto& p : node.panels) {
      out->push_back(p.panel_id);
    }
    return;
  }
  if (node.child_a)
    CollectPanelIdsInSubtree(*node.child_a, out);
  if (node.child_b)
    CollectPanelIdsInSubtree(*node.child_b, out);
}

}  // namespace

absl::Status LayoutManager::ApplyDockTree(const layout_designer::DockTree& tree,
                                          ImGuiID dockspace_id) {
  if (!window_manager_) {
    return absl::FailedPreconditionError(
        "LayoutManager::ApplyDockTree: WorkspaceWindowManager not bound");
  }
  std::string validation_error;
  if (!tree.Validate(&validation_error)) {
    return absl::InvalidArgumentError("LayoutManager::ApplyDockTree: " +
                                      validation_error);
  }

  // Visibility pass (Phase 8 review 2026-04-24, refined 2026-04-25):
  //   - Open every panel referenced in the tree so users see what they
  //     docked rather than empty slots.
  //   - Close every non-pinned panel that is NOT in the tree, so a saved
  //     subset doesn't leave previously-visible panels lingering as
  //     floating ghosts after apply. Pinned panels are intentionally
  //     persistent (rev-7 / rev-17 force-pinned panels like
  //     `agent.oracle_ram`, `workflow.output`, `layout.designer`) and
  //     must survive — closing them would silently hide cross-editor
  //     functionality the user can't easily put back.
  std::vector<std::string> panel_ids;
  if (tree.root) {
    CollectPanelIdsInSubtree(*tree.root, &panel_ids);
  }
  const std::unordered_set<std::string> tree_id_set(panel_ids.begin(),
                                                    panel_ids.end());
  for (const auto& id : panel_ids) {
    window_manager_->OpenWindow(id);
  }
  const size_t session_id = window_manager_->GetActiveSessionId();
  for (const std::string& id :
       window_manager_->GetWindowsInSession(session_id)) {
    if (tree_id_set.count(id) > 0)
      continue;
    if (window_manager_->IsWindowPinned(session_id, id))
      continue;
    // Phase 8.2 review 3 (2026-04-25): skip already-closed panels.
    // CloseWindowImpl fires on_hide and publishes
    // WindowVisibilityChanged(false) regardless of prior state, so
    // blindly closing every non-tree panel produces a burst of
    // redundant hide events for unrelated already-hidden windows.
    if (!window_manager_->IsWindowOpen(session_id, id))
      continue;
    window_manager_->CloseWindow(id);
  }

  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

  ImVec2 size(1280.0f, 720.0f);
  if (const ImGuiViewport* vp = ImGui::GetMainViewport()) {
    if (vp->WorkSize.x > 0.0f && vp->WorkSize.y > 0.0f) {
      size = vp->WorkSize;
    }
  }
  ImGui::DockBuilderSetNodeSize(dockspace_id, size);

  ApplyDockNodeRecursive(window_manager_, session_id, *tree.root, dockspace_id);
  ImGui::DockBuilderFinish(dockspace_id);

  last_dockspace_id_ = dockspace_id;

  // Init-tracking pass (Phase 8.2 review 2026-04-25; refined 8.2 review 3):
  //   - When the user is in an editor (current_editor_type_ != kUnknown),
  //     mark only that editor type initialized. Other editors keep their
  //     lazy first-run init — their preset fires on activation as before.
  //   - When no editor is active yet (kUnknown — startup-reapply OR
  //     manual apply from the dashboard/settings shell BEFORE any editor
  //     activation), the per-editor mark wouldn't protect anything, so
  //     arm the one-shot `startup_reapply_pending_protection_` flag that
  //     the next InitializeEditorLayout call consumes. Round-3 Codex
  //     noted that without this branch, applying from a no-editor
  //     context still left the original clobber bug intact.
  if (current_editor_type_ != EditorType::kUnknown) {
    MarkLayoutInitialized(current_editor_type_);
  } else {
    startup_reapply_pending_protection_ = true;
  }

  return absl::OkStatus();
}

absl::Status LayoutManager::MaybeReapplyStartupLayout(UserSettings* settings) {
  if (startup_layout_consumed_) {
    return absl::OkStatus();
  }
  if (settings == nullptr) {
    startup_layout_consumed_ = true;
    return absl::OkStatus();
  }

  const std::string& name = settings->prefs().last_applied_layout_name;
  if (name.empty()) {
    // Nothing to reapply. Mark consumed so we stop checking each frame.
    startup_layout_consumed_ = true;
    return absl::OkStatus();
  }

  if (main_dockspace_id_ == 0) {
    // The controller hasn't bound the main dockspace yet — try again
    // next frame. Leave the flag clear.
    return absl::OkStatus();
  }

  const auto& named_layouts = settings->prefs().named_layouts;
  const auto it = named_layouts.find(name);
  if (it == named_layouts.end()) {
    startup_layout_consumed_ = true;
    util::logf(
        "LayoutManager: startup layout '%s' missing from "
        "named_layouts; falling through to default.",
        name.c_str());
    return absl::NotFoundError(absl::StrCat("startup layout \"", name,
                                            "\" not found in named_layouts"));
  }

  nlohmann::json parsed;
  try {
    parsed = nlohmann::json::parse(it->second);
  } catch (const nlohmann::json::parse_error& e) {
    startup_layout_consumed_ = true;
    util::logf("LayoutManager: startup layout '%s' JSON parse error: %s",
               name.c_str(), e.what());
    return absl::InvalidArgumentError(
        absl::StrCat("startup layout \"", name, "\" parse error: ", e.what()));
  }

  auto tree_or = layout_designer::DockTreeFromJson(parsed);
  if (!tree_or.ok()) {
    startup_layout_consumed_ = true;
    util::logf("LayoutManager: startup layout '%s' failed to parse: %s",
               name.c_str(), std::string(tree_or.status().message()).c_str());
    return tree_or.status();
  }

  std::string validation_error;
  if (!tree_or->Validate(&validation_error)) {
    startup_layout_consumed_ = true;
    util::logf("LayoutManager: startup layout '%s' failed validation: %s",
               name.c_str(), validation_error.c_str());
    return absl::InvalidArgumentError(absl::StrCat(
        "startup layout \"", name, "\" validation failed: ", validation_error));
  }

  absl::Status apply_status = ApplyDockTree(*tree_or, main_dockspace_id_);
  startup_layout_consumed_ = true;
  if (!apply_status.ok()) {
    util::logf("LayoutManager: startup layout '%s' ApplyDockTree failed: %s",
               name.c_str(), std::string(apply_status.message()).c_str());
  }
  // ApplyDockTree itself arms `startup_reapply_pending_protection_`
  // because `current_editor_type_` is still `kUnknown` at this point
  // in the boot — no need to set it again here. Same arming covers
  // dashboard-time manual apply, which round-3 Codex flagged.
  return apply_status;
}

absl::StatusOr<layout_designer::DockTree> LayoutManager::CaptureDockTree(
    ImGuiID dockspace_id) const {
  if (!window_manager_) {
    return absl::FailedPreconditionError(
        "LayoutManager::CaptureDockTree: WorkspaceWindowManager not bound");
  }
  ImGuiDockNode* root_node = ImGui::DockBuilderGetNode(dockspace_id);
  if (!root_node) {
    return absl::NotFoundError(
        "LayoutManager::CaptureDockTree: no dock node at given id");
  }

  std::unordered_map<std::string, PanelLookupEntry> by_window_name;
  for (const auto& [panel_id, desc] :
       window_manager_->GetAllWindowDescriptors()) {
    const std::string title = window_manager_->GetWorkspaceWindowName(desc);
    if (title.empty())
      continue;
    by_window_name.emplace(
        title, PanelLookupEntry{panel_id, desc.display_name, desc.icon});
  }

  layout_designer::DockTree tree;
  tree.root = CaptureDockNodeRecursive(root_node, by_window_name);
  if (!tree.root) {
    tree.root = layout_designer::DockNode::MakeLeaf({});
  }
  return tree;
}

}  // namespace editor
}  // namespace yaze
