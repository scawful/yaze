#include "app/editor/ui/workspace_manager.h"
#include "app/editor/system/toast_manager.h"
#include "app/rom.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace editor {

absl::Status WorkspaceManager::SaveWorkspaceLayout(const std::string& name) {
  // TODO: Serialize ImGui docking layout
  if (toast_manager_) {
    toast_manager_->Show("Layout saved", ToastType::kSuccess);
  }
  return absl::OkStatus();
}

absl::Status WorkspaceManager::LoadWorkspaceLayout(const std::string& name) {
  // TODO: Deserialize ImGui docking layout
  if (toast_manager_) {
    toast_manager_->Show("Layout loaded", ToastType::kSuccess);
  }
  return absl::OkStatus();
}

absl::Status WorkspaceManager::ResetWorkspaceLayout() {
  // TODO: Reset to default layout
  if (toast_manager_) {
    toast_manager_->Show("Layout reset to default", ToastType::kInfo);
  }
  return absl::OkStatus();
}

void WorkspaceManager::SaveWorkspacePreset(const std::string& name) {
  last_workspace_preset_ = name;
  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Preset '%s' saved", name), 
                        ToastType::kSuccess);
  }
}

void WorkspaceManager::LoadWorkspacePreset(const std::string& name) {
  last_workspace_preset_ = name;
  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Preset '%s' loaded", name), 
                        ToastType::kSuccess);
  }
}

void WorkspaceManager::LoadDeveloperLayout() {
  // TODO: Load preset with all debug tools
  if (toast_manager_) {
    toast_manager_->Show("Developer layout loaded", ToastType::kInfo);
  }
}

void WorkspaceManager::LoadDesignerLayout() {
  // TODO: Load preset focused on graphics
  if (toast_manager_) {
    toast_manager_->Show("Designer layout loaded", ToastType::kInfo);
  }
}

void WorkspaceManager::LoadModderLayout() {
  // TODO: Load preset for ROM hacking
  if (toast_manager_) {
    toast_manager_->Show("Modder layout loaded", ToastType::kInfo);
  }
}

void WorkspaceManager::ShowAllWindows() {
  // TODO: Set all editor windows to visible
}

void WorkspaceManager::HideAllWindows() {
  // TODO: Hide all editor windows
}

void WorkspaceManager::MaximizeCurrentWindow() {
  // TODO: Maximize focused window
}

void WorkspaceManager::RestoreAllWindows() {
  // TODO: Restore all windows to default size
}

void WorkspaceManager::CloseAllFloatingWindows() {
  // TODO: Close undocked windows
}

size_t WorkspaceManager::GetActiveSessionCount() const {
  if (!sessions_) return 0;
  
  size_t count = 0;
  for (const auto& session : *sessions_) {
    if (session.rom && session.rom->is_loaded()) {
      count++;
    }
  }
  return count;
}

bool WorkspaceManager::HasDuplicateSession(const std::string& filepath) const {
  if (!sessions_) return false;
  
  for (const auto& session : *sessions_) {
    if (session.filepath == filepath && session.rom && session.rom->is_loaded()) {
      return true;
    }
  }
  return false;
}

}  // namespace editor
}  // namespace yaze
