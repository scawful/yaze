#include "app/editor/layout/layout_coordinator.h"

#include <algorithm>
#include <filesystem>

#include "absl/strings/str_format.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/shell/coordinator/ui_coordinator.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/system/editor_registry.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace editor {

void LayoutCoordinator::Initialize(const Dependencies& deps) {
  layout_manager_ = deps.layout_manager;
  window_manager_ = deps.window_manager;
  ui_coordinator_ = deps.ui_coordinator;
  toast_manager_ = deps.toast_manager;
  status_bar_ = deps.status_bar;
  right_drawer_manager_ = deps.right_drawer_manager;
}

// ==========================================================================
// Layout Offset Calculations
// ==========================================================================

LayoutCoordinator::WorkspaceChromeBudget
LayoutCoordinator::ResolveWorkspaceChromeBudget(
    float viewport_width, float activity_bar_width, float left_side_panel_width,
    float right_sidebar_width, float right_sidebar_budget_width,
    float min_canvas_width) {
  const float activity = std::max(0.0f, activity_bar_width);
  const float left_panel = std::max(0.0f, left_side_panel_width);
  const float right_panel = std::max(0.0f, right_sidebar_width);
  const float right_budget = right_sidebar_budget_width >= 0.0f
                                 ? right_sidebar_budget_width
                                 : right_panel;
  const float desired_left = activity + left_panel;

  if (viewport_width > 0.0f && viewport_width - desired_left - right_budget <
                                   std::max(0.0f, min_canvas_width)) {
    return {
        .left_offset = activity,
        .right_offset = 0.0f,
        .side_panels_overlay = left_panel > 0.0f || right_panel > 0.0f,
    };
  }

  return {
      .left_offset = desired_left,
      .right_offset = right_panel,
      .side_panels_overlay = false,
  };
}

LayoutCoordinator::WorkspaceChromeBudget
LayoutCoordinator::GetWorkspaceChromeBudget() const {
  float viewport_width = 0.0f;
  if (ImGui::GetCurrentContext()) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    viewport_width = viewport ? viewport->WorkSize.x : 0.0f;
  }

  float activity_width = 0.0f;
  float left_panel_width = 0.0f;
  const bool show_left_chrome =
      ui_coordinator_ && ui_coordinator_->IsPanelSidebarVisible() &&
      ui_coordinator_->ShouldShowActivityBar() && window_manager_ &&
      window_manager_->IsSidebarVisible();
  if (show_left_chrome) {
    activity_width = WorkspaceWindowManager::GetSidebarWidth();
    if (window_manager_->IsSidebarExpanded()) {
      left_panel_width =
          window_manager_->GetActiveSidePanelWidth(viewport_width);
    }
  }

  const float right_sidebar_width =
      right_drawer_manager_ ? right_drawer_manager_->GetDrawerWidth() : 0.0f;
  const float right_sidebar_budget_width =
      right_drawer_manager_ ? right_drawer_manager_->GetExpandedDrawerWidth()
                            : 0.0f;
  return ResolveWorkspaceChromeBudget(viewport_width, activity_width,
                                      left_panel_width, right_sidebar_width,
                                      right_sidebar_budget_width);
}

float LayoutCoordinator::GetLeftLayoutOffset() const {
  return GetWorkspaceChromeBudget().left_offset;
}

float LayoutCoordinator::GetRightLayoutOffset() const {
  return GetWorkspaceChromeBudget().right_offset;
}

float LayoutCoordinator::GetBottomLayoutOffset() const {
  return (status_bar_ && status_bar_->IsEnabled()) ? status_bar_->GetHeight()
                                                   : 0.0f;
}

// ==========================================================================
// Layout Operations
// ==========================================================================

void LayoutCoordinator::ResetWorkspaceLayout() {
  if (!layout_manager_) {
    return;
  }

  if (ImGui::GetCurrentContext()) {
    static const char kEmptyIni[] = "\n";
    ImGui::LoadIniSettingsFromMemory(kEmptyIni, sizeof(kEmptyIni) - 1);
  }

  if (auto ini_path = util::PlatformPaths::GetImGuiIniPath(); ini_path.ok()) {
    std::error_code ec;
    std::filesystem::remove(*ini_path, ec);
    if (ec) {
      LOG_WARN("LayoutCoordinator", "Failed to remove ImGui ini: %s",
               ec.message().c_str());
    }
  }

  layout_manager_->ClearInitializationFlags();
  layout_manager_->RequestRebuild();

  // Force immediate rebuild for active context
  ImGuiContext* imgui_ctx = ImGui::GetCurrentContext();
  if (imgui_ctx && imgui_ctx->WithinFrameScope) {
    const ImGuiID cached_dockspace = layout_manager_->GetMainDockspaceId();
    const ImGuiID dockspace_id = cached_dockspace != 0
                                     ? cached_dockspace
                                     : ImGui::GetID("MainDockSpace");

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

bool LayoutCoordinator::ApplyLayoutPreset(const std::string& preset_name,
                                          size_t session_id,
                                          EditorType editor_type) {
  if (!window_manager_) {
    return false;
  }

  auto preset = LayoutPresets::GetNamedPreset(preset_name);
  if (!preset.has_value()) {
    LOG_WARN("LayoutCoordinator", "Unknown layout preset: %s",
             preset_name.c_str());
    if (toast_manager_) {
      toast_manager_->Show(absl::StrFormat("Unknown preset: %s", preset_name),
                           ToastType::kWarning);
    }
    return false;
  }

  if (editor_type == EditorType::kUnknown) {
    editor_type = EditorRegistry::GetEditorTypeFromCategory(
        window_manager_->GetActiveCategory());
  }
  if (editor_type == EditorType::kUnknown) {
    editor_type = preset->editor_type;
  }
  if (!layout_manager_ || !layout_manager_->ApplyNamedPreset(
                              preset_name, session_id, editor_type)) {
    return false;
  }

  LOG_INFO("LayoutCoordinator", "Applied layout preset: %s",
           preset_name.c_str());
  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Layout: %s", preset_name),
                         ToastType::kSuccess);
  }
  return true;
}

std::string LayoutCoordinator::DefaultLayoutBackupName(EditorType type) {
  return "Before default: " + EditorRegistry::GetEditorCategory(type);
}

bool LayoutCoordinator::HasPreviousEditorLayout(EditorType type) const {
  return layout_manager_ &&
         layout_manager_->HasLayout(DefaultLayoutBackupName(type));
}

bool LayoutCoordinator::RestorePreviousEditorLayout(EditorType type) {
  if (!HasPreviousEditorLayout(type))
    return false;
  const std::string backup = DefaultLayoutBackupName(type);
  layout_manager_->LoadLayout(backup);
  layout_manager_->ClearRebuildRequest();
  layout_manager_->DeleteLayout(backup);
  if (toast_manager_) {
    toast_manager_->Show("Previous layout restored", ToastType::kSuccess);
  }
  return true;
}

void LayoutCoordinator::ResetCurrentEditorLayout(EditorType editor_type,
                                                 size_t session_id) {
  if (!window_manager_) {
    if (toast_manager_) {
      toast_manager_->Show("No active editor to reset", ToastType::kWarning);
    }
    return;
  }

  // Keep the first pre-reset workspace until it is explicitly restored.
  // Repeated default-layout tests must not overwrite the user's recovery copy.
  if (layout_manager_) {
    const std::string backup = DefaultLayoutBackupName(editor_type);
    if (!layout_manager_->HasLayout(backup)) {
      layout_manager_->SaveCurrentLayout(backup);
    }
  }

  // Reset panels to defaults
  window_manager_->ResetToDefaults(session_id, editor_type);

  // Rebuild dock layout for this editor on next frame
  if (layout_manager_) {
    layout_manager_->ResetToDefaultLayout(editor_type);
    layout_manager_->RequestRebuild();
  }

  LOG_INFO("LayoutCoordinator", "Reset editor layout to defaults for type %d",
           static_cast<int>(editor_type));
  if (toast_manager_) {
    toast_manager_->Show("Default layout applied; previous layout saved",
                         ToastType::kSuccess);
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

  const ImGuiID cached_dockspace = layout_manager_->GetMainDockspaceId();
  const ImGuiID dockspace_id =
      cached_dockspace != 0 ? cached_dockspace : ImGui::GetID("MainDockSpace");

  // A role can queue its rebuild before the controller creates the host.
  // Keep the request until that dockspace exists, rather than dropping it.
  if (!ImGui::DockBuilderGetNode(dockspace_id)) {
    return;
  }

  // Determine which editor layout to rebuild
  EditorType rebuild_type = layout_manager_->RequestedRebuildEditor();
  if (rebuild_type != EditorType::kUnknown) {
    // An explicit named preset owns its editor even when it includes debug
    // windows from the Emulator category.
  } else if (is_emulator_visible) {
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
      const ImGuiID cached_dockspace = layout_manager_->GetMainDockspaceId();
      const ImGuiID dockspace_id = cached_dockspace != 0
                                       ? cached_dockspace
                                       : ImGui::GetID("MainDockSpace");
      layout_manager_->InitializeEditorLayout(type, dockspace_id);
    }
  });
}

void LayoutCoordinator::QueueDeferredAction(std::function<void()> action) {
  deferred_actions_.push_back(std::move(action));
  pending_deferred_actions_.fetch_add(1, std::memory_order_relaxed);
}

void LayoutCoordinator::ProcessDeferredActions() {
  if (deferred_actions_.empty()) {
    return;
  }

  std::vector<std::function<void()>> actions_to_execute;
  actions_to_execute.swap(deferred_actions_);
  const int processed_count = static_cast<int>(actions_to_execute.size());

  for (auto& action : actions_to_execute) {
    action();
  }

  if (processed_count > 0) {
    const int remaining = pending_deferred_actions_.fetch_sub(
                              processed_count, std::memory_order_relaxed) -
                          processed_count;
    if (remaining < 0) {
      pending_deferred_actions_.store(0, std::memory_order_relaxed);
    }
  }
}

}  // namespace editor
}  // namespace yaze
