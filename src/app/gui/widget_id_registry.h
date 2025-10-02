#ifndef YAZE_APP_GUI_WIDGET_ID_REGISTRY_H_
#define YAZE_APP_GUI_WIDGET_ID_REGISTRY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @class WidgetIdScope
 * @brief RAII helper for managing hierarchical ImGui ID scopes
 *
 * This class automatically pushes/pops ImGui IDs and maintains a thread-local
 * stack for building hierarchical widget paths. Use this to create predictable,
 * stable widget IDs for test automation.
 *
 * Example:
 *   {
 *     WidgetIdScope editor("Overworld");
 *     // Widgets here have "Overworld/" prefix
 *     {
 *       WidgetIdScope section("Canvas");
 *       // Widgets here have "Overworld/Canvas/" prefix
 *       ImGui::BeginChild("Map", ...);  // Full path: Overworld/Canvas/Map
 *     }
 *   }
 */
class WidgetIdScope {
 public:
  explicit WidgetIdScope(const std::string& name);
  ~WidgetIdScope();

  // Get the full hierarchical path at this scope level
  std::string GetFullPath() const;

  // Get the full path with a widget suffix
  std::string GetWidgetPath(const std::string& widget_type,
                            const std::string& widget_name) const;

 private:
  std::string name_;
  static thread_local std::vector<std::string> id_stack_;
};

/**
 * @class WidgetIdRegistry
 * @brief Centralized registry for discoverable GUI widgets
 *
 * This singleton maintains a catalog of all registered widgets in the
 * application, enabling test automation and AI-driven GUI interaction.
 * Widgets are identified by hierarchical paths like:
 *   "Overworld/Main/Toolset/button:DrawTile"
 *
 * The registry provides:
 * - Widget discovery by pattern matching
 * - Mapping widget paths to ImGui IDs
 * - Export to machine-readable formats for z3ed agent
 */
class WidgetIdRegistry {
 public:
  struct WidgetInfo {
    std::string full_path;   // e.g. "Overworld/Canvas/canvas:Map"
    std::string type;        // e.g. "button", "input", "canvas", "table"
    ImGuiID imgui_id;        // ImGui's internal ID
    std::string description; // Optional human-readable description
  };

  static WidgetIdRegistry& Instance();

  // Register a widget for discovery
  // Should be called after widget is created (when ImGui::GetItemID() is valid)
  void RegisterWidget(const std::string& full_path, const std::string& type,
                      ImGuiID imgui_id,
                      const std::string& description = "");

  // Query widgets for test automation
  std::vector<std::string> FindWidgets(const std::string& pattern) const;
  ImGuiID GetWidgetId(const std::string& full_path) const;
  const WidgetInfo* GetWidgetInfo(const std::string& full_path) const;

  // Get all registered widgets
  const std::unordered_map<std::string, WidgetInfo>& GetAllWidgets() const {
    return widgets_;
  }

  // Clear all registered widgets (useful between frames for dynamic UIs)
  void Clear();

  // Export catalog for z3ed agent describe
  // Format: "yaml" or "json"
  std::string ExportCatalog(const std::string& format = "yaml") const;
  void ExportCatalogToFile(const std::string& output_file,
                           const std::string& format = "yaml") const;

 private:
  WidgetIdRegistry() = default;
  std::unordered_map<std::string, WidgetInfo> widgets_;
};

// RAII helper macros for convenient scoping
#define YAZE_WIDGET_SCOPE(name) yaze::gui::WidgetIdScope _yaze_scope_##__LINE__(name)

// Register a widget after creation (when GetItemID() is valid)
#define YAZE_REGISTER_WIDGET(widget_type, widget_name)                        \
  do {                                                                         \
    if (ImGui::GetItemID() != 0) {                                            \
      yaze::gui::WidgetIdRegistry::Instance().RegisterWidget(                 \
          _yaze_scope_##__LINE__.GetWidgetPath(#widget_type, widget_name),   \
          #widget_type, ImGui::GetItemID());                                  \
    }                                                                          \
  } while (0)

// Convenience macro for registering with automatic name extraction
// Usage: YAZE_REGISTER_CURRENT_WIDGET("button")
#define YAZE_REGISTER_CURRENT_WIDGET(widget_type)                             \
  do {                                                                         \
    if (ImGui::GetItemID() != 0) {                                            \
      yaze::gui::WidgetIdRegistry::Instance().RegisterWidget(                 \
          _yaze_scope_##__LINE__.GetWidgetPath(widget_type,                  \
                                                ImGui::GetLastItemLabel()),   \
          widget_type, ImGui::GetItemID());                                   \
    }                                                                          \
  } while (0)

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGET_ID_REGISTRY_H_
