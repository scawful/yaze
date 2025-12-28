#define IMGUI_DEFINE_MATH_OPERATORS
#include "lab/layout_designer/layout_designer_window.h"

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "absl/strings/str_format.h"
#include "lab/layout_designer/layout_serialization.h"
#include "lab/layout_designer/widget_code_generator.h"
#include "lab/layout_designer/yaze_widgets.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"

namespace yaze {
namespace editor {
namespace layout_designer {

namespace {
constexpr const char kPanelPayloadType[] = "PANEL_ID";

struct DockSplitResult {
  ImGuiID first = 0;
  ImGuiID second = 0;
};

// Thin wrapper around ImGui DockBuilder calls so we can swap implementations
// in one place if the API changes.
class DockBuilderFacade {
 public:
  explicit DockBuilderFacade(ImGuiID dockspace_id) : dockspace_id_(dockspace_id) {}

  bool Reset(const ImVec2& size) const {
    if (dockspace_id_ == 0) {
      return false;
    }
    ImGui::DockBuilderRemoveNode(dockspace_id_);
    ImGui::DockBuilderAddNode(dockspace_id_, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id_, size);
    return true;
  }

  DockSplitResult Split(ImGuiID node_id, ImGuiDir dir, float ratio) const {
    DockSplitResult result;
    result.first = node_id;
    ImGui::DockBuilderSplitNode(node_id, dir, ratio, &result.first,
                                &result.second);
    return result;
  }

  void DockWindow(const std::string& title, ImGuiID node_id) const {
    if (!title.empty()) {
      ImGui::DockBuilderDockWindow(title.c_str(), node_id);
    }
  }

  void Finish() const { ImGui::DockBuilderFinish(dockspace_id_); }

  ImGuiID dockspace_id() const { return dockspace_id_; }

 private:
  ImGuiID dockspace_id_ = 0;
};

bool ClearDockspace(ImGuiID dockspace_id) {
  if (dockspace_id == 0) {
    return false;
  }
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderFinish(dockspace_id);
  return true;
}

bool ApplyLayoutToDockspace(LayoutDefinition* layout_def,
                            PanelManager* panel_manager,
                            size_t session_id,
                            ImGuiID dockspace_id) {
  if (!layout_def || !layout_def->root) {
    LOG_WARN("LayoutDesigner", "No layout definition to apply");
    return false;
  }
  if (!panel_manager) {
    LOG_WARN("LayoutDesigner", "PanelManager not available for docking");
    return false;
  }

  DockBuilderFacade facade(dockspace_id);
  if (!facade.Reset(ImGui::GetMainViewport()->WorkSize)) {
    LOG_WARN("LayoutDesigner", "Failed to reset dockspace %u", dockspace_id);
    return false;
  }

  std::function<void(DockNode*, ImGuiID)> build_tree =
      [&](DockNode* node, ImGuiID node_id) {
        if (!node) return;

        if (node->IsSplit() && node->child_left && node->child_right) {
          DockSplitResult split = facade.Split(node_id, node->split_dir,
                                               node->split_ratio);

          DockNode* first = node->child_left.get();
          DockNode* second = node->child_right.get();
          ImGuiID first_id = split.first;
          ImGuiID second_id = split.second;

          // Preserve the visual intent for Right/Down splits
          if (node->split_dir == ImGuiDir_Right ||
              node->split_dir == ImGuiDir_Down) {
            first = node->child_right.get();
            second = node->child_left.get();
          }

          build_tree(first, first_id);
          build_tree(second, second_id);
          return;
        }

        // Leaf/root: dock panels here
        for (const auto& panel : node->panels) {
          const PanelDescriptor* desc =
              panel_manager->GetPanelDescriptor(session_id, panel.panel_id);
          if (!desc) {
            LOG_WARN("LayoutDesigner",
                     "Skipping panel '%s' (descriptor not found for session %zu)",
                     panel.panel_id.c_str(), session_id);
            continue;
          }

          std::string window_title = desc->GetWindowTitle();
          if (window_title.empty()) {
            LOG_WARN("LayoutDesigner",
                     "Skipping panel '%s' (missing window title)",
                     panel.panel_id.c_str());
            continue;
          }

          panel_manager->ShowPanel(session_id, panel.panel_id);
          facade.DockWindow(window_title, node_id);
        }
      };

  build_tree(layout_def->root.get(), dockspace_id);
  facade.Finish();
  return true;
}
}  // namespace

void LayoutDesignerWindow::Initialize(PanelManager* panel_manager,
                                      LayoutManager* layout_manager,
                                      EditorManager* editor_manager) {
  panel_manager_ = panel_manager;
  layout_manager_ = layout_manager;
  editor_manager_ = editor_manager;
  LOG_INFO("LayoutDesigner", "Initialized with PanelManager and LayoutManager");
}

void LayoutDesignerWindow::Open() {
  is_open_ = true;
  if (!current_layout_) {
    NewLayout();
  }
}

void LayoutDesignerWindow::Close() {
  is_open_ = false;
}

void LayoutDesignerWindow::Draw() {
  if (!is_open_) {
    return;
  }
  
  ImGui::SetNextWindowSize(ImVec2(1400, 900), ImGuiCond_FirstUseEver);
  
  if (ImGui::Begin(ICON_MD_DASHBOARD " Layout Designer", &is_open_,
                   ImGuiWindowFlags_MenuBar)) {
    DrawMenuBar();
    DrawToolbar();
    
    // Main content area with 3-panel layout
    ImGui::BeginChild("MainContent", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollbar);
    
    // Left panel: Palette
    float palette_width = 250.0f;
    ImGui::BeginChild("Palette", ImVec2(palette_width, 0), true);
    if (design_mode_ == DesignMode::PanelLayout) {
      DrawPalette();
    } else {
      DrawWidgetPalette();
    }
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Center panel: Canvas
    float properties_width = 300.0f;
    float canvas_width = ImGui::GetContentRegionAvail().x - properties_width - 8;
    ImGui::BeginChild("Canvas", ImVec2(canvas_width, 0), true);
    if (design_mode_ == DesignMode::PanelLayout) {
      DrawCanvas();
    } else {
      DrawWidgetCanvas();
    }
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right panel: Properties
    ImGui::BeginChild("Properties", ImVec2(properties_width, 0), true);
    if (design_mode_ == DesignMode::PanelLayout) {
      DrawProperties();
    } else {
      DrawWidgetProperties();
    }
    ImGui::EndChild();
    
    ImGui::EndChild();
  }
  ImGui::End();
  
  // Separate code preview window if enabled
  if (show_code_preview_) {
    DrawCodePreview();
  }
  
  // Theme properties window if enabled
  if (show_theme_panel_) {
    DrawThemeProperties();
  }
}

void LayoutDesignerWindow::DrawMenuBar() {
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem(ICON_MD_NOTE_ADD " New", "Ctrl+N")) {
        NewLayout();
      }
      if (ImGui::MenuItem(ICON_MD_FOLDER_OPEN " Open...", "Ctrl+O")) {
        // TODO(scawful): File dialog
        LOG_INFO("LayoutDesigner", "Open layout dialog");
      }
      if (ImGui::MenuItem(ICON_MD_SAVE " Save", "Ctrl+S",
                          false, current_layout_ != nullptr)) {
        // TODO(scawful): File dialog
        LOG_INFO("LayoutDesigner", "Save layout dialog");
      }
      if (ImGui::MenuItem(ICON_MD_SAVE_AS " Save As...", "Ctrl+Shift+S",
                          false, current_layout_ != nullptr)) {
        // TODO(scawful): File dialog
        LOG_INFO("LayoutDesigner", "Save As dialog");
      }
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_UPLOAD " Import Layout from Runtime")) {
        ImportFromRuntime();
      }
      if (ImGui::BeginMenu(ICON_MD_WIDGETS " Import Panel Design")) {
        if (panel_manager_) {
          auto panels = panel_manager_->GetAllPanelDescriptors();
          for (const auto& [pid, desc] : panels) {
            if (ImGui::MenuItem(desc.display_name.c_str())) {
              ImportPanelDesign(pid);
            }
          }
        } else {
          ImGui::TextDisabled("No panels available");
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem(ICON_MD_DOWNLOAD " Export Code...",
                          nullptr, false, current_layout_ != nullptr)) {
        show_code_preview_ = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_CLOSE " Close", "Ctrl+W")) {
        Close();
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem(ICON_MD_UNDO " Undo", "Ctrl+Z", false, false)) {
        // TODO(scawful): Undo
      }
      if (ImGui::MenuItem(ICON_MD_REDO " Redo", "Ctrl+Y", false, false)) {
        // TODO(scawful): Redo
      }
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_DELETE " Delete Selected", "Del",
                          false, selected_panel_ != nullptr)) {
        // TODO(scawful): Delete panel
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Show Code Preview", nullptr, &show_code_preview_);
      ImGui::MenuItem("Show Tree View", nullptr, &show_tree_view_);
      ImGui::MenuItem("Show Theme Panel", nullptr, &show_theme_panel_);
      ImGui::Separator();
      if (ImGui::MenuItem(ICON_MD_ZOOM_IN " Zoom In", "Ctrl++")) {
        canvas_zoom_ = std::min(canvas_zoom_ + 0.1f, 2.0f);
      }
      if (ImGui::MenuItem(ICON_MD_ZOOM_OUT " Zoom Out", "Ctrl+-")) {
        canvas_zoom_ = std::max(canvas_zoom_ - 0.1f, 0.5f);
      }
      if (ImGui::MenuItem(ICON_MD_ZOOM_OUT_MAP " Reset Zoom", "Ctrl+0")) {
        canvas_zoom_ = 1.0f;
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Layout")) {
      if (ImGui::MenuItem(ICON_MD_PLAY_ARROW " Preview Layout",
                          nullptr, false, current_layout_ != nullptr)) {
        PreviewLayout();
      }
      if (ImGui::MenuItem(ICON_MD_CHECK " Validate",
                          nullptr, false, current_layout_ != nullptr)) {
        std::string error;
        if (current_layout_->Validate(&error)) {
          LOG_INFO("LayoutDesigner", "Layout is valid!");
        } else {
          LOG_ERROR("LayoutDesigner", "Layout validation failed: %s",
                    error.c_str());
        }
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem(ICON_MD_HELP " Documentation")) {
        // TODO(scawful): Open docs
      }
      if (ImGui::MenuItem(ICON_MD_INFO " About")) {
        // TODO(scawful): About dialog
      }
      ImGui::EndMenu();
    }
    
    ImGui::EndMenuBar();
  }
}

void LayoutDesignerWindow::DrawToolbar() {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
  
  // Mode switcher
  bool is_panel_mode = (design_mode_ == DesignMode::PanelLayout);
  if (ImGui::RadioButton(ICON_MD_DASHBOARD " Panel Layout", is_panel_mode)) {
    design_mode_ = DesignMode::PanelLayout;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton(ICON_MD_WIDGETS " Widget Design", !is_panel_mode)) {
    design_mode_ = DesignMode::WidgetDesign;
  }
  
  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();
  
  if (ImGui::Button(ICON_MD_NOTE_ADD " New")) {
    if (design_mode_ == DesignMode::PanelLayout) {
      NewLayout();
    } else {
      // Create new panel design
      current_panel_design_ = std::make_unique<PanelDesign>();
      current_panel_design_->panel_id = "new_panel";
      current_panel_design_->panel_name = "New Panel";
    }
  }
  ImGui::SameLine();
  
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open")) {
    LOG_INFO("LayoutDesigner", "Open clicked");
  }
  ImGui::SameLine();
  
  if (ImGui::Button(ICON_MD_SAVE " Save")) {
    LOG_INFO("LayoutDesigner", "Save clicked");
  }
  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();
  
  if (ImGui::Button(ICON_MD_PLAY_ARROW " Preview")) {
    PreviewLayout();
  }
  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_RESET_TV " Clear Dockspace")) {
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    if (ClearDockspace(dockspace_id)) {
      LOG_INFO("LayoutDesigner", "Cleared dockspace %u", dockspace_id);
    } else {
      LOG_WARN("LayoutDesigner", "Failed to clear dockspace %u", dockspace_id);
    }
  }
  ImGui::SameLine();
  
  if (ImGui::Button(ICON_MD_CODE " Export Code")) {
    show_code_preview_ = !show_code_preview_;
  }
  
  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();
  
  // Info display
  if (design_mode_ == DesignMode::PanelLayout) {
    if (current_layout_) {
      ImGui::Text("%s | %zu panels",
                  current_layout_->name.c_str(),
                  current_layout_->GetAllPanels().size());
    }
  } else {
    if (current_panel_design_) {
      ImGui::Text("%s | %zu widgets",
                  current_panel_design_->panel_name.c_str(),
                  current_panel_design_->GetAllWidgets().size());
    }
  }
  
  ImGui::PopStyleVar();
  ImGui::Separator();
}

void LayoutDesignerWindow::DrawPalette() {
  ImGui::Text(ICON_MD_WIDGETS " Panel Palette");
  ImGui::Separator();
  
  // Search bar
  ImGui::SetNextItemWidth(-1);
  if (ImGui::InputTextWithHint("##search", ICON_MD_SEARCH " Search panels...",
                               search_filter_, sizeof(search_filter_))) {
    // Search changed, might want to auto-expand categories
  }
  
  // Category filter dropdown
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##category_filter", selected_category_filter_.c_str())) {
    if (ImGui::Selectable("All", selected_category_filter_ == "All")) {
      selected_category_filter_ = "All";
    }
    
    // Get all unique categories
    auto panels = GetAvailablePanels();
    std::set<std::string> categories;
    for (const auto& panel : panels) {
      categories.insert(panel.category);
    }
    
    for (const auto& cat : categories) {
      if (ImGui::Selectable(cat.c_str(), selected_category_filter_ == cat)) {
        selected_category_filter_ = cat;
      }
    }
    ImGui::EndCombo();
  }
  
  ImGui::Spacing();
  ImGui::Separator();
  
  // Get available panels
  auto panels = GetAvailablePanels();
  
  // Group panels by category
  std::map<std::string, std::vector<PalettePanel>> grouped_panels;
  int visible_count = 0;
  
  for (const auto& panel : panels) {
    // Apply category filter
    if (selected_category_filter_ != "All" && 
        panel.category != selected_category_filter_) {
      continue;
    }
    
    // Apply search filter
    if (!MatchesSearchFilter(panel)) {
      continue;
    }
    
    grouped_panels[panel.category].push_back(panel);
    visible_count++;
  }
  
  // Draw panels grouped by category
  for (const auto& [category, category_panels] : grouped_panels) {
    // Collapsible category header
    bool category_open = ImGui::CollapsingHeader(
        absl::StrFormat("%s (%d)", category, category_panels.size()).c_str(),
        ImGuiTreeNodeFlags_DefaultOpen);
    
    if (category_open) {
      for (const auto& panel : category_panels) {
        ImGui::PushID(panel.id.c_str());
        
        // Panel card with icon and name
        ImVec4 bg_color = ImVec4(0.2f, 0.2f, 0.25f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Header, bg_color);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 
                              ImVec4(0.25f, 0.25f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              ImVec4(0.3f, 0.3f, 0.4f, 1.0f));
        
        bool clicked = ImGui::Selectable(
            absl::StrFormat("%s %s", panel.icon, panel.name).c_str(),
            false, 0, ImVec2(0, 32));
        
        ImGui::PopStyleColor(3);
        
        if (clicked) {
          LOG_INFO("LayoutDesigner", "Selected panel: %s", panel.name.c_str());
        }
        
        // Tooltip with description
        if (ImGui::IsItemHovered() && !panel.description.empty()) {
          ImGui::SetTooltip("%s\n\nID: %s\nPriority: %d",
                            panel.description.c_str(),
                            panel.id.c_str(),
                            panel.priority);
        }
        
        // Drag source - use stable pointer to panel in vector
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
          // Copy metadata into persistent drag state and send panel ID payload
          dragging_panel_ = panel;
          is_dragging_panel_ = true;
          ImGui::SetDragDropPayload(kPanelPayloadType,
                                    panel.id.c_str(),
                                    panel.id.size() + 1);  // include null terminator
          ImGui::Text("%s %s", panel.icon.c_str(), panel.name.c_str());
          ImGui::TextDisabled("Drag to canvas");
          ImGui::EndDragDropSource();
          
          LOG_INFO("DragDrop", "Drag started: %s", panel.name.c_str());
        } else {
          is_dragging_panel_ = false;
        }
        
        ImGui::PopID();
      }
      
      ImGui::Spacing();
    }
  }
  
  // Show count at bottom
  ImGui::Separator();
  ImGui::TextDisabled("%d panels available", visible_count);
}

void LayoutDesignerWindow::DrawCanvas() {
  ImGui::Text(ICON_MD_DASHBOARD " Canvas");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_ZOOM_IN)) {
    canvas_zoom_ = std::min(canvas_zoom_ + 0.1f, 2.0f);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_ZOOM_OUT)) {
    canvas_zoom_ = std::max(canvas_zoom_ - 0.1f, 0.5f);
  }
  ImGui::SameLine();
  ImGui::Text("%.0f%%", canvas_zoom_ * 100);
  
  // Debug: Show drag state
  const ImGuiPayload* drag_payload = ImGui::GetDragDropPayload();
  is_dragging_panel_ = drag_payload && drag_payload->DataType &&
                       strcmp(drag_payload->DataType, kPanelPayloadType) == 0;
  if (drag_payload && drag_payload->DataType) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0, 1, 0, 1), 
                       ICON_MD_DRAG_INDICATOR " Dragging: %s", 
                       drag_payload->DataType);
  }
  
  ImGui::Separator();
  
  if (!current_layout_ || !current_layout_->root) {
    ImGui::TextWrapped("No layout loaded. Create a new layout or open an existing one.");
    
    if (ImGui::Button("Create New Layout")) {
      NewLayout();
    }
    return;
  }
  
  // Canvas area with scrolling
  ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  ImVec2 scaled_size = ImVec2(
      current_layout_->canvas_size.x * canvas_zoom_,
      current_layout_->canvas_size.y * canvas_zoom_);
  
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImU32 grid_color = ImGui::GetColorU32(ImGuiCol_TableBorderStrong);
  
  // Background grid
  const float grid_step = 50.0f * canvas_zoom_;
  for (float x_pos = 0; x_pos < scaled_size.x; x_pos += grid_step) {
    draw_list->AddLine(
        ImVec2(canvas_pos.x + x_pos, canvas_pos.y),
        ImVec2(canvas_pos.x + x_pos, canvas_pos.y + scaled_size.y),
        grid_color);
  }
  for (float y_pos = 0; y_pos < scaled_size.y; y_pos += grid_step) {
    draw_list->AddLine(
        ImVec2(canvas_pos.x, canvas_pos.y + y_pos),
        ImVec2(canvas_pos.x + scaled_size.x, canvas_pos.y + y_pos),
        grid_color);
  }
  
  // Reset drop state at start of frame
  ResetDropState();
  
  // Draw dock nodes recursively (this sets drop_target_node_)
  DrawDockNode(current_layout_->root.get(), canvas_pos, scaled_size);
  
  // Create an invisible button for the entire canvas to be a drop target
  ImGui::SetCursorScreenPos(canvas_pos);
  ImGui::InvisibleButton("canvas_drop_zone", scaled_size);
  
  // Set up drop target on the invisible button
  
  if (ImGui::BeginDragDropTarget()) {
    // Show preview while dragging
    const ImGuiPayload* preview = ImGui::GetDragDropPayload();
    if (preview && strcmp(preview->DataType, kPanelPayloadType) == 0) {
      // We're dragging - drop zones should have been shown in DrawDockNode
    }
    
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(kPanelPayloadType)) {
      const char* panel_id_cstr = static_cast<const char*>(payload->Data);
      std::string panel_id = panel_id_cstr ? panel_id_cstr : "";
      auto resolved = ResolvePanelById(panel_id);
      if (!resolved.has_value()) {
        LOG_WARN("DragDrop", "Unknown panel payload: %s", panel_id.c_str());
      } else {
        AddPanelToTarget(*resolved);
      }
    }
    ImGui::EndDragDropTarget();
  }
}

void LayoutDesignerWindow::DrawDockNode(DockNode* node,
                                        const ImVec2& pos,
                                        const ImVec2& size) {
  if (!node) return;
  
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 rect_max = ImVec2(pos.x + size.x, pos.y + size.y);
  auto alpha_color = [](ImU32 base, float alpha_scale) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(base);
    c.w *= alpha_scale;
    return ImGui::ColorConvertFloat4ToU32(c);
  };
  
  // Check if we're dragging a panel
  const ImGuiPayload* drag_payload = ImGui::GetDragDropPayload();
  bool is_drag_active = drag_payload != nullptr &&
                        drag_payload->DataType != nullptr &&
                        strcmp(drag_payload->DataType, "PANEL_ID") == 0;
  
  if (node->IsLeaf()) {
    // Draw leaf node with panels
    ImU32 border_color = ImGui::GetColorU32(ImGuiCol_Border);
    
    // Highlight if selected
    bool is_selected = (selected_node_ == node);
    if (is_selected) {
      border_color = ImGui::GetColorU32(ImGuiCol_CheckMark);
    }
    
    // Highlight if mouse is over this node during drag
    if (is_drag_active) {
      if (IsMouseOverRect(pos, rect_max)) {
        if (node->flags & ImGuiDockNodeFlags_NoDockingOverMe) {
          border_color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        } else {
          border_color = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
          // Show drop zones and update drop state
          DrawDropZones(pos, size, node);
        }
      }
    }
    
    draw_list->AddRect(pos, rect_max, border_color, 4.0f, 0, 2.0f);
    
    // Handle click selection (when not dragging)
    if (!is_drag_active && IsMouseOverRect(pos, rect_max) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      selected_node_ = node;
      selected_panel_ = node->panels.empty() ? nullptr : &node->panels[0];
      LOG_INFO("LayoutDesigner", "Selected dock node with %zu panels",
               node->panels.size());
    }
    
    // Draw panel capsules for clarity
    const float panel_padding = 8.0f;
    const float capsule_height = 26.0f;
    ImVec2 capsule_pos = ImVec2(pos.x + panel_padding, pos.y + panel_padding);
    for (const auto& panel : node->panels) {
      ImVec2 capsule_min = capsule_pos;
      ImVec2 capsule_max = ImVec2(rect_max.x - panel_padding,
                                  capsule_pos.y + capsule_height);
      ImU32 capsule_fill = alpha_color(ImGui::GetColorU32(ImGuiCol_Header), 0.7f);
      ImU32 capsule_border = ImGui::GetColorU32(ImGuiCol_HeaderActive);
      draw_list->AddRectFilled(capsule_min, capsule_max, capsule_fill, 6.0f);
      draw_list->AddRect(capsule_min, capsule_max, capsule_border, 6.0f, 0, 1.5f);

      std::string label = absl::StrFormat("%s %s", panel.icon, panel.display_name);
      draw_list->AddText(ImVec2(capsule_min.x + 8, capsule_min.y + 5),
                         ImGui::GetColorU32(ImGuiCol_Text), label.c_str());

      // Secondary line for ID
      std::string sub = absl::StrFormat("ID: %s", panel.panel_id.c_str());
      draw_list->AddText(ImVec2(capsule_min.x + 8, capsule_min.y + 5 + 12),
                         alpha_color(ImGui::GetColorU32(ImGuiCol_Text), 0.7f),
                         sub.c_str());

      // Tooltip on hover
      ImRect capsule_rect(capsule_min, capsule_max);
      if (capsule_rect.Contains(ImGui::GetMousePos())) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(label.c_str());
        ImGui::TextDisabled("%s", panel.panel_id.c_str());
        ImGui::EndTooltip();
      }

      capsule_pos.y += capsule_height + 6.0f;
    }
    
    // Draw Node Flags
    std::string node_flags_str;
    if (node->flags & ImGuiDockNodeFlags_NoTabBar) node_flags_str += "[NoTab] ";
    if (node->flags & ImGuiDockNodeFlags_HiddenTabBar) node_flags_str += "[HiddenTab] ";
    if (node->flags & ImGuiDockNodeFlags_NoCloseButton) node_flags_str += "[NoClose] ";
    if (node->flags & ImGuiDockNodeFlags_NoDockingOverMe) node_flags_str += "[NoDock] ";
    
    if (!node_flags_str.empty()) {
      ImVec2 flags_size = ImGui::CalcTextSize(node_flags_str.c_str());
      draw_list->AddText(ImVec2(rect_max.x - flags_size.x - 5, pos.y + 5), 
                         alpha_color(ImGui::GetColorU32(ImGuiCol_Text), 0.8f),
                         node_flags_str.c_str());
    }
    
    if (node->panels.empty()) {
      const char* empty_text = is_drag_active ? "Drop panel here" : "Empty";
      if (node->flags & ImGuiDockNodeFlags_NoDockingOverMe) {
        empty_text = "Docking Disabled";
      }
      ImVec2 text_size = ImGui::CalcTextSize(empty_text);
      draw_list->AddText(
          ImVec2(pos.x + (size.x - text_size.x) / 2, 
                 pos.y + (size.y - text_size.y) / 2),
          ImGui::GetColorU32(ImGuiCol_TextDisabled), empty_text);
    }
  } else if (node->IsSplit()) {
    // Draw split node
    ImVec2 left_size;
    ImVec2 right_size;
    ImVec2 left_pos = pos;
    ImVec2 right_pos;
    
    if (node->split_dir == ImGuiDir_Left || node->split_dir == ImGuiDir_Right) {
      // Horizontal split
      float split_x = size.x * node->split_ratio;
      left_size = ImVec2(split_x - 2, size.y);
      right_size = ImVec2(size.x - split_x - 2, size.y);
      right_pos = ImVec2(pos.x + split_x + 2, pos.y);
      
      // Draw split line
      draw_list->AddLine(
          ImVec2(pos.x + split_x, pos.y),
          ImVec2(pos.x + split_x, pos.y + size.y),
          IM_COL32(200, 200, 100, 255), 3.0f);
    } else {
      // Vertical split
      float split_y = size.y * node->split_ratio;
      left_size = ImVec2(size.x, split_y - 2);
      right_size = ImVec2(size.x, size.y - split_y - 2);
      right_pos = ImVec2(pos.x, pos.y + split_y + 2);
      
      // Draw split line
      draw_list->AddLine(
          ImVec2(pos.x, pos.y + split_y),
          ImVec2(pos.x + size.x, pos.y + split_y),
          IM_COL32(200, 200, 100, 255), 3.0f);
    }
    
    DrawDockNode(node->child_left.get(), left_pos, left_size);
    DrawDockNode(node->child_right.get(), right_pos, right_size);
  }
}

void LayoutDesignerWindow::DrawProperties() {
  ImGui::Text(ICON_MD_TUNE " Properties");
  ImGui::Separator();
  
  if (selected_panel_) {
    DrawPanelProperties(selected_panel_);
  } else if (selected_node_) {
    DrawNodeProperties(selected_node_);
  } else if (current_layout_) {
    ImGui::TextWrapped("Select a panel or node to edit properties");
    ImGui::Spacing();
    
    ImGui::Text("Layout: %s", current_layout_->name.c_str());
    ImGui::Text("Panels: %zu", current_layout_->GetAllPanels().size());
    
    if (show_tree_view_) {
      ImGui::Separator();
      DrawTreeView();
    }
  } else {
    ImGui::TextWrapped("No layout loaded");
  }
}

void LayoutDesignerWindow::DrawPanelProperties(LayoutPanel* panel) {
  if (!panel) return;
  
  ImGui::Text("Panel: %s", panel->display_name.c_str());
  ImGui::Separator();
  
  ImGui::Text("Behavior");
  ImGui::Checkbox("Visible by default", &panel->visible_by_default);
  ImGui::Checkbox("Closable", &panel->closable);
  ImGui::Checkbox("Minimizable", &panel->minimizable);
  ImGui::Checkbox("Pinnable", &panel->pinnable);
  ImGui::Checkbox("Headless", &panel->headless);
  
  ImGui::Separator();
  ImGui::Text("Window Flags");
  ImGui::CheckboxFlags("No Title Bar", &panel->flags, ImGuiWindowFlags_NoTitleBar);
  ImGui::CheckboxFlags("No Resize", &panel->flags, ImGuiWindowFlags_NoResize);
  ImGui::CheckboxFlags("No Move", &panel->flags, ImGuiWindowFlags_NoMove);
  ImGui::CheckboxFlags("No Scrollbar", &panel->flags, ImGuiWindowFlags_NoScrollbar);
  ImGui::CheckboxFlags("No Collapse", &panel->flags, ImGuiWindowFlags_NoCollapse);
  ImGui::CheckboxFlags("No Background", &panel->flags, ImGuiWindowFlags_NoBackground);
  
  ImGui::Separator();
  ImGui::SliderInt("Priority", &panel->priority, 0, 1000);
}

void LayoutDesignerWindow::DrawNodeProperties(DockNode* node) {
  if (!node) return;
  
  ImGui::Text("Dock Node");
  ImGui::Separator();
  
  if (node->IsSplit()) {
    ImGui::Text("Type: Split");
    ImGui::SliderFloat("Split Ratio", &node->split_ratio, 0.1f, 0.9f);
    
    const char* dir_names[] = {"None", "Left", "Right", "Up", "Down"};
    int dir_idx = static_cast<int>(node->split_dir);
    if (ImGui::Combo("Direction", &dir_idx, dir_names, 5)) {
      node->split_dir = static_cast<ImGuiDir>(dir_idx);
    }
  } else {
    ImGui::Text("Type: Leaf");
    ImGui::Text("Panels: %zu", node->panels.size());
  }
  
  ImGui::Separator();
  ImGui::Text("Dock Node Flags");
  ImGui::CheckboxFlags("Auto-Hide Tab Bar", &node->flags, ImGuiDockNodeFlags_AutoHideTabBar);
  ImGui::CheckboxFlags("No Docking Over Central", &node->flags, ImGuiDockNodeFlags_NoDockingOverCentralNode);
  ImGui::CheckboxFlags("No Docking Split", &node->flags, ImGuiDockNodeFlags_NoDockingSplit);
  ImGui::CheckboxFlags("No Resize", &node->flags, ImGuiDockNodeFlags_NoResize);
  ImGui::CheckboxFlags("No Undocking", &node->flags, ImGuiDockNodeFlags_NoUndocking);
}

void LayoutDesignerWindow::DrawTreeView() {
  ImGui::Text(ICON_MD_ACCOUNT_TREE " Layout Tree");
  ImGui::Separator();
  
  if (current_layout_ && current_layout_->root) {
    int node_index = 0;
    DrawDockNodeTree(current_layout_->root.get(), node_index);
  } else {
    ImGui::TextDisabled("No layout loaded");
  }
}

void LayoutDesignerWindow::DrawCodePreview() {
  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
  
  if (ImGui::Begin(ICON_MD_CODE " Generated Code", &show_code_preview_)) {
    if (design_mode_ == DesignMode::PanelLayout) {
      // Panel layout code
      if (ImGui::BeginTabBar("CodeTabs")) {
        if (ImGui::BeginTabItem("DockBuilder Code")) {
          std::string code = GenerateDockBuilderCode();
          ImGui::TextUnformatted(code.c_str());
          ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Layout Preset")) {
          std::string code = GenerateLayoutPresetCode();
          ImGui::TextUnformatted(code.c_str());
          ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
      }
    } else {
      // Widget design code
      if (ImGui::BeginTabBar("WidgetCodeTabs")) {
        if (ImGui::BeginTabItem("Draw() Method")) {
          DrawWidgetCodePreview();
          ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Member Variables")) {
          if (current_panel_design_) {
            std::string code = WidgetCodeGenerator::GenerateMemberVariables(*current_panel_design_);
            ImGui::TextUnformatted(code.c_str());
          } else {
            ImGui::Text("// No panel design loaded");
          }
          ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
      }
    }
  }
  ImGui::End();
}

std::string LayoutDesignerWindow::GenerateDockBuilderCode() const {
  if (!current_layout_ || !current_layout_->root) {
    return "// No layout loaded";
  }
  
  std::string code;
  absl::StrAppend(&code, absl::StrFormat(
      "// Generated by YAZE Layout Designer\n"
      "// Layout: \"%s\"\n"
      "// Generated: <timestamp>\n\n"
      "void LayoutManager::Build%sLayout(ImGuiID dockspace_id) {\n"
      "  ImGui::DockBuilderRemoveNode(dockspace_id);\n"
      "  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);\n"
      "  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);\n\n"
      "  ImGuiID dock_main_id = dockspace_id;\n",
      current_layout_->name,
      current_layout_->name));
      
  // Helper to generate split code
  std::function<void(DockNode*, std::string)> generate_splits = 
      [&](DockNode* node, std::string parent_id_var) {
    if (!node || node->IsLeaf()) {
      // Apply flags to leaf node if any
      if (node && node->flags != ImGuiDockNodeFlags_None) {
        absl::StrAppend(&code, absl::StrFormat(
            "  if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(%s)) {\n"
            "    node->LocalFlags = %d;\n"
            "  }\n",
            parent_id_var, node->flags));
      }
      return;
    }
    
    // It's a split node
    std::string child_left_var = absl::StrFormat("dock_id_%d", node->child_left->node_id);
    std::string child_right_var = absl::StrFormat("dock_id_%d", node->child_right->node_id);
    
    // Assign IDs if not already assigned (simple counter for now)
    static int id_counter = 1000;
    if (node->child_left->node_id == 0) node->child_left->node_id = id_counter++;
    if (node->child_right->node_id == 0) node->child_right->node_id = id_counter++;
    
    // Determine split direction
    std::string dir_str;
    switch (node->split_dir) {
      case ImGuiDir_Left: dir_str = "ImGuiDir_Left"; break;
      case ImGuiDir_Right: dir_str = "ImGuiDir_Right"; break;
      case ImGuiDir_Up: dir_str = "ImGuiDir_Up"; break;
      case ImGuiDir_Down: dir_str = "ImGuiDir_Down"; break;
      default: dir_str = "ImGuiDir_Left"; break;
    }
    
    absl::StrAppend(&code, absl::StrFormat(
        "  ImGuiID %s, %s;\n"
        "  ImGui::DockBuilderSplitNode(%s, %s, %.2ff, &%s, &%s);\n",
        child_left_var, child_right_var,
        parent_id_var, dir_str, node->split_ratio,
        child_left_var, child_right_var));
        
    // Apply flags to split node if any
    if (node->flags != ImGuiDockNodeFlags_None) {
      absl::StrAppend(&code, absl::StrFormat(
          "  if (ImGuiDockNode* node = ImGui::DockBuilderGetNode(%s)) {\n"
          "    node->LocalFlags = %d;\n"
          "  }\n",
          parent_id_var, node->flags));
    }
        
    generate_splits(node->child_left.get(), child_left_var);
    generate_splits(node->child_right.get(), child_right_var);
  };
  
  // Helper to generate dock code
  std::function<void(DockNode*, std::string)> generate_docking = 
      [&](DockNode* node, std::string node_id_var) {
    if (!node) return;
    
    if (node->IsLeaf()) {
      for (const auto& panel : node->panels) {
        if (panel.flags != ImGuiWindowFlags_None) {
          absl::StrAppend(&code, absl::StrFormat(
              "  // Note: Panel '%s' requires flags: %d\n",
              panel.panel_id, panel.flags));
        }
        absl::StrAppend(&code, absl::StrFormat(
            "  ImGui::DockBuilderDockWindow(\"%s\", %s);\n",
            panel.panel_id, node_id_var));
      }
    } else {
      std::string child_left_var = absl::StrFormat("dock_id_%d", node->child_left->node_id);
      std::string child_right_var = absl::StrFormat("dock_id_%d", node->child_right->node_id);
      
      generate_docking(node->child_left.get(), child_left_var);
      generate_docking(node->child_right.get(), child_right_var);
    }
  };
  
  generate_splits(current_layout_->root.get(), "dock_main_id");
  
  absl::StrAppend(&code, "\n");
  
  generate_docking(current_layout_->root.get(), "dock_main_id");
  
  absl::StrAppend(&code, "\n  ImGui::DockBuilderFinish(dockspace_id);\n}\n");
  
  return code;
}

std::string LayoutDesignerWindow::GenerateLayoutPresetCode() const {
  if (!current_layout_) {
    return "// No layout loaded";
  }
  
  std::string code = absl::StrFormat(
      "// Generated by YAZE Layout Designer\n"
      "// Layout: \"%s\"\n\n"
      "PanelLayoutPreset LayoutPresets::Get%sPreset() {\n"
      "  return {\n"
      "    .name = \"%s\",\n"
      "    .description = \"%s\",\n"
      "    .editor_type = EditorType::kUnknown,\n"
      "    .default_visible_panels = {},\n"
      "    .optional_panels = {},\n"
      "    .panel_positions = {}\n"
      "  };\n"
      "}\n",
      current_layout_->name,
      current_layout_->name,
      current_layout_->name,
      current_layout_->description);
      
  // Append comments about panel flags
  bool has_flags = false;
  std::function<void(DockNode*)> check_flags = [&](DockNode* node) {
    if (!node) return;
    for (const auto& panel : node->panels) {
      if (panel.flags != ImGuiWindowFlags_None) {
        if (!has_flags) {
          absl::StrAppend(&code, "\n// Note: The following panels require window flags:\n");
          has_flags = true;
        }
        absl::StrAppend(&code, absl::StrFormat("// - %s: %d\n", panel.panel_id, panel.flags));
      }
    }
    check_flags(node->child_left.get());
    check_flags(node->child_right.get());
  };
  check_flags(current_layout_->root.get());
  
  return code;
}

void LayoutDesignerWindow::DrawDockNodeTree(DockNode* node, int& node_index) {
  if (!node) return;
  
  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | 
                             ImGuiTreeNodeFlags_OpenOnDoubleClick | 
                             ImGuiTreeNodeFlags_DefaultOpen;
  
  if (selected_node_ == node) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
  
  std::string label;
  if (node->IsRoot()) {
    label = "Root";
  } else if (node->IsSplit()) {
    label = absl::StrFormat("Split (%s, %.2f)", 
                            node->split_dir == ImGuiDir_Left || node->split_dir == ImGuiDir_Right ? "Horizontal" : "Vertical",
                            node->split_ratio);
  } else {
    label = absl::StrFormat("Leaf (%zu panels)", node->panels.size());
  }
  
  bool open = ImGui::TreeNodeEx((void*)(intptr_t)node_index, flags, "%s", label.c_str());
  
  if (ImGui::IsItemClicked()) {
    selected_node_ = node;
    selected_panel_ = nullptr;
  }
  
  // Context menu for nodes
  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Delete Node")) {
      DeleteNode(node);
      ImGui::EndPopup();
      if (open) ImGui::TreePop();
      return;
    }
    ImGui::EndPopup();
  }
  
  node_index++;
  
  if (open) {
    if (node->IsSplit()) {
      DrawDockNodeTree(node->child_left.get(), node_index);
      DrawDockNodeTree(node->child_right.get(), node_index);
    } else {
      // Draw panels in leaf
      for (size_t i = 0; i < node->panels.size(); i++) {
        auto& panel = node->panels[i];
        ImGuiTreeNodeFlags panel_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selected_panel_ == &panel) {
          panel_flags |= ImGuiTreeNodeFlags_Selected;
        }
        
        ImGui::TreeNodeEx((void*)(intptr_t)node_index, panel_flags, "%s %s", panel.icon.c_str(), panel.display_name.c_str());
        
        if (ImGui::IsItemClicked()) {
          selected_panel_ = &panel;
          selected_node_ = node;
        }
        
        // Context menu for panels
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem("Delete Panel")) {
            DeletePanel(&panel);
            ImGui::EndPopup();
            if (open) ImGui::TreePop();
            return;
          }
          ImGui::EndPopup();
        }
        
        node_index++;
      }
    }
    ImGui::TreePop();
  }
}

void LayoutDesignerWindow::DeleteNode(DockNode* node_to_delete) {
  if (!current_layout_ || !current_layout_->root || node_to_delete == current_layout_->root.get()) {
    LOG_WARN("LayoutDesigner", "Cannot delete root node");
    return;
  }
  
  PushUndoState();
  
  // Find parent of node_to_delete
  DockNode* parent = nullptr;
  bool is_left_child = false;
  
  std::function<bool(DockNode*)> find_parent = [&](DockNode* n) -> bool {
    if (!n || n->IsLeaf()) return false;
    
    if (n->child_left.get() == node_to_delete) {
      parent = n;
      is_left_child = true;
      return true;
    }
    if (n->child_right.get() == node_to_delete) {
      parent = n;
      is_left_child = false;
      return true;
    }
    
    if (find_parent(n->child_left.get())) return true;
    if (find_parent(n->child_right.get())) return true;
    
    return false;
  };
  
  if (find_parent(current_layout_->root.get())) {
    // Replace parent with the OTHER child
    std::unique_ptr<DockNode> other_child;
    if (is_left_child) {
      other_child = std::move(parent->child_right);
    } else {
      other_child = std::move(parent->child_left);
    }
    
    // We need to replace 'parent' with 'other_child' in 'parent's parent'
    // But since we don't have back pointers, this is tricky.
    // Easier approach: Copy content of other_child into parent
    
    parent->type = other_child->type;
    parent->split_dir = other_child->split_dir;
    parent->split_ratio = other_child->split_ratio;
    parent->flags = other_child->flags;
    parent->panels = std::move(other_child->panels);
    parent->child_left = std::move(other_child->child_left);
    parent->child_right = std::move(other_child->child_right);
    
    selected_node_ = nullptr;
    selected_panel_ = nullptr;
    current_layout_->Touch();
  }
}

void LayoutDesignerWindow::DeletePanel(LayoutPanel* panel_to_delete) {
  if (!current_layout_ || !current_layout_->root) return;
  
  PushUndoState();
  
  std::function<bool(DockNode*)> find_and_delete = [&](DockNode* n) -> bool {
    if (!n) return false;
    
    if (n->IsLeaf()) {
      auto it = std::find_if(n->panels.begin(), n->panels.end(), 
                             [&](const LayoutPanel& p) { return &p == panel_to_delete; });
      if (it != n->panels.end()) {
        n->panels.erase(it);
        return true;
      }
    } else {
      if (find_and_delete(n->child_left.get())) return true;
      if (find_and_delete(n->child_right.get())) return true;
    }
    return false;
  };
  
  if (find_and_delete(current_layout_->root.get())) {
    selected_panel_ = nullptr;
    current_layout_->Touch();
  }
}

void LayoutDesignerWindow::PushUndoState() {
  if (!current_layout_) return;
  
  if (undo_stack_.size() >= kMaxUndoSteps) {
    undo_stack_.erase(undo_stack_.begin());
  }
  undo_stack_.push_back(current_layout_->Clone());
  
  // Clear redo stack when new action is performed
  redo_stack_.clear();
}

void LayoutDesignerWindow::Undo() {
  if (undo_stack_.empty()) return;
  
  // Save current state to redo stack
  redo_stack_.push_back(current_layout_->Clone());
  
  // Restore from undo stack
  current_layout_ = std::move(undo_stack_.back());
  undo_stack_.pop_back();
  
  // Reset selection to avoid dangling pointers
  selected_node_ = nullptr;
  selected_panel_ = nullptr;
}

void LayoutDesignerWindow::Redo() {
  if (redo_stack_.empty()) return;
  
  // Save current state to undo stack
  undo_stack_.push_back(current_layout_->Clone());
  
  // Restore from redo stack
  current_layout_ = std::move(redo_stack_.back());
  redo_stack_.pop_back();
  
  // Reset selection
  selected_node_ = nullptr;
  selected_panel_ = nullptr;
}

std::vector<LayoutDesignerWindow::PalettePanel>
LayoutDesignerWindow::GetAvailablePanels() const {
  // Return cached panels if available
  if (!panel_cache_dirty_ && !panel_cache_.empty()) {
    return panel_cache_;
  }
  
  panel_cache_.clear();
  
  if (panel_manager_) {
    // Query real panels from PanelManager
    auto all_descriptors = panel_manager_->GetAllPanelDescriptors();
    
    for (const auto& [panel_id, descriptor] : all_descriptors) {
      PalettePanel panel;
      panel.id = panel_id;
      panel.name = descriptor.display_name;
      panel.icon = descriptor.icon;
      panel.category = descriptor.category;
      panel.description = descriptor.disabled_tooltip;
      panel.priority = descriptor.priority;
      panel_cache_.push_back(panel);
    }
    
    // Sort by category, then priority, then name
    std::sort(panel_cache_.begin(), panel_cache_.end(),
              [](const PalettePanel& panel_a, const PalettePanel& panel_b) {
                if (panel_a.category != panel_b.category) {
                  return panel_a.category < panel_b.category;
                }
                if (panel_a.priority != panel_b.priority) {
                  return panel_a.priority < panel_b.priority;
                }
                return panel_a.name < panel_b.name;
              });
  } else {
    // Fallback: Example panels for testing without PanelManager
    panel_cache_.push_back({"dungeon.room_selector", "Room List",
                            ICON_MD_LIST, "Dungeon", 
                            "Browse and select dungeon rooms", 20});
    panel_cache_.push_back({"dungeon.object_editor", "Object Editor",
                            ICON_MD_EDIT, "Dungeon",
                            "Edit room objects and properties", 30});
    panel_cache_.push_back({"dungeon.palette_editor", "Palette Editor",
                            ICON_MD_PALETTE, "Dungeon",
                            "Edit dungeon color palettes", 70});
    panel_cache_.push_back({"dungeon.room_graphics", "Room Graphics",
                            ICON_MD_IMAGE, "Dungeon",
                            "View room tileset graphics", 50});
    
    panel_cache_.push_back({"graphics.tile16_editor", "Tile16 Editor",
                            ICON_MD_GRID_ON, "Graphics",
                            "Edit 16x16 tile graphics", 10});
    panel_cache_.push_back({"graphics.sprite_editor", "Sprite Editor",
                            ICON_MD_PERSON, "Graphics",
                            "Edit sprite graphics", 20});
  }
  
  panel_cache_dirty_ = false;
  return panel_cache_;
}

void LayoutDesignerWindow::RefreshPanelCache() {
  panel_cache_dirty_ = true;
}

bool LayoutDesignerWindow::MatchesSearchFilter(const PalettePanel& panel) const {
  if (search_filter_[0] == '\0') {
    return true;  // Empty filter matches all
  }
  
  std::string filter_lower = search_filter_;
  std::transform(filter_lower.begin(), filter_lower.end(),
                 filter_lower.begin(), ::tolower);
  
  // Search in name
  std::string name_lower = panel.name;
  std::transform(name_lower.begin(), name_lower.end(),
                 name_lower.begin(), ::tolower);
  if (name_lower.find(filter_lower) != std::string::npos) {
    return true;
  }
  
  // Search in ID
  std::string id_lower = panel.id;
  std::transform(id_lower.begin(), id_lower.end(),
                 id_lower.begin(), ::tolower);
  if (id_lower.find(filter_lower) != std::string::npos) {
    return true;
  }
  
  // Search in description
  std::string desc_lower = panel.description;
  std::transform(desc_lower.begin(), desc_lower.end(),
                 desc_lower.begin(), ::tolower);
  if (desc_lower.find(filter_lower) != std::string::npos) {
    return true;
  }
  
  return false;
}

void LayoutDesignerWindow::NewLayout() {
  current_layout_ = std::make_unique<LayoutDefinition>(
      LayoutDefinition::CreateEmpty("New Layout"));
  selected_panel_ = nullptr;
  selected_node_ = nullptr;
  LOG_INFO("LayoutDesigner", "Created new layout");
}

void LayoutDesignerWindow::LoadLayout(const std::string& filepath) {
  LOG_INFO("LayoutDesigner", "Loading layout from: %s", filepath.c_str());
  
  auto result = LayoutSerializer::LoadFromFile(filepath);
  if (result.ok()) {
    current_layout_ = std::make_unique<LayoutDefinition>(std::move(result.value()));
    selected_panel_ = nullptr;
    selected_node_ = nullptr;
    LOG_INFO("LayoutDesigner", "Successfully loaded layout: %s",
             current_layout_->name.c_str());
  } else {
    LOG_ERROR("LayoutDesigner", "Failed to load layout: %s",
              result.status().message().data());
  }
}

void LayoutDesignerWindow::SaveLayout(const std::string& filepath) {
  if (!current_layout_) {
    LOG_ERROR("LayoutDesigner", "No layout to save");
    return;
  }
  
  LOG_INFO("LayoutDesigner", "Saving layout to: %s", filepath.c_str());
  
  auto status = LayoutSerializer::SaveToFile(*current_layout_, filepath);
  if (status.ok()) {
    LOG_INFO("LayoutDesigner", "Successfully saved layout");
  } else {
    LOG_ERROR("LayoutDesigner", "Failed to save layout: %s",
              status.message().data());
  }
}

void LayoutDesignerWindow::ImportFromRuntime() {
  LOG_INFO("LayoutDesigner", "Importing layout from runtime");
  
  if (!panel_manager_) {
    LOG_ERROR("LayoutDesigner", "PanelManager not available for import");
    return;
  }
  
  // Create new layout from runtime state
  current_layout_ = std::make_unique<LayoutDefinition>(
      LayoutDefinition::CreateEmpty("Imported Layout"));
  current_layout_->description = "Imported from runtime state";
  
  // Get all visible panels
  auto all_panels = panel_manager_->GetAllPanelDescriptors();
  
  // Add visible panels to layout
  for (const auto& [panel_id, descriptor] : all_panels) {
    LayoutPanel panel;
    panel.panel_id = panel_id;
    panel.display_name = descriptor.display_name;
    panel.icon = descriptor.icon;
    panel.priority = descriptor.priority;
    panel.visible_by_default = true;  // Currently visible
    panel.closable = true;
    panel.pinnable = true;
    
    // Add to root (simple flat layout for now)
    current_layout_->root->AddPanel(panel);
  }
  
  LOG_INFO("LayoutDesigner", "Imported %zu panels from runtime",
           all_panels.size());
}

void LayoutDesignerWindow::ImportPanelDesign(const std::string& panel_id) {
  LOG_INFO("LayoutDesigner", "Importing panel design: %s", panel_id.c_str());
  
  if (!panel_manager_) {
    LOG_ERROR("LayoutDesigner", "PanelManager not available");
    return;
  }
  
  auto all_panels = panel_manager_->GetAllPanelDescriptors();
  auto it = all_panels.find(panel_id);
  
  if (it == all_panels.end()) {
    LOG_ERROR("LayoutDesigner", "Panel not found: %s", panel_id.c_str());
    return;
  }
  
  // Create new panel design
  current_panel_design_ = std::make_unique<PanelDesign>();
  current_panel_design_->panel_id = panel_id;
  current_panel_design_->panel_name = it->second.display_name;
  selected_panel_for_design_ = panel_id;
  
  // Switch to widget design mode
  design_mode_ = DesignMode::WidgetDesign;
  
  LOG_INFO("LayoutDesigner", "Created panel design for: %s", 
           current_panel_design_->panel_name.c_str());
}

void LayoutDesignerWindow::ExportCode(const std::string& filepath) {
  LOG_INFO("LayoutDesigner", "Exporting code to: %s", filepath.c_str());
  // TODO(scawful): Write generated code to file
}

void LayoutDesignerWindow::PreviewLayout() {
  if (!current_layout_ || !current_layout_->root) {
    LOG_WARN("LayoutDesigner", "No layout loaded; cannot preview");
    return;
  }
  if (!layout_manager_ || !panel_manager_) {
    LOG_WARN("LayoutDesigner", "Preview requires LayoutManager and PanelManager");
    return;
  }

  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
  if (dockspace_id == 0) {
    LOG_WARN("LayoutDesigner", "MainDockSpace not found; cannot preview");
    return;
  }

  const size_t session_id = panel_manager_->GetActiveSessionId();
  if (ApplyLayoutToDockspace(current_layout_.get(), panel_manager_, session_id,
                             dockspace_id)) {
    last_drop_node_for_preview_ = current_layout_->root.get();
    LOG_INFO("LayoutDesigner", "Preview applied to dockspace %u (session %zu)",
             dockspace_id, session_id);
  } else {
    LOG_WARN("LayoutDesigner", "Preview failed to apply to dockspace %u",
             dockspace_id);
  }
}

void LayoutDesignerWindow::DrawDropZones(const ImVec2& pos, const ImVec2& size,
                                         DockNode* target_node) {
  if (!target_node) {
    LOG_WARN("DragDrop", "DrawDropZones called with null target_node");
    return;
  }
  
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 mouse_pos = ImGui::GetMousePos();
  ImVec2 rect_min = pos;
  ImVec2 rect_max = ImVec2(pos.x + size.x, pos.y + size.y);
  auto alpha_color = [](ImU32 base, float alpha_scale) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(base);
    c.w *= alpha_scale;
    return ImGui::ColorConvertFloat4ToU32(c);
  };
  
  // Determine which drop zone the mouse is in
  ImGuiDir zone = GetDropZone(mouse_pos, rect_min, rect_max);
  
  LOG_INFO("DragDrop", "Drawing drop zones, mouse zone: %d", static_cast<int>(zone));
  
  // Define drop zone sizes (20% of each edge)
  float zone_size = 0.25f;
  
  // Calculate drop zone rectangles
  struct DropZone {
    ImVec2 min;
    ImVec2 max;
    ImGuiDir dir;
  };
  
  std::vector<DropZone> zones = {
    // Left zone
    {rect_min,
     ImVec2(rect_min.x + size.x * zone_size, rect_max.y),
     ImGuiDir_Left},
    // Right zone
    {ImVec2(rect_max.x - size.x * zone_size, rect_min.y),
     rect_max,
     ImGuiDir_Right},
    // Top zone
    {rect_min,
     ImVec2(rect_max.x, rect_min.y + size.y * zone_size),
     ImGuiDir_Up},
    // Bottom zone
    {ImVec2(rect_min.x, rect_max.y - size.y * zone_size),
     rect_max,
     ImGuiDir_Down},
    // Center zone
    {ImVec2(rect_min.x + size.x * zone_size, rect_min.y + size.y * zone_size),
     ImVec2(rect_max.x - size.x * zone_size, rect_max.y - size.y * zone_size),
     ImGuiDir_None}
  };
  
  // Draw drop zones
  for (const auto& drop_zone : zones) {
    bool is_hovered = (zone == drop_zone.dir);
    ImU32 base_zone = ImGui::GetColorU32(ImGuiCol_Header);
    ImU32 color = is_hovered 
        ? alpha_color(base_zone, 0.8f)
        : alpha_color(base_zone, 0.35f);
    
    draw_list->AddRectFilled(drop_zone.min, drop_zone.max, color, 4.0f);
    draw_list->AddRect(drop_zone.min, drop_zone.max,
                       ImGui::GetColorU32(ImGuiCol_HeaderActive), 4.0f, 0, 1.0f);
    
    if (is_hovered) {
      // Store the target for when drop happens
      drop_target_node_ = target_node;
      drop_direction_ = zone;
      
      LOG_INFO("DragDrop", "✓ Drop target set: zone=%d", static_cast<int>(zone));
      
      // Draw direction indicator
      const char* dir_text = "";
      switch (zone) {
        case ImGuiDir_Left: dir_text = "← Left 30%"; break;
        case ImGuiDir_Right: dir_text = "Right 30% →"; break;
        case ImGuiDir_Up: dir_text = "↑ Top 30%"; break;
        case ImGuiDir_Down: dir_text = "↓ Bottom 30%"; break;
        case ImGuiDir_None: dir_text = "⊕ Add to Center"; break;
        case ImGuiDir_COUNT: break;
      }
      
      ImVec2 text_size = ImGui::CalcTextSize(dir_text);
      ImVec2 text_pos = ImVec2(
          (drop_zone.min.x + drop_zone.max.x - text_size.x) / 2,
          (drop_zone.min.y + drop_zone.max.y - text_size.y) / 2);
      
      // Draw background for text
      draw_list->AddRectFilled(
          ImVec2(text_pos.x - 5, text_pos.y - 2),
          ImVec2(text_pos.x + text_size.x + 5, text_pos.y + text_size.y + 2),
          IM_COL32(0, 0, 0, 200), 4.0f);
      
      draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), dir_text);
    }
  }
}

bool LayoutDesignerWindow::IsMouseOverRect(const ImVec2& rect_min,
                                           const ImVec2& rect_max) const {
  ImVec2 mouse_pos = ImGui::GetMousePos();
  return mouse_pos.x >= rect_min.x && mouse_pos.x <= rect_max.x &&
         mouse_pos.y >= rect_min.y && mouse_pos.y <= rect_max.y;
}

ImGuiDir LayoutDesignerWindow::GetDropZone(const ImVec2& mouse_pos,
                                            const ImVec2& rect_min,
                                            const ImVec2& rect_max) const {
  if (!IsMouseOverRect(rect_min, rect_max)) {
    return ImGuiDir_None;
  }
  
  float zone_size = 0.25f;  // 25% of each edge
  
  ImVec2 size = ImVec2(rect_max.x - rect_min.x, rect_max.y - rect_min.y);
  ImVec2 relative_pos = ImVec2(mouse_pos.x - rect_min.x, mouse_pos.y - rect_min.y);
  
  // Check edge zones first (they have priority over center)
  if (relative_pos.x < size.x * zone_size) {
    return ImGuiDir_Left;
  }
  if (relative_pos.x > size.x * (1.0f - zone_size)) {
    return ImGuiDir_Right;
  }
  if (relative_pos.y < size.y * zone_size) {
    return ImGuiDir_Up;
  }
  if (relative_pos.y > size.y * (1.0f - zone_size)) {
    return ImGuiDir_Down;
  }
  
  // Center zone (tab addition)
  return ImGuiDir_None;
}

void LayoutDesignerWindow::ResetDropState() {
  drop_target_node_ = nullptr;
  drop_direction_ = ImGuiDir_None;
}

std::optional<LayoutDesignerWindow::PalettePanel>
LayoutDesignerWindow::ResolvePanelById(const std::string& panel_id) const {
  if (panel_id.empty()) {
    return std::nullopt;
  }

  if (panel_manager_) {
    const auto& descriptors = panel_manager_->GetAllPanelDescriptors();
    auto it = descriptors.find(panel_id);
    if (it != descriptors.end()) {
      PalettePanel panel;
      panel.id = panel_id;
      panel.name = it->second.display_name;
      panel.icon = it->second.icon;
      panel.category = it->second.category;
      panel.priority = it->second.priority;
      return panel;
    }
  }

  // Fallback: cached palette
  auto available = GetAvailablePanels();
  for (const auto& cached : available) {
    if (cached.id == panel_id) {
      return cached;
    }
  }

  return std::nullopt;
}

void LayoutDesignerWindow::AddPanelToTarget(const PalettePanel& panel) {
  if (!current_layout_) {
    LOG_WARN("LayoutDesigner", "No active layout; cannot add panel");
    return;
  }

  DockNode* target = drop_target_node_;
  if (!target) {
    target = current_layout_->root.get();
  }

  LayoutPanel new_panel;
  new_panel.panel_id = panel.id;
  new_panel.display_name = panel.name;
  new_panel.icon = panel.icon;
  new_panel.priority = panel.priority;
  new_panel.visible_by_default = true;
  new_panel.closable = true;
  new_panel.pinnable = true;

  LOG_INFO("DragDrop", "Creating panel: %s, target node type: %s",
           new_panel.display_name.c_str(),
           target->IsLeaf() ? "Leaf" : "Split");

  // Empty leaf/root: drop directly
  if (target->IsLeaf() && target->panels.empty()) {
    target->AddPanel(new_panel);
    current_layout_->Touch();
    ResetDropState();
    LOG_INFO("LayoutDesigner", "✓ Added panel '%s' to leaf/root", panel.name.c_str());
    return;
  }

  // Otherwise require a drop direction to split
  if (drop_direction_ == ImGuiDir_None) {
    LOG_WARN("DragDrop", "No drop direction set; ignoring drop");
    return;
  }

  float split_ratio = (drop_direction_ == ImGuiDir_Right || drop_direction_ == ImGuiDir_Down)
                          ? 0.7f
                          : 0.3f;

  target->Split(drop_direction_, split_ratio);

  DockNode* child = (drop_direction_ == ImGuiDir_Left || drop_direction_ == ImGuiDir_Up)
                        ? target->child_left.get()
                        : target->child_right.get();
  if (child) {
    child->AddPanel(new_panel);
    LOG_INFO("LayoutDesigner", "✓ Added panel to split child (dir=%d)", drop_direction_);
  } else {
    LOG_WARN("LayoutDesigner", "Split child missing; drop ignored");
  }

  current_layout_->Touch();
  ResetDropState();
}

// ============================================================================
// Widget Design Mode UI
// ============================================================================

void LayoutDesignerWindow::DrawWidgetPalette() {
  ImGui::Text(ICON_MD_WIDGETS " Widget Palette");
  ImGui::Separator();
  
  // Search bar
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##widget_search", ICON_MD_SEARCH " Search widgets...",
                           widget_search_filter_, sizeof(widget_search_filter_));
  
  // Category filter
  ImGui::SetNextItemWidth(-1);
  const char* categories[] = {"All", "Basic", "Layout", "Containers", "Tables", "Custom"};
  if (ImGui::BeginCombo("##widget_category", selected_widget_category_.c_str())) {
    for (const char* cat : categories) {
      if (ImGui::Selectable(cat, selected_widget_category_ == cat)) {
        selected_widget_category_ = cat;
      }
    }
    ImGui::EndCombo();
  }
  
  ImGui::Spacing();
  ImGui::Separator();
  
  // Widget list by category
  struct WidgetPaletteItem {
    WidgetType type;
    std::string category;
  };
  
  std::vector<WidgetPaletteItem> widgets = {
    // Basic Widgets
    {WidgetType::Text, "Basic"},
    {WidgetType::TextWrapped, "Basic"},
    {WidgetType::Button, "Basic"},
    {WidgetType::SmallButton, "Basic"},
    {WidgetType::Checkbox, "Basic"},
    {WidgetType::InputText, "Basic"},
    {WidgetType::InputInt, "Basic"},
    {WidgetType::SliderInt, "Basic"},
    {WidgetType::SliderFloat, "Basic"},
    {WidgetType::ColorEdit, "Basic"},
    
    // Layout Widgets
    {WidgetType::Separator, "Layout"},
    {WidgetType::SameLine, "Layout"},
    {WidgetType::Spacing, "Layout"},
    {WidgetType::Dummy, "Layout"},
    
    // Container Widgets
    {WidgetType::BeginGroup, "Containers"},
    {WidgetType::BeginChild, "Containers"},
    {WidgetType::CollapsingHeader, "Containers"},
    {WidgetType::TreeNode, "Containers"},
    {WidgetType::TabBar, "Containers"},
    
    // Table Widgets
    {WidgetType::BeginTable, "Tables"},
    {WidgetType::TableNextRow, "Tables"},
    {WidgetType::TableNextColumn, "Tables"},
    
    // Custom Widgets
    {WidgetType::Canvas, "Custom"},
    {WidgetType::ProgressBar, "Custom"},
    {WidgetType::Image, "Custom"},
  };
  
  // Group by category
  std::string current_category;
  int visible_count = 0;
  
  for (const auto& item : widgets) {
    // Category filter
    if (selected_widget_category_ != "All" && item.category != selected_widget_category_) {
      continue;
    }
    
    // Search filter
    if (widget_search_filter_[0] != '\0') {
      std::string name_lower = GetWidgetTypeName(item.type);
      std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
      std::string filter_lower = widget_search_filter_;
      std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);
      if (name_lower.find(filter_lower) == std::string::npos) {
        continue;
      }
    }
    
    // Category header
    if (item.category != current_category) {
      if (!current_category.empty()) {
        ImGui::Spacing();
      }
      current_category = item.category;
      ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", current_category.c_str());
      ImGui::Separator();
    }
    
    // Widget item
    ImGui::PushID(static_cast<int>(item.type));
    std::string label = absl::StrFormat("%s %s", 
                                        GetWidgetTypeIcon(item.type),
                                        GetWidgetTypeName(item.type));
    
    if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0, 28))) {
      LOG_INFO("LayoutDesigner", "Selected widget: %s", GetWidgetTypeName(item.type));
    }
    
    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      ImGui::SetDragDropPayload("WIDGET_TYPE", &item.type, sizeof(WidgetType));
      ImGui::Text("%s", label.c_str());
      ImGui::TextDisabled("Drag to canvas");
      ImGui::EndDragDropSource();
    }
    
    ImGui::PopID();
    visible_count++;
  }
  
  ImGui::Separator();
  ImGui::TextDisabled("%d widgets available", visible_count);
}

void LayoutDesignerWindow::DrawWidgetCanvas() {
  ImGui::Text(ICON_MD_DRAW " Widget Canvas");
  ImGui::SameLine();
  ImGui::TextDisabled("Design panel internal layout");
  ImGui::Separator();
  
  if (!current_panel_design_) {
    ImGui::TextWrapped("No panel design loaded.");
    ImGui::Spacing();
    ImGui::TextWrapped("Create a new panel design or select a panel from the Panel Layout mode.");
    
    if (ImGui::Button("Create New Panel Design")) {
      current_panel_design_ = std::make_unique<PanelDesign>();
      current_panel_design_->panel_id = "new_panel";
      current_panel_design_->panel_name = "New Panel";
    }
    return;
  }
  
  // Panel info
  ImGui::Text("Panel: %s", current_panel_design_->panel_name.c_str());
  ImGui::Separator();
  
  // Canvas area
  ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_size = ImGui::GetContentRegionAvail();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  
  // Background
  draw_list->AddRectFilled(canvas_pos, 
                           ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                           IM_COL32(30, 30, 35, 255));
  
  // Grid
  const float grid_step = 20.0f;
  for (float x_pos = 0; x_pos < canvas_size.x; x_pos += grid_step) {
    draw_list->AddLine(
        ImVec2(canvas_pos.x + x_pos, canvas_pos.y),
        ImVec2(canvas_pos.x + x_pos, canvas_pos.y + canvas_size.y),
        IM_COL32(50, 50, 55, 255));
  }
  for (float y_pos = 0; y_pos < canvas_size.y; y_pos += grid_step) {
    draw_list->AddLine(
        ImVec2(canvas_pos.x, canvas_pos.y + y_pos),
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y_pos),
        IM_COL32(50, 50, 55, 255));
  }
  
  // Draw widgets
  ImVec2 widget_pos = ImVec2(canvas_pos.x + 20, canvas_pos.y + 20);
  for (const auto& widget : current_panel_design_->widgets) {
    // Draw widget preview
    const char* icon = GetWidgetTypeIcon(widget->type);
    const char* name = GetWidgetTypeName(widget->type);
    std::string label = absl::StrFormat("%s %s", icon, name);
    
    ImU32 color = (selected_widget_ == widget.get()) 
        ? IM_COL32(255, 200, 100, 255)  // Selected: Orange
        : IM_COL32(100, 150, 200, 255);  // Normal: Blue
    
    ImVec2 widget_size = ImVec2(200, 30);
    draw_list->AddRectFilled(widget_pos, 
                             ImVec2(widget_pos.x + widget_size.x, widget_pos.y + widget_size.y),
                             color, 4.0f);
    draw_list->AddText(ImVec2(widget_pos.x + 10, widget_pos.y + 8),
                       IM_COL32(255, 255, 255, 255), label.c_str());
    
    // Check if clicked
    ImVec2 mouse_pos = ImGui::GetMousePos();
    if (mouse_pos.x >= widget_pos.x && mouse_pos.x <= widget_pos.x + widget_size.x &&
        mouse_pos.y >= widget_pos.y && mouse_pos.y <= widget_pos.y + widget_size.y) {
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        selected_widget_ = widget.get();
        LOG_INFO("LayoutDesigner", "Selected widget: %s", widget->id.c_str());
      }
    }
    
    widget_pos.y += widget_size.y + 10;
  }
  
  // Drop target for new widgets
  ImGui::Dummy(canvas_size);
  if (ImGui::BeginDragDropTarget()) {
    // Handle standard widget drops
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WIDGET_TYPE")) {
      WidgetType* widget_type = static_cast<WidgetType*>(payload->Data);
      
      // Create new widget
      auto new_widget = std::make_unique<WidgetDefinition>();
      new_widget->id = absl::StrFormat("widget_%zu", current_panel_design_->widgets.size());
      new_widget->type = *widget_type;
      new_widget->label = GetWidgetTypeName(*widget_type);
      
      // Add default properties
      auto props = GetDefaultProperties(*widget_type);
      for (const auto& prop : props) {
        new_widget->properties.push_back(prop);
      }
      
      current_panel_design_->AddWidget(std::move(new_widget));
      LOG_INFO("LayoutDesigner", "Added widget: %s", GetWidgetTypeName(*widget_type));
    }
    
    // Handle yaze widget drops
    ImGui::EndDragDropTarget();
  }
}

void LayoutDesignerWindow::DrawWidgetProperties() {
  ImGui::Text(ICON_MD_TUNE " Widget Properties");
  ImGui::Separator();
  
  if (!selected_widget_) {
    ImGui::TextWrapped("Select a widget to edit its properties");
    
    if (current_panel_design_) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Text("Panel: %s", current_panel_design_->panel_name.c_str());
      ImGui::Text("Widgets: %zu", current_panel_design_->widgets.size());
      
      if (ImGui::Button("Clear All Widgets")) {
        current_panel_design_->widgets.clear();
        selected_widget_ = nullptr;
      }
    }
    return;
  }
  
  // Widget info
  ImGui::Text("Widget: %s", selected_widget_->id.c_str());
  ImGui::Text("Type: %s", GetWidgetTypeName(selected_widget_->type));
  ImGui::Separator();
  
  // Edit properties
  for (auto& prop : selected_widget_->properties) {
    ImGui::PushID(prop.name.c_str());
    
    switch (prop.type) {
      case WidgetProperty::Type::String: {
        char buffer[256];
        strncpy(buffer, prop.string_value.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText(prop.name.c_str(), buffer, sizeof(buffer))) {
          prop.string_value = buffer;
        }
        break;
      }
      case WidgetProperty::Type::Int:
        ImGui::InputInt(prop.name.c_str(), &prop.int_value);
        break;
      case WidgetProperty::Type::Float:
        ImGui::InputFloat(prop.name.c_str(), &prop.float_value);
        break;
      case WidgetProperty::Type::Bool:
        ImGui::Checkbox(prop.name.c_str(), &prop.bool_value);
        break;
      case WidgetProperty::Type::Color:
        ImGui::ColorEdit4(prop.name.c_str(), &prop.color_value.x);
        break;
      case WidgetProperty::Type::Vec2:
        ImGui::InputFloat2(prop.name.c_str(), &prop.vec2_value.x);
        break;
      default:
        ImGui::Text("%s: (unsupported type)", prop.name.c_str());
        break;
    }
    
    ImGui::PopID();
  }
  
  ImGui::Separator();
  
  // Widget options
  ImGui::Checkbox("Same Line", &selected_widget_->same_line);
  
  char tooltip_buffer[256];
  strncpy(tooltip_buffer, selected_widget_->tooltip.c_str(), sizeof(tooltip_buffer) - 1);
  tooltip_buffer[sizeof(tooltip_buffer) - 1] = '\0';
  if (ImGui::InputText("Tooltip", tooltip_buffer, sizeof(tooltip_buffer))) {
    selected_widget_->tooltip = tooltip_buffer;
  }
  
  char callback_buffer[256];
  strncpy(callback_buffer, selected_widget_->callback_name.c_str(), sizeof(callback_buffer) - 1);
  callback_buffer[sizeof(callback_buffer) - 1] = '\0';
  if (ImGui::InputText("Callback", callback_buffer, sizeof(callback_buffer))) {
    selected_widget_->callback_name = callback_buffer;
  }
  
  ImGui::Separator();
  
  if (ImGui::Button("Delete Widget")) {
    // TODO(scawful): Implement widget deletion
    LOG_INFO("LayoutDesigner", "Delete widget: %s", selected_widget_->id.c_str());
  }
}

void LayoutDesignerWindow::DrawWidgetTree() {
  ImGui::Text(ICON_MD_ACCOUNT_TREE " Widget Tree");
  ImGui::Separator();
  
  if (!current_panel_design_) {
    ImGui::TextWrapped("No panel design loaded");
    return;
  }
  
  // Show widget hierarchy
  for (const auto& widget : current_panel_design_->widgets) {
    bool is_selected = (selected_widget_ == widget.get());
    if (ImGui::Selectable(absl::StrFormat("%s %s", 
                                          GetWidgetTypeIcon(widget->type),
                                          widget->id).c_str(), is_selected)) {
      selected_widget_ = widget.get();
    }
  }
}

void LayoutDesignerWindow::DrawWidgetCodePreview() {
  if (!current_panel_design_) {
    ImGui::Text("// No panel design loaded");
    return;
  }
  
  // Generate and display code
  std::string code = WidgetCodeGenerator::GeneratePanelDrawMethod(*current_panel_design_);
  ImGui::TextUnformatted(code.c_str());
}

void LayoutDesignerWindow::DrawThemeProperties() {
  ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
  
  if (ImGui::Begin(ICON_MD_PALETTE " Theme Properties", &show_theme_panel_)) {
    if (theme_panel_.Draw(theme_properties_)) {
      // Theme was modified
    }
    
    ImGui::Separator();
    
    if (ImGui::CollapsingHeader("Generated Code")) {
      std::string code = theme_properties_.GenerateStyleCode();
      ImGui::TextUnformatted(code.c_str());
      
      if (ImGui::Button("Copy Code to Clipboard")) {
        ImGui::SetClipboardText(code.c_str());
      }
    }
  }
  ImGui::End();
}

}  // namespace layout_designer
}  // namespace editor
}  // namespace yaze
