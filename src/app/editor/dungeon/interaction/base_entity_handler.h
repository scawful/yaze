#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_BASE_ENTITY_HANDLER_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_BASE_ENTITY_HANDLER_H_

#include <algorithm>
#include <optional>
#include <utility>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @brief Abstract base class for entity interaction handlers
 *
 * Each entity type (object, door, sprite, item) has its own handler
 * that implements this interface. This provides consistent interaction
 * patterns while allowing specialized behavior.
 *
 * The InteractionCoordinator manages mode switching and dispatches
 * calls to the appropriate handler.
 */
class BaseEntityHandler {
 public:
  virtual ~BaseEntityHandler() = default;

  /**
   * @brief Set the interaction context
   *
   * Must be called before using any other methods.
   * The context provides access to canvas, room data, and callbacks.
   */
  void SetContext(InteractionContext* ctx) { ctx_ = ctx; }

  /**
   * @brief Get the interaction context
   */
  InteractionContext* context() const { return ctx_; }

  // ========================================================================
  // Placement Lifecycle
  // ========================================================================

  /**
   * @brief Begin placement mode
   *
   * Called when user selects an entity to place from the palette.
   * Override to initialize placement state.
   */
  virtual void BeginPlacement() = 0;

  /**
   * @brief Cancel current placement
   *
   * Called when user presses Escape or switches modes.
   * Override to clean up placement state.
   */
  virtual void CancelPlacement() = 0;

  /**
   * @brief Check if placement mode is active
   */
  virtual bool IsPlacementActive() const = 0;

  // ========================================================================
  // Mouse Interaction
  // ========================================================================

  /**
   * @brief Handle mouse click at canvas position
   *
   * @param canvas_x Unscaled X position relative to canvas origin
   * @param canvas_y Unscaled Y position relative to canvas origin
   * @return true if click was handled by this handler
   */
  virtual bool HandleClick(int canvas_x, int canvas_y) = 0;

  /**
   * @brief Handle mouse drag
   *
   * @param current_pos Current mouse position (screen coords)
   * @param delta Mouse movement since last frame
   */
  virtual void HandleDrag(ImVec2 current_pos, ImVec2 delta) = 0;

  /**
   * @brief Handle mouse release
   *
   * Called when left mouse button is released after a drag.
   */
  virtual void HandleRelease() = 0;
  virtual bool HandleMouseWheel(float delta) { return false; }

  // ========================================================================
  // Rendering
  // ========================================================================

  /**
   * @brief Draw ghost preview during placement
   *
   * Called every frame when placement mode is active.
   * Shows preview of entity at cursor position.
   */
  virtual void DrawGhostPreview() = 0;

  /**
   * @brief Draw selection highlight for selected entities
   *
   * Called every frame to show selection state.
   */
  virtual void DrawSelectionHighlight() = 0;

  // ========================================================================
  // Hit Testing
  // ========================================================================

  /**
   * @brief Get entity at canvas position
   *
   * @param canvas_x Unscaled X position relative to canvas origin
   * @param canvas_y Unscaled Y position relative to canvas origin
   * @return Entity index if found, nullopt otherwise
   */
  virtual std::optional<size_t> GetEntityAtPosition(int canvas_x,
                                                     int canvas_y) const = 0;

  // Per-frame toast render called unconditionally by InteractionCoordinator so
  // the "Placed" message remains visible even after the user exits placement
  // mode immediately after a successful click.
  void DrawPostPlacementToast() {
    if (ImGui::GetCurrentContext() == nullptr) return;
    DrawSuccessToastOverlay("Placed",
                            ImGui::GetColorU32(AgentUI::GetTheme().status_success));
  }

 protected:
  InteractionContext* ctx_ = nullptr;

  // ========================================================================
  // Placement Success Toast (shared by all derived handlers)
  // ========================================================================

  // Call after a successful entity placement.  Extends the toast display by
  // kToastDuration seconds; rapid placements simply prolong the same toast.
  void TriggerSuccessToast() {
    if (ImGui::GetCurrentContext() == nullptr) return;
    toast_expire_time_ =
        static_cast<float>(ImGui::GetTime()) + kToastDuration;
  }

  // Draw a fading "OK" toast near the canvas origin.
  // @param msg  Short message to display (e.g. "Placed").
  // @param color Opaque RGBA colour for the text (alpha will be modulated).
  void DrawSuccessToastOverlay(const char* msg, ImU32 color) const {
    if (ImGui::GetCurrentContext() == nullptr) return;
    if (toast_expire_time_ <= 0.0f ||
        ImGui::GetTime() >= toast_expire_time_) {
      return;
    }
    float remaining =
        toast_expire_time_ - static_cast<float>(ImGui::GetTime());
    // Fade out during the last 0.4 s; keep full opacity the rest of the time.
    float alpha = std::min(1.0f, remaining / 0.4f);
    // Blend alpha into supplied colour.
    ImU32 base_rgb = color & 0x00FFFFFFu;
    ImU32 alpha_ch = static_cast<ImU32>(alpha * 255.0f) << 24;
    ImU32 toast_color = base_rgb | alpha_ch;

    ImVec2 canvas_pos = GetCanvasZeroPoint();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    // Render near the top-left canvas corner so it never overlaps tiles.
    draw_list->AddText(ImVec2(canvas_pos.x + 8.0f, canvas_pos.y + 6.0f),
                       toast_color, msg);
  }

  // Shared toast expiry timestamp (seconds, from ImGui::GetTime()).
  float toast_expire_time_ = 0.0f;

  static constexpr float kToastDuration = 1.5f;

  // ========================================================================
  // Helper Methods (available to all derived handlers)
  // ========================================================================

  /**
   * @brief Convert room tile coordinates to canvas pixel coordinates
   */
  std::pair<int, int> RoomToCanvas(int room_x, int room_y) const {
    return dungeon_coords::RoomToCanvas(room_x, room_y);
  }

  /**
   * @brief Convert canvas pixel coordinates to room tile coordinates
   */
  std::pair<int, int> CanvasToRoom(int canvas_x, int canvas_y) const {
    return dungeon_coords::CanvasToRoom(canvas_x, canvas_y);
  }

  /**
   * @brief Check if coordinates are within room bounds
   */
  bool IsWithinBounds(int canvas_x, int canvas_y) const {
    return dungeon_coords::IsWithinBounds(canvas_x, canvas_y);
  }

  /**
   * @brief Get canvas zero point (for screen coordinate conversion)
   */
  ImVec2 GetCanvasZeroPoint() const {
    if (!ctx_ || !ctx_->canvas) return ImVec2(0, 0);
    return ctx_->canvas->zero_point();
  }

  /**
   * @brief Get canvas global scale
   */
  float GetCanvasScale() const {
    if (!ctx_ || !ctx_->canvas) return 1.0f;
    float scale = ctx_->canvas->global_scale();
    return scale > 0.0f ? scale : 1.0f;
  }

  /**
   * @brief Check if context is valid
   */
  bool HasValidContext() const { return ctx_ && ctx_->IsValid(); }

  /**
   * @brief Get current room (convenience method)
   */
  zelda3::Room* GetCurrentRoom() const {
    return ctx_ ? ctx_->GetCurrentRoom() : nullptr;
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_BASE_ENTITY_HANDLER_H_
