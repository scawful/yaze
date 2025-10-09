#include "app/editor/ui/workspace_manager.h"
#include "app/editor/system/toast_manager.h"
#include "app/rom.h"
#include "absl/strings/str_format.h"
#include "util/file_util.h"
#include "util/platform_paths.h"

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
  if (name.empty()) return;
  std::string ini_name = absl::StrFormat("yaze_workspace_%s.ini", name.c_str());
  ImGui::SaveIniSettingsToDisk(ini_name.c_str());

  if (!workspace_presets_loaded_) {
    // RefreshWorkspacePresets(); // This will be implemented next
  }

  if (std::find(workspace_presets_.begin(), workspace_presets_.end(), name) ==
      workspace_presets_.end()) {
    workspace_presets_.emplace_back(name);
    try {
      std::ostringstream ss;
      for (const auto& n : workspace_presets_)
        ss << n << "\n";
      // This should use a platform-agnostic path
      util::SaveFile("workspace_presets.txt", ss.str());
    } catch (const std::exception& e) {
      // LOG_WARN("WorkspaceManager", "Failed to save presets: %s", e.what());
    }
  }
  last_workspace_preset_ = name;
  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Preset '%s' saved", name), 
                        ToastType::kSuccess);
  }
}

void WorkspaceManager::LoadWorkspacePreset(const std::string& name) {
  if (name.empty()) return;
  std::string ini_name = absl::StrFormat("yaze_workspace_%s.ini", name.c_str());
  ImGui::LoadIniSettingsFromDisk(ini_name.c_str());
  last_workspace_preset_ = name;
  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Preset '%s' loaded", name), 
                        ToastType::kSuccess);
  }
}

void WorkspaceManager::RefreshPresets() {
  try {
    std::vector<std::string> new_presets;
    auto config_dir = util::PlatformPaths::GetConfigDirectory();
    if (config_dir.ok()) {
      std::string presets_path = (*config_dir / "workspace_presets.txt").string();
      auto data = util::LoadFile(presets_path);
      if (!data.empty()) {
        std::istringstream ss(data);
        std::string name;
        while (std::getline(ss, name)) {
          name.erase(0, name.find_first_not_of(" \t\r\n"));
          name.erase(name.find_last_not_of(" \t\r\n") + 1);
          if (!name.empty() && name.length() < 256) {
            new_presets.emplace_back(std::move(name));
          }
        }
      }
    }
    workspace_presets_ = std::move(new_presets);
    workspace_presets_loaded_ = true;
  } catch (const std::exception& e) {
    // LOG_ERROR("WorkspaceManager", "Error refreshing presets: %s", e.what());
    workspace_presets_.clear();
    workspace_presets_loaded_ = true;
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
