#ifndef YAZE_APP_EDITOR_LAYOUT_WINDOW_DELEGATE_H_
#define YAZE_APP_EDITOR_LAYOUT_WINDOW_DELEGATE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @class WindowDelegate
 * @brief Low-level window operations with minimal dependencies
 *
 * Provides window management functionality extracted from EditorManager:
 * - Window visibility management
 * - Docking operations
 * - Layout persistence
 * - Focus management
 *
 * This class has minimal dependencies (only ImGui and absl) to avoid
 * linker issues and circular dependencies.
 */
class WindowDelegate {
 public:
  WindowDelegate() = default;
  ~WindowDelegate() = default;

  // Window visibility management
  void ShowAllWindows();
  void HideAllWindows();
  void ShowWindow(const std::string& window_id);
  void HideWindow(const std::string& window_id);
  void ToggleWindow(const std::string& window_id);
  bool IsWindowVisible(const std::string& window_id) const;

  // Focus and positioning
  void FocusWindow(const std::string& window_id);
  void MaximizeWindow(const std::string& window_id);
  void RestoreWindow(const std::string& window_id);
  void CenterWindow(const std::string& window_id);

  // Docking operations
  void DockWindow(const std::string& window_id, ImGuiDir dock_direction);
  void UndockWindow(const std::string& window_id);
  void SetDockSpace(const std::string& dock_space_id,
                    const ImVec2& size = ImVec2(0, 0));

  // Layout management
  absl::Status SaveLayout(const std::string& preset_name);
  absl::Status LoadLayout(const std::string& preset_name);
  absl::Status ResetLayout();
  std::vector<std::string> GetAvailableLayouts() const;

  // Workspace-specific layout methods (match EditorManager API)
  void SaveWorkspaceLayout();
  void LoadWorkspaceLayout();
  void ResetWorkspaceLayout();

  // Layout presets
  void LoadDeveloperLayout();
  void LoadDesignerLayout();
  void LoadModderLayout();

  // Window state queries
  std::vector<std::string> GetVisibleWindows() const;
  std::vector<std::string> GetHiddenWindows() const;
  ImVec2 GetWindowSize(const std::string& window_id) const;
  ImVec2 GetWindowPosition(const std::string& window_id) const;

  // Batch operations
  void ShowWindowsInCategory(const std::string& category);
  void HideWindowsInCategory(const std::string& category);
  void ShowOnlyWindow(const std::string& window_id);  // Hide all others

  // Window registration (for tracking)
  void RegisterWindow(const std::string& window_id,
                      const std::string& category = "");
  void UnregisterWindow(const std::string& window_id);

  // Layout presets
  void LoadMinimalLayout();

 private:
  // Window registry for tracking
  struct WindowInfo {
    std::string id;
    std::string category;
    bool is_registered = false;
  };

  std::unordered_map<std::string, WindowInfo> registered_windows_;

  // Helper methods
  bool IsWindowRegistered(const std::string& window_id) const;
  std::string GetLayoutFilePath(const std::string& preset_name) const;
  void ApplyLayoutToWindow(const std::string& window_id,
                           const std::string& layout_data);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_WINDOW_DELEGATE_H_

