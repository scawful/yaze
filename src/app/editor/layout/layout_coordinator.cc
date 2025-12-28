#include "app/editor/layout/layout_coordinator.h"

#include "absl/strings/str_format.h"
#include "app/editor/menu/right_panel_manager.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/ui/ui_coordinator.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"

namespace yaze {
namespace editor {

void LayoutCoordinator::Initialize(const Dependencies& deps) {
  layout_manager_ = deps.layout_manager;
  panel_manager_ = deps.panel_manager;
  ui_coordinator_ = deps.ui_coordinator;
  toast_manager_ = deps.toast_manager;
  status_bar_ = deps.status_bar;
  right_panel_manager_ = deps.right_panel_manager;
}

// ==========================================================================
// Layout Offset Calculations
// ==========================================================================

float LayoutCoordinator::GetLeftLayoutOffset() const {
  // Global UI toggle override
  if (!ui_coordinator_ || !ui_coordinator_->IsPanelSidebarVisible()) {
    return 0.0f;
  }

  // Check startup surface state - Activity Bar hidden on cold start
  if (!ui_coordinator_->ShouldShowActivityBar()) {
    return 0.0f;
  }

  // Check Activity Bar visibility
  if (!panel_manager_ || !panel_manager_->IsSidebarVisible()) {
    return 0.0f;
  }

  // Base width = Activity Bar
  float width = PanelManager::GetSidebarWidth();  // 48px

  // Add Side Panel width if expanded
  if (panel_manager_->IsPanelExpanded()) {
    float viewport_width = 0.0f;
    if (ImGui::GetCurrentContext()) {
      const ImGuiViewport* viewport = ImGui::GetMainViewport();
      viewport_width = viewport ? viewport->WorkSize.x : 0.0f;
    }
    width += PanelManager::GetSidePanelWidthForViewport(viewport_width);
  }

  return width;
}

float LayoutCoordinator::GetRightLayoutOffset() const {
  return right_panel_manager_ ? right_panel_manager_->GetPanelWidth() : 0.0f;
}

float LayoutCoordinator::GetBottomLayoutOffset() const {
  return status_bar_ ? status_bar_->GetHeight() : 0.0f;
}

// ==========================================================================
// Layout Operations
// ==========================================================================

void LayoutCoordinator::ResetWorkspaceLayout() {
  if (!layout_manager_) {
    return;
  }

  layout_manager_->ClearInitializationFlags();
  layout_manager_->RequestRebuild();

  // Force immediate rebuild for active context
  ImGuiContext* imgui_ctx = ImGui::GetCurrentContext();
  if (imgui_ctx && imgui_ctx->WithinFrameScope) {
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

    // Determine which layout to rebuild based on emulator visibility
    if (ui_coordinator_ && ui_coordinator_->IsEmulatorVisible()) {
      layout_manager_->RebuildLayout(EditorType::kEmulator, dockspace_id);
      LOG_INFO("LayoutCoordinator", "Emulator layout reset complete");
    }
    // Note: Current editor check would need to be passed in or stored
  } else {
    // Not in frame scope - rebuild will happen on next tick via RequestRebuild()
    LOG_INFO("LayoutCoordinator", "Layout reset queued for next frame");
  }
}

void LayoutCoordinator::ApplyLayoutPreset(const std::string& preset_name,
                                          size_t session_id) {
  if (!panel_manager_) {
    return;
  }

  PanelLayoutPreset preset;

  // Get the preset by name
  if (preset_name == "Minimal") {
    preset = LayoutPresets::GetMinimalPreset();
  } else if (preset_name == "Developer") {
    preset = LayoutPresets::GetDeveloperPreset();
  } else if (preset_name == "Designer") {
    preset = LayoutPresets::GetDesignerPreset();
  } else if (preset_name == "Modder") {
    preset = LayoutPresets::GetModderPreset();
  } else if (preset_name == "Overworld Expert") {
    preset = LayoutPresets::GetOverworldExpertPreset();
  } else if (preset_name == "Dungeon Expert") {
    preset = LayoutPresets::GetDungeonExpertPreset();
  } else if (preset_name == "Testing") {
    preset = LayoutPresets::GetTestingPreset();
  } else if (preset_name == "Audio") {
    preset = LayoutPresets::GetAudioPreset();
  } else {
    LOG_WARN("LayoutCoordinator", "Unknown layout preset: %s",
             preset_name.c_str());
    if (toast_manager_) {
      toast_manager_->Show(absl::StrFormat("Unknown preset: %s", preset_name),
                           ToastType::kWarning);
    }
    return;
  }

  // Hide all panels first
  panel_manager_->HideAll();

  // Show only the panels defined in the preset
  for (const auto& panel_id : preset.default_visible_panels) {
    panel_manager_->ShowPanel(session_id, panel_id);
  }

  // Request a dock rebuild so the preset positions are applied
  if (layout_manager_) {
    layout_manager_->RequestRebuild();
  }

  LOG_INFO("LayoutCoordinator", "Applied layout preset: %s",
           preset_name.c_str());
  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Layout: %s", preset_name),
                         ToastType::kSuccess);
  }
}

void LayoutCoordinator::ResetCurrentEditorLayout(EditorType editor_type,
                                                 size_t session_id) {
  if (!panel_manager_) {
    if (toast_manager_) {
      toast_manager_->Show("No active editor to reset", ToastType::kWarning);
    }
    return;
  }

  // Get the default preset for the current editor
  auto preset = LayoutPresets::GetDefaultPreset(editor_type);

  // Reset panels to defaults
  panel_manager_->ResetToDefaults(session_id, editor_type);

  // Rebuild dock layout for this editor on next frame
  if (layout_manager_) {
    layout_manager_->ResetToDefaultLayout(editor_type);
    layout_manager_->RequestRebuild();
  }

  LOG_INFO("LayoutCoordinator", "Reset editor layout to defaults for type %d",
           static_cast<int>(editor_type));
  if (toast_manager_) {
    toast_manager_->Show("Layout reset to defaults", ToastType::kSuccess);
  }
}

// ==========================================================================
// Layout Rebuild Handling
// ==========================================================================

void LayoutCoordinator::ProcessLayoutRebuild(EditorType current_editor_type,
                                             bool is_emulator_visible) {
  if (!layout_manager_ || !layout_manager_->IsRebuildRequested()) {
    return;
  }

  // Only rebuild if we're in a valid ImGui frame with dockspace
  ImGuiContext* imgui_ctx = ImGui::GetCurrentContext();
  if (!imgui_ctx || !imgui_ctx->WithinFrameScope) {
    return;
  }

  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

  // Determine which editor layout to rebuild
  EditorType rebuild_type = EditorType::kUnknown;
  if (is_emulator_visible) {
    rebuild_type = EditorType::kEmulator;
  } else if (current_editor_type != EditorType::kUnknown) {
    rebuild_type = current_editor_type;
  }

  // Execute rebuild if we have a valid editor type
  if (rebuild_type != EditorType::kUnknown) {
    layout_manager_->RebuildLayout(rebuild_type, dockspace_id);
    LOG_INFO("LayoutCoordinator", "Layout rebuilt for editor type %d",
             static_cast<int>(rebuild_type));
  }

  layout_manager_->ClearRebuildRequest();
}

void LayoutCoordinator::InitializeEditorLayout(EditorType type) {
  if (!layout_manager_) {
    return;
  }

  if (layout_manager_->IsLayoutInitialized(type)) {
    return;
  }

  // Defer layout initialization to ensure we are in the correct scope
  QueueDeferredAction([this, type]() {
    if (layout_manager_ && !layout_manager_->IsLayoutInitialized(type)) {
      ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
      layout_manager_->InitializeEditorLayout(type, dockspace_id);
    }
  });
}

void LayoutCoordinator::QueueDeferredAction(std::function<void()> action) {
  deferred_actions_.push_back(std::move(action));
}

void LayoutCoordinator::ProcessDeferredActions() {
  if (deferred_actions_.empty()) {
    return;
  }

  std::vector<std::function<void()>> actions_to_execute;
  actions_to_execute.swap(deferred_actions_);

  for (auto& action : actions_to_execute) {
    action();
  }
}

}  // namespace editor
}  // namespace yaze
