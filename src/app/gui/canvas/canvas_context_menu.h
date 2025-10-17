#ifndef YAZE_APP_GUI_CANVAS_CANVAS_CONTEXT_MENU_H
#define YAZE_APP_GUI_CANVAS_CANVAS_CONTEXT_MENU_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/icons.h"
#include "app/gui/canvas/canvas_modals.h"
#include "app/gui/canvas/canvas_menu.h"
#include "canvas_usage_tracker.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Forward declarations
class PaletteEditorWidget;
class Canvas;

class CanvasContextMenu {
 public:
  enum class Command {
    kNone,
    kResetView,
    kZoomToFit,
    kZoomIn,
    kZoomOut,
    kToggleGrid,
    kToggleHexLabels,
    kToggleCustomLabels,
    kToggleContextMenu,
    kToggleAutoResize,
    kToggleDraggable,
    kOpenAdvancedProperties,
    kOpenScalingControls,
    kSetGridStep,
    kSetScale,
  };

  CanvasContextMenu() = default;

  // Phase 4: Use unified CanvasMenuItem from canvas_menu.h
  using CanvasMenuItem = gui::CanvasMenuItem;

  void Initialize(const std::string& canvas_id);
  void SetUsageMode(CanvasUsage usage);
  void AddMenuItem(const CanvasMenuItem& item);
  void AddMenuItem(const CanvasMenuItem& item, CanvasUsage usage);
  void ClearMenuItems();

  // Phase 4: Render with editor menu integration and priority ordering
  void Render(const std::string& context_id,
              const ImVec2& mouse_pos,
              Rom* rom,
              const gfx::Bitmap* bitmap,
              const gfx::SnesPalette* palette,
              const std::function<void(Command, const CanvasConfig&)>& command_handler,
              CanvasConfig current_config,
              Canvas* canvas);

  bool ShouldShowContextMenu() const;
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  bool IsEnabled() const { return enabled_; }
  CanvasUsage GetUsageMode() const { return current_usage_; }

  void SetCanvasState(const ImVec2& canvas_size,
                      const ImVec2& content_size,
                      float global_scale,
                      float grid_step,
                      bool enable_grid,
                      bool enable_hex_labels,
                      bool enable_custom_labels,
                      bool enable_context_menu,
                      bool is_draggable,
                      bool auto_resize,
                      const ImVec2& scrolling);

 private:
  std::string canvas_id_;
  bool enabled_ = true;
  CanvasUsage current_usage_ = CanvasUsage::kTilePainting;

  ImVec2 canvas_size_;
  ImVec2 content_size_;
  float global_scale_ = 1.0f;
  float grid_step_ = 32.0f;
  bool enable_grid_ = true;
  bool enable_hex_labels_ = false;
  bool enable_custom_labels_ = false;
  bool enable_context_menu_ = true;
  bool is_draggable_ = false;
  bool auto_resize_ = false;
  ImVec2 scrolling_;

  std::unique_ptr<PaletteEditorWidget> palette_editor_;
  uint64_t edit_palette_group_name_index_ = 0;
  uint64_t edit_palette_index_ = 0;
  uint64_t edit_palette_sub_index_ = 0;
  bool refresh_graphics_ = false;

  void DrawROMPaletteSelector();

  std::unordered_map<CanvasUsage, std::vector<CanvasMenuItem>> usage_specific_items_;
  std::vector<CanvasMenuItem> global_items_;

  void RenderMenuItem(const CanvasMenuItem& item,
                     std::function<void(const std::string&, std::function<void()>)> popup_callback);
  void RenderMenuSection(const std::string& title,
                         const std::vector<CanvasMenuItem>& items,
                         std::function<void(const std::string&, std::function<void()>)> popup_callback);
  void RenderUsageSpecificMenu(std::function<void(const std::string&, std::function<void()>)> popup_callback);
  void RenderViewControlsMenu(const std::function<void(Command, const CanvasConfig&)>& command_handler,
                              CanvasConfig current_config);
  void RenderCanvasPropertiesMenu(const std::function<void(Command, const CanvasConfig&)>& command_handler,
                                  CanvasConfig current_config);
  void RenderBitmapOperationsMenu(gfx::Bitmap* bitmap);
  void RenderPaletteOperationsMenu(Rom* rom, gfx::Bitmap* bitmap);
  void RenderBppOperationsMenu(const gfx::Bitmap* bitmap);
  void RenderPerformanceMenu();
  void RenderGridControlsMenu(const std::function<void(Command, const CanvasConfig&)>& command_handler,
                              CanvasConfig current_config);
  void RenderScalingControlsMenu(const std::function<void(Command, const CanvasConfig&)>& command_handler,
                                 CanvasConfig current_config);

  void RenderMaterialIcon(const std::string& icon_name,
                          const ImVec4& color = ImVec4(1, 1, 1, 1));
  std::string GetUsageModeName(CanvasUsage usage) const;
  ImVec4 GetUsageModeColor(CanvasUsage usage) const;

  void CreateDefaultMenuItems();
  CanvasMenuItem CreateViewMenuItem(const std::string& label,
                                   const std::string& icon,
                                   std::function<void()> callback);
  CanvasMenuItem CreateBitmapMenuItem(const std::string& label,
                                     const std::string& icon,
                                     std::function<void()> callback);
  CanvasMenuItem CreatePaletteMenuItem(const std::string& label,
                                      const std::string& icon,
                                      std::function<void()> callback);
  CanvasMenuItem CreateBppMenuItem(const std::string& label,
                                  const std::string& icon,
                                  std::function<void()> callback);
  CanvasMenuItem CreatePerformanceMenuItem(const std::string& label,
                                          const std::string& icon,
                                          std::function<void()> callback);
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_CONTEXT_MENU_H
