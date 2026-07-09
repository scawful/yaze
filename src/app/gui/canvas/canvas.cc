#include "canvas.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "app/editor/core/content_registry.h"
#include "app/editor/core/event_bus.h"
#include "app/editor/events/core_events.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/util/bpp_format_manager.h"
#include "app/gui/canvas/canvas_automation_api.h"
#include "app/gui/canvas/canvas_extensions.h"
#include "app/gui/canvas/canvas_utils.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"

namespace yaze::gui {

// Define constructors and destructor in .cc to avoid incomplete type issues
// with unique_ptr

// Default constructor
Canvas::Canvas() : renderer_(nullptr) {
  InitializeDefaults();
}

Canvas::~Canvas() {
  // The outer ImVector never runs its elements' destructors, so each nested
  // ImVector<std::string>'s backing array leaks when labels_ is destroyed.
  // clear() releases that array (flagged by AddressSanitizer/LeakSanitizer
  // otherwise). Do NOT destroy the individual std::strings: labels are copied
  // in via ImVector::operator= (a shallow memcpy), so their internal pointers
  // are not independently owned and destroying them would be a bad free.
  for (auto& label_group : labels_) {
    label_group.clear();
  }
}

Canvas::Canvas(const std::string& id) : Canvas() {
  Init(id, ImVec2(0, 0));
}

Canvas::Canvas(const std::string& id, ImVec2 canvas_size) : Canvas() {
  Init(id, canvas_size);
}

Canvas::Canvas(const std::string& id, ImVec2 canvas_size,
               CanvasGridSize grid_size)
    : Canvas() {
  Init(id, canvas_size);
  SetGridSize(grid_size);
}

Canvas::Canvas(const std::string& id, ImVec2 canvas_size,
               CanvasGridSize grid_size, float global_scale)
    : Canvas() {
  Init(id, canvas_size);
  SetGridSize(grid_size);
  config_.global_scale = global_scale;
  global_scale_ = global_scale;
}

Canvas::Canvas(gfx::IRenderer* renderer) : Canvas() {
  SetRenderer(renderer);
}

Canvas::Canvas(gfx::IRenderer* renderer, const std::string& id) : Canvas() {
  SetRenderer(renderer);
  Init(id, ImVec2(0, 0));
}

Canvas::Canvas(gfx::IRenderer* renderer, const std::string& id,
               ImVec2 canvas_size)
    : Canvas() {
  SetRenderer(renderer);
  Init(id, canvas_size);
}

Canvas::Canvas(gfx::IRenderer* renderer, const std::string& id,
               ImVec2 canvas_size, CanvasGridSize grid_size)
    : Canvas() {
  SetRenderer(renderer);
  Init(id, canvas_size);
  SetGridSize(grid_size);
}

Canvas::Canvas(gfx::IRenderer* renderer, const std::string& id,
               ImVec2 canvas_size, CanvasGridSize grid_size, float global_scale)
    : Canvas() {
  SetRenderer(renderer);
  Init(id, canvas_size);
  SetGridSize(grid_size);
  config_.global_scale = global_scale;
  global_scale_ = global_scale;
}

void Canvas::SyncLegacyGeometryFromState() {
  canvas_p0_ = state_.geometry.canvas_p0;
  canvas_p1_ = state_.geometry.canvas_p1;
  canvas_sz_ = state_.geometry.canvas_sz;
  scrolling_ = state_.geometry.scrolling;
}

void Canvas::EnsurePerformanceIntegration() {
  if (!config_.enable_metrics || !usage_tracker_) {
    return;
  }
  if (performance_integration_) {
    return;
  }
  performance_integration_ = std::make_shared<CanvasPerformanceIntegration>();
  performance_integration_->Initialize(canvas_id_);
  performance_integration_->SetUsageTracker(usage_tracker_);
  performance_integration_->StartMonitoring();
}

void Canvas::Init(const CanvasConfig& config) {
  config_ = config;
  canvas_sz_ = config.canvas_size;
  custom_step_ = config.grid_step;
  global_scale_ = config.global_scale;
  enable_grid_ = config.enable_grid;
  enable_hex_tile_labels_ = config.enable_hex_labels;
  enable_custom_labels_ = config.enable_custom_labels;
  enable_context_menu_ = config.enable_context_menu;
  draggable_ = config.is_draggable;
  custom_canvas_size_ = config.custom_canvas_size;
  scrolling_ = config.scrolling;
}

void Canvas::Init(const std::string& id, ImVec2 canvas_size) {
  canvas_id_ = id;
  context_id_ = id + "Context";
  if (canvas_size.x > 0 || canvas_size.y > 0) {
    config_.canvas_size = canvas_size;
    config_.custom_canvas_size = true;
    canvas_sz_ = canvas_size;
  }
  interaction_handler_.Initialize(canvas_id_);
}

CanvasExtensions& Canvas::EnsureExtensions() {
  if (!extensions_) {
    extensions_ = std::make_unique<CanvasExtensions>();
  }
  return *extensions_;
}

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

  // Note: palette_editor is now in CanvasExtensions (lazy-initialized)

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

void Canvas::ZoomIn() {
  float old_scale = global_scale_;
  global_scale_ += 0.25f;
  config_.global_scale = global_scale_;

  // Publish zoom changed event
  if (auto* bus = editor::ContentRegistry::Context::event_bus()) {
    bus->Publish(
        editor::ZoomChangedEvent::Create(canvas_id_, old_scale, global_scale_));
  }
}

void Canvas::ZoomOut() {
  float old_scale = global_scale_;
  global_scale_ -= 0.25f;
  config_.global_scale = global_scale_;

  // Publish zoom changed event
  if (auto* bus = editor::ContentRegistry::Context::event_bus()) {
    bus->Publish(
        editor::ZoomChangedEvent::Create(canvas_id_, old_scale, global_scale_));
  }
}

void Canvas::set_global_scale(float scale) {
  float old_scale = global_scale_;
  global_scale_ = scale;
  config_.global_scale = scale;

  // Publish zoom changed event only if scale actually changed
  if (old_scale != scale) {
    if (auto* bus = editor::ContentRegistry::Context::event_bus()) {
      bus->Publish(
          editor::ZoomChangedEvent::Create(canvas_id_, old_scale, scale));
    }
  }
}

void Canvas::Cleanup() {
  // Cleanup extensions (if initialized)
  if (extensions_) {
    extensions_->Cleanup();
  }
  extensions_.reset();

  selection_.Clear();

  // Stop performance monitoring before cleanup to prevent segfault
  if (performance_integration_) {
    performance_integration_->StopMonitoring();
  }

  // Cleanup enhanced components (non-extension ones)
  context_menu_.reset();
  usage_tracker_.reset();
  performance_integration_.reset();
}

void Canvas::InitializeEnhancedComponents() {
  // Note: modals is now in CanvasExtensions (lazy-initialized on first use)

  // Initialize context menu system
  context_menu_ = std::make_unique<CanvasContextMenu>();
  context_menu_->Initialize(canvas_id_);

  // Initialize usage tracker (optional, controlled by config.enable_metrics)
  if (config_.enable_metrics) {
    usage_tracker_ = std::make_shared<CanvasUsageTracker>();
    usage_tracker_->Initialize(canvas_id_);
    usage_tracker_->StartSession();
    // CanvasPerformanceIntegration is created lazily on first use
    // (ShowPerformanceUI / RecordCanvasOperation) to avoid idle overhead.
  }
}

void Canvas::SetUsageMode(CanvasUsage usage) {
  if (usage_tracker_) {
    usage_tracker_->SetUsageMode(usage);
  }
  if (context_menu_) {
    context_menu_->SetUsageMode(usage);
  }
  config_.usage_mode = usage;
}

void Canvas::RecordCanvasOperation(const std::string& operation_name,
                                   double time_ms) {
  if (usage_tracker_) {
    usage_tracker_->RecordOperation(operation_name, time_ms);
  }
  EnsurePerformanceIntegration();
  if (performance_integration_) {
    performance_integration_->RecordOperation(operation_name, time_ms,
                                              usage_mode());
  }
}

void Canvas::ShowPerformanceUI() {
  EnsurePerformanceIntegration();
  if (performance_integration_) {
    performance_integration_->RenderPerformanceUI();
  }
}

void Canvas::ShowUsageReport() {
  if (usage_tracker_) {
    std::string report = usage_tracker_->ExportUsageReport();
    // Show report in a modal or window (uses ImGui directly, no modals_ needed)
    ImGui::OpenPopup("Canvas Usage Report");
    if (ImGui::BeginPopupModal("Canvas Usage Report", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text(tr("Canvas Usage Report"));
      ImGui::Separator();
      ImGui::TextWrapped("%s", report.c_str());
      ImGui::Separator();
      if (ImGui::Button(tr("Close"))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
}

void Canvas::InitializePaletteEditor(Rom* rom) {
  rom_ = rom;
  auto& ext = EnsureExtensions();
  ext.InitializePaletteEditor();
  if (ext.palette_editor) {
    ext.palette_editor->Initialize(rom);
  }
}

void Canvas::SetGameData(zelda3::GameData* game_data) {
  game_data_ = game_data;
  if (extensions_ && extensions_->palette_editor && game_data) {
    extensions_->palette_editor->Initialize(game_data);
  }
}

void Canvas::ShowPaletteEditor() {
  if (bitmap_) {
    auto& ext = EnsureExtensions();
    ext.InitializePaletteEditor();
    if (ext.palette_editor) {
      auto mutable_palette = bitmap_->mutable_palette();
      ext.palette_editor->ShowPaletteEditor(*mutable_palette,
                                            "Canvas Palette Editor");
    }
  }
}

void Canvas::ShowColorAnalysis() {
  if (bitmap_) {
    auto& ext = EnsureExtensions();
    ext.InitializePaletteEditor();
    if (ext.palette_editor) {
      ext.palette_editor->ShowColorAnalysis(*bitmap_, "Canvas Color Analysis");
    }
  }
}

bool Canvas::ApplyROMPalette(int group_index, int palette_index) {
  if (bitmap_ && extensions_ && extensions_->palette_editor) {
    return extensions_->palette_editor->ApplyROMPalette(bitmap_, group_index,
                                                        palette_index);
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

  // Use NoScrollbar - canvas handles its own scrolling via internal mechanism
  bool result =
      ImGui::BeginChild(child_id.c_str(), child_size,
                        true,  // Always show border for table integration
                        ImGuiWindowFlags_NoScrollbar);

  if (!label.empty()) {
    ImGui::Text("%s", label.c_str());
  }

  return result;
}

void Canvas::EndTableCanvas() {
  ImGui::EndChild();
}

CanvasRuntime Canvas::BeginInTable(const std::string& label,
                                   const CanvasFrameOptions& options) {
  // Calculate child size from options or auto-resize
  ImVec2 child_size = options.canvas_size;
  if (child_size.x <= 0 || child_size.y <= 0) {
    child_size = config_.auto_resize ? GetPreferredSize() : config_.canvas_size;
  }

  if (config_.auto_resize && child_size.x > 0 && child_size.y > 0) {
    CanvasUtils::SetNextCanvasSize(child_size, true);
  }

  // Begin child window for table integration
  // Use NoScrollbar - canvas handles its own scrolling via internal mechanism
  std::string child_id = canvas_id_ + "_TableChild";
  ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoScrollbar;
  if (options.show_scrollbar) {
    child_flags = ImGuiWindowFlags_AlwaysVerticalScrollbar;
  }
  ImGui::BeginChild(child_id.c_str(), child_size, true, child_flags);

  if (!label.empty()) {
    ImGui::Text("%s", label.c_str());
  }

  // Draw background and set up canvas state
  Begin(options);

  // Build and return runtime
  CanvasRuntime rt = BuildCurrentRuntime();
  if (options.grid_step.has_value()) {
    rt.grid_step = options.grid_step.value();
  }
  return rt;
}

void Canvas::EndInTable(CanvasRuntime& runtime,
                        const CanvasFrameOptions& options) {
  // Draw grid if enabled
  if (options.draw_grid) {
    float step = options.grid_step.value_or(config_.grid_step);
    DrawGrid(step);
  }

  // Draw overlay
  if (options.draw_overlay) {
    DrawOverlay();
  }

  // Render persistent popups if enabled
  if (options.render_popups) {
    RenderPersistentPopups();
  }

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

void Canvas::Begin(const CanvasFrameOptions& options) {
  gui::BeginPadding(1);

  // Only wrap in child window if explicitly requested
  if (options.use_child_window) {
    // Calculate effective size
    ImVec2 effective_size = options.canvas_size;
    if (effective_size.x == 0 && effective_size.y == 0) {
      if (IsAutoResize()) {
        effective_size = GetPreferredSize();
      } else {
        effective_size = GetCurrentSize();
      }
    }

    ImGuiWindowFlags child_flags = ImGuiWindowFlags_None;
    if (options.show_scrollbar) {
      child_flags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
    }
    ImGui::BeginChild(canvas_id().c_str(), effective_size, true, child_flags);
  }

  // Apply grid step from options if specified
  if (options.grid_step.has_value()) {
    SetCustomGridStep(options.grid_step.value());
  }

  DrawBackground(options.canvas_size);
  gui::EndPadding();

  if (options.draw_context_menu) {
    DrawContextMenu();
  }
}

void Canvas::End(const CanvasFrameOptions& options) {
  if (options.draw_grid) {
    DrawGrid(options.grid_step.value_or(GetGridStep()));
  }
  if (options.draw_overlay) {
    DrawOverlay();
  }
  if (options.render_popups) {
    RenderPersistentPopups();
  }
  // Only end child if we started one
  if (options.use_child_window) {
    ImGui::EndChild();
  }
}

// ==================== Legacy Interface ====================

void Canvas::UpdateColorPainter(gfx::IRenderer* /*renderer*/,
                                gfx::Bitmap& bitmap, const ImVec4& color,
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

  // Phase 1: Calculate geometry using new helper
  state_.geometry = CalculateCanvasGeometry(
      config_, canvas_size, GetCursorScreenPos(), GetContentRegionAvail());

  SyncLegacyGeometryFromState();

  // Update config if explicit size provided
  if (canvas_size.x != 0) {
    config_.canvas_size = canvas_size;
  }

  // Phase 1: Render background using helper
  RenderCanvasBackground(draw_list_, state_.geometry);

  ImGui::InvisibleButton(canvas_id_.c_str(), state_.geometry.scaled_size,
                         kMouseFlags);

  // CRITICAL FIX: Always update hover mouse position when hovering over canvas
  // This fixes the regression where CheckForCurrentMap() couldn't track hover
  // Phase 1: Use geometry helper for mouse calculation
  if (IsItemHovered()) {
    const ImGuiIO& io = GetIO();
    mouse_pos_in_canvas_ = CalculateMouseInCanvas(state_.geometry, io.MousePos);
    state_.mouse_pos_in_canvas = mouse_pos_in_canvas_;  // Sync to state
    state_.is_hovered = true;
    is_hovered_ = true;
  } else {
    state_.is_hovered = false;
    is_hovered_ = false;
  }

  // iOS/tablet gestures currently synthesize ImGui wheel deltas (see src/ios/main.mm).
  // Consume them here to pan/zoom the canvas without triggering ImGui window scrolling.
  if (LayoutHelpers::IsTouchDevice() && IsItemHovered()) {
    ImGuiIO& io = GetIO();
    const float wheel_x = io.MouseWheelH;
    const float wheel_y = io.MouseWheel;

    if (wheel_x != 0.0f || wheel_y != 0.0f) {
      // Prevent parent windows/child regions from scrolling on touch gestures.
      io.MouseWheelH = 0.0f;
      io.MouseWheel = 0.0f;

      if (io.KeyCtrl && wheel_y != 0.0f) {
        // Ctrl+wheel: zoom (pinch on iOS).
        constexpr float kMinScale = 0.25f;
        constexpr float kMaxScale = 8.0f;
        const float unclamped = global_scale_ * (1.0f + wheel_y);
        const float new_scale = std::clamp(unclamped, kMinScale, kMaxScale);

        if (new_scale != global_scale_) {
          const ImVec2 new_scroll = ComputeScrollForZoomAtScreenPos(
              state_.geometry, global_scale_, new_scale, io.MousePos);
          set_global_scale(new_scale);
          state_.geometry.scrolling = new_scroll;
          SyncLegacyGeometryFromState();
          config_.scrolling = scrolling_;
        }
      } else {
        // Plain wheel: pan (two-finger pan on iOS).
        constexpr float kTouchWheelToPixels = 10.0f;
        ApplyScrollDelta(state_.geometry,
                         ImVec2(wheel_x * kTouchWheelToPixels,
                                wheel_y * kTouchWheelToPixels));
        SyncLegacyGeometryFromState();
        config_.scrolling = scrolling_;
      }
    }
  }

  // Pan handling (Phase 1: Use geometry helper)
  if (config_.is_draggable && IsItemHovered()) {
    const ImGuiIO& io = GetIO();
    const bool is_active = IsItemActive();  // Held

    // Pan (we use a zero mouse threshold when there's no context menu)
    if (const float mouse_threshold_for_pan =
            enable_context_menu_ ? -1.0f : 0.0f;
        is_active &&
        IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan)) {
      ApplyScrollDelta(state_.geometry, io.MouseDelta);
      SyncLegacyGeometryFromState();
      config_.scrolling = scrolling_;
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
    CanvasConfig snapshot;
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
        [this](CanvasContextMenu::Command command,
               const CanvasConfig& updated_config) {
          switch (command) {
            case CanvasContextMenu::Command::kResetView:
              ResetView();
              break;
            case CanvasContextMenu::Command::kZoomToFit:
              if (bitmap_) {
                SetZoomToFit(*bitmap_);
              }
              break;
            case CanvasContextMenu::Command::kZoomIn:
              SetGlobalScale(config_.global_scale * 1.25f);
              break;
            case CanvasContextMenu::Command::kZoomOut:
              SetGlobalScale(config_.global_scale * 0.8f);
              break;
            case CanvasContextMenu::Command::kToggleGrid:
              config_.enable_grid = !config_.enable_grid;
              enable_grid_ = config_.enable_grid;
              break;
            case CanvasContextMenu::Command::kToggleHexLabels:
              config_.enable_hex_labels = !config_.enable_hex_labels;
              enable_hex_tile_labels_ = config_.enable_hex_labels;
              break;
            case CanvasContextMenu::Command::kToggleCustomLabels:
              config_.enable_custom_labels = !config_.enable_custom_labels;
              enable_custom_labels_ = config_.enable_custom_labels;
              break;
            case CanvasContextMenu::Command::kToggleContextMenu:
              config_.enable_context_menu = !config_.enable_context_menu;
              enable_context_menu_ = config_.enable_context_menu;
              break;
            case CanvasContextMenu::Command::kToggleAutoResize:
              config_.auto_resize = !config_.auto_resize;
              break;
            case CanvasContextMenu::Command::kToggleDraggable:
              config_.is_draggable = !config_.is_draggable;
              draggable_ = config_.is_draggable;
              break;
            case CanvasContextMenu::Command::kSetGridStep:
              config_.grid_step = updated_config.grid_step;
              custom_step_ = config_.grid_step;
              break;
            case CanvasContextMenu::Command::kSetScale:
              config_.global_scale = updated_config.global_scale;
              global_scale_ = config_.global_scale;
              break;
            case CanvasContextMenu::Command::kOpenAdvancedProperties: {
              auto& ext = EnsureExtensions();
              ext.InitializeModals();
              if (ext.modals) {
                CanvasConfig modal_config = updated_config;
                modal_config.on_config_changed =
                    [this](const CanvasConfig& cfg) {
                      ApplyConfigSnapshot(cfg);
                    };
                modal_config.on_scale_changed =
                    [this](const CanvasConfig& cfg) {
                      ApplyScaleSnapshot(cfg);
                    };
                ext.modals->ShowAdvancedProperties(canvas_id_, modal_config,
                                                   bitmap_);
              }
            } break;
            case CanvasContextMenu::Command::kOpenScalingControls: {
              auto& ext = EnsureExtensions();
              ext.InitializeModals();
              if (ext.modals) {
                CanvasConfig modal_config = updated_config;
                modal_config.on_config_changed =
                    [this](const CanvasConfig& cfg) {
                      ApplyConfigSnapshot(cfg);
                    };
                modal_config.on_scale_changed =
                    [this](const CanvasConfig& cfg) {
                      ApplyScaleSnapshot(cfg);
                    };
                ext.modals->ShowScalingControls(canvas_id_, modal_config,
                                                bitmap_);
              }
            } break;
            default:
              break;
          }
        },
        snapshot, this);  // Phase 4: Pass Canvas* for editor menu integration

    if (extensions_ && extensions_->modals) {
      extensions_->modals->Render();
    }

    return;
  }

  // Draw enhanced property dialogs
  ShowAdvancedCanvasProperties();
  ShowScalingControls();
}

void Canvas::DrawContextMenuItem(const gui::CanvasMenuItem& item) {
  // Phase 4: Use RenderMenuItem from canvas_menu.h for consistent rendering
  auto popup_callback = [this](const std::string& id,
                               std::function<void()> callback) {
    popup_registry_.Open(id, callback);
  };

  gui::RenderMenuItem(item, popup_callback);
}

void Canvas::AddContextMenuItem(const gui::CanvasMenuItem& item) {
  // Phase 4: Add to editor menu definition
  // Items are added to a default section with editor-specific priority
  if (editor_menu_.sections.empty()) {
    CanvasMenuSection section;
    section.priority = MenuSectionPriority::kEditorSpecific;
    section.separator_after = true;
    editor_menu_.sections.push_back(section);
  }

  // Add to the last section (or create new if the last isn't editor-specific)
  auto& last_section = editor_menu_.sections.back();
  if (last_section.priority != MenuSectionPriority::kEditorSpecific) {
    CanvasMenuSection new_section;
    new_section.priority = MenuSectionPriority::kEditorSpecific;
    new_section.separator_after = true;
    editor_menu_.sections.push_back(new_section);
    editor_menu_.sections.back().items.push_back(item);
  } else {
    last_section.items.push_back(item);
  }
}

void Canvas::ClearContextMenuItems() {
  editor_menu_.sections.clear();
}

void Canvas::OpenPersistentPopup(const std::string& popup_id,
                                 std::function<void()> render_callback) {
  // Phase 4: Simplified popup management (no legacy synchronization)
  popup_registry_.Open(popup_id, render_callback);
}

void Canvas::ClosePersistentPopup(const std::string& popup_id) {
  // Phase 4: Simplified popup management (no legacy synchronization)
  popup_registry_.Close(popup_id);
}

void Canvas::RenderPersistentPopups() {
  // Phase 4: Simplified rendering (no legacy synchronization)
  popup_registry_.RenderAll();
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
  config_.scrolling = ImVec2(0, 0);  // Sync config for persistence
}

void Canvas::ApplyConfigSnapshot(const CanvasConfig& snapshot) {
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

void Canvas::ApplyScaleSnapshot(const CanvasConfig& snapshot) {
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
  // Update hover state for backward compatibility
  is_hovered_ = IsItemHovered();

  // Clear points if not hovered (legacy behavior)
  if (!is_hovered_) {
    points_.clear();
    return false;
  }

  // Build runtime and delegate to stateless helper
  CanvasRuntime rt = BuildCurrentRuntime();
  ImVec2 drawn_pos;
  bool result = gui::DrawTilemapPainter(rt, tilemap, current_tile, &drawn_pos);

  // Sync legacy state from stateless call
  if (is_hovered_) {
    const ImGuiIO& io = GetIO();
    const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                        canvas_p0_.y + scrolling_.y);
    const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
    const float scaled_size = tilemap.tile_size.x * global_scale_;
    ImVec2 paint_pos = AlignPosToGrid(mouse_pos, scaled_size);
    mouse_pos_in_canvas_ = paint_pos;

    points_.clear();
    points_.push_back(paint_pos);
    points_.push_back(
        ImVec2(paint_pos.x + scaled_size, paint_pos.y + scaled_size));
  }

  if (result) {
    drawn_tile_pos_ = drawn_pos;
  }

  return result;
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
  // Update hover state for backward compatibility
  is_hovered_ = IsItemHovered();

  if (size_y == 0) {
    size_y = size;
  }

  // Build runtime and delegate to stateless helper
  CanvasRuntime rt = BuildCurrentRuntime();
  ImVec2 selected_pos;
  bool double_clicked = gui::DrawTileSelector(rt, size, size_y, &selected_pos);

  // Sync legacy state: update points_ on click
  if (is_hovered_ && IsMouseClicked(ImGuiMouseButton_Left)) {
    const ImGuiIO& io = GetIO();
    const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                        canvas_p0_.y + scrolling_.y);
    const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
    ImVec2 painter_pos = AlignPosToGrid(mouse_pos, static_cast<float>(size));

    points_.clear();
    points_.push_back(painter_pos);
    points_.push_back(ImVec2(painter_pos.x + size, painter_pos.y + size_y));
    mouse_pos_in_canvas_ = painter_pos;
  }

  return double_clicked;
}

void Canvas::DrawSelectRect(int current_map, int tile_size, float scale) {
  gfx::ScopedTimer timer("canvas_select_rect");

  // Update hover state
  is_hovered_ = IsItemHovered();
  if (!is_hovered_) {
    return;
  }

  // Build runtime and delegate to stateless helper
  CanvasRuntime rt = BuildCurrentRuntime();
  rt.scale = scale;  // Use the passed scale, not global_scale_

  // Use a temporary selection to capture output from stateless helper
  CanvasSelection temp_selection;
  temp_selection.selected_tiles = selected_tiles_;
  temp_selection.selected_tile_pos = selected_tile_pos_;
  temp_selection.select_rect_active = select_rect_active_;
  for (int i = 0; i < selected_points_.size(); ++i) {
    temp_selection.selected_points.push_back(selected_points_[i]);
  }

  gui::DrawSelectRect(rt, current_map, tile_size, scale, temp_selection);

  // Sync back to legacy members
  selected_tiles_ = temp_selection.selected_tiles;
  selected_tile_pos_ = temp_selection.selected_tile_pos;
  select_rect_active_ = temp_selection.select_rect_active;
  selected_points_.clear();
  for (const auto& pt : temp_selection.selected_points) {
    selected_points_.push_back(pt);
  }
}

void Canvas::DrawBitmap(Bitmap& bitmap, int border_offset, float scale) {
  if (!bitmap.is_active()) {
    return;
  }
  bitmap_ = &bitmap;

  // Update content size for table integration
  config_.content_size = ImVec2(bitmap.width(), bitmap.height());

  // Phase 1: Use rendering helper
  RenderBitmapOnCanvas(draw_list_, state_.geometry, bitmap, border_offset,
                       scale);
}

void Canvas::DrawBitmap(Bitmap& bitmap, int x_offset, int y_offset, float scale,
                        int alpha) {
  if (!bitmap.is_active()) {
    return;
  }
  bitmap_ = &bitmap;

  // Update content size for table integration
  // CRITICAL: Store UNSCALED bitmap size as content - scale is applied during
  // rendering
  config_.content_size = ImVec2(bitmap.width(), bitmap.height());

  // Phase 1: Use rendering helper
  RenderBitmapOnCanvas(draw_list_, state_.geometry, bitmap, x_offset, y_offset,
                       scale, alpha);
}

void Canvas::DrawBitmap(Bitmap& bitmap, ImVec2 dest_pos, ImVec2 dest_size,
                        ImVec2 src_pos, ImVec2 src_size) {
  if (!bitmap.is_active()) {
    return;
  }
  bitmap_ = &bitmap;

  // Update content size for table integration
  config_.content_size = ImVec2(bitmap.width(), bitmap.height());

  // Phase 1: Use rendering helper
  RenderBitmapOnCanvas(draw_list_, state_.geometry, bitmap, dest_pos, dest_size,
                       src_pos, src_size);
}

// TODO: Add parameters for sizing and positioning
void Canvas::DrawBitmapTable(const BitmapTable& gfx_bin) {
  for (const auto& [key, value] : gfx_bin) {
    // Skip null or inactive bitmaps without valid textures
    if (!value || !value->is_active() || !value->texture()) {
      continue;
    }
    int offset = 0x40 * (key + 1);
    int top_left_y = canvas_p0_.y + 2;
    if (key >= 1) {
      top_left_y = canvas_p0_.y + 0x40 * key;
    }
    draw_list_->AddImage((ImTextureID)(intptr_t)value->texture(),
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
                             int tile_size, float /*scale*/, int local_map_size,
                             ImVec2 total_map_size) {
  if (selected_points_.size() != 2) {
    // points_ should contain exactly two points
    return;
  }
  if (group.empty()) {
    // group should not be empty
    return;
  }

  // CRITICAL: Use config_.global_scale for consistency with DrawOverlay
  // which also uses config_.global_scale for the selection rectangle outline.
  // Using the passed 'scale' parameter would cause misalignment if they differ.
  const float effective_scale = config_.global_scale;

  // OPTIMIZATION: Use optimized rendering for large groups to improve
  // performance
  bool use_optimized_rendering =
      group.size() > 128;  // Optimize for large selections

  // Use provided map sizes for proper boundary handling
  const int small_map = local_map_size;
  const float large_map_width = total_map_size.x;
  const float large_map_height = total_map_size.y;

  // Pre-calculate common values to avoid repeated computation
  const float tile_scale = tile_size * effective_scale;
  const int atlas_tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;

  // Top-left and bottom-right corners of the rectangle (in world coordinates)
  ImVec2 rect_top_left = selected_points_[0];
  ImVec2 rect_bottom_right = selected_points_[1];

  // Calculate the start and end tiles in the grid
  // selected_points are now in world coordinates, so divide by tile_size only
  int start_tile_x = static_cast<int>(std::floor(rect_top_left.x / tile_size));
  int start_tile_y = static_cast<int>(std::floor(rect_top_left.y / tile_size));
  int end_tile_x =
      static_cast<int>(std::floor(rect_bottom_right.x / tile_size));
  int end_tile_y =
      static_cast<int>(std::floor(rect_bottom_right.y / tile_size));

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
        int tile_pos_x = (x + start_tile_x) * tile_size * effective_scale;
        int tile_pos_y = (y + start_tile_y) * tile_size * effective_scale;

        // OPTIMIZATION: Use pre-calculated values for better performance with
        // large selections
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
            float screen_w = tilemap.tile_size.x * effective_scale;
            float screen_h = tilemap.tile_size.y * effective_scale;

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

  // Performance optimization completed - tiles are now rendered with
  // pre-calculated values

  // Reposition rectangle to follow mouse, but clamp to prevent wrapping across
  // map boundaries
  const ImGuiIO& io = GetIO();
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // CRITICAL FIX: Clamp BEFORE grid alignment for smoother dragging behavior
  // This prevents the rectangle from even attempting to cross boundaries during
  // drag
  ImVec2 clamped_mouse_pos = mouse_pos;

  if (config_.clamp_rect_to_local_maps) {
    // Calculate which local map the mouse is in
    int mouse_local_map_x = static_cast<int>(mouse_pos.x) / small_map;
    int mouse_local_map_y = static_cast<int>(mouse_pos.y) / small_map;

    // Calculate where the rectangle END would be if we place it at mouse
    // position
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

  // Now grid-align the clamped position (in screen coords)
  auto new_start_pos_screen =
      AlignPosToGrid(clamped_mouse_pos, tile_size * effective_scale);

  // Convert to world coordinates for storage (selected_points_ stores world coords)
  ImVec2 new_start_pos_world(new_start_pos_screen.x / effective_scale,
                             new_start_pos_screen.y / effective_scale);

  // Additional safety: clamp to overall map bounds (in world coordinates)
  new_start_pos_world.x =
      std::clamp(new_start_pos_world.x, 0.0f, large_map_width - rect_width);
  new_start_pos_world.y =
      std::clamp(new_start_pos_world.y, 0.0f, large_map_height - rect_height);

  selected_points_.clear();
  selected_points_.push_back(new_start_pos_world);
  selected_points_.push_back(ImVec2(new_start_pos_world.x + rect_width,
                                    new_start_pos_world.y + rect_height));
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

  // Use high-level utility function with local points (synchronized from
  // interaction handler)
  CanvasUtils::DrawCanvasOverlay(ctx, points_, selected_points_);

  // Render any persistent popups from context menu actions
  RenderPersistentPopups();
}

void Canvas::DrawLayeredElements() {
  // Based on ImGui demo, should be adapted to use for OAM
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  {
    Text(tr("Blue shape is drawn first: appears in back"));
    Text(tr("Red shape is drawn after: appears in front"));
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
    Text(tr("Blue shape is drawn first, into channel 1: appears in front"));
    Text(tr("Red shape is drawn after, into channel 0: appears in back"));
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
    Text(
        tr("After reordering, contents of channel 0 appears below channel 1."));
  }
}

void Canvas::ShowAdvancedCanvasProperties() {
  // Use the new modal system (lazy-initialized via extensions)
  auto& ext = EnsureExtensions();
  ext.InitializeModals();
  if (ext.modals) {
    CanvasConfig modal_config;
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
        [this](const CanvasConfig& updated_config) {
          // Update legacy variables when config changes
          enable_grid_ = updated_config.enable_grid;
          enable_hex_tile_labels_ = updated_config.enable_hex_labels;
          enable_custom_labels_ = updated_config.enable_custom_labels;
        };
    modal_config.on_scale_changed = [this](const CanvasConfig& updated_config) {
      global_scale_ = updated_config.global_scale;
      scrolling_ = updated_config.scrolling;
    };

    ext.modals->ShowAdvancedProperties(canvas_id_, modal_config, bitmap_);
    return;
  }

  // Fallback to legacy modal system
  if (ImGui::BeginPopupModal("Advanced Canvas Properties", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(tr("Advanced Canvas Configuration"));
    ImGui::Separator();

    // Canvas properties (read-only info)
    ImGui::Text(tr("Canvas Properties"));
    ImGui::Text(tr("ID: %s"), canvas_id_.c_str());
    ImGui::Text(tr("Canvas Size: %.0f x %.0f"), config_.canvas_size.x,
                config_.canvas_size.y);
    ImGui::Text(tr("Content Size: %.0f x %.0f"), config_.content_size.x,
                config_.content_size.y);
    ImGui::Text(tr("Global Scale: %.3f"), config_.global_scale);
    ImGui::Text(tr("Grid Step: %.1f"), config_.grid_step);

    if (config_.content_size.x > 0 && config_.content_size.y > 0) {
      ImVec2 min_size = GetMinimumSize();
      ImVec2 preferred_size = GetPreferredSize();
      ImGui::Text(tr("Minimum Size: %.0f x %.0f"), min_size.x, min_size.y);
      ImGui::Text(tr("Preferred Size: %.0f x %.0f"), preferred_size.x,
                  preferred_size.y);
    }

    // Editable properties using new config system
    ImGui::Separator();
    ImGui::Text(tr("View Settings"));
    if (ImGui::Checkbox(tr("Enable Grid"), &config_.enable_grid)) {
      enable_grid_ = config_.enable_grid;  // Legacy sync
    }
    if (ImGui::Checkbox(tr("Enable Hex Labels"), &config_.enable_hex_labels)) {
      enable_hex_tile_labels_ = config_.enable_hex_labels;  // Legacy sync
    }
    if (ImGui::Checkbox(tr("Enable Custom Labels"),
                        &config_.enable_custom_labels)) {
      enable_custom_labels_ = config_.enable_custom_labels;  // Legacy sync
    }
    if (ImGui::Checkbox(tr("Enable Context Menu"),
                        &config_.enable_context_menu)) {
      enable_context_menu_ = config_.enable_context_menu;  // Legacy sync
    }
    if (ImGui::Checkbox(tr("Draggable"), &config_.is_draggable)) {
      draggable_ = config_.is_draggable;  // Legacy sync
    }
    if (ImGui::Checkbox(tr("Auto Resize for Tables"), &config_.auto_resize)) {
      // Auto resize setting changed
    }

    // Grid controls
    ImGui::Separator();
    ImGui::Text(tr("Grid Configuration"));
    if (ImGui::SliderFloat(tr("Grid Step"), &config_.grid_step, 1.0f, 128.0f,
                           "%.1f")) {
      custom_step_ = config_.grid_step;  // Legacy sync
    }

    // Scale controls
    ImGui::Separator();
    ImGui::Text(tr("Scale Configuration"));
    if (ImGui::SliderFloat(tr("Global Scale"), &config_.global_scale, 0.1f,
                           10.0f, "%.2f")) {
      global_scale_ = config_.global_scale;  // Legacy sync
    }

    // Scrolling controls
    ImGui::Separator();
    ImGui::Text(tr("Scrolling Configuration"));
    ImGui::Text(tr("Current Scroll: %.1f, %.1f"), scrolling_.x, scrolling_.y);
    if (ImGui::Button(tr("Reset Scroll"))) {
      scrolling_ = ImVec2(0, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Center View"))) {
      if (bitmap_) {
        scrolling_ = ImVec2(
            -(bitmap_->width() * config_.global_scale - config_.canvas_size.x) /
                2.0f,
            -(bitmap_->height() * config_.global_scale -
              config_.canvas_size.y) /
                2.0f);
      }
    }

    if (ImGui::Button(tr("Close"))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

// Old ShowPaletteManager method removed - now handled by PaletteWidget

void Canvas::ShowScalingControls() {
  // Use the new modal system (lazy-initialized via extensions)
  auto& ext = EnsureExtensions();
  ext.InitializeModals();
  if (ext.modals) {
    CanvasConfig modal_config;
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
        [this](const CanvasConfig& updated_config) {
          // Update legacy variables when config changes
          enable_grid_ = updated_config.enable_grid;
          enable_hex_tile_labels_ = updated_config.enable_hex_labels;
          enable_custom_labels_ = updated_config.enable_custom_labels;
          enable_context_menu_ = updated_config.enable_context_menu;
        };
    modal_config.on_scale_changed = [this](const CanvasConfig& updated_config) {
      draggable_ = updated_config.is_draggable;
      custom_step_ = updated_config.grid_step;
      global_scale_ = updated_config.global_scale;
      scrolling_ = updated_config.scrolling;
    };

    ext.modals->ShowScalingControls(canvas_id_, modal_config);
    return;
  }

  // Fallback to legacy modal system
  if (ImGui::BeginPopupModal("Scaling Controls", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(tr("Canvas Scaling and Display Controls"));
    ImGui::Separator();

    // Global scale with new config system
    ImGui::Text(tr("Global Scale: %.3f"), config_.global_scale);
    if (ImGui::SliderFloat("##GlobalScale", &config_.global_scale, 0.1f, 10.0f,
                           "%.2f")) {
      global_scale_ = config_.global_scale;  // Legacy sync
    }

    // Preset scale buttons
    ImGui::Text(tr("Preset Scales:"));
    if (ImGui::Button(tr("0.25x"))) {
      config_.global_scale = 0.25f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("0.5x"))) {
      config_.global_scale = 0.5f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("1x"))) {
      config_.global_scale = 1.0f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("2x"))) {
      config_.global_scale = 2.0f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("4x"))) {
      config_.global_scale = 4.0f;
      global_scale_ = config_.global_scale;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("8x"))) {
      config_.global_scale = 8.0f;
      global_scale_ = config_.global_scale;
    }

    // Grid configuration
    ImGui::Separator();
    ImGui::Text(tr("Grid Configuration"));
    ImGui::Text(tr("Grid Step: %.1f"), config_.grid_step);
    if (ImGui::SliderFloat("##GridStep", &config_.grid_step, 1.0f, 128.0f,
                           "%.1f")) {
      custom_step_ = config_.grid_step;  // Legacy sync
    }

    // Grid size presets
    ImGui::Text(tr("Grid Presets:"));
    if (ImGui::Button(tr("8x8"))) {
      config_.grid_step = 8.0f;
      custom_step_ = config_.grid_step;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("16x16"))) {
      config_.grid_step = 16.0f;
      custom_step_ = config_.grid_step;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("32x32"))) {
      config_.grid_step = 32.0f;
      custom_step_ = config_.grid_step;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("64x64"))) {
      config_.grid_step = 64.0f;
      custom_step_ = config_.grid_step;
    }

    // Canvas size info
    ImGui::Separator();
    ImGui::Text(tr("Canvas Information"));
    ImGui::Text(tr("Canvas Size: %.0f x %.0f"), config_.canvas_size.x,
                config_.canvas_size.y);
    ImGui::Text(tr("Scaled Size: %.0f x %.0f"),
                config_.canvas_size.x * config_.global_scale,
                config_.canvas_size.y * config_.global_scale);
    if (bitmap_) {
      ImGui::Text(tr("Bitmap Size: %d x %d"), bitmap_->width(),
                  bitmap_->height());
      ImGui::Text(
          tr("Effective Scale: %.3f x %.3f"),
          (config_.canvas_size.x * config_.global_scale) / bitmap_->width(),
          (config_.canvas_size.y * config_.global_scale) / bitmap_->height());
    }

    if (ImGui::Button(tr("Close"))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

// BPP format management methods
void Canvas::ShowBppFormatSelector() {
  auto& ext = EnsureExtensions();
  ext.InitializeBppUI(canvas_id_);

  if (bitmap_ && ext.bpp_format_ui) {
    ext.bpp_format_ui->RenderFormatSelector(
        bitmap_, bitmap_->palette(),
        [this](gfx::BppFormat format) { ConvertBitmapFormat(format); });
  }
}

void Canvas::ShowBppAnalysis() {
  auto& ext = EnsureExtensions();
  ext.InitializeBppUI(canvas_id_);

  if (bitmap_ && ext.bpp_format_ui) {
    ext.bpp_format_ui->RenderAnalysisPanel(*bitmap_, bitmap_->palette());
  }
}

void Canvas::ShowBppConversionDialog() {
  auto& ext = EnsureExtensions();
  if (!ext.bpp_conversion_dialog) {
    ext.bpp_conversion_dialog = std::make_unique<gui::BppConversionDialog>(
        canvas_id_ + "_bpp_conversion");
  }

  if (bitmap_ && ext.bpp_conversion_dialog) {
    ext.bpp_conversion_dialog->Show(
        *bitmap_, bitmap_->palette(),
        [this](gfx::BppFormat format, bool /*preserve_palette*/) {
          ConvertBitmapFormat(format);
        });
  }

  if (ext.bpp_conversion_dialog) {
    ext.bpp_conversion_dialog->Render();
  }
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
  auto& ext = EnsureExtensions();
  ext.InitializeAutomation(this);
  return ext.automation_api.get();
}

// =============================================================================
// Canvas::AddXxxAt Methods
// =============================================================================

void Canvas::AddImageAt(ImTextureID texture, ImVec2 local_top_left,
                        ImVec2 size) {
  if (draw_list_ == nullptr)
    return;
  ImVec2 screen_pos(canvas_p0_.x + local_top_left.x * global_scale_,
                    canvas_p0_.y + local_top_left.y * global_scale_);
  ImVec2 screen_end(screen_pos.x + size.x * global_scale_,
                    screen_pos.y + size.y * global_scale_);
  draw_list_->AddImage(texture, screen_pos, screen_end);
}

void Canvas::AddRectFilledAt(ImVec2 local_top_left, ImVec2 size,
                             uint32_t color) {
  if (draw_list_ == nullptr)
    return;
  ImVec2 screen_pos(canvas_p0_.x + local_top_left.x * global_scale_,
                    canvas_p0_.y + local_top_left.y * global_scale_);
  ImVec2 screen_end(screen_pos.x + size.x * global_scale_,
                    screen_pos.y + size.y * global_scale_);
  draw_list_->AddRectFilled(screen_pos, screen_end, color);
}

void Canvas::AddTextAt(ImVec2 local_pos, const std::string& text,
                       uint32_t color) {
  if (draw_list_ == nullptr)
    return;
  ImVec2 screen_pos(canvas_p0_.x + local_pos.x * global_scale_,
                    canvas_p0_.y + local_pos.y * global_scale_);
  draw_list_->AddText(screen_pos, color, text.c_str());
}

// =============================================================================
// CanvasFrame RAII Class
// =============================================================================

CanvasFrame::CanvasFrame(Canvas& canvas, CanvasFrameOptions options)
    : canvas_(&canvas), options_(options), active_(true) {
  canvas_->Begin(options_);
}

CanvasFrame::~CanvasFrame() {
  if (active_) {
    canvas_->End(options_);
  }
}

CanvasFrame::CanvasFrame(CanvasFrame&& other) noexcept
    : canvas_(other.canvas_), options_(other.options_), active_(other.active_) {
  other.active_ = false;
}

CanvasFrame& CanvasFrame::operator=(CanvasFrame&& other) noexcept {
  if (this != &other) {
    if (active_) {
      canvas_->End(options_);
    }
    canvas_ = other.canvas_;
    options_ = other.options_;
    active_ = other.active_;
    other.active_ = false;
  }
  return *this;
}

}  // namespace yaze::gui
