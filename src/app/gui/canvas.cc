#include "canvas.h"

#include <cmath>
#include <string>
#include "app/gfx/util/bpp_format_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gui/style.h"
#include "canvas/canvas_utils.h"
#include "canvas/canvas_automation_api.h"
#include "imgui/imgui.h"

namespace yaze::gui {


// Define constructors and destructor in .cc to avoid incomplete type issues with unique_ptr

// Default constructor
Canvas::Canvas() : renderer_(nullptr) { InitializeDefaults(); }

// Legacy constructors (renderer is optional for backward compatibility)
Canvas::Canvas(const std::string& id) 
    : renderer_(nullptr), canvas_id_(id), context_id_(id + "Context") {
  InitializeDefaults();
}

Canvas::Canvas(const std::string& id, ImVec2 canvas_size)
    : renderer_(nullptr), canvas_id_(id), context_id_(id + "Context") {
  InitializeDefaults();
  config_.canvas_size = canvas_size;
  config_.custom_canvas_size = true;
}

Canvas::Canvas(const std::string& id, ImVec2 canvas_size, CanvasGridSize grid_size)
    : renderer_(nullptr), canvas_id_(id), context_id_(id + "Context") {
  InitializeDefaults();
  config_.canvas_size = canvas_size;
  config_.custom_canvas_size = true;
  SetGridSize(grid_size);
}

Canvas::Canvas(const std::string& id, ImVec2 canvas_size, CanvasGridSize grid_size, float global_scale)
    : renderer_(nullptr), canvas_id_(id), context_id_(id + "Context") {
  InitializeDefaults();
  config_.canvas_size = canvas_size;
  config_.custom_canvas_size = true;
  config_.global_scale = global_scale;
  SetGridSize(grid_size);
}

// New constructors with renderer support (for migration to IRenderer pattern)
Canvas::Canvas(gfx::IRenderer* renderer) : renderer_(renderer) { InitializeDefaults(); }

Canvas::Canvas(gfx::IRenderer* renderer, const std::string& id) 
    : renderer_(renderer), canvas_id_(id), context_id_(id + "Context") {
  InitializeDefaults();
}

Canvas::Canvas(gfx::IRenderer* renderer, const std::string& id, ImVec2 canvas_size)
    : renderer_(renderer), canvas_id_(id), context_id_(id + "Context") {
  InitializeDefaults();
  config_.canvas_size = canvas_size;
  config_.custom_canvas_size = true;
}

Canvas::Canvas(gfx::IRenderer* renderer, const std::string& id, ImVec2 canvas_size, CanvasGridSize grid_size)
    : renderer_(renderer), canvas_id_(id), context_id_(id + "Context") {
  InitializeDefaults();
  config_.canvas_size = canvas_size;
  config_.custom_canvas_size = true;
  SetGridSize(grid_size);
}

Canvas::Canvas(gfx::IRenderer* renderer, const std::string& id, ImVec2 canvas_size, CanvasGridSize grid_size, float global_scale)
    : renderer_(renderer), canvas_id_(id), context_id_(id + "Context") {
  InitializeDefaults();
  config_.canvas_size = canvas_size;
  config_.custom_canvas_size = true;
  config_.global_scale = global_scale;
  SetGridSize(grid_size);
}

Canvas::~Canvas() = default;

using ImGui::GetContentRegionAvail;
using ImGui::GetCursorScreenPos;
using ImGui::GetIO;
using ImGui::GetWindowDrawList;
using ImGui::IsItemActive;
using ImGui::IsItemHovered;
using ImGui::IsMouseClicked;
using ImGui::IsMouseDragging;
using ImGui::Text;

constexpr uint32_t kRectangleColor = IM_COL32(32, 32, 32, 255);
constexpr uint32_t kWhiteColor = IM_COL32(255, 255, 255, 255);

constexpr ImGuiButtonFlags kMouseFlags =
    ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight;

namespace {
ImVec2 AlignPosToGrid(ImVec2 pos, float scale) {
  return ImVec2(std::floor(pos.x / scale) * scale,
                std::floor(pos.y / scale) * scale);
}
}  // namespace

// Canvas class implementation begins here

void Canvas::InitializeDefaults() {
  // Initialize configuration with sensible defaults
  config_.enable_grid = true;
  config_.enable_hex_labels = false;
  config_.enable_custom_labels = false;
  config_.enable_context_menu = true;
  config_.is_draggable = false;
  config_.grid_step = 32.0f;
  config_.global_scale = 1.0f;
  config_.canvas_size = ImVec2(0, 0);
  config_.custom_canvas_size = false;
  config_.clamp_rect_to_local_maps = false;

  // Initialize selection state
  selection_.Clear();

  // Initialize palette editor
  palette_editor_ = std::make_unique<PaletteWidget>();

  // Initialize interaction handler
  interaction_handler_.Initialize(canvas_id_);

  // Initialize enhanced components
  InitializeEnhancedComponents();

  // Initialize legacy compatibility variables to match config
  enable_grid_ = config_.enable_grid;
  enable_hex_tile_labels_ = config_.enable_hex_labels;
  enable_custom_labels_ = config_.enable_custom_labels;
  enable_context_menu_ = config_.enable_context_menu;
  draggable_ = config_.is_draggable;
  custom_step_ = config_.grid_step;
  global_scale_ = config_.global_scale;
  custom_canvas_size_ = config_.custom_canvas_size;
  select_rect_active_ = selection_.select_rect_active;
  selected_tile_pos_ = selection_.selected_tile_pos;
}

void Canvas::Cleanup() {
  palette_editor_.reset();
  selection_.Clear();

  // Stop performance monitoring before cleanup to prevent segfault
  if (performance_integration_) {
    performance_integration_->StopMonitoring();
  }

  // Cleanup enhanced components
  modals_.reset();
  context_menu_.reset();
  usage_tracker_.reset();
  performance_integration_.reset();
}

void Canvas::InitializeEnhancedComponents() {
  // Initialize modals system
  modals_ = std::make_unique<canvas::CanvasModals>();

  // Initialize context menu system
  context_menu_ = std::make_unique<canvas::CanvasContextMenu>();
  context_menu_->Initialize(canvas_id_);

  // Initialize usage tracker
  usage_tracker_ = std::make_shared<canvas::CanvasUsageTracker>();
  usage_tracker_->Initialize(canvas_id_);
  canvas::CanvasUsageManager::Get().RegisterTracker(canvas_id_, usage_tracker_);

  // Initialize performance integration
  performance_integration_ =
      std::make_shared<canvas::CanvasPerformanceIntegration>();
  performance_integration_->Initialize(canvas_id_);
  performance_integration_->SetUsageTracker(usage_tracker_);
  canvas::CanvasPerformanceManager::Get().RegisterIntegration(
      canvas_id_, performance_integration_);

  // Start performance monitoring
  performance_integration_->StartMonitoring();
  usage_tracker_->StartSession();
}

void Canvas::SetUsageMode(canvas::CanvasUsage usage) {
  if (usage_tracker_) {
    usage_tracker_->SetUsageMode(usage);
  }
  if (context_menu_) {
    context_menu_->SetUsageMode(usage);
  }
}

canvas::CanvasUsage Canvas::GetUsageMode() const {
  if (usage_tracker_) {
    return usage_tracker_->GetCurrentStats().usage_mode;
  }
  return canvas::CanvasUsage::kUnknown;
}

void Canvas::RecordCanvasOperation(const std::string& operation_name,
                                   double time_ms) {
  if (usage_tracker_) {
    usage_tracker_->RecordOperation(operation_name, time_ms);
  }
  if (performance_integration_) {
    performance_integration_->RecordOperation(operation_name, time_ms,
                                              GetUsageMode());
  }
}

void Canvas::ShowPerformanceUI() {
  if (performance_integration_) {
    performance_integration_->RenderPerformanceUI();
  }
}

void Canvas::ShowUsageReport() {
  if (usage_tracker_) {
    std::string report = usage_tracker_->ExportUsageReport();
    // Show report in a modal or window
    if (modals_) {
      // Create a simple text display modal
      ImGui::OpenPopup("Canvas Usage Report");
      if (ImGui::BeginPopupModal("Canvas Usage Report", nullptr,
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Canvas Usage Report");
        ImGui::Separator();
        ImGui::TextWrapped("%s", report.c_str());
        ImGui::Separator();
        if (ImGui::Button("Close")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
  }
}

void Canvas::InitializePaletteEditor(Rom* rom) {
  rom_ = rom;
  if (palette_editor_) {
    palette_editor_->Initialize(rom);
  }
}

void Canvas::ShowPaletteEditor() {
  if (palette_editor_ && bitmap_) {
    auto mutable_palette = bitmap_->mutable_palette();
    palette_editor_->ShowPaletteEditor(*mutable_palette,
                                       "Canvas Palette Editor");
  }
}

void Canvas::ShowColorAnalysis() {
  if (palette_editor_ && bitmap_) {
    palette_editor_->ShowColorAnalysis(*bitmap_, "Canvas Color Analysis");
  }
}

bool Canvas::ApplyROMPalette(int group_index, int palette_index) {
  if (palette_editor_ && bitmap_) {
    return palette_editor_->ApplyROMPalette(bitmap_, group_index, palette_index);
  }
  return false;
}

// Size reporting methods for table integration
ImVec2 Canvas::GetMinimumSize() const {
  return CanvasUtils::CalculateMinimumCanvasSize(config_.content_size,
                                                 config_.global_scale);
}

ImVec2 Canvas::GetPreferredSize() const {
  return CanvasUtils::CalculatePreferredCanvasSize(config_.content_size,
                                                   config_.global_scale);
}

void Canvas::ReserveTableSpace(const std::string& label) {
  ImVec2 size = config_.auto_resize ? GetPreferredSize() : config_.canvas_size;
  CanvasUtils::ReserveCanvasSpace(size, label);
}

bool Canvas::BeginTableCanvas(const std::string& label) {
  if (config_.auto_resize) {
    ImVec2 preferred_size = GetPreferredSize();
    CanvasUtils::SetNextCanvasSize(preferred_size, true);
  }

  // Begin child window that properly reports size to tables
  std::string child_id = canvas_id_ + "_TableChild";
  ImVec2 child_size = config_.auto_resize ? ImVec2(0, 0) : config_.canvas_size;

  bool result =
      ImGui::BeginChild(child_id.c_str(), child_size,
                        true,  // Always show border for table integration
                        ImGuiWindowFlags_AlwaysVerticalScrollbar);

  if (!label.empty()) {
    ImGui::Text("%s", label.c_str());
  }

  return result;
}

void Canvas::EndTableCanvas() {
  ImGui::EndChild();
}

// Improved interaction detection methods
bool Canvas::HasValidSelection() const {
  return !points_.empty() && points_.size() >= 2;
}

bool Canvas::WasClicked(ImGuiMouseButton button) const {
  return ImGui::IsItemClicked(button) && HasValidSelection();
}

bool Canvas::WasDoubleClicked(ImGuiMouseButton button) const {
  return ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(button) &&
         HasValidSelection();
}

ImVec2 Canvas::GetLastClickPosition() const {
  if (HasValidSelection()) {
    return points_[0];  // Return the first point of the selection
  }
  return ImVec2(-1, -1);  // Invalid position
}

// ==================== Modern ImGui-Style Interface ====================

void Canvas::Begin(ImVec2 canvas_size) {
  // Modern ImGui-style begin - combines DrawBackground + DrawContextMenu
  DrawBackground(canvas_size);
  DrawContextMenu();
}

void Canvas::End() {
  // Modern ImGui-style end - automatically draws grid and overlay
  if (config_.enable_grid) {
    DrawGrid();
  }
  DrawOverlay();
  
  // Render any persistent popups from context menu actions
  RenderPersistentPopups();
}

// ==================== Legacy Interface ====================

void Canvas::UpdateColorPainter(gfx::IRenderer* /*renderer*/, gfx::Bitmap& bitmap, const ImVec4& color,
                                const std::function<void()>& event,
                                int tile_size, float scale) {
  config_.global_scale = scale;
  global_scale_ = scale;  // Legacy compatibility
  DrawBackground();
  DrawContextMenu();
  DrawBitmap(bitmap, 2, scale);
  if (DrawSolidTilePainter(color, tile_size)) {
    event();
    bitmap.UpdateTexture();
  }
  DrawGrid();
  DrawOverlay();
}

void Canvas::UpdateInfoGrid(ImVec2 bg_size, float grid_size, int label_id) {
  config_.enable_custom_labels = true;
  enable_custom_labels_ = true;  // Legacy compatibility
  DrawBackground(bg_size);
  DrawInfoGrid(grid_size, 8, label_id);
  DrawOverlay();
}

void Canvas::DrawBackground(ImVec2 canvas_size) {
  draw_list_ = GetWindowDrawList();
  canvas_p0_ = GetCursorScreenPos();

  // Calculate canvas size using utility function
  ImVec2 content_region = GetContentRegionAvail();
  canvas_sz_ = CanvasUtils::CalculateCanvasSize(
      content_region, config_.canvas_size, config_.custom_canvas_size);

  if (canvas_size.x != 0) {
    canvas_sz_ = canvas_size;
    config_.canvas_size = canvas_size;
  }

  // Calculate scaled canvas bounds
  ImVec2 scaled_size =
      CanvasUtils::CalculateScaledCanvasSize(canvas_sz_, config_.global_scale);

  // CRITICAL FIX: Ensure minimum size to prevent ImGui assertions
  if (scaled_size.x <= 0.0f)
    scaled_size.x = 1.0f;
  if (scaled_size.y <= 0.0f)
    scaled_size.y = 1.0f;

  canvas_p1_ =
      ImVec2(canvas_p0_.x + scaled_size.x, canvas_p0_.y + scaled_size.y);

  // Draw border and background color
  draw_list_->AddRectFilled(canvas_p0_, canvas_p1_, kRectangleColor);
  draw_list_->AddRect(canvas_p0_, canvas_p1_, kWhiteColor);

  ImGui::InvisibleButton(canvas_id_.c_str(), scaled_size, kMouseFlags);

  // CRITICAL FIX: Always update hover mouse position when hovering over canvas
  // This fixes the regression where CheckForCurrentMap() couldn't track hover
  if (IsItemHovered()) {
    const ImGuiIO& io = GetIO();
    const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
    const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
    mouse_pos_in_canvas_ = mouse_pos;
  }

  if (config_.is_draggable && IsItemHovered()) {
    const ImGuiIO& io = GetIO();
    const bool is_active = IsItemActive();  // Held
    const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                        canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
    const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

    // Pan (we use a zero mouse threshold when there's no context menu)
    if (const float mouse_threshold_for_pan =
            enable_context_menu_ ? -1.0f : 0.0f;
        is_active &&
        IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan)) {
      scrolling_.x += io.MouseDelta.x;
      scrolling_.y += io.MouseDelta.y;
    }
  }
}

void Canvas::DrawContextMenu() {
  const ImGuiIO& io = GetIO();
  const ImVec2 scaled_sz(canvas_sz_.x * global_scale_,
                         canvas_sz_.y * global_scale_);
  const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                      canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Update canvas state for enhanced components
  if (usage_tracker_) {
    usage_tracker_->UpdateCanvasState(
        canvas_sz_, config_.content_size, global_scale_, custom_step_,
        enable_grid_, enable_hex_tile_labels_, enable_custom_labels_);
  }

  // Use enhanced context menu if available
  if (context_menu_) {
    canvas::CanvasConfig snapshot;
    snapshot.canvas_size = canvas_sz_;
    snapshot.content_size = config_.content_size;
    snapshot.global_scale = global_scale_;
    snapshot.grid_step = custom_step_;
    snapshot.enable_grid = enable_grid_;
    snapshot.enable_hex_labels = enable_hex_tile_labels_;
    snapshot.enable_custom_labels = enable_custom_labels_;
    snapshot.enable_context_menu = enable_context_menu_;
    snapshot.is_draggable = draggable_;
    snapshot.auto_resize = config_.auto_resize;
    snapshot.scrolling = scrolling_;

    context_menu_->SetCanvasState(
        canvas_sz_, config_.content_size, global_scale_, custom_step_,
        enable_grid_, enable_hex_tile_labels_, enable_custom_labels_,
        enable_context_menu_, draggable_, config_.auto_resize, scrolling_);

    context_menu_->Render(
        context_id_, mouse_pos, rom_, bitmap_,
        bitmap_ ? bitmap_->mutable_palette() : nullptr,
        [this](canvas::CanvasContextMenu::Command command,
               const canvas::CanvasConfig& updated_config) {
          switch (command) {
            case canvas::CanvasContextMenu::Command::kResetView:
              ResetView();
              break;
            case canvas::CanvasContextMenu::Command::kZoomToFit:
              if (bitmap_) {
                SetZoomToFit(*bitmap_);
              }
              break;
            case canvas::CanvasContextMenu::Command::kZoomIn:
              SetGlobalScale(config_.global_scale * 1.25f);
              break;
            case canvas::CanvasContextMenu::Command::kZoomOut:
              SetGlobalScale(config_.global_scale * 0.8f);
              break;
            case canvas::CanvasContextMenu::Command::kToggleGrid:
              config_.enable_grid = !config_.enable_grid;
              enable_grid_ = config_.enable_grid;
              break;
            case canvas::CanvasContextMenu::Command::kToggleHexLabels:
              config_.enable_hex_labels = !config_.enable_hex_labels;
              enable_hex_tile_labels_ = config_.enable_hex_labels;
              break;
            case canvas::CanvasContextMenu::Command::kToggleCustomLabels:
              config_.enable_custom_labels = !config_.enable_custom_labels;
              enable_custom_labels_ = config_.enable_custom_labels;
              break;
            case canvas::CanvasContextMenu::Command::kToggleContextMenu:
              config_.enable_context_menu = !config_.enable_context_menu;
              enable_context_menu_ = config_.enable_context_menu;
              break;
            case canvas::CanvasContextMenu::Command::kToggleAutoResize:
              config_.auto_resize = !config_.auto_resize;
              break;
            case canvas::CanvasContextMenu::Command::kToggleDraggable:
              config_.is_draggable = !config_.is_draggable;
              draggable_ = config_.is_draggable;
              break;
            case canvas::CanvasContextMenu::Command::kSetGridStep:
              config_.grid_step = updated_config.grid_step;
              custom_step_ = config_.grid_step;
              break;
            case canvas::CanvasContextMenu::Command::kSetScale:
              config_.global_scale = updated_config.global_scale;
              global_scale_ = config_.global_scale;
              break;
            case canvas::CanvasContextMenu::Command::kOpenAdvancedProperties:
              if (modals_) {
                canvas::CanvasConfig modal_config = updated_config;
                modal_config.on_config_changed =
                    [this](const canvas::CanvasConfig& cfg) {
                      ApplyConfigSnapshot(cfg);
                    };
                modal_config.on_scale_changed =
                    [this](const canvas::CanvasConfig& cfg) {
                      ApplyScaleSnapshot(cfg);
                    };
                modals_->ShowAdvancedProperties(canvas_id_, modal_config,
                                                bitmap_);
              }
              break;
            case canvas::CanvasContextMenu::Command::kOpenScalingControls:
              if (modals_) {
                canvas::CanvasConfig modal_config = updated_config;
                modal_config.on_config_changed =
                    [this](const canvas::CanvasConfig& cfg) {
                      ApplyConfigSnapshot(cfg);
                    };
                modal_config.on_scale_changed =
                    [this](const canvas::CanvasConfig& cfg) {
                      ApplyScaleSnapshot(cfg);
                    };
                modals_->ShowScalingControls(canvas_id_, modal_config, bitmap_);
              }
              break;
            default:
              break;
          }
        },
        snapshot);

    if (modals_) {
      modals_->Render();
    }

    // CRITICAL: Render custom context menu items AFTER enhanced menu
    // Don't return early - we need to show custom items too!
    if (!context_menu_items_.empty() && ImGui::BeginPopupContextItem(context_id_.c_str())) {
      for (const auto& item : context_menu_items_) {
        DrawContextMenuItem(item);
      }
      ImGui::EndPopup();
    }

    return;
  }



  // Draw enhanced property dialogs
  ShowAdvancedCanvasProperties();
  ShowScalingControls();
}

void Canvas::DrawContextMenuItem(const ContextMenuItem& item) {
  if (!item.enabled_condition()) {
    ImGui::BeginDisabled();
  }

  if (item.subitems.empty()) {
    // Simple menu item
    if (ImGui::MenuItem(item.label.c_str(), item.shortcut.empty()
                                                ? nullptr
                                                : item.shortcut.c_str())) {
      item.callback();
    }
  } else {
    // Menu with subitems
    if (ImGui::BeginMenu(item.label.c_str())) {
      for (const auto& subitem : item.subitems) {
        DrawContextMenuItem(subitem);
      }
      ImGui::EndMenu();
    }
  }

  if (!item.enabled_condition()) {
    ImGui::EndDisabled();
  }
}

void Canvas::AddContextMenuItem(const ContextMenuItem& item) {
  context_menu_items_.push_back(item);
}

void Canvas::ClearContextMenuItems() {
  context_menu_items_.clear();
}

void Canvas::OpenPersistentPopup(const std::string& popup_id, std::function<void()> render_callback) {
  // Check if popup already exists
  for (auto& popup : active_popups_) {
    if (popup.popup_id == popup_id) {
      popup.is_open = true;
      popup.render_callback = std::move(render_callback);
      ImGui::OpenPopup(popup_id.c_str());
      return;
    }
  }
  
  // Add new popup
  PopupState new_popup;
  new_popup.popup_id = popup_id;
  new_popup.is_open = true;
  new_popup.render_callback = std::move(render_callback);
  active_popups_.push_back(new_popup);
  ImGui::OpenPopup(popup_id.c_str());
}

void Canvas::ClosePersistentPopup(const std::string& popup_id) {
  for (auto& popup : active_popups_) {
    if (popup.popup_id == popup_id) {
      popup.is_open = false;
      ImGui::CloseCurrentPopup();
      return;
    }
  }
}

void Canvas::RenderPersistentPopups() {
  // Render all active popups
  auto it = active_popups_.begin();
  while (it != active_popups_.end()) {
    if (it->is_open && it->render_callback) {
      // Call the render callback which should handle BeginPopup/EndPopup
      it->render_callback();
      
      // If popup was closed by user, mark it for removal
      if (!ImGui::IsPopupOpen(it->popup_id.c_str())) {
        it->is_open = false;
      }
    }
    
    // Remove closed popups
    if (!it->is_open) {
      it = active_popups_.erase(it);
    } else {
      ++it;
    }
  }
}

void Canvas::SetZoomToFit(const gfx::Bitmap& bitmap) {
  if (!bitmap.is_active())
    return;

  ImVec2 available = ImGui::GetContentRegionAvail();
  float scale_x = available.x / bitmap.width();
  float scale_y = available.y / bitmap.height();
  config_.global_scale = std::min(scale_x, scale_y);

  // Ensure minimum readable scale
  if (config_.global_scale < 0.25f)
    config_.global_scale = 0.25f;

  global_scale_ = config_.global_scale;  // Legacy compatibility

  // Center the view
  scrolling_ = ImVec2(0, 0);
}

void Canvas::ResetView() {
  config_.global_scale = 1.0f;
  global_scale_ = 1.0f;  // Legacy compatibility
  scrolling_ = ImVec2(0, 0);
}

void Canvas::ApplyConfigSnapshot(const canvas::CanvasConfig& snapshot) {
  config_.enable_grid = snapshot.enable_grid;
  config_.enable_hex_labels = snapshot.enable_hex_labels;
  config_.enable_custom_labels = snapshot.enable_custom_labels;
  config_.enable_context_menu = snapshot.enable_context_menu;
  config_.is_draggable = snapshot.is_draggable;
  config_.auto_resize = snapshot.auto_resize;
  config_.grid_step = snapshot.grid_step;
  config_.global_scale = snapshot.global_scale;
  config_.canvas_size = snapshot.canvas_size;
  config_.content_size = snapshot.content_size;
  config_.custom_canvas_size =
      snapshot.canvas_size.x > 0 && snapshot.canvas_size.y > 0;

  enable_grid_ = config_.enable_grid;
  enable_hex_tile_labels_ = config_.enable_hex_labels;
  enable_custom_labels_ = config_.enable_custom_labels;
  enable_context_menu_ = config_.enable_context_menu;
  draggable_ = config_.is_draggable;
  custom_step_ = config_.grid_step;
  global_scale_ = config_.global_scale;
  scrolling_ = snapshot.scrolling;
}

void Canvas::ApplyScaleSnapshot(const canvas::CanvasConfig& snapshot) {
  config_.global_scale = snapshot.global_scale;
  global_scale_ = config_.global_scale;
  scrolling_ = snapshot.scrolling;
}

bool Canvas::DrawTilePainter(const Bitmap& bitmap, int size, float scale) {
  const ImGuiIO& io = GetIO();
  const bool is_hovered = IsItemHovered();
  is_hovered_ = is_hovered;
  // Lock scrolled origin
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
  const auto scaled_size = size * scale;

  // Erase the hover when the mouse is not in the canvas window.
  if (!is_hovered) {
    points_.clear();
    return false;
  }

  // Reset the previous tile hover
  if (!points_.empty()) {
    points_.clear();
  }

  // Calculate the coordinates of the mouse
  ImVec2 paint_pos = AlignPosToGrid(mouse_pos, scaled_size);
  mouse_pos_in_canvas_ = paint_pos;
  auto paint_pos_end =
      ImVec2(paint_pos.x + scaled_size, paint_pos.y + scaled_size);
  points_.push_back(paint_pos);
  points_.push_back(paint_pos_end);

  if (bitmap.is_active()) {
    draw_list_->AddImage((ImTextureID)(intptr_t)bitmap.texture(),
                         ImVec2(origin.x + paint_pos.x, origin.y + paint_pos.y),
                         ImVec2(origin.x + paint_pos.x + scaled_size,
                                origin.y + paint_pos.y + scaled_size));
  }

  if (IsMouseClicked(ImGuiMouseButton_Left) &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // Draw the currently selected tile on the overworld here
    // Save the coordinates of the selected tile.
    drawn_tile_pos_ = paint_pos;
    return true;
  }

  return false;
}

bool Canvas::DrawTilemapPainter(gfx::Tilemap& tilemap, int current_tile) {
  const ImGuiIO& io = GetIO();
  const bool is_hovered = IsItemHovered();
  is_hovered_ = is_hovered;
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Safety check: ensure tilemap is properly initialized
  if (!tilemap.atlas.is_active() || tilemap.tile_size.x <= 0) {
    return false;
  }

  const auto scaled_size = tilemap.tile_size.x * global_scale_;

  if (!is_hovered) {
    points_.clear();
    return false;
  }

  if (!points_.empty()) {
    points_.clear();
  }

  ImVec2 paint_pos = AlignPosToGrid(mouse_pos, scaled_size);
  mouse_pos_in_canvas_ = paint_pos;

  points_.push_back(paint_pos);
  points_.push_back(
      ImVec2(paint_pos.x + scaled_size, paint_pos.y + scaled_size));

  // CRITICAL FIX: Disable tile cache system to prevent crashes
  // Just draw a simple preview tile using the atlas directly
  if (tilemap.atlas.is_active() && tilemap.atlas.texture()) {
    // Draw the tile directly from the atlas without caching
    int tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;
    if (tiles_per_row > 0) {
      int tile_x = (current_tile % tiles_per_row) * tilemap.tile_size.x;
      int tile_y = (current_tile / tiles_per_row) * tilemap.tile_size.y;

      // Simple bounds check
      if (tile_x >= 0 && tile_x < tilemap.atlas.width() && tile_y >= 0 &&
          tile_y < tilemap.atlas.height()) {

        // Draw directly from atlas texture
        ImVec2 uv0 =
            ImVec2(static_cast<float>(tile_x) / tilemap.atlas.width(),
                   static_cast<float>(tile_y) / tilemap.atlas.height());
        ImVec2 uv1 = ImVec2(static_cast<float>(tile_x + tilemap.tile_size.x) /
                                tilemap.atlas.width(),
                            static_cast<float>(tile_y + tilemap.tile_size.y) /
                                tilemap.atlas.height());

        draw_list_->AddImage(
            (ImTextureID)(intptr_t)tilemap.atlas.texture(),
            ImVec2(origin.x + paint_pos.x, origin.y + paint_pos.y),
            ImVec2(origin.x + paint_pos.x + scaled_size,
                   origin.y + paint_pos.y + scaled_size),
            uv0, uv1);
      }
    }
  }

  if (IsMouseClicked(ImGuiMouseButton_Left) ||
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    drawn_tile_pos_ = paint_pos;
    return true;
  }

  return false;
}

bool Canvas::DrawSolidTilePainter(const ImVec4& color, int tile_size) {
  const ImGuiIO& io = GetIO();
  const bool is_hovered = IsItemHovered();
  is_hovered_ = is_hovered;
  // Lock scrolled origin
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
  auto scaled_tile_size = tile_size * global_scale_;
  static bool is_dragging = false;
  static ImVec2 start_drag_pos;

  // Erase the hover when the mouse is not in the canvas window.
  if (!is_hovered) {
    points_.clear();
    return false;
  }

  // Reset the previous tile hover
  if (!points_.empty()) {
    points_.clear();
  }

  // Calculate the coordinates of the mouse
  ImVec2 paint_pos = AlignPosToGrid(mouse_pos, scaled_tile_size);
  mouse_pos_in_canvas_ = paint_pos;

  // Clamp the size to a grid
  paint_pos.x = std::clamp(paint_pos.x, 0.0f, canvas_sz_.x * global_scale_);
  paint_pos.y = std::clamp(paint_pos.y, 0.0f, canvas_sz_.y * global_scale_);

  points_.push_back(paint_pos);
  points_.push_back(
      ImVec2(paint_pos.x + scaled_tile_size, paint_pos.y + scaled_tile_size));

  draw_list_->AddRectFilled(
      ImVec2(origin.x + paint_pos.x + 1, origin.y + paint_pos.y + 1),
      ImVec2(origin.x + paint_pos.x + scaled_tile_size,
             origin.y + paint_pos.y + scaled_tile_size),
      IM_COL32(color.x * 255, color.y * 255, color.z * 255, 255));

  if (IsMouseClicked(ImGuiMouseButton_Left)) {
    is_dragging = true;
    start_drag_pos = paint_pos;
  }

  if (is_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    is_dragging = false;
    drawn_tile_pos_ = start_drag_pos;
    return true;
  }

  return false;
}

void Canvas::DrawTileOnBitmap(int tile_size, gfx::Bitmap* bitmap,
                              ImVec4 color) {
  const ImVec2 position = drawn_tile_pos_;
  int tile_index_x = static_cast<int>(position.x / global_scale_) / tile_size;
  int tile_index_y = static_cast<int>(position.y / global_scale_) / tile_size;

  ImVec2 start_position(tile_index_x * tile_size, tile_index_y * tile_size);

  // Update the bitmap's pixel data based on the start_position and color
  for (int y = 0; y < tile_size; ++y) {
    for (int x = 0; x < tile_size; ++x) {
      // Calculate the actual pixel index in the bitmap
      int pixel_index =
          (start_position.y + y) * bitmap->width() + (start_position.x + x);

      // Write the color to the pixel
      bitmap->WriteColor(pixel_index, color);
    }
  }
}

bool Canvas::DrawTileSelector(int size, int size_y) {
  const ImGuiIO& io = GetIO();
  const bool is_hovered = IsItemHovered();
  is_hovered_ = is_hovered;
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
  if (size_y == 0) {
    size_y = size;
  }

  if (is_hovered && IsMouseClicked(ImGuiMouseButton_Left)) {
    if (!points_.empty()) {
      points_.clear();
    }
    ImVec2 painter_pos = AlignPosToGrid(mouse_pos, size);

    points_.push_back(painter_pos);
    points_.push_back(ImVec2(painter_pos.x + size, painter_pos.y + size_y));
    mouse_pos_in_canvas_ = painter_pos;
  }

  if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    return true;
  }

  return false;
}

void Canvas::DrawSelectRect(int current_map, int tile_size, float scale) {
  gfx::ScopedTimer timer("canvas_select_rect");

  const ImGuiIO& io = GetIO();
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
  static ImVec2 drag_start_pos;
  const float scaled_size = tile_size * scale;
  static bool dragging = false;
  constexpr int small_map_size = 0x200;

  // Only handle mouse events if the canvas is hovered
  const bool is_hovered = IsItemHovered();
  if (!is_hovered) {
    return;
  }

  // Calculate superX and superY accounting for world offset
  int superY, superX;
  if (current_map < 0x40) {
    // Light World
    superY = current_map / 8;
    superX = current_map % 8;
  } else if (current_map < 0x80) {
    // Dark World
    superY = (current_map - 0x40) / 8;
    superX = (current_map - 0x40) % 8;
  } else {
    // Special World
    superY = (current_map - 0x80) / 8;
    superX = (current_map - 0x80) % 8;
  }

  // Handle right click for single tile selection
  if (IsMouseClicked(ImGuiMouseButton_Right)) {
    ImVec2 painter_pos = AlignPosToGrid(mouse_pos, scaled_size);
    int painter_x = painter_pos.x;
    int painter_y = painter_pos.y;

    auto tile16_x = (painter_x % small_map_size) / (small_map_size / 0x20);
    auto tile16_y = (painter_y % small_map_size) / (small_map_size / 0x20);

    int index_x = superX * 0x20 + tile16_x;
    int index_y = superY * 0x20 + tile16_y;
    selected_tile_pos_ = ImVec2(index_x, index_y);
    selected_points_.clear();
    select_rect_active_ = false;

    // Start drag position for rectangle selection
    drag_start_pos = AlignPosToGrid(mouse_pos, scaled_size);
  }

  // Calculate the rectangle's top-left and bottom-right corners
  ImVec2 drag_end_pos = AlignPosToGrid(mouse_pos, scaled_size);
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
    // FIX: Origin used to be canvas_p0_, revert if there is regression.
    auto start = ImVec2(origin.x + drag_start_pos.x,
                        origin.y + drag_start_pos.y);
    auto end = ImVec2(origin.x + drag_end_pos.x + tile_size,
                      origin.y + drag_end_pos.y + tile_size);
    draw_list_->AddRect(start, end, kWhiteColor);
    dragging = true;
  }

  if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    // Release dragging mode
    dragging = false;

    // Calculate the bounds of the rectangle in terms of 16x16 tile indices
    constexpr int tile16_size = 16;
    int start_x = std::floor(drag_start_pos.x / scaled_size) * tile16_size;
    int start_y = std::floor(drag_start_pos.y / scaled_size) * tile16_size;
    int end_x = std::floor(drag_end_pos.x / scaled_size) * tile16_size;
    int end_y = std::floor(drag_end_pos.y / scaled_size) * tile16_size;

    // Swap the start and end positions if they are in the wrong order
    if (start_x > end_x)
      std::swap(start_x, end_x);
    if (start_y > end_y)
      std::swap(start_y, end_y);

    selected_tiles_.clear();
    selected_tiles_.reserve(((end_x - start_x) / tile16_size + 1) *
                            ((end_y - start_y) / tile16_size + 1));

    // Number of tiles per local map (since each tile is 16x16)
    constexpr int tiles_per_local_map = small_map_size / 16;

    // Loop through the tiles in the rectangle and store their positions
    for (int y = start_y; y <= end_y; y += tile16_size) {
      for (int x = start_x; x <= end_x; x += tile16_size) {
        // Determine which local map (512x512) the tile is in
        int local_map_x = (x / small_map_size) % 8;
        int local_map_y = (y / small_map_size) % 8;

        // Calculate the tile's position within its local map
        int tile16_x = (x % small_map_size) / tile16_size;
        int tile16_y = (y % small_map_size) / tile16_size;

        // Calculate the index within the overall map structure
        int index_x = local_map_x * tiles_per_local_map + tile16_x;
        int index_y = local_map_y * tiles_per_local_map + tile16_y;

        selected_tiles_.emplace_back(index_x, index_y);
      }
    }
    // Clear and add the calculated rectangle points
    selected_points_.clear();
    selected_points_.push_back(drag_start_pos);
    selected_points_.push_back(drag_end_pos);
    select_rect_active_ = true;
  }
}

void Canvas::DrawBitmap(Bitmap& bitmap, int /*border_offset*/, float scale) {
  if (!bitmap.is_active()) {
    return;
  }
  bitmap_ = &bitmap;

  // Update content size for table integration
  config_.content_size = ImVec2(bitmap.width(), bitmap.height());

  draw_list_->AddImage((ImTextureID)(intptr_t)bitmap.texture(),
                       ImVec2(canvas_p0_.x, canvas_p0_.y),
                       ImVec2(canvas_p0_.x + (bitmap.width() * scale),
                              canvas_p0_.y + (bitmap.height() * scale)));
  draw_list_->AddRect(canvas_p0_, canvas_p1_, kWhiteColor);
}

void Canvas::DrawBitmap(Bitmap& bitmap, int x_offset, int y_offset, float scale,
                        int alpha) {
  if (!bitmap.is_active()) {
    return;
  }
  bitmap_ = &bitmap;

  // Update content size for table integration
  // CRITICAL: Store UNSCALED bitmap size as content - scale is applied during rendering
  config_.content_size = ImVec2(bitmap.width(), bitmap.height());

  // Calculate the actual rendered size including scale and offsets
  // CRITICAL: Use scale parameter (NOT global_scale_) for per-bitmap scaling
  ImVec2 rendered_size(bitmap.width() * scale, bitmap.height() * scale);
  ImVec2 total_size(x_offset + rendered_size.x, y_offset + rendered_size.y);

  // CRITICAL FIX: Draw bitmap WITHOUT additional global_scale multiplication
  // The scale parameter already contains the correct scale factor
  // The scrolling should NOT be scaled - it's already in screen space
  draw_list_->AddImage(
      (ImTextureID)(intptr_t)bitmap.texture(),
      ImVec2(canvas_p0_.x + x_offset + scrolling_.x,
             canvas_p0_.y + y_offset + scrolling_.y),
      ImVec2(canvas_p0_.x + x_offset + scrolling_.x + rendered_size.x,
             canvas_p0_.y + y_offset + scrolling_.y + rendered_size.y),
      ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, alpha));

  // Note: Content size for child windows should be set before BeginChild, not here
}

void Canvas::DrawBitmap(Bitmap& bitmap, ImVec2 dest_pos, ImVec2 dest_size,
                        ImVec2 src_pos, ImVec2 src_size) {
  if (!bitmap.is_active()) {
    return;
  }
  bitmap_ = &bitmap;

  // Update content size for table integration
  config_.content_size = ImVec2(bitmap.width(), bitmap.height());

  draw_list_->AddImage(
      (ImTextureID)(intptr_t)bitmap.texture(),
      ImVec2(canvas_p0_.x + dest_pos.x, canvas_p0_.y + dest_pos.y),
      ImVec2(canvas_p0_.x + dest_pos.x + dest_size.x,
             canvas_p0_.y + dest_pos.y + dest_size.y),
      ImVec2(src_pos.x / bitmap.width(), src_pos.y / bitmap.height()),
      ImVec2((src_pos.x + src_size.x) / bitmap.width(),
             (src_pos.y + src_size.y) / bitmap.height()));
}

// TODO: Add parameters for sizing and positioning
void Canvas::DrawBitmapTable(const BitmapTable& gfx_bin) {
  for (const auto& [key, value] : gfx_bin) {
    int offset = 0x40 * (key + 1);
    int top_left_y = canvas_p0_.y + 2;
    if (key >= 1) {
      top_left_y = canvas_p0_.y + 0x40 * key;
    }
    draw_list_->AddImage((ImTextureID)(intptr_t)value.texture(),
                         ImVec2(canvas_p0_.x + 2, top_left_y),
                         ImVec2(canvas_p0_.x + 0x100, canvas_p0_.y + offset));
  }
}

void Canvas::DrawOutline(int x, int y, int w, int h) {
  CanvasUtils::DrawCanvasOutline(draw_list_, canvas_p0_, scrolling_, x, y, w, h,
                                 IM_COL32(255, 255, 255, 200));
}

void Canvas::DrawOutlineWithColor(int x, int y, int w, int h, ImVec4 color) {
  CanvasUtils::DrawCanvasOutlineWithColor(draw_list_, canvas_p0_, scrolling_, x,
                                          y, w, h, color);
}

void Canvas::DrawOutlineWithColor(int x, int y, int w, int h, uint32_t color) {
  CanvasUtils::DrawCanvasOutline(draw_list_, canvas_p0_, scrolling_, x, y, w, h,
                                 color);
}

void Canvas::DrawBitmapGroup(std::vector<int>& group, gfx::Tilemap& tilemap,
                             int tile_size, float scale, int local_map_size,
                             ImVec2 total_map_size) {
  if (selected_points_.size() != 2) {
    // points_ should contain exactly two points
    return;
  }
  if (group.empty()) {
    // group should not be empty
    return;
  }

  // OPTIMIZATION: Use optimized rendering for large groups to improve performance
  bool use_optimized_rendering =
      group.size() > 128;  // Optimize for large selections

  // Use provided map sizes for proper boundary handling
  const int small_map = local_map_size;
  const float large_map_width = total_map_size.x;
  const float large_map_height = total_map_size.y;

  // Pre-calculate common values to avoid repeated computation
  const float tile_scale = tile_size * scale;
  const int atlas_tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;

  // Top-left and bottom-right corners of the rectangle
  ImVec2 rect_top_left = selected_points_[0];
  ImVec2 rect_bottom_right = selected_points_[1];

  // Calculate the start and end tiles in the grid
  int start_tile_x =
      static_cast<int>(std::floor(rect_top_left.x / (tile_size * scale)));
  int start_tile_y =
      static_cast<int>(std::floor(rect_top_left.y / (tile_size * scale)));
  int end_tile_x =
      static_cast<int>(std::floor(rect_bottom_right.x / (tile_size * scale)));
  int end_tile_y =
      static_cast<int>(std::floor(rect_bottom_right.y / (tile_size * scale)));

  if (start_tile_x > end_tile_x)
    std::swap(start_tile_x, end_tile_x);
  if (start_tile_y > end_tile_y)
    std::swap(start_tile_y, end_tile_y);

  // Calculate the size of the rectangle in 16x16 grid form
  int rect_width = (end_tile_x - start_tile_x) * tile_size;
  int rect_height = (end_tile_y - start_tile_y) * tile_size;

  int tiles_per_row = rect_width / tile_size;
  int tiles_per_col = rect_height / tile_size;

  int i = 0;
  for (int y = 0; y < tiles_per_col + 1; ++y) {
    for (int x = 0; x < tiles_per_row + 1; ++x) {
      // Check bounds to prevent access violations
      if (i >= static_cast<int>(group.size())) {
        break;
      }

      int tile_id = group[i];

      // Check if tile_id is within the range of tile16_individual_
      auto tilemap_size = tilemap.map_size.x;
      if (tile_id >= 0 && tile_id < tilemap_size) {
        // Calculate the position of the tile within the rectangle
        int tile_pos_x = (x + start_tile_x) * tile_size * scale;
        int tile_pos_y = (y + start_tile_y) * tile_size * scale;

        // OPTIMIZATION: Use pre-calculated values for better performance with large selections
        if (tilemap.atlas.is_active() && tilemap.atlas.texture() &&
            atlas_tiles_per_row > 0) {
          int atlas_tile_x =
              (tile_id % atlas_tiles_per_row) * tilemap.tile_size.x;
          int atlas_tile_y =
              (tile_id / atlas_tiles_per_row) * tilemap.tile_size.y;

          // Simple bounds check
          if (atlas_tile_x >= 0 && atlas_tile_x < tilemap.atlas.width() &&
              atlas_tile_y >= 0 && atlas_tile_y < tilemap.atlas.height()) {

            // Calculate UV coordinates once for efficiency
            const float atlas_width = static_cast<float>(tilemap.atlas.width());
            const float atlas_height =
                static_cast<float>(tilemap.atlas.height());
            ImVec2 uv0 =
                ImVec2(atlas_tile_x / atlas_width, atlas_tile_y / atlas_height);
            ImVec2 uv1 =
                ImVec2((atlas_tile_x + tilemap.tile_size.x) / atlas_width,
                       (atlas_tile_y + tilemap.tile_size.y) / atlas_height);

            // Calculate screen positions
            float screen_x = canvas_p0_.x + scrolling_.x + tile_pos_x;
            float screen_y = canvas_p0_.y + scrolling_.y + tile_pos_y;
            float screen_w = tilemap.tile_size.x * scale;
            float screen_h = tilemap.tile_size.y * scale;

            // Use higher alpha for large selections to make them more visible
            uint32_t alpha_color = use_optimized_rendering
                                       ? IM_COL32(255, 255, 255, 200)
                                       : IM_COL32(255, 255, 255, 150);

            // Draw from atlas texture with optimized parameters
            draw_list_->AddImage(
                (ImTextureID)(intptr_t)tilemap.atlas.texture(),
                ImVec2(screen_x, screen_y),
                ImVec2(screen_x + screen_w, screen_y + screen_h), uv0, uv1,
                alpha_color);
          }
        }
      }
      i++;
    }
    // Break outer loop if we've run out of tiles
    if (i >= static_cast<int>(group.size())) {
      break;
    }
  }

  // Performance optimization completed - tiles are now rendered with pre-calculated values

  // Reposition rectangle to follow mouse, but clamp to prevent wrapping across map boundaries
  const ImGuiIO& io = GetIO();
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // CRITICAL FIX: Clamp BEFORE grid alignment for smoother dragging behavior
  // This prevents the rectangle from even attempting to cross boundaries during drag
  ImVec2 clamped_mouse_pos = mouse_pos;

  if (config_.clamp_rect_to_local_maps) {
    // Calculate which local map the mouse is in
    int mouse_local_map_x = static_cast<int>(mouse_pos.x) / small_map;
    int mouse_local_map_y = static_cast<int>(mouse_pos.y) / small_map;

    // Calculate where the rectangle END would be if we place it at mouse position
    float potential_end_x = mouse_pos.x + rect_width;
    float potential_end_y = mouse_pos.y + rect_height;

    // Check if this would cross local map boundary (512x512 blocks)
    int potential_end_map_x = static_cast<int>(potential_end_x) / small_map;
    int potential_end_map_y = static_cast<int>(potential_end_y) / small_map;

    // Clamp mouse position to prevent crossing during drag
    if (potential_end_map_x != mouse_local_map_x) {
      // Would cross horizontal boundary - clamp mouse to safe zone
      float max_mouse_x = (mouse_local_map_x + 1) * small_map - rect_width;
      clamped_mouse_pos.x = std::min(mouse_pos.x, max_mouse_x);
    }

    if (potential_end_map_y != mouse_local_map_y) {
      // Would cross vertical boundary - clamp mouse to safe zone
      float max_mouse_y = (mouse_local_map_y + 1) * small_map - rect_height;
      clamped_mouse_pos.y = std::min(mouse_pos.y, max_mouse_y);
    }
  }

  // Now grid-align the clamped position
  auto new_start_pos = AlignPosToGrid(clamped_mouse_pos, tile_size * scale);

  // Additional safety: clamp to overall map bounds
  new_start_pos.x =
      std::clamp(new_start_pos.x, 0.0f, large_map_width - rect_width);
  new_start_pos.y =
      std::clamp(new_start_pos.y, 0.0f, large_map_height - rect_height);

  selected_points_.clear();
  selected_points_.push_back(new_start_pos);
  selected_points_.push_back(
      ImVec2(new_start_pos.x + rect_width, new_start_pos.y + rect_height));
  select_rect_active_ = true;
}

void Canvas::DrawRect(int x, int y, int w, int h, ImVec4 color) {
  CanvasUtils::DrawCanvasRect(draw_list_, canvas_p0_, scrolling_, x, y, w, h,
                              color, config_.global_scale);
}

void Canvas::DrawText(const std::string& text, int x, int y) {
  CanvasUtils::DrawCanvasText(draw_list_, canvas_p0_, scrolling_, text, x, y,
                              config_.global_scale);
}

void Canvas::DrawGridLines(float grid_step) {
  CanvasUtils::DrawCanvasGridLines(draw_list_, canvas_p0_, canvas_p1_,
                                   scrolling_, grid_step, config_.global_scale);
}

void Canvas::DrawInfoGrid(float grid_step, int tile_id_offset, int label_id) {
  // Draw grid + all lines in the canvas
  draw_list_->PushClipRect(canvas_p0_, canvas_p1_, true);
  if (enable_grid_) {
    if (custom_step_ != 0.f)
      grid_step = custom_step_;
    grid_step *= global_scale_;  // Apply global scale to grid step

    DrawGridLines(grid_step);
    DrawCustomHighlight(grid_step);

    if (!enable_custom_labels_) {
      return;
    }

    // Draw the contents of labels on the grid
    for (float x = fmodf(scrolling_.x, grid_step);
         x < canvas_sz_.x * global_scale_; x += grid_step) {
      for (float y = fmodf(scrolling_.y, grid_step);
           y < canvas_sz_.y * global_scale_; y += grid_step) {
        int tile_x = (x - scrolling_.x) / grid_step;
        int tile_y = (y - scrolling_.y) / grid_step;
        int tile_id = tile_x + (tile_y * tile_id_offset);

        if (tile_id >= labels_[label_id].size()) {
          break;
        }
        std::string label = labels_[label_id][tile_id];
        draw_list_->AddText(
            ImVec2(canvas_p0_.x + x + (grid_step / 2) - tile_id_offset,
                   canvas_p0_.y + y + (grid_step / 2) - tile_id_offset),
            kWhiteColor, label.data());
      }
    }
  }
}

void Canvas::DrawCustomHighlight(float grid_step) {
  CanvasUtils::DrawCustomHighlight(draw_list_, canvas_p0_, scrolling_,
                                   highlight_tile_id, grid_step);
}

void Canvas::DrawGrid(float grid_step, int tile_id_offset) {
  if (config_.grid_step != 0.f)
    grid_step = config_.grid_step;

  // Create render context for utilities
  CanvasUtils::CanvasRenderContext ctx = {
      .draw_list = draw_list_,
      .canvas_p0 = canvas_p0_,
      .canvas_p1 = canvas_p1_,
      .scrolling = scrolling_,
      .global_scale = config_.global_scale,
      .enable_grid = config_.enable_grid,
      .enable_hex_labels = config_.enable_hex_labels,
      .grid_step = grid_step};

  // Use high-level utility function
  CanvasUtils::DrawCanvasGrid(ctx, highlight_tile_id);

  // Draw custom labels if enabled
  if (config_.enable_custom_labels) {
    draw_list_->PushClipRect(canvas_p0_, canvas_p1_, true);
    CanvasUtils::DrawCanvasLabels(ctx, labels_, current_labels_,
                                  tile_id_offset);
    draw_list_->PopClipRect();
  }
}

void Canvas::DrawOverlay() {
  // Create render context for utilities
  CanvasUtils::CanvasRenderContext ctx = {
      .draw_list = draw_list_,
      .canvas_p0 = canvas_p0_,
      .canvas_p1 = canvas_p1_,
      .scrolling = scrolling_,
      .global_scale = config_.global_scale,
      .enable_grid = config_.enable_grid,
      .enable_hex_labels = config_.enable_hex_labels,
      .grid_step = config_.grid_step};

  // Use high-level utility function with local points (synchronized from interaction handler)
  CanvasUtils::DrawCanvasOverlay(ctx, points_, selected_points_);
  
  // Render any persistent popups from context menu actions
  RenderPersistentPopups();
}

void Canvas::DrawLayeredElements() {
  // Based on ImGui demo, should be adapted to use for OAM
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  {
    Text("Blue shape is drawn first: appears in back");
    Text("Red shape is drawn after: appears in front");
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    draw_list->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p0.x + 50, p0.y + 50),
                             IM_COL32(0, 0, 255, 255));  // Blue
    draw_list->AddRectFilled(ImVec2(p0.x + 25, p0.y + 25),
                             ImVec2(p0.x + 75, p0.y + 75),
                             IM_COL32(255, 0, 0, 255));  // Red
    ImGui::Dummy(ImVec2(75, 75));
  }
  ImGui::Separator();
  {
    Text("Blue shape is drawn first, into channel 1: appears in front");
    Text("Red shape is drawn after, into channel 0: appears in back");
    ImVec2 p1 = ImGui::GetCursorScreenPos();

    // Create 2 channels and draw a Blue shape THEN a Red shape.
    // You can create any number of channels. Tables API use 1 channel per
    // column in order to better batch draw calls.
    draw_list->ChannelsSplit(2);
    draw_list->ChannelsSetCurrent(1);
    draw_list->AddRectFilled(ImVec2(p1.x, p1.y), ImVec2(p1.x + 50, p1.y + 50),
                             IM_COL32(0, 0, 255, 255));  // Blue
    draw_list->ChannelsSetCurrent(0);
    draw_list->AddRectFilled(ImVec2(p1.x + 25, p1.y + 25),
                             ImVec2(p1.x + 75, p1.y + 75),
                             IM_COL32(255, 0, 0, 255));  // Red

    // Flatten/reorder channels. Red shape is in channel 0 and it appears
    // below the Blue shape in channel 1. This works by copying draw indices
    // only (vertices are not copied).
    draw_list->ChannelsMerge();
    ImGui::Dummy(ImVec2(75, 75));
    Text("After reordering, contents of channel 0 appears below channel 1.");
  }
}

void BeginCanvas(Canvas& canvas, ImVec2 child_size) {
  gui::BeginPadding(1);

  // Use improved canvas sizing for table integration
  ImVec2 effective_size = child_size;
  if (child_size.x == 0 && child_size.y == 0) {
    // Auto-size based on canvas configuration
    if (canvas.IsAutoResize()) {
      effective_size = canvas.GetPreferredSize();
    } else {
      effective_size = canvas.GetCurrentSize();
    }
  }

  ImGui::BeginChild(canvas.canvas_id().c_str(), effective_size, true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  canvas.DrawBackground();
  gui::EndPadding();
  canvas.DrawContextMenu();
}

void EndCanvas(Canvas& canvas) {
  canvas.DrawGrid();
  canvas.DrawOverlay();
  ImGui::EndChild();
}

void GraphicsBinCanvasPipeline(int width, int height, int tile_size,
                               int num_sheets_to_load, int canvas_id,
                               bool is_loaded, gfx::BitmapTable& graphics_bin) {
  gui::Canvas canvas;
  if (ImGuiID child_id =
          ImGui::GetID((ImTextureID)(intptr_t)(intptr_t)canvas_id);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    canvas.DrawBackground(ImVec2(width + 1, num_sheets_to_load * height + 1));
    canvas.DrawContextMenu();
    if (is_loaded) {
      for (const auto& [key, value] : graphics_bin) {
        int offset = height * (key + 1);
        int top_left_y = canvas.zero_point().y + 2;
        if (key >= 1) {
          top_left_y = canvas.zero_point().y + height * key;
        }
        canvas.draw_list()->AddImage(
            (ImTextureID)(intptr_t)value.texture(),
            ImVec2(canvas.zero_point().x + 2, top_left_y),
            ImVec2(canvas.zero_point().x + 0x100,
                   canvas.zero_point().y + offset));
      }
    }
    canvas.DrawTileSelector(tile_size);
    canvas.DrawGrid(tile_size);
    canvas.DrawOverlay();
  }
  ImGui::EndChild();
}

void BitmapCanvasPipeline(gui::Canvas& canvas, gfx::Bitmap& bitmap, int width,
                          int height, int tile_size, bool is_loaded,
                          bool scrollbar, int canvas_id) {
  auto draw_canvas = [&](gui::Canvas& canvas, gfx::Bitmap& bitmap, int width,
                         int height, int tile_size, bool is_loaded) {
    canvas.DrawBackground(ImVec2(width + 1, height + 1));
    canvas.DrawContextMenu();
    canvas.DrawBitmap(bitmap, 2, is_loaded);
    canvas.DrawTileSelector(tile_size);
    canvas.DrawGrid(tile_size);
    canvas.DrawOverlay();
  };

  if (scrollbar) {
    if (ImGuiID child_id =
            ImGui::GetID((ImTextureID)(intptr_t)(intptr_t)canvas_id);
        ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      draw_canvas(canvas, bitmap, width, height, tile_size, is_loaded);
    }
    ImGui::EndChild();
  } else {
    draw_canvas(canvas, bitmap, width, height, tile_size, is_loaded);
  }
}

void TableCanvasPipeline(gui::Canvas& canvas, gfx::Bitmap& bitmap,
                         const std::string& label, bool auto_resize) {
  // Configure canvas for table integration
  canvas.SetAutoResize(auto_resize);

  if (auto_resize && bitmap.is_active()) {
    // Auto-calculate size based on bitmap content
    ImVec2 content_size = ImVec2(bitmap.width(), bitmap.height());
    ImVec2 preferred_size = CanvasUtils::CalculatePreferredCanvasSize(
        content_size, canvas.GetGlobalScale());
    canvas.SetCanvasSize(preferred_size);
  }

  // Begin table-aware canvas
  if (canvas.BeginTableCanvas(label)) {
    // Draw the canvas content
    canvas.DrawBackground();
    canvas.DrawContextMenu();

    if (bitmap.is_active()) {
      canvas.DrawBitmap(bitmap, 2, 2, canvas.GetGlobalScale());
    }

    canvas.DrawGrid();
    canvas.DrawOverlay();
  }
  canvas.EndTableCanvas();
}

void Canvas::ShowAdvancedCanvasProperties() {
  // Use the new modal system if available
  if (modals_) {
    canvas::CanvasConfig modal_config;
    modal_config.canvas_size = canvas_sz_;
    modal_config.content_size = config_.content_size;
    modal_config.global_scale = global_scale_;
    modal_config.grid_step = custom_step_;
    modal_config.enable_grid = enable_grid_;
    modal_config.enable_hex_labels = enable_hex_tile_labels_;
    modal_config.enable_custom_labels = enable_custom_labels_;
    modal_config.enable_context_menu = enable_context_menu_;
    modal_config.is_draggable = draggable_;
    modal_config.auto_resize = config_.auto_resize;
    modal_config.scrolling = scrolling_;
    modal_config.on_config_changed =
        [this](const canvas::CanvasConfig& updated_config) {
          // Update legacy variables when config changes
          enable_grid_ = updated_config.enable_grid;
          enable_hex_tile_labels_ = updated_config.enable_hex_labels;
          enable_custom_labels_ = updated_config.enable_custom_labels;
        };
    modal_config.on_scale_changed =
        [this](const canvas::CanvasConfig& updated_config) {
          global_scale_ = updated_config.global_scale;
          scrolling_ = updated_config.scrolling;
        };

    modals_->ShowAdvancedProperties(canvas_id_, modal_config, bitmap_);
    return;
  }

  // Fallback to legacy modal system
  if (ImGui::BeginPopupModal("Advanced Canvas Properties", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Advanced Canvas Configuration");
    ImGui::Separator();

    // Canvas properties (read-only info)
    ImGui::Text("Canvas Properties");
    ImGui::Text("ID: %s", canvas_id_.c_str());
    ImGui::Text("Canvas Size: %.0f x %.0f", config_.canvas_size.x,
                config_.canvas_size.y);
    ImGui::Text("Content Size: %.0f x %.0f", config_.content_size.x,
                config_.content_size.y);
    ImGui::Text("Global Scale: %.3f", config_.global_scale);
    ImGui::Text("Grid Step: %.1f", config_.grid_step);

    if (config_.content_size.x > 0 && config_.content_size.y > 0) {
      ImVec2 min_size = GetMinimumSize();
      ImVec2 preferred_size = GetPreferredSize();
      ImGui::Text("Minimum Size: %.0f x %.0f", min_size.x, min_size.y);
      ImGui::Text("Preferred Size: %.0f x %.0f", preferred_size.x,
                  preferred_size.y);
    }

    // Editable properties using new config system
    ImGui::Separator();
    ImGui::Text("View Settings");
    if (ImGui::Checkbox("Enable Grid", &config_.enable_grid)) {
      enable_grid_ = config_.enable_grid;  // Legacy sync
    }
    if (ImGui::Checkbox("Enable Hex Labels", &config_.enable_hex_labels)) {
      enable_hex_tile_labels_ = config_.enable_hex_labels;  // Legacy sync
    }
    if (ImGui::Checkbox("Enable Custom Labels",
                        &config_.enable_custom_labels)) {
      enable_custom_labels_ = config_.enable_custom_labels;  // Legacy sync
    }
    if (ImGui::Checkbox("Enable Context Menu", &config_.enable_context_menu)) {
      enable_context_menu_ = config_.enable_context_menu;  // Legacy sync
    }
    if (ImGui::Checkbox("Draggable", &config_.is_draggable)) {
      draggable_ = config_.is_draggable;  // Legacy sync
    }
    if (ImGui::Checkbox("Auto Resize for Tables", &config_.auto_resize)) {
      // Auto resize setting changed
    }

    // Grid controls
    ImGui::Separator();
    ImGui::Text("Grid Configuration");
    if (ImGui::SliderFloat("Grid Step", &config_.grid_step, 1.0f, 128.0f,
                           "%.1f")) {
      custom_step_ = config_.grid_step;  // Legacy sync
    }

    // Scale controls
    ImGui::Separator();
    ImGui::Text("Scale Configuration");
    if (ImGui::SliderFloat("Global Scale", &config_.global_scale, 0.1f, 10.0f,
                           "%.2f")) {
      global_scale_ = config_.global_scale;  // Legacy sync
    }

    // Scrolling controls
    ImGui::Separator();
    ImGui::Text("Scrolling Configuration");
    ImGui::Text("Current Scroll: %.1f, %.1f", scrolling_.x, scrolling_.y);
    if (ImGui::Button("Reset Scroll")) {
      scrolling_ = ImVec2(0, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Center View")) {
      if (bitmap_) {
        scrolling_ = ImVec2(
            -(bitmap_->width() * config_.global_scale - config_.canvas_size.x) /
                2.0f,
            -(bitmap_->height() * config_.global_scale -
              config_.canvas_size.y) /
                2.0f);
      }
    }

    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

// Old ShowPaletteManager method removed - now handled by PaletteWidget

void Canvas::ShowScalingControls() {
  // Use the new modal system if available
  if (modals_) {
    canvas::CanvasConfig modal_config;
    modal_config.canvas_size = canvas_sz_;
    modal_config.content_size = config_.content_size;
    modal_config.global_scale = global_scale_;
    modal_config.grid_step = custom_step_;
    modal_config.enable_grid = enable_grid_;
    modal_config.enable_hex_labels = enable_hex_tile_labels_;
    modal_config.enable_custom_labels = enable_custom_labels_;
    modal_config.enable_context_menu = enable_context_menu_;
    modal_config.is_draggable = draggable_;
    modal_config.auto_resize = config_.auto_resize;
    modal_config.scrolling = scrolling_;
    modal_config.on_config_changed =
        [this](const canvas::CanvasConfig& updated_config) {
          // Update legacy variables when config changes
          enable_grid_ = updated_config.enable_grid;
          enable_hex_tile_labels_ = updated_config.enable_hex_labels;
          enable_custom_labels_ = updated_config.enable_custom_labels;
          enable_context_menu_ = updated_config.enable_context_menu;
        };
    modal_config.on_scale_changed =
        [this](const canvas::CanvasConfig& updated_config) {
          draggable_ = updated_config.is_draggable;
          custom_step_ = updated_config.grid_step;
          global_scale_ = updated_config.global_scale;
          scrolling_ = updated_config.scrolling;
        };

    modals_->ShowScalingControls(canvas_id_, modal_config);
    return;
  }

  // Fallback to legacy modal system
  if (ImGui::BeginPopupModal("Scaling Controls", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Canvas Scaling and Display Controls");
    ImGui::Separator();

    // Global scale with new config system
    ImGui::Text("Global Scale: %.3f", config_.global_scale);
    if (ImGui::SliderFloat("##GlobalScale", &config_.global_scale, 0.1f, 10.0f,
                           "%.2f")) {
      global_scale_ = config_.global_scale;  // Legacy sync
    }

    // Preset scale buttons
    ImGui::Text("Preset Scales:");
    if (ImGui::Button("0.25x")) {
      config_.global_scale = 0.25f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button("0.5x")) {
      config_.global_scale = 0.5f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button("1x")) {
      config_.global_scale = 1.0f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button("2x")) {
      config_.global_scale = 2.0f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button("4x")) {
      config_.global_scale = 4.0f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button("8x")) {
      config_.global_scale = 8.0f;
      global_scale_ = config_.global_scale;
    }

    // Grid configuration
    ImGui::Separator();
    ImGui::Text("Grid Configuration");
    ImGui::Text("Grid Step: %.1f", config_.grid_step);
    if (ImGui::SliderFloat("##GridStep", &config_.grid_step, 1.0f, 128.0f,
                           "%.1f")) {
      custom_step_ = config_.grid_step;  // Legacy sync
    }

    // Grid size presets
    ImGui::Text("Grid Presets:");
    if (ImGui::Button("8x8")) {
      config_.grid_step = 8.0f;
      custom_step_ = config_.grid_step;
    }
    ImGui::SameLine();
    if (ImGui::Button("16x16")) {
      config_.grid_step = 16.0f;
      custom_step_ = config_.grid_step;
    }
    ImGui::SameLine();
    if (ImGui::Button("32x32")) {
      config_.grid_step = 32.0f;
      custom_step_ = config_.grid_step;
    }
    ImGui::SameLine();
    if (ImGui::Button("64x64")) {
      config_.grid_step = 64.0f;
      custom_step_ = config_.grid_step;
    }

    // Canvas size info
    ImGui::Separator();
    ImGui::Text("Canvas Information");
    ImGui::Text("Canvas Size: %.0f x %.0f", config_.canvas_size.x,
                config_.canvas_size.y);
    ImGui::Text("Scaled Size: %.0f x %.0f",
                config_.canvas_size.x * config_.global_scale,
                config_.canvas_size.y * config_.global_scale);
    if (bitmap_) {
      ImGui::Text("Bitmap Size: %d x %d", bitmap_->width(), bitmap_->height());
      ImGui::Text(
          "Effective Scale: %.3f x %.3f",
          (config_.canvas_size.x * config_.global_scale) / bitmap_->width(),
          (config_.canvas_size.y * config_.global_scale) / bitmap_->height());
    }

    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

// BPP format management methods
void Canvas::ShowBppFormatSelector() {
  if (!bpp_format_ui_) {
    bpp_format_ui_ =
        std::make_unique<gui::BppFormatUI>(canvas_id_ + "_bpp_format");
  }

  if (bitmap_) {
    bpp_format_ui_->RenderFormatSelector(
        bitmap_, bitmap_->palette(),
        [this](gfx::BppFormat format) { ConvertBitmapFormat(format); });
  }
}

void Canvas::ShowBppAnalysis() {
  if (!bpp_format_ui_) {
    bpp_format_ui_ =
        std::make_unique<gui::BppFormatUI>(canvas_id_ + "_bpp_format");
  }

  if (bitmap_) {
    bpp_format_ui_->RenderAnalysisPanel(*bitmap_, bitmap_->palette());
  }
}

void Canvas::ShowBppConversionDialog() {
  if (!bpp_conversion_dialog_) {
    bpp_conversion_dialog_ = std::make_unique<gui::BppConversionDialog>(
        canvas_id_ + "_bpp_conversion");
  }

  if (bitmap_) {
    bpp_conversion_dialog_->Show(
        *bitmap_, bitmap_->palette(),
        [this](gfx::BppFormat format, bool /*preserve_palette*/) {
          ConvertBitmapFormat(format);
        });
  }

  bpp_conversion_dialog_->Render();
}

bool Canvas::ConvertBitmapFormat(gfx::BppFormat target_format) {
  if (!bitmap_)
    return false;

  gfx::BppFormat current_format = GetCurrentBppFormat();
  if (current_format == target_format) {
    return true;  // No conversion needed
  }

  try {
    // Convert the bitmap data
    auto converted_data = gfx::BppFormatManager::Get().ConvertFormat(
        bitmap_->vector(), current_format, target_format, bitmap_->width(),
        bitmap_->height());

    // Update the bitmap with converted data
    bitmap_->set_data(converted_data);

    // Update the renderer
    bitmap_->UpdateTexture();

    return true;
  } catch (const std::exception& e) {
    SDL_Log("Failed to convert bitmap format: %s", e.what());
    return false;
  }
}

gfx::BppFormat Canvas::GetCurrentBppFormat() const {
  if (!bitmap_)
    return gfx::BppFormat::kBpp8;

  return gfx::BppFormatManager::Get().DetectFormat(
      bitmap_->vector(), bitmap_->width(), bitmap_->height());
}

// Phase 4A: Canvas Automation API
CanvasAutomationAPI* Canvas::GetAutomationAPI() {
  if (!automation_api_) {
    automation_api_ = std::make_unique<CanvasAutomationAPI>(this);
  }
  return automation_api_.get();
}

}  // namespace yaze::gui
