#define IMGUI_DEFINE_MATH_OPERATORS

#include "app/editor/ui/workspace_manager.h"

#include "absl/strings/str_format.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "rom/rom.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
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
  if (name.empty())
    return;
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
  if (name.empty())
    return;
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
      std::string presets_path =
          (*config_dir / "workspace_presets.txt").string();
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
  if (panel_manager_) {
    panel_manager_->ShowAll();
  }
  if (toast_manager_) {
    toast_manager_->Show("All windows shown", ToastType::kInfo);
  }
}

void WorkspaceManager::HideAllWindows() {
  if (panel_manager_) {
    panel_manager_->HideAll();
  }
  if (toast_manager_) {
    toast_manager_->Show("All windows hidden", ToastType::kInfo);
  }
}

void WorkspaceManager::MaximizeCurrentWindow() {
  // Use ImGui internal API to maximize current window
  ImGuiWindow* window = ImGui::GetCurrentWindowRead();
  if (window && window->DockNode) {
    ImGuiID central_node_id =
        ImGui::DockBuilderGetCentralNode(ImGui::GetID("MainDockSpace"))->ID;
    ImGui::DockBuilderDockWindow(window->Name, central_node_id);
  }
  if (toast_manager_) {
    toast_manager_->Show("Window maximized", ToastType::kInfo);
  }
}

void WorkspaceManager::RestoreAllWindows() {
  // Reset all window sizes - ImGui will auto-restore based on docking
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx) {
    for (ImGuiWindow* window : ctx->Windows) {
      if (window && !window->Collapsed) {
        ImGui::SetWindowSize(window->Name, ImVec2(0, 0));  // Auto-size
      }
    }
  }
  if (toast_manager_) {
    toast_manager_->Show("All windows restored", ToastType::kInfo);
  }
}

void WorkspaceManager::CloseAllFloatingWindows() {
  // Close all windows that are not docked
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx) {
    for (ImGuiWindow* window : ctx->Windows) {
      if (window && !window->DockNode && !window->Collapsed) {
        window->Hidden = true;
      }
    }
  }
  if (toast_manager_) {
    toast_manager_->Show("Floating windows closed", ToastType::kInfo);
  }
}

size_t WorkspaceManager::GetActiveSessionCount() const {
  if (!sessions_)
    return 0;

  size_t count = 0;
  for (const auto& session : *sessions_) {
    if (session.rom && session.rom->is_loaded()) {
      count++;
    }
  }
  return count;
}

bool WorkspaceManager::HasDuplicateSession(const std::string& filepath) const {
  if (!sessions_)
    return false;

  for (const auto& session : *sessions_) {
    if (session.filepath == filepath && session.rom &&
        session.rom->is_loaded()) {
      return true;
    }
  }
  return false;
}

// Window navigation operations
void WorkspaceManager::FocusNextWindow() {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx && ctx->NavWindow) {
    ImGui::FocusWindow(ImGui::FindWindowByName(ctx->NavWindow->Name));
  }
  // TODO: Implement proper window cycling
}

void WorkspaceManager::FocusPreviousWindow() {
  // TODO: Implement window cycling backward
}

void WorkspaceManager::SplitWindowHorizontal() {
  ImGuiWindow* window = ImGui::GetCurrentWindowRead();
  if (window && window->DockNode) {
    ImGuiID node_id = window->DockNode->ID;
    ImGuiID out_id_at_dir = 0;
    ImGuiID out_id_at_opposite_dir = 0;
    ImGui::DockBuilderSplitNode(node_id, ImGuiDir_Down, 0.5f, &out_id_at_dir,
                                &out_id_at_opposite_dir);
  }
}

void WorkspaceManager::SplitWindowVertical() {
  ImGuiWindow* window = ImGui::GetCurrentWindowRead();
  if (window && window->DockNode) {
    ImGuiID node_id = window->DockNode->ID;
    ImGuiID out_id_at_dir = 0;
    ImGuiID out_id_at_opposite_dir = 0;
    ImGui::DockBuilderSplitNode(node_id, ImGuiDir_Right, 0.5f, &out_id_at_dir,
                                &out_id_at_opposite_dir);
  }
}

void WorkspaceManager::CloseCurrentWindow() {
  ImGuiWindow* window = ImGui::GetCurrentWindowRead();
  if (window) {
    window->Hidden = true;
  }
}

// Command execution for WhichKey integration
void WorkspaceManager::ExecuteWorkspaceCommand(const std::string& command_id) {
  // Window commands (Space + w)
  if (command_id == "w.s") {
    ShowAllWindows();
  } else if (command_id == "w.h") {
    HideAllWindows();
  } else if (command_id == "w.m") {
    MaximizeCurrentWindow();
  } else if (command_id == "w.r") {
    RestoreAllWindows();
  } else if (command_id == "w.c") {
    CloseCurrentWindow();
  } else if (command_id == "w.f") {
    CloseAllFloatingWindows();
  } else if (command_id == "w.v") {
    SplitWindowVertical();
  } else if (command_id == "w.H") {
    SplitWindowHorizontal();
  }

  // Layout commands (Space + l)
  else if (command_id == "l.s") {
    SaveWorkspaceLayout();
  } else if (command_id == "l.l") {
    LoadWorkspaceLayout();
  } else if (command_id == "l.r") {
    ResetWorkspaceLayout();
  } else if (command_id == "l.d") {
    LoadDeveloperLayout();
  } else if (command_id == "l.g") {
    LoadDesignerLayout();
  } else if (command_id == "l.m") {
    LoadModderLayout();
  }

  // Unknown command
  else if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Unknown command: %s", command_id),
                         ToastType::kWarning);
  }
}

}  // namespace editor
}  // namespace yaze
