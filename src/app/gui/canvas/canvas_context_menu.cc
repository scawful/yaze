#include "canvas_context_menu.h"

#include "app/gfx/resource/arena.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/debug/performance/performance_dashboard.h"
#include "app/gui/widgets/palette_widget.h"
#include "app/gui/icons.h"
#include "app/gui/color.h"
#include "app/gui/canvas/canvas_modals.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {
namespace canvas {

namespace {
inline void Dispatch(
    const std::function<void(CanvasContextMenu::Command, const CanvasConfig&)>& handler,
    CanvasContextMenu::Command command, CanvasConfig config) {
  if (handler) {
    handler(command, config);
  }
}
}  // namespace

void CanvasContextMenu::Initialize(const std::string& canvas_id) {
  canvas_id_ = canvas_id;
  enabled_ = true;
  current_usage_ = CanvasUsage::kTilePainting;
  palette_editor_ = std::make_unique<PaletteWidget>();
  
  // Initialize canvas state
  canvas_size_ = ImVec2(0, 0);
  content_size_ = ImVec2(0, 0);
  global_scale_ = 1.0F;
  grid_step_ = 32.0F;
  enable_grid_ = true;
  enable_hex_labels_ = false;
  enable_custom_labels_ = false;
  enable_context_menu_ = true;
  is_draggable_ = false;
  auto_resize_ = false;
  scrolling_ = ImVec2(0, 0);
  
  // Create default menu items
  CreateDefaultMenuItems();
}

void CanvasContextMenu::SetUsageMode(CanvasUsage usage) {
  current_usage_ = usage;
}

void CanvasContextMenu::AddMenuItem(const ContextMenuItem& item) {
  global_items_.push_back(item);
}

void CanvasContextMenu::AddMenuItem(const ContextMenuItem& item, CanvasUsage usage) {
  usage_specific_items_[usage].push_back(item);
}

void CanvasContextMenu::ClearMenuItems() {
  global_items_.clear();
  usage_specific_items_.clear();
}

void CanvasContextMenu::Render(const std::string& context_id, const ImVec2& mouse_pos, Rom* rom,
                              const gfx::Bitmap* bitmap, const gfx::SnesPalette* palette,
                              const std::function<void(Command, const CanvasConfig&)>& command_handler,
                              CanvasConfig current_config) {
  if (!enabled_) return;
  
  // Context menu (under default mouse threshold)
  if (ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
      enable_context_menu_ && drag_delta.x == 0.0F && drag_delta.y == 0.0F) {
    ImGui::OpenPopupOnItemClick(context_id.c_str(), ImGuiPopupFlags_MouseButtonRight);
  }
  
  // Contents of the Context Menu
  if (ImGui::BeginPopup(context_id.c_str())) {
    // Render usage-specific menu first
    RenderUsageSpecificMenu();
    
    // Add separator if there are usage-specific items
    if (!usage_specific_items_[current_usage_].empty()) {
      ImGui::Separator();
    }
    
    // Render view controls
    RenderViewControlsMenu(command_handler, current_config);
    ImGui::Separator();
    
    // Render canvas properties
    RenderCanvasPropertiesMenu(command_handler, current_config);
    ImGui::Separator();
    
    // Render bitmap operations if bitmap is available
    if (bitmap) {
      RenderBitmapOperationsMenu(const_cast<gfx::Bitmap*>(bitmap));
      ImGui::Separator();
    }
    
    // Render palette operations if bitmap is available
    if (bitmap) {
      RenderPaletteOperationsMenu(rom, const_cast<gfx::Bitmap*>(bitmap));
      ImGui::Separator();
    }

    if (bitmap) {
      RenderBppOperationsMenu(bitmap);
      ImGui::Separator();
    }

    RenderPerformanceMenu();
    ImGui::Separator();

    RenderGridControlsMenu(command_handler, current_config);
    ImGui::Separator();

    RenderScalingControlsMenu(command_handler, current_config);

    // Render global menu items
    if (!global_items_.empty()) {
      ImGui::Separator();
      RenderMenuSection("Custom Actions", global_items_);
    }
    
    ImGui::EndPopup();
  }
}

bool CanvasContextMenu::ShouldShowContextMenu() const {
  return enabled_ && enable_context_menu_;
}

void CanvasContextMenu::SetCanvasState(const ImVec2& canvas_size, 
                                      const ImVec2& content_size,
                                      float global_scale,
                                      float grid_step,
                                      bool enable_grid,
                                      bool enable_hex_labels,
                                      bool enable_custom_labels,
                                      bool enable_context_menu,
                                      bool is_draggable,
                                      bool auto_resize,
                                      const ImVec2& scrolling) {
  canvas_size_ = canvas_size;
  content_size_ = content_size;
  global_scale_ = global_scale;
  grid_step_ = grid_step;
  enable_grid_ = enable_grid;
  enable_hex_labels_ = enable_hex_labels_;
  enable_custom_labels_ = enable_custom_labels_;
  enable_context_menu_ = enable_context_menu_;
  is_draggable_ = is_draggable_;
  auto_resize_ = auto_resize_;
  scrolling_ = scrolling;
}

void CanvasContextMenu::RenderMenuItem(const ContextMenuItem& item) {
  if (!item.visible_condition()) {
    return;
  }
  
  if (!item.enabled_condition()) {
    ImGui::BeginDisabled();
  }
  
  if (item.subitems.empty()) {
    // Simple menu item
    if (ImGui::MenuItem(item.label.c_str(), 
                       item.shortcut.empty() ? nullptr : item.shortcut.c_str())) {
      item.callback();
    }
  } else {
    // Menu with subitems
    if (ImGui::BeginMenu(item.label.c_str())) {
      for (const auto& subitem : item.subitems) {
        RenderMenuItem(subitem);
      }
      ImGui::EndMenu();
    }
  }
  
  if (!item.enabled_condition()) {
    ImGui::EndDisabled();
  }
  
  if (item.separator_after) {
    ImGui::Separator();
  }
}

void CanvasContextMenu::RenderMenuSection(const std::string& title, 
                                         const std::vector<ContextMenuItem>& items) {
  if (items.empty()) return;
  
  ImGui::TextColored(ImVec4(0.7F, 0.7F, 0.7F, 1.0F), "%s", title.c_str());
  for (const auto& item : items) {
    RenderMenuItem(item);
  }
}

void CanvasContextMenu::RenderUsageSpecificMenu() {
  auto it = usage_specific_items_.find(current_usage_);
  if (it == usage_specific_items_.end() || it->second.empty()) {
    return;
  }
  
  std::string usage_name = GetUsageModeName(current_usage_);
  ImVec4 usage_color = GetUsageModeColor(current_usage_);
  
  ImGui::TextColored(usage_color, "%s %s Mode", ICON_MD_COLOR_LENS, usage_name.c_str());
  ImGui::Separator();
  
  for (const auto& item : it->second) {
    RenderMenuItem(item);
  }
}

void CanvasContextMenu::RenderViewControlsMenu(
    const std::function<void(Command, const CanvasConfig&)>& command_handler,
    CanvasConfig current_config) {
  if (ImGui::BeginMenu("View Controls")) {
    if (ImGui::MenuItem("Reset View", "Ctrl+R")) {
      Dispatch(command_handler, Command::kResetView, current_config);
    }
    if (ImGui::MenuItem("Zoom to Fit", "Ctrl+F")) {
      Dispatch(command_handler, Command::kZoomToFit, current_config);
    }
    if (ImGui::MenuItem("Zoom In", "Ctrl++")) {
      CanvasConfig updated = current_config;
      updated.global_scale *= 1.25F;
      Dispatch(command_handler, Command::kSetScale, updated);
    }
    if (ImGui::MenuItem("Zoom Out", "Ctrl+-")) {
      CanvasConfig updated = current_config;
      updated.global_scale *= 0.8F;
      Dispatch(command_handler, Command::kSetScale, updated);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Show Grid", nullptr, enable_grid_)) {
      CanvasConfig updated = current_config;
      updated.enable_grid = !enable_grid_;
      Dispatch(command_handler, Command::kToggleGrid, updated);
    }
    if (ImGui::MenuItem("Show Hex Labels", nullptr, enable_hex_labels_)) {
      CanvasConfig updated = current_config;
      updated.enable_hex_labels = !enable_hex_labels_;
      Dispatch(command_handler, Command::kToggleHexLabels, updated);
    }
    if (ImGui::MenuItem("Show Custom Labels", nullptr, enable_custom_labels_)) {
      CanvasConfig updated = current_config;
      updated.enable_custom_labels = !enable_custom_labels_;
      Dispatch(command_handler, Command::kToggleCustomLabels, updated);
    }
    ImGui::EndMenu();
  }
}

void CanvasContextMenu::RenderCanvasPropertiesMenu(
    const std::function<void(Command, const CanvasConfig&)>& command_handler,
    CanvasConfig current_config) {
  if (ImGui::BeginMenu(ICON_MD_SETTINGS " Canvas Properties")) {
    ImGui::Text("Canvas Size: %.0f x %.0f", canvas_size_.x, canvas_size_.y);
    ImGui::Text("Content Size: %.0f x %.0f", content_size_.x, content_size_.y);
    ImGui::Text("Global Scale: %.2f", global_scale_);
    ImGui::Text("Grid Step: %.1f", grid_step_);
    ImGui::Text("Mouse Position: %.0f x %.0f", 0.0F, 0.0F); // Would need actual mouse pos
    
    if (ImGui::MenuItem("Advanced Properties...")) {
      CanvasConfig updated = current_config;
      updated.enable_grid = enable_grid_;
      updated.enable_hex_labels = enable_hex_labels_;
      updated.enable_custom_labels = enable_custom_labels_;
      updated.enable_context_menu = enable_context_menu_;
      updated.is_draggable = is_draggable_;
      updated.auto_resize = auto_resize_;
      updated.grid_step = grid_step_;
      updated.canvas_size = canvas_size_;
      updated.content_size = content_size_;
      updated.scrolling = scrolling_;
      Dispatch(command_handler, Command::kOpenAdvancedProperties, updated);
    }
    
    ImGui::EndMenu();
  }
}

void CanvasContextMenu::RenderBitmapOperationsMenu(gfx::Bitmap* bitmap) {
  if (!bitmap) return;

  if (ImGui::BeginMenu(ICON_MD_IMAGE " Bitmap Properties")) {
    ImGui::Text("Size: %d x %d", bitmap->width(), bitmap->height());
    ImGui::Text("Pitch: %d", bitmap->surface()->pitch);
    ImGui::Text("BitsPerPixel: %d", bitmap->surface()->format->BitsPerPixel);
    ImGui::Text("BytesPerPixel: %d", bitmap->surface()->format->BytesPerPixel);

    if (ImGui::BeginMenu("Format")) {
      if (ImGui::MenuItem("Indexed")) {
        bitmap->Reformat(gfx::BitmapFormat::kIndexed);
        // Queue texture update via Arena's deferred system
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, bitmap);
      }
      if (ImGui::MenuItem("4BPP")) {
        bitmap->Reformat(gfx::BitmapFormat::k4bpp);
        // Queue texture update via Arena's deferred system
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, bitmap);
      }
      if (ImGui::MenuItem("8BPP")) {
        bitmap->Reformat(gfx::BitmapFormat::k8bpp);
        // Queue texture update via Arena's deferred system
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, bitmap);
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
}

void CanvasContextMenu::RenderPaletteOperationsMenu(Rom* rom, gfx::Bitmap* bitmap) {
  if (!bitmap) return;

  if (ImGui::BeginMenu(ICON_MD_PALETTE " Palette Operations")) {
    if (ImGui::MenuItem("Edit Palette...")) {
      palette_editor_->ShowPaletteEditor(*bitmap->mutable_palette(), "Palette Editor");
    }
    if (ImGui::MenuItem("Color Analysis...")) {
      palette_editor_->ShowColorAnalysis(*bitmap, "Color Analysis");
    }

    if (rom && ImGui::BeginMenu("ROM Palette Selection")) {
      palette_editor_->Initialize(rom);
      
      // Render palette selector inline
      ImGui::Text("Group:"); ImGui::SameLine();
      ImGui::InputScalar("##group", ImGuiDataType_U64, &edit_palette_group_name_index_);
      ImGui::Text("Palette:"); ImGui::SameLine();
      ImGui::InputScalar("##palette", ImGuiDataType_U64, &edit_palette_index_);

      if (ImGui::Button("Apply to Canvas")) {
        palette_editor_->ApplyROMPalette(bitmap, edit_palette_group_name_index_, edit_palette_index_);
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View Palette")) {
      DisplayEditablePalette(*bitmap->mutable_palette(), "Palette", true, 8);
      ImGui::EndMenu();
    }
    ImGui::EndMenu();
  }
}

void CanvasContextMenu::DrawROMPaletteSelector() {
    if (!palette_editor_) return;

    palette_editor_->DrawROMPaletteSelector();
}

void CanvasContextMenu::RenderBppOperationsMenu(const gfx::Bitmap* bitmap) {
  if (ImGui::BeginMenu(ICON_MD_SWAP_HORIZ " BPP Operations")) {
    if (ImGui::MenuItem("Format Analysis...")) {
      // Open BPP analysis
    }
    if (ImGui::MenuItem("Convert Format...")) {
      // Open BPP conversion dialog
    }
    if (ImGui::MenuItem("Format Comparison...")) {
      // Open format comparison tool
    }
    
    ImGui::EndMenu();
  }
}

void CanvasContextMenu::RenderPerformanceMenu() {
  if (ImGui::BeginMenu(ICON_MD_TRENDING_UP " Performance")) {
    auto& profiler = gfx::PerformanceProfiler::Get();
    auto canvas_stats = profiler.GetStats("canvas_operations");
    auto draw_stats = profiler.GetStats("canvas_draw");
    
    ImGui::Text("Canvas Operations: %zu", canvas_stats.sample_count);
    ImGui::Text("Average Time: %.2f ms", draw_stats.avg_time_us / 1000.0);
    
    if (ImGui::MenuItem("Performance Dashboard...")) {
      gfx::PerformanceDashboard::Get().SetVisible(true);
    }
    if (ImGui::MenuItem("Usage Report...")) {
      // Open usage report
    }
    
    ImGui::EndMenu();
  }
}

void CanvasContextMenu::RenderGridControlsMenu(
    const std::function<void(Command, const CanvasConfig&)>& command_handler,
    CanvasConfig current_config) {
  if (ImGui::BeginMenu(ICON_MD_GRID_ON " Grid Controls")) {
    const struct GridOption {
      const char* label;
      float value;
    } options[] = {{"8x8", 8.0F}, {"16x16", 16.0F},
                   {"32x32", 32.0F}, {"64x64", 64.0F}};

    for (const auto& option : options) {
      bool selected = grid_step_ == option.value;
      if (ImGui::MenuItem(option.label, nullptr, selected)) {
        CanvasConfig updated = current_config;
        updated.grid_step = option.value;
        Dispatch(command_handler, Command::kSetGridStep, updated);
      }
    }

    ImGui::EndMenu();
  }
}

void CanvasContextMenu::RenderScalingControlsMenu(
    const std::function<void(Command, const CanvasConfig&)>& command_handler,
    CanvasConfig current_config) {
  if (ImGui::BeginMenu(ICON_MD_ZOOM_IN " Scaling Controls")) {
    const struct ScaleOption {
      const char* label;
      float value;
    } options[] = {{"0.25x", 0.25F}, {"0.5x", 0.5F}, {"1x", 1.0F},
                   {"2x", 2.0F},   {"4x", 4.0F},   {"8x", 8.0F}};

    for (const auto& option : options) {
      if (ImGui::MenuItem(option.label)) {
        CanvasConfig updated = current_config;
        updated.global_scale = option.value;
        Dispatch(command_handler, Command::kSetScale, updated);
      }
    }

    ImGui::EndMenu();
  }
}

void CanvasContextMenu::RenderMaterialIcon(const std::string& icon_name, const ImVec4& color) {
  // Simple material icon rendering using Unicode symbols
  static std::unordered_map<std::string, const char*> icon_map = {
    {"grid_on", ICON_MD_GRID_ON}, {"label", ICON_MD_LABEL}, {"edit", ICON_MD_EDIT}, {"menu", ICON_MD_MENU},
    {"drag_indicator", ICON_MD_DRAG_INDICATOR}, {"fit_screen", ICON_MD_FIT_SCREEN}, {"zoom_in", ICON_MD_ZOOM_IN},
    {"speed", ICON_MD_SPEED}, {"timer", ICON_MD_TIMER}, {"functions", ICON_MD_FUNCTIONS}, {"schedule", ICON_MD_SCHEDULE},
    {"refresh", ICON_MD_REFRESH}, {"settings", ICON_MD_SETTINGS}, {"info", ICON_MD_INFO},
    {"view", ICON_MD_VISIBILITY}, {"properties", ICON_MD_SETTINGS}, {"bitmap", ICON_MD_IMAGE}, {"palette", ICON_MD_PALETTE},
    {"bpp", ICON_MD_SWAP_HORIZ}, {"performance", ICON_MD_TRENDING_UP}, {"grid", ICON_MD_GRID_ON}, {"scaling", ICON_MD_ZOOM_IN}
  };
  
  auto it = icon_map.find(icon_name);
  if (it != icon_map.end()) {
    ImGui::TextColored(color, "%s", it->second);
  }
}

std::string CanvasContextMenu::GetUsageModeName(CanvasUsage usage) const {
  switch (usage) {
    case CanvasUsage::kTilePainting: return "Tile Painting";
    case CanvasUsage::kTileSelecting: return "Tile Selecting";
    case CanvasUsage::kSelectRectangle: return "Rectangle Selection";
    case CanvasUsage::kColorPainting: return "Color Painting";
    case CanvasUsage::kBitmapEditing: return "Bitmap Editing";
    case CanvasUsage::kPaletteEditing: return "Palette Editing";
    case CanvasUsage::kBppConversion: return "BPP Conversion";
    case CanvasUsage::kPerformanceMode: return "Performance Mode";
    case CanvasUsage::kUnknown: return "Unknown";
    default: return "Unknown";
  }
}

ImVec4 CanvasContextMenu::GetUsageModeColor(CanvasUsage usage) const {
  switch (usage) {
    case CanvasUsage::kTilePainting: return ImVec4(0.2F, 1.0F, 0.2F, 1.0F); // Green
    case CanvasUsage::kTileSelecting: return ImVec4(0.2F, 0.8F, 1.0F, 1.0F); // Blue
    case CanvasUsage::kSelectRectangle: return ImVec4(1.0F, 0.8F, 0.2F, 1.0F); // Yellow
    case CanvasUsage::kColorPainting: return ImVec4(1.0F, 0.2F, 1.0F, 1.0F); // Magenta
    case CanvasUsage::kBitmapEditing: return ImVec4(1.0F, 0.5F, 0.2F, 1.0F); // Orange
    case CanvasUsage::kPaletteEditing: return ImVec4(0.8F, 0.2F, 1.0F, 1.0F); // Purple
    case CanvasUsage::kBppConversion: return ImVec4(0.2F, 1.0F, 1.0F, 1.0F); // Cyan
    case CanvasUsage::kPerformanceMode: return ImVec4(1.0F, 0.2F, 0.2F, 1.0F); // Red
    case CanvasUsage::kUnknown: return ImVec4(0.7F, 0.7F, 0.7F, 1.0F); // Gray
    default: return ImVec4(0.7F, 0.7F, 0.7F, 1.0F); // Gray
  }
}

void CanvasContextMenu::CreateDefaultMenuItems() {
  // Create default menu items for different usage modes
  
  // Tile Painting mode items
  ContextMenuItem tile_paint_item("Paint Tile", "paint", []() {
    // Tile painting action
  });
  usage_specific_items_[CanvasUsage::kTilePainting].push_back(tile_paint_item);
  
  // Tile Selecting mode items
  ContextMenuItem tile_select_item("Select Tile", "select", []() {
    // Tile selection action
  });
  usage_specific_items_[CanvasUsage::kTileSelecting].push_back(tile_select_item);
  
  // Rectangle Selection mode items
  ContextMenuItem rect_select_item("Select Rectangle", "rect", []() {
    // Rectangle selection action
  });
  usage_specific_items_[CanvasUsage::kSelectRectangle].push_back(rect_select_item);
  
  // Color Painting mode items
  ContextMenuItem color_paint_item("Paint Color", "color", []() {
    // Color painting action
  });
  usage_specific_items_[CanvasUsage::kColorPainting].push_back(color_paint_item);
  
  // Bitmap Editing mode items
  ContextMenuItem bitmap_edit_item("Edit Bitmap", "edit", []() {
    // Bitmap editing action
  });
  usage_specific_items_[CanvasUsage::kBitmapEditing].push_back(bitmap_edit_item);
  
  // Palette Editing mode items
  ContextMenuItem palette_edit_item("Edit Palette", "palette", []() {
    // Palette editing action
  });
  usage_specific_items_[CanvasUsage::kPaletteEditing].push_back(palette_edit_item);
  
  // BPP Conversion mode items
  ContextMenuItem bpp_convert_item("Convert Format", "convert", []() {
    // BPP conversion action
  });
  usage_specific_items_[CanvasUsage::kBppConversion].push_back(bpp_convert_item);
  
  // Performance Mode items
  ContextMenuItem perf_item("Performance Analysis", "perf", []() {
    // Performance analysis action
  });
  usage_specific_items_[CanvasUsage::kPerformanceMode].push_back(perf_item);
}

canvas::CanvasContextMenu::ContextMenuItem canvas::CanvasContextMenu::CreateViewMenuItem(const std::string& label, 
                                                     const std::string& icon, 
                                                     std::function<void()> callback) {
  return ContextMenuItem(label, icon, callback);
}

canvas::CanvasContextMenu::ContextMenuItem canvas::CanvasContextMenu::CreateBitmapMenuItem(const std::string& label, 
                                                       const std::string& icon, 
                                                       std::function<void()> callback) {
  return ContextMenuItem(label, icon, callback);
}

canvas::CanvasContextMenu::ContextMenuItem canvas::CanvasContextMenu::CreatePaletteMenuItem(const std::string& label, 
                                                        const std::string& icon, 
                                                        std::function<void()> callback) {
  return ContextMenuItem(label, icon, callback);
}

canvas::CanvasContextMenu::ContextMenuItem canvas::CanvasContextMenu::CreateBppMenuItem(const std::string& label, 
                                                    const std::string& icon, 
                                                    std::function<void()> callback) {
  return ContextMenuItem(label, icon, callback);
}

canvas::CanvasContextMenu::ContextMenuItem canvas::CanvasContextMenu::CreatePerformanceMenuItem(const std::string& label, 
                                                            const std::string& icon, 
                                                            std::function<void()> callback) {
  return ContextMenuItem(label, icon, callback);
}

}  // namespace canvas
}  // namespace gui
}  // namespace yaze