#include "app/editor/layout/layout_manager.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
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

void LayoutManager::DisableLazyDefaultDocking() {
  lazy_default_dock_state_ = LazyDefaultDockState{};
}

void LayoutManager::RefreshLazyDefaultDockingContext(EditorType type,
                                                     ImGuiID dockspace_id) {
  const PanelLayoutPreset preset = LayoutPresets::GetDefaultPreset(type);
  // Each editor owns its dockspace, so resume that editor's stashed geometry
  // instead of treating another editor's state as stale.
  const bool restored = RestoreLazyDefaultDockState(type, dockspace_id);
  auto& state = lazy_default_dock_state_;
  if (!restored || !preset.dock_only_default_visible_panels ||
      !window_manager_ || state.dockspace != dockspace_id ||
      !ImGui::DockBuilderGetNode(state.dockspace) ||
      !ImGui::DockBuilderGetNode(state.center)) {
    state.enabled = false;
    return;
  }

  const size_t session_id = window_manager_->GetActiveSessionId();
  state.enabled = true;
  state.editor_type = type;
  state.session_id = session_id;
  state.attempted_panels = lazy_default_dock_attempts_[session_id][type];
}

void LayoutManager::MarkLazyDefaultDockAttempted(const std::string& panel_id) {
  auto& state = lazy_default_dock_state_;
  state.attempted_panels.insert(panel_id);
  lazy_default_dock_attempts_[state.session_id][state.editor_type].insert(
      panel_id);
}

void LayoutManager::DockOpenPresetPanels(EditorType type) {
  if (!window_manager_) {
    return;
  }

  const size_t session_id = window_manager_->GetActiveSessionId();
  const PanelLayoutPreset preset = LayoutPresets::GetDefaultPreset(type);
  const auto dock_if_open = [&](const std::string& panel_id) {
    if (window_manager_->IsWindowOpen(session_id, panel_id)) {
      DockPresetPositionOnFirstOpen(session_id, panel_id,
                                    /*include_default_visible=*/true);
    }
  };
  for (const auto& panel_id : preset.default_visible_panels) {
    dock_if_open(panel_id);
  }
  for (const auto& panel_id : preset.optional_panels) {
    dock_if_open(panel_id);
  }
}

void LayoutManager::ProtectRestoredLayoutFromDefaultInitialization() {
  if (current_editor_type_ != EditorType::kUnknown) {
    MarkLayoutInitialized(current_editor_type_);
  } else {
    startup_reapply_pending_protection_ = true;
  }
}

void LayoutManager::InitializeEditorLayout(EditorType type,
                                           ImGuiID dockspace_id) {
  // Phase 8.2 review (2026-04-25): one-shot protection set by
  // MaybeReapplyStartupLayout. The very first lazy preset init after a
  // successful startup-reapply must NOT rebuild the dockspace —
  // doing so would silently overwrite the user's saved layout on
  // their first editor activation. Consume the flag, mark the type
  // initialized so subsequent activations behave normally, and bail.
  if (startup_reapply_pending_protection_) {
    DisableLazyDefaultDocking();
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

  // Refresh the active editor/session even when its default dock tree was
  // initialized earlier. Lazy optional-panel placement uses the live tree but
  // keeps its one-shot history scoped to this ROM session and editor.
  last_dockspace_id_ = dockspace_id;
  current_editor_type_ = type;
  active_editor_type_ = type;

  // Don't reinitialize if already set up
  if (IsLayoutInitialized(type)) {
    RefreshLazyDefaultDockingContext(type, dockspace_id);
    // Session visibility restoration intentionally does not publish panel-open
    // events. Sweep the restored state after refreshing the session context so
    // an already-visible optional panel still receives its one-shot default.
    DockOpenPresetPanels(type);
    LOG_INFO("LayoutManager",
             "Layout for editor type %d already initialized, skipping",
             static_cast<int>(type));
    return;
  }

  LOG_INFO("LayoutManager", "Initializing layout for editor type %d",
           static_cast<int>(type));

  // Clear existing layout for this dockspace
  pending_dock_tab_selections_.clear();
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

  ImGui::DockBuilderSetNodeSize(dockspace_id, EditorDockspaceSize());

  // Build layout based on editor type using generic builder
  BuildLayoutFromPreset(type, dockspace_id);

  // Show default windows from LayoutPresets (single source of truth)
  ShowDefaultWindowsForEditor(window_manager_, type);

  // Finalize the layout
  ImGui::DockBuilderFinish(dockspace_id);

  // Visibility may have been restored before this editor's first layout
  // build. Give already-open optional panels the same first-open placement.
  DockOpenPresetPanels(type);

  // Mark as initialized
  MarkLayoutInitialized(type);
}

void LayoutManager::RebuildLayout(EditorType type, ImGuiID dockspace_id) {
  pending_dock_tab_selections_.clear();
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
  active_editor_type_ = type;

  // Clear the layout initialization flag to force rebuild
  layouts_initialized_[type] = false;

  // Clear existing layout for this dockspace
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, EditorDockspaceSize());

  // Build layout based on editor type using generic builder
  BuildLayoutFromPreset(type, dockspace_id);

  // Show default cards from LayoutPresets (single source of truth)
  ShowDefaultWindowsForEditor(window_manager_, type);

  // Finalize the layout
  ImGui::DockBuilderFinish(dockspace_id);

  DockOpenPresetPanels(type);

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

struct RegionWidthPreference {
  float width = 0.0f;
  // True when the region's first docked panel reports an exact content width
  // (HasExactPreferredWidth); the region then takes exactly that width.
  bool exact = false;
};

RegionWidthPreference ResolvePreferredRegionWidth(
    WorkspaceWindowManager* window_manager,
    const std::vector<std::pair<std::string, DockPosition>>& docked_panels,
    bool (*matches_region)(DockPosition)) {
  RegionWidthPreference preference;
  if (!window_manager) {
    return preference;
  }

  bool first_in_region = true;
  for (const auto& [panel_id, position] : docked_panels) {
    if (!matches_region(position)) {
      continue;
    }
    WindowContent* panel = window_manager->GetWindowContent(panel_id);
    if (!panel) {
      continue;
    }
    const float width = panel->GetPreferredWidth();
    if (first_in_region) {
      first_in_region = false;
      // docked_panels lists default-visible panels first, so the first panel
      // in a region is the one the user sees on top. A fixed-width grid there
      // owns the region width; panels stacked below it adapt.
      if (panel->HasExactPreferredWidth() && width > 0.0f) {
        return {width, true};
      }
    }
    preference.width = std::max(preference.width, width);
  }
  return preference;
}

// Ratio for an exact-width region. Splits are relative to the node being
// split (the dockspace minus any region split off before it), not the
// viewport, so compute against that width.
float ExactRegionRatio(float preferred_width, float available_width,
                       float max_ratio) {
  if (available_width <= 0.0f) {
    return max_ratio;
  }
  return std::clamp(preferred_width / available_width, 0.05f, max_ratio);
}

void ApplyPreferredSplitWidths(
    DockSplitConfig* cfg, const DockSplitNeeds& needs, float viewport_width,
    float dockspace_width, WorkspaceWindowManager* window_manager,
    const std::vector<std::pair<std::string, DockPosition>>& docked_panels) {
  if (!cfg || !window_manager || viewport_width <= 0.0f) {
    return;
  }
  const float split_width =
      dockspace_width > 0.0f ? dockspace_width : viewport_width;

  if (needs.left) {
    const auto preferred_left = ResolvePreferredRegionWidth(
        window_manager, docked_panels, IsLeftDockPosition);
    if (preferred_left.exact) {
      cfg->left = ExactRegionRatio(preferred_left.width, split_width, 0.36f);
    } else if (preferred_left.width > 0.0f) {
      cfg->left =
          std::clamp(preferred_left.width / viewport_width, 0.14f, 0.36f);
    }
  }

  if (needs.right) {
    const auto preferred_right = ResolvePreferredRegionWidth(
        window_manager, docked_panels, IsRightDockPosition);
    if (preferred_right.exact) {
      const float remaining =
          needs.left ? split_width * (1.0f - cfg->left) : split_width;
      cfg->right = ExactRegionRatio(preferred_right.width, remaining, 0.42f);
    } else if (preferred_right.width > 0.0f) {
      cfg->right =
          std::clamp(preferred_right.width / viewport_width, 0.18f, 0.42f);
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

namespace {

constexpr char kOuterDockspaceName[] = "yaze.MainDockSpace";
constexpr char kEditorDockspacePrefix[] = "yaze.EditorDock/";
constexpr char kEditorHostWindowName[] = "###YazeEditorHost";
constexpr char kLayoutIniTypeName[] = "YazeLayout";
constexpr char kLayoutIniInitializedKey[] = "Initialized=";
constexpr char kLayoutIniPinSideKey[] = "PinSide=";

// Share of the outer dockspace given to a new pin node with no saved size,
// and the range a saved size is clamped to on a smaller window.
constexpr float kPinRatio = 0.22f;
constexpr float kPinMinRatio = 0.05f;
constexpr float kPinMaxRatio = 0.6f;

// imgui.ini names for ImGuiDir_Left..ImGuiDir_Down.
constexpr std::array<const char*, 4> kPinSideNames = {"Left", "Right", "Up",
                                                      "Down"};

ImGuiAxis PinAxis(ImGuiDir dir) {
  return (dir == ImGuiDir_Left || dir == ImGuiDir_Right) ? ImGuiAxis_X
                                                         : ImGuiAxis_Y;
}

// Central leaf under `node`. Reads LocalFlags because ImGuiDockNode::
// CentralNode on the root is only refreshed on the next frame after a split.
ImGuiDockNode* FindCentralLeaf(ImGuiDockNode* node) {
  if (!node) {
    return nullptr;
  }
  if (node->LocalFlags & ImGuiDockNodeFlags_CentralNode) {
    return node;
  }
  for (ImGuiDockNode* child : node->ChildNodes) {
    if (ImGuiDockNode* found = FindCentralLeaf(child)) {
      return found;
    }
  }
  return nullptr;
}

ImGuiID DockRootId(ImGuiID node_id) {
  ImGuiDockNode* node = ImGui::DockBuilderGetNode(node_id);
  while (node && node->ParentNode) {
    node = node->ParentNode;
  }
  return node ? node->ID : 0;
}

void LayoutIniClearAll(ImGuiContext*, ImGuiSettingsHandler* handler) {
  if (auto* manager = static_cast<LayoutManager*>(handler->UserData)) {
    manager->ResetLayoutIniSettings();
  }
}

void* LayoutIniReadOpen(ImGuiContext*, ImGuiSettingsHandler* handler,
                        const char* name) {
  if (std::strcmp(name, "Data") != 0) {
    return nullptr;
  }
  return handler->UserData;
}

void LayoutIniReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry,
                       const char* line) {
  static_cast<LayoutManager*>(entry)->ReadLayoutIniLine(line);
}

void LayoutIniApplyAll(ImGuiContext*, ImGuiSettingsHandler* handler) {
  if (auto* manager = static_cast<LayoutManager*>(handler->UserData)) {
    manager->ApplyLayoutIniSettings();
  }
}

void LayoutIniWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler,
                       ImGuiTextBuffer* buf) {
  if (auto* manager = static_cast<LayoutManager*>(handler->UserData)) {
    manager->WriteLayoutIniSettings(buf);
  }
}

// Keep the pre-B2 imgui.ini once, before its dock trees are dropped.
void BackupLegacyLayoutIni() {
  const char* ini_path = ImGui::GetIO().IniFilename;
  if (!ini_path || ini_path[0] == '\0') {
    return;
  }
  std::error_code ec;
  const std::filesystem::path source(ini_path);
  const std::filesystem::path backup(std::string(ini_path) + ".pre-b2");
  if (!std::filesystem::exists(source, ec) ||
      std::filesystem::exists(backup, ec)) {
    return;
  }
  std::filesystem::copy_file(source, backup, ec);
  if (ec) {
    LOG_WARN("LayoutManager", "Could not back up %s: %s", ini_path,
             ec.message().c_str());
  } else {
    LOG_INFO("LayoutManager", "Backed up legacy layout to %s",
             backup.string().c_str());
  }
}

}  // namespace

LayoutManager::~LayoutManager() {
  if (!ImGui::GetCurrentContext()) {
    return;
  }
  if (ImGuiSettingsHandler* handler =
          ImGui::FindSettingsHandler(kLayoutIniTypeName);
      handler && handler->UserData == this) {
    handler->UserData = nullptr;
  }
}

ImGuiID LayoutManager::OuterDockspaceId() {
  return ImHashStr(kOuterDockspaceName);
}

ImGuiID LayoutManager::EditorDockspaceId(EditorType type) {
  const size_t index = EditorTypeIndex(type);
  const char* name = index < kEditorNames.size() ? kEditorNames[index] : "";
  return ImHashStr(absl::StrCat(kEditorDockspacePrefix, name).c_str());
}

ImGuiID LayoutManager::ActiveEditorDockspaceId() const {
  if (active_editor_type_ == EditorType::kUnknown) {
    return 0;
  }
  return EditorDockspaceId(active_editor_type_);
}

ImGuiID LayoutManager::GetMainDockspaceId() const {
  if (const ImGuiID active = ActiveEditorDockspaceId()) {
    return active;
  }
  return main_dockspace_id_;
}

ImGuiID LayoutManager::FindPinNode() const {
  ImGuiDockNode* outer = ImGui::DockBuilderGetNode(OuterDockspaceId());
  if (!outer || !outer->IsSplitNode()) {
    return 0;
  }
  for (ImGuiDockNode* child : outer->ChildNodes) {
    if (child && child->IsLeafNode() && !FindCentralLeaf(child)) {
      return child->ID;
    }
  }
  return 0;
}

ImGuiID LayoutManager::EnsurePinNode() {
  if (const ImGuiID pin = FindPinNode()) {
    return pin;
  }
  const ImGuiID outer_id = OuterDockspaceId();
  ImGuiDockNode* outer = ImGui::DockBuilderGetNode(outer_id);
  // The outer dockspace takes no user splits, so a split root without a pin
  // leaf is not a tree this code built. Leave it alone.
  if (!outer || outer->IsSplitNode()) {
    return 0;
  }
  // DockNodeTreeSplit sizes the new node from the outer size minus the
  // separator, so the saved pixel size maps back to a ratio of that.
  const ImGuiAxis axis = PinAxis(pin_dir_);
  const float avail =
      outer->Size[axis] - ImGui::GetStyle().DockingSeparatorSize;
  float ratio = kPinRatio;
  if (pin_size_ > 0.0f && avail > 0.0f) {
    ratio = ImClamp(pin_size_ / avail, kPinMinRatio, kPinMaxRatio);
  }
  ImGuiID central = 0;
  const ImGuiID pin =
      ImGui::DockBuilderSplitNode(outer_id, pin_dir_, ratio, nullptr, &central);
  ImGui::DockBuilderFinish(outer_id);
  return pin;
}

void LayoutManager::RecordPinSpot() {
  ImGuiDockNode* outer = ImGui::DockBuilderGetNode(OuterDockspaceId());
  ImGuiDockNode* pin = ImGui::DockBuilderGetNode(FindPinNode());
  if (!outer || !pin) {
    return;
  }
  const ImGuiAxis axis = outer->SplitAxis;
  const float size = pin->Size[axis];
  if (size <= 0.0f) {
    return;  // Split this frame; laid out on the next one.
  }
  const bool first = outer->ChildNodes[0] == pin;
  if (axis == ImGuiAxis_X) {
    pin_dir_ = first ? ImGuiDir_Left : ImGuiDir_Right;
  } else {
    pin_dir_ = first ? ImGuiDir_Up : ImGuiDir_Down;
  }
  pin_size_ = size;
}

ImVec2 LayoutManager::EditorDockspaceSize() const {
  if (editor_host_size_.x > 0.0f && editor_host_size_.y > 0.0f) {
    return editor_host_size_;
  }
  const ImVec2 last_size = gui::DockSpaceRenderer::GetLastDockspaceSize();
  if (last_size.x > 0.0f && last_size.y > 0.0f) {
    return last_size;
  }
  if (const ImGuiViewport* viewport = ImGui::GetMainViewport();
      viewport && viewport->WorkSize.x > 0.0f && viewport->WorkSize.y > 0.0f) {
    return viewport->WorkSize;
  }
  return ImVec2(1280.0f, 720.0f);
}

void LayoutManager::StashLazyDefaultDockState() {
  const auto& state = lazy_default_dock_state_;
  if (state.editor_type == EditorType::kUnknown || state.dockspace == 0) {
    return;
  }
  lazy_states_by_type_[state.editor_type] = state;
}

bool LayoutManager::RestoreLazyDefaultDockState(EditorType type,
                                                ImGuiID dockspace_id) {
  auto& state = lazy_default_dock_state_;
  // A dockspace shared by every editor (test override) keeps one live state.
  if (dockspace_id != EditorDockspaceId(type) ||
      (state.editor_type == type && state.dockspace == dockspace_id)) {
    return true;
  }

  StashLazyDefaultDockState();
  if (auto it = lazy_states_by_type_.find(type);
      it != lazy_states_by_type_.end() &&
      it->second.dockspace == dockspace_id &&
      ImGui::DockBuilderGetNode(it->second.center)) {
    state = it->second;
    return true;
  }

  // No stash: the tree came from imgui.ini. Resume from its central leaf;
  // optional panels split new regions off it on first open.
  ImGuiDockNode* center =
      FindCentralLeaf(ImGui::DockBuilderGetNode(dockspace_id));
  if (!center) {
    DisableLazyDefaultDocking();
    return false;
  }
  const DockSplitConfig cfg = DockSplitConfig::ForEditor(type);
  state = LazyDefaultDockState{};
  state.enabled = true;
  state.session_id =
      window_manager_ ? window_manager_->GetActiveSessionId() : 0;
  state.editor_type = type;
  state.dockspace = dockspace_id;
  state.center = center->ID;
  state.left_ratio = cfg.left;
  state.right_ratio = cfg.right;
  state.bottom_ratio = cfg.bottom;
  state.top_ratio = cfg.top;
  state.vertical_split = cfg.vertical_split;
  return true;
}

void LayoutManager::DrawEditorHost() {
  ImGuiDockNode* central =
      FindCentralLeaf(ImGui::DockBuilderGetNode(OuterDockspaceId()));
  if (!central) {
    return;  // DockSpaceWindow has not submitted the outer dockspace yet.
  }

  // Public and internal ImGuiDockNodeFlags are separate enums; combine them
  // through the int typedef.
  ImGuiDockNodeFlags host_node_flags =
      ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_NoDockingSplit;
  host_node_flags |= ImGuiDockNodeFlags_NoTabBar;
  host_node_flags |= ImGuiDockNodeFlags_NoDockingOverMe;
  host_node_flags |= ImGuiDockNodeFlags_NoWindowMenuButton;
  host_node_flags |= ImGuiDockNodeFlags_NoCloseButton;
  ImGuiWindowClass host_class;
  host_class.DockNodeFlagsOverrideSet = host_node_flags;
  ImGui::SetNextWindowClass(&host_class);
  ImGui::SetNextWindowDockID(central->ID, ImGuiCond_Always);

  // The host keeps its ini entry: the outer tree is pruned at boot when no
  // saved window references it.
  constexpr ImGuiWindowFlags kHostFlags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  if (ImGui::Begin(kEditorHostWindowName, nullptr, kHostFlags)) {
    editor_host_size_ = ImGui::GetContentRegionAvail();
  }
  ImGui::PopStyleVar(3);

  // Inactive editor trees stay alive so their panels keep their nodes.
  const ImGuiID active_id = ActiveEditorDockspaceId();
  for (size_t index = 1; index < kEditorTypeCount; ++index) {
    const ImGuiID id = EditorDockspaceId(static_cast<EditorType>(index));
    if (id != active_id && ImGui::DockBuilderGetNode(id)) {
      ImGui::DockSpace(id, ImVec2(0.0f, 0.0f),
                       ImGuiDockNodeFlags_KeepAliveOnly);
    }
  }
  if (active_id != 0) {
    ImGui::DockSpace(active_id, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);
  }
  ImGui::End();

  RecordPinSpot();
  EnforcePinnedPanels(FindPinNode());
}

void LayoutManager::EnforcePinnedPanels(ImGuiID pin_node) {
  if (!window_manager_) {
    return;
  }
  const ImGuiID outer_id = OuterDockspaceId();
  const size_t session_id = window_manager_->GetActiveSessionId();
  const auto find_window = [&](const std::string& panel_id) -> ImGuiWindow* {
    const WindowDescriptor* desc =
        window_manager_->GetWindowDescriptor(session_id, panel_id);
    if (!desc) {
      return nullptr;
    }
    const std::string title = window_manager_->GetWorkspaceWindowName(*desc);
    return title.empty() ? nullptr : ImGui::FindWindowByName(title.c_str());
  };

  // Docked pinned panels belong in the pin node. Floating ones stay floating.
  for (const auto& panel_id : window_manager_->GetPinnedWindows(session_id)) {
    ImGuiWindow* window = find_window(panel_id);
    if (!window || window->DockId == 0 ||
        DockRootId(window->DockId) == outer_id) {
      continue;
    }
    if (pin_node == 0) {
      pin_node = EnsurePinNode();
    }
    if (pin_node == 0) {
      break;
    }
    ImGui::DockBuilderDockWindow(window->Name, pin_node);
  }

  // An unpinned panel left in the pin node goes back to the active editor.
  const ImGuiID home_dockspace = ActiveEditorDockspaceId();
  if (home_dockspace == 0) {
    return;
  }
  // Preset regions are only valid while the live lazy state is this editor's.
  const bool preset_regions_live =
      lazy_default_dock_state_.editor_type == active_editor_type_ &&
      lazy_default_dock_state_.dockspace == home_dockspace;
  for (const auto& desc : window_manager_->GetWindowsInCategory(
           session_id, window_manager_->GetActiveCategory())) {
    if (window_manager_->IsWindowPinned(session_id, desc.card_id)) {
      continue;
    }
    ImGuiWindow* window = find_window(desc.card_id);
    if (!window || window->DockId == 0 ||
        DockRootId(window->DockId) != outer_id) {
      continue;
    }
    if (preset_regions_live &&
        DockPresetPositionOnFirstOpen(session_id, desc.card_id,
                                      /*include_default_visible=*/true,
                                      /*allow_redock=*/true)) {
      continue;
    }
    if (ImGuiDockNode* center =
            FindCentralLeaf(ImGui::DockBuilderGetNode(home_dockspace))) {
      ImGui::DockBuilderDockWindow(window->Name, center->ID);
    }
  }
}

void LayoutManager::RegisterLayoutIniHandler() {
  if (!ImGui::GetCurrentContext()) {
    return;
  }
  if (ImGuiSettingsHandler* existing =
          ImGui::FindSettingsHandler(kLayoutIniTypeName)) {
    existing->UserData = this;
    return;
  }
  ImGuiSettingsHandler handler;
  handler.TypeName = kLayoutIniTypeName;
  handler.TypeHash = ImHashStr(kLayoutIniTypeName);
  handler.ClearAllFn = LayoutIniClearAll;
  handler.ReadInitFn = LayoutIniClearAll;
  handler.ReadOpenFn = LayoutIniReadOpen;
  handler.ReadLineFn = LayoutIniReadLine;
  handler.ApplyAllFn = LayoutIniApplyAll;
  handler.WriteAllFn = LayoutIniWriteAll;
  handler.UserData = this;
  ImGui::AddSettingsHandler(&handler);
}

void LayoutManager::ResetLayoutIniSettings() {
  layout_ini_ = LayoutIniState{};
}

void LayoutManager::ReadLayoutIniLine(const char* line) {
  if (!line) {
    return;
  }
  layout_ini_.section_seen = true;
  int revision = 0;
  if (std::sscanf(line, "Revision=%d", &revision) == 1) {
    layout_ini_.revision = revision;
    return;
  }
  float pin_size = 0.0f;
  if (std::sscanf(line, "PinSize=%f", &pin_size) == 1) {
    layout_ini_.pin_size = pin_size;
    return;
  }
  constexpr size_t kPinSideKeyLength = sizeof(kLayoutIniPinSideKey) - 1;
  if (std::strncmp(line, kLayoutIniPinSideKey, kPinSideKeyLength) == 0) {
    const char* side = line + kPinSideKeyLength;
    for (size_t dir = 0; dir < kPinSideNames.size(); ++dir) {
      if (std::strcmp(side, kPinSideNames[dir]) == 0) {
        layout_ini_.pin_dir = static_cast<ImGuiDir>(dir);
      }
    }
    return;
  }
  constexpr size_t kKeyLength = sizeof(kLayoutIniInitializedKey) - 1;
  if (std::strncmp(line, kLayoutIniInitializedKey, kKeyLength) != 0) {
    return;
  }
  const char* name = line + kKeyLength;
  for (size_t index = 1; index < kEditorNames.size(); ++index) {
    if (std::strcmp(name, kEditorNames[index]) == 0) {
      layout_ini_.initialized.push_back(static_cast<EditorType>(index));
      return;
    }
  }
}

void LayoutManager::ApplyLayoutIniSettings() {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (!ctx) {
    return;
  }
  ClearInitializationFlags();
  const bool first_load = !layout_ini_backup_checked_;
  layout_ini_backup_checked_ = true;

  // The pin spot is a preference, not a tree: keep it across a tree reset.
  if (layout_ini_.pin_dir != ImGuiDir_None) {
    pin_dir_ = layout_ini_.pin_dir;
  }
  if (layout_ini_.pin_size > 0.0f) {
    pin_size_ = layout_ini_.pin_size;
  }

  if (layout_ini_.section_seen && layout_ini_.revision == kLayoutIniRevision) {
    // Editors whose saved tree survived load skip the default preset build.
    for (const EditorType type : layout_ini_.initialized) {
      if (ImGui::DockContextFindNodeByID(ctx, EditorDockspaceId(type))) {
        MarkLayoutInitialized(type);
      }
    }
  } else {
    // Older layouts docked every editor into one shared tree. Drop all dock
    // trees so each editor starts from its default preset.
    if (first_load) {
      BackupLegacyLayoutIni();
    }
    ctx->DockContext.NodesSettings.clear();
    ImGui::DockContextClearNodes(ctx, 0, true);
    LOG_INFO("LayoutManager",
             "Layout revision %d does not match %d; reset dock layouts",
             layout_ini_.revision, kLayoutIniRevision);
    // A mid-session load (reset, pre-B2 named layout) left the live editor
    // without a tree. The boot load has no windows yet and needs no rebuild.
    if (ctx->Windows.Size > 0) {
      RequestRebuild();
    }
  }
}

void LayoutManager::WriteLayoutIniSettings(ImGuiTextBuffer* buf) const {
  if (!buf) {
    return;
  }
  buf->appendf("[%s][Data]\n", kLayoutIniTypeName);
  buf->appendf("Revision=%d\n", kLayoutIniRevision);
  for (size_t index = 1; index < kEditorTypeCount; ++index) {
    const auto type = static_cast<EditorType>(index);
    if (IsLayoutInitialized(type) &&
        ImGui::DockBuilderGetNode(EditorDockspaceId(type))) {
      buf->appendf("%s%s\n", kLayoutIniInitializedKey, kEditorNames[index]);
    }
  }
  if (pin_dir_ >= 0 && static_cast<size_t>(pin_dir_) < kPinSideNames.size()) {
    buf->appendf("%s%s\n", kLayoutIniPinSideKey,
                 kPinSideNames[static_cast<size_t>(pin_dir_)]);
  }
  if (pin_size_ > 0.0f) {
    buf->appendf("PinSize=%.0f\n", pin_size_);
  }
  buf->append("\n");
}

void LayoutManager::BuildLayoutFromPreset(EditorType type,
                                          ImGuiID dockspace_id) {
  // Keep the outgoing editor's geometry; its dockspace is untouched by this
  // build. Only this editor's first-open records point at destroyed nodes.
  StashLazyDefaultDockState();
  DisableLazyDefaultDocking();
  lazy_states_by_type_.erase(type);
  for (auto& [session, by_type] : lazy_default_dock_attempts_) {
    by_type.erase(type);
  }
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
    const ImGuiDockNode* dockspace_node =
        ImGui::DockBuilderGetNode(dockspace_id);
    const float dockspace_width =
        dockspace_node ? dockspace_node->Size.x : 0.0f;
    ApplyPreferredSplitWidths(&cfg, needs, viewport_width, dockspace_width,
                              window_manager_, docked_panels);
  }
  // When compact, needs is all-false → BuildDockTree produces center-only.
  DockNodeIds ids = BuildDockTree(dockspace_id, needs, cfg);

  // Keep the live default-tree geometry even for editors that do not use lazy
  // docking. If the user returns to Dungeon/Overworld without rebuilding the
  // shared dockspace, their optional panels can still resolve valid nodes.
  auto& state = lazy_default_dock_state_;
  state.enabled = preset.dock_only_default_visible_panels;
  state.compact = is_compact;
  state.editor_type = type;
  state.session_id = session_id;
  state.dockspace = dockspace_id;
  state.center = ids.center;
  state.left = ids.left;
  state.right = ids.right;
  state.bottom = ids.bottom;
  state.top = ids.top;
  state.left_top = ids.left_top;
  state.left_bottom = ids.left_bottom;
  state.right_top = ids.right_top;
  state.right_bottom = ids.right_bottom;
  state.left_ratio = cfg.left;
  state.right_ratio = cfg.right;
  state.bottom_ratio = cfg.bottom;
  state.top_ratio = cfg.top;
  state.vertical_split = cfg.vertical_split;
  if (state.enabled) {
    state.attempted_panels = lazy_default_dock_attempts_[session_id][type];
  }

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

    // Pinned panels live in the shared pin node, never in an editor tree.
    if (window_manager_->IsWindowPinned(session_id, panel_id)) {
      if (const ImGuiID pin_node = EnsurePinNode()) {
        ImGui::DockBuilderDockWindow(window_title.c_str(), pin_node);
      }
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

bool LayoutManager::DockDefaultPositionOnFirstOpen(
    size_t session_id, const std::string& panel_id) {
  return DockPresetPositionOnFirstOpen(session_id, panel_id,
                                       /*include_default_visible=*/false);
}

bool LayoutManager::DockPresetPositionOnPanelOpen(size_t session_id,
                                                  const std::string& panel_id) {
  return DockPresetPositionOnFirstOpen(session_id, panel_id,
                                       /*include_default_visible=*/true);
}

bool LayoutManager::DockPresetPositionOnFirstOpen(size_t session_id,
                                                  const std::string& panel_id,
                                                  bool include_default_visible,
                                                  bool allow_redock) {
  auto& state = lazy_default_dock_state_;
  // allow_redock sends a panel home from the pin node. The live geometry is
  // kept for every preset, so it does not require lazy docking.
  if ((!state.enabled && !allow_redock) || !window_manager_ ||
      panel_id.empty() || session_id != state.session_id ||
      !ImGui::DockBuilderGetNode(state.dockspace)) {
    return false;
  }

  const PanelLayoutPreset preset =
      LayoutPresets::GetDefaultPreset(state.editor_type);
  const bool is_default_visible =
      std::find(preset.default_visible_panels.begin(),
                preset.default_visible_panels.end(),
                panel_id) != preset.default_visible_panels.end();
  if ((!preset.dock_only_default_visible_panels && !allow_redock) ||
      (!include_default_visible && is_default_visible)) {
    return false;
  }

  const auto position_it = preset.panel_positions.find(panel_id);
  if (position_it == preset.panel_positions.end() ||
      (!allow_redock && state.attempted_panels.contains(panel_id))) {
    return false;
  }

  const WindowDescriptor* desc =
      window_manager_->GetWindowDescriptor(session_id, panel_id);
  if (!desc) {
    return false;
  }
  const std::string window_title =
      window_manager_->GetWorkspaceWindowName(*desc);
  if (window_title.empty()) {
    return false;
  }

  // A panel restored into a valid dock node already has an intentional
  // position. Count that as its one shot without moving it.
  if (!allow_redock) {
    if (ImGuiWindow* window = ImGui::FindWindowByName(window_title.c_str());
        window && window->DockId != 0 &&
        ImGui::DockBuilderGetNode(window->DockId)) {
      MarkLazyDefaultDockAttempted(panel_id);
      return false;
    }
    if (ImGuiWindowSettings* settings =
            ImGui::FindWindowSettingsByID(ImHashStr(window_title.c_str()));
        settings && settings->DockId != 0 &&
        ImGui::DockBuilderGetNode(settings->DockId)) {
      MarkLazyDefaultDockAttempted(panel_id);
      return false;
    }
  }

  auto split_center = [&](ImGuiDir direction, float ratio,
                          ImGuiID* region) -> ImGuiID {
    if (*region != 0 && ImGui::DockBuilderGetNode(*region)) {
      return *region;
    }
    if (state.center == 0 || !ImGui::DockBuilderGetNode(state.center)) {
      return 0;
    }
    *region = ImGui::DockBuilderSplitNode(state.center, direction, ratio,
                                          nullptr, &state.center);
    return *region;
  };

  auto resolve_vertical_region = [&](bool right_side,
                                     bool want_bottom) -> ImGuiID {
    ImGuiID& region = right_side ? state.right : state.left;
    ImGuiID& top = right_side ? state.right_top : state.left_top;
    ImGuiID& bottom = right_side ? state.right_bottom : state.left_bottom;
    const ImGuiDir side_direction = right_side ? ImGuiDir_Right : ImGuiDir_Left;
    const float side_ratio = right_side ? state.right_ratio : state.left_ratio;
    if (!split_center(side_direction, side_ratio, &region)) {
      return 0;
    }

    if (want_bottom) {
      if (bottom != 0 && ImGui::DockBuilderGetNode(bottom)) {
        return bottom;
      }
      if (top == 0) {
        bottom = region;
        return bottom;
      }
      ImGuiID new_top = 0;
      bottom = ImGui::DockBuilderSplitNode(
          region, ImGuiDir_Down, state.vertical_split, nullptr, &new_top);
      top = new_top;
      return bottom;
    }

    if (top != 0 && ImGui::DockBuilderGetNode(top)) {
      return top;
    }
    if (bottom == 0) {
      top = region;
      return top;
    }
    ImGuiID new_bottom = 0;
    top = ImGui::DockBuilderSplitNode(
        region, ImGuiDir_Up, 1.0f - state.vertical_split, nullptr, &new_bottom);
    bottom = new_bottom;
    return top;
  };

  ImGuiID target = 0;
  if (state.compact) {
    target = state.center;
  } else {
    switch (position_it->second) {
      case DockPosition::Left:
      case DockPosition::LeftTop:
        target = resolve_vertical_region(false, false);
        break;
      case DockPosition::LeftBottom:
        target = resolve_vertical_region(false, true);
        break;
      case DockPosition::Right:
      case DockPosition::RightTop:
        target = resolve_vertical_region(true, false);
        break;
      case DockPosition::RightBottom:
        target = resolve_vertical_region(true, true);
        break;
      case DockPosition::Bottom:
        target = split_center(ImGuiDir_Down, state.bottom_ratio, &state.bottom);
        break;
      case DockPosition::Top:
        target = split_center(ImGuiDir_Up, state.top_ratio, &state.top);
        break;
      case DockPosition::Center:
      default:
        target = state.center;
        break;
    }
  }

  if (target == 0 || !ImGui::DockBuilderGetNode(target)) {
    return false;
  }

  MarkLazyDefaultDockAttempted(panel_id);
  ImGui::DockBuilderDockWindow(window_title.c_str(), target);
  if (WindowContent* panel = window_manager_->GetWindowContent(panel_id);
      panel && panel->PreferAutoHideTabBar()) {
    if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(target)) {
      node->LocalFlags |= ImGuiDockNodeFlags_AutoHideTabBar;
    }
  }
  ImGui::DockBuilderFinish(state.dockspace);
  return true;
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

  const auto imgui_it = saved_imgui_layouts_.find(name);
  const bool has_imgui_layout =
      imgui_it != saved_imgui_layouts_.end() && !imgui_it->second.empty();
  // Only a saved ImGui layout owns placement. Legacy visibility-only layouts
  // must leave the current lazy-default context intact.
  if (has_imgui_layout) {
    DisableLazyDefaultDocking();
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
  if (has_imgui_layout) {
    ImGui::LoadIniSettingsFromMemory(imgui_it->second.c_str(),
                                     imgui_it->second.size());
    ProtectRestoredLayoutFromDefaultInitialization();
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

  const bool has_imgui_layout = !temp_session_imgui_layout_.empty();
  if (has_imgui_layout) {
    DisableLazyDefaultDocking();
  }

  window_manager_->RestoreVisibilityState(session_id, temp_session_visibility_,
                                          /*publish_events=*/true);
  window_manager_->RestorePinnedState(temp_session_pinned_);

  if (has_imgui_layout) {
    ImGui::LoadIniSettingsFromMemory(temp_session_imgui_layout_.c_str(),
                                     temp_session_imgui_layout_.size());
    ProtectRestoredLayoutFromDefaultInitialization();
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
  const bool has_imgui_layout = !snapshot.imgui_layout.empty();
  if (has_imgui_layout) {
    DisableLazyDefaultDocking();
  }
  window_manager_->RestoreVisibilityState(session_id, snapshot.visibility,
                                          /*publish_events=*/true);
  window_manager_->RestorePinnedState(snapshot.pinned);
  if (has_imgui_layout) {
    ImGui::LoadIniSettingsFromMemory(snapshot.imgui_layout.c_str(),
                                     snapshot.imgui_layout.size());
    ProtectRestoredLayoutFromDefaultInitialization();
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
  if (!LayoutPresets::TryGetNamedPreset(matched_profile.preset_name, &preset)) {
    LOG_WARN("LayoutManager", "Unable to resolve preset '%s' for profile '%s'",
             matched_profile.preset_name.c_str(), profile_id.c_str());
    return false;
  }

  // The requested rebuild will establish a fresh lazy-default state. Avoid
  // mutating the current dock tree while profile visibility is changing.
  DisableLazyDefaultDocking();

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
  DisableLazyDefaultDocking();
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
  DisableLazyDefaultDocking();
  lazy_states_by_type_.clear();
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

void ApplyDockNodeRecursive(
    WorkspaceWindowManager* wm, size_t session_id,
    const layout_designer::DockNode& node, ImGuiID target_id,
    const std::function<void(ImGuiID, ImGuiID, ImGuiID, std::vector<ImGuiID>)>&
        queue_selection) {
  using NodeType = layout_designer::DockNode::Type;
  if (node.type == NodeType::kLeaf) {
    std::vector<ImGuiID> tab_ids;
    ImGuiID selected_id = 0;
    ImGuiID selected_window_id = 0;
    size_t index = 0;
    for (const auto& panel : node.panels) {
      const std::string title = ResolveDockWindowTitle(wm, session_id, panel);
      if (!title.empty()) {
        ImGui::DockBuilderDockWindow(title.c_str(), target_id);
        const ImGuiID window_id = ImHashStr(title.c_str());
        const ImGuiID tab_id = ImHashStr("#TAB", 0, window_id);
        tab_ids.push_back(tab_id);
        if (index == static_cast<size_t>(node.active_tab_index)) {
          selected_id = tab_id;
          selected_window_id = window_id;
        }
      }
      ++index;
    }
    if (selected_id && tab_ids.size() > 1) {
      queue_selection(target_id, selected_id, selected_window_id,
                      std::move(tab_ids));
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
    ApplyDockNodeRecursive(wm, session_id, *node.child_a, id_at_dir,
                           queue_selection);
  }
  if (node.child_b) {
    ApplyDockNodeRecursive(wm, session_id, *node.child_b, id_other,
                           queue_selection);
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

  // A custom tree owns all panel placement. Disable default first-open
  // docking before the visibility pass publishes open events.
  DisableLazyDefaultDocking();
  pending_dock_tab_selections_.clear();

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
  const size_t session_id = window_manager_->GetActiveSessionId();
  for (const auto& id : panel_ids) {
    // Phase 9 review (2026-04-25): skip already-open panels. Mirrors
    // the close-pass guard below — OpenWindowImpl publishes
    // WindowVisibilityChanged(true) and runs `on_show` regardless of
    // prior state, so a no-op Re-apply (or a startup reapply where
    // every panel is already visible) would otherwise dirty settings
    // and fire a burst of redundant show events.
    if (window_manager_->IsWindowOpen(session_id, id))
      continue;
    window_manager_->OpenWindow(session_id, id);
  }
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

  ImGui::DockBuilderSetNodeSize(dockspace_id, EditorDockspaceSize());

  ApplyDockNodeRecursive(
      window_manager_, session_id, *tree.root, dockspace_id,
      [this, session_id](ImGuiID node_id, ImGuiID selected_id,
                         ImGuiID selected_window_id,
                         std::vector<ImGuiID> tab_ids) {
        pending_dock_tab_selections_.push_back(
            {node_id, selected_id, selected_window_id, std::move(tab_ids),
             session_id, ImGui::GetFrameCount()});
      });
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
  ProtectRestoredLayoutFromDefaultInitialization();

  return absl::OkStatus();
}

void LayoutManager::ProcessPendingDockTabSelections() {
  const ImGuiContext* context = ImGui::GetCurrentContext();
  if (pending_dock_tab_selections_.empty() || !context ||
      !context->WithinFrameScope) {
    return;
  }
  const ImGuiIO& io = ImGui::GetIO();
  bool input = io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f;
  for (int button = 0; button < ImGuiMouseButton_COUNT; ++button) {
    input |= ImGui::IsMouseClicked(button);
  }
  for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
    input |= ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false);
  }
  const int frame = ImGui::GetFrameCount();
  std::erase_if(pending_dock_tab_selections_, [&](const auto& pending) {
    if (!window_manager_ ||
        window_manager_->GetActiveSessionId() != pending.session_id ||
        (frame > pending.applied_frame && input)) {
      return true;
    }
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(pending.node_id);
    if (!node) {
      return true;
    }
    if (frame <= pending.applied_frame || !node->TabBar ||
        !node->TabBar->SelectedTabId) {
      return false;
    }
    for (const ImGuiID tab_id : pending.tab_ids) {
      if (!ImGui::TabBarFindTabByID(node->TabBar, tab_id)) {
        return false;
      }
    }
    ImGuiWindow* selected = ImGui::FindWindowByID(pending.selected_window_id);
    if (!selected || selected->DockNode != node ||
        selected->TabId != pending.selected_id) {
      return false;
    }
    ImGuiWindow* focused = GImGui->NavWindow;
    if (focused && focused->RootWindow->DockNode == node &&
        focused->RootWindow != selected->RootWindow) {
      if (!GImGui->OpenPopupStack.empty()) {
        return false;
      }
      ImGui::FocusWindow(selected);
    }
    node->TabBar->NextSelectedTabId = selected->TabId;
    return true;
  });
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

  const ImGuiID target_dockspace = GetMainDockspaceId();
  if (target_dockspace == 0) {
    // No editor dockspace is active yet. Named layouts apply to one editor
    // tree, so wait for the first editor. Leave the flag clear.
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

  absl::Status apply_status = ApplyDockTree(*tree_or, target_dockspace);
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
