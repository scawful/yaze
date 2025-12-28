#ifndef YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_DESIGNER_WINDOW_H_
#define YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_DESIGNER_WINDOW_H_

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "lab/layout_designer/layout_definition.h"
#include "lab/layout_designer/widget_definition.h"
#include "lab/layout_designer/theme_properties.h"
#include "app/editor/system/panel_manager.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"

namespace yaze { namespace editor { class LayoutManager; class EditorManager; } }

namespace yaze {
namespace editor {
namespace layout_designer {

/**
 * @enum DesignMode
 * @brief Design mode for the layout designer
 */
enum class DesignMode {
  PanelLayout,    // Design panel/window layout
  WidgetDesign    // Design internal panel widgets
};

/**
 * @class LayoutDesignerWindow
 * @brief Main window for the WYSIWYG layout designer
 * 
 * Provides a visual interface for designing ImGui panel layouts:
 * - Panel Layout Mode: Drag-and-drop panel placement, dock splits
 * - Widget Design Mode: Design internal panel layouts with widgets
 * - Properties editing
 * - Code generation
 * - Import/Export
 */
class LayoutDesignerWindow {
 public:
  LayoutDesignerWindow() = default;
  explicit LayoutDesignerWindow(yaze::editor::LayoutManager* layout_manager,
                                PanelManager* panel_manager,
                                yaze::editor::EditorManager* editor_manager)
      : layout_manager_(layout_manager),
        editor_manager_(editor_manager),
        panel_manager_(panel_manager) {}
  
  /**
   * @brief Initialize the designer with manager references
   * @param panel_manager Reference to PanelManager for importing panels
   */
  void Initialize(PanelManager* panel_manager, yaze::editor::LayoutManager* layout_manager = nullptr, yaze::editor::EditorManager* editor_manager = nullptr);
  
  /**
   * @brief Open the designer window
   */
  void Open();
  
  /**
   * @brief Close the designer window
   */
  void Close();
  
  /**
   * @brief Check if designer window is open
   */
  bool IsOpen() const { return is_open_; }
  
  /**
   * @brief Draw the designer window (call every frame)
   */
  void Draw();
  
  /**
   * @brief Create a new empty layout
   */
  void NewLayout();
  
  /**
   * @brief Load a layout from JSON file
   */
  void LoadLayout(const std::string& filepath);
  
  /**
   * @brief Save current layout to JSON file
   */
  void SaveLayout(const std::string& filepath);
  
  /**
   * @brief Import layout from current runtime state
   */
  void ImportFromRuntime();
  
  /**
   * @brief Import a specific panel's design from runtime
   * @param panel_id The panel ID to import
   */
  void ImportPanelDesign(const std::string& panel_id);
  
  /**
   * @brief Export layout as C++ code
   */
  void ExportCode(const std::string& filepath);
  
  /**
   * @brief Apply current layout to the application (live preview)
   */
  void PreviewLayout();
  
 private:
  // Panel palette
  struct PalettePanel {
    std::string id;
    std::string name;
    std::string icon;
    std::string category;
    std::string description;
    int priority;
  };

  // UI Components
  void DrawMenuBar();
  void DrawToolbar();
  void DrawPalette();
  void DrawCanvas();
  void DrawProperties();
  void DrawTreeView();
  void DrawCodePreview();
  
  // Widget Design Mode UI
  void DrawWidgetPalette();
  void DrawWidgetCanvas();
  void DrawWidgetProperties();
  void DrawWidgetTree();
  void DrawWidgetCodePreview();
  
  // Theme UI
  void DrawThemeProperties();
  
  // Canvas interaction
  void HandleCanvasDragDrop();
  void DrawDockNode(DockNode* node, const ImVec2& pos, const ImVec2& size);
  void DrawDropZones(const ImVec2& pos, const ImVec2& size, DockNode* target_node);
  bool IsMouseOverRect(const ImVec2& rect_min, const ImVec2& rect_max) const;
  ImGuiDir GetDropZone(const ImVec2& mouse_pos, const ImVec2& rect_min, 
                       const ImVec2& rect_max) const;
  void ResetDropState();
  std::optional<PalettePanel> ResolvePanelById(const std::string& panel_id) const;
  void AddPanelToTarget(const PalettePanel& panel);
  
  // Properties
  void DrawPanelProperties(LayoutPanel* panel);
  void DrawNodeProperties(DockNode* node);
  
  // Code generation
  std::string GenerateDockBuilderCode() const;
  std::string GenerateLayoutPresetCode() const;
  
  // Tree View
  void DrawDockNodeTree(DockNode* node, int& node_index);
  
  // Edit operations
  void DeleteNode(DockNode* node);
  void DeletePanel(LayoutPanel* panel);
  
  // Undo/Redo
  void PushUndoState();
  void Undo();
  void Redo();
  
  // Undo/Redo stacks
  std::vector<std::unique_ptr<LayoutDefinition>> undo_stack_;
  std::vector<std::unique_ptr<LayoutDefinition>> redo_stack_;
  static constexpr size_t kMaxUndoSteps = 50;
  
  std::vector<PalettePanel> GetAvailablePanels() const;
  void RefreshPanelCache();
  bool MatchesSearchFilter(const PalettePanel& panel) const;
  
  // State
  bool is_open_ = false;
  bool show_code_preview_ = false;
  bool show_tree_view_ = true;
  
  // Design mode
  DesignMode design_mode_ = DesignMode::PanelLayout;
  
  // Current layout being edited
  std::unique_ptr<LayoutDefinition> current_layout_;
  
  // Widget design state
  std::unique_ptr<PanelDesign> current_panel_design_;
  std::string selected_panel_for_design_;  // Panel ID to design widgets for
  
  // Theme properties
  ThemeProperties theme_properties_;
  ThemePropertiesPanel theme_panel_;
  bool show_theme_panel_ = false;
  
  // Selection state (Panel Layout Mode)
  LayoutPanel* selected_panel_ = nullptr;
  DockNode* selected_node_ = nullptr;
  DockNode* last_drop_node_for_preview_ = nullptr;
  
  // Selection state (Widget Design Mode)
  WidgetDefinition* selected_widget_ = nullptr;
  
  // Drag and drop state
  bool is_dragging_panel_ = false;
  PalettePanel dragging_panel_;
  ImVec2 drop_zone_pos_;
  ImVec2 drop_zone_size_;
  ImGuiDir drop_direction_ = ImGuiDir_None;
  DockNode* drop_target_node_ = nullptr;

  // Preview/application hooks
  yaze::editor::LayoutManager* layout_manager_ = nullptr;
  yaze::editor::EditorManager* editor_manager_ = nullptr;
  // Panel manager reference (for importing panels)
  PanelManager* panel_manager_ = nullptr;
  
  // Search filter for palette
  char search_filter_[256] = "";
  std::string selected_category_filter_ = "All";
  
  // Widget palette search
  char widget_search_filter_[256] = "";
  std::string selected_widget_category_ = "All";
  
  // Panel cache
  mutable std::vector<PalettePanel> panel_cache_;
  mutable bool panel_cache_dirty_ = true;
  
  // Canvas state
  ImVec2 canvas_scroll_ = ImVec2(0, 0);
  float canvas_zoom_ = 1.0f;
};

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_LAYOUT_DESIGNER_LAYOUT_DESIGNER_WINDOW_H_
