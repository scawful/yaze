#ifndef YAZE_APP_EDITOR_MENU_STATUS_BAR_H_
#define YAZE_APP_EDITOR_MENU_STATUS_BAR_H_

#include <string>
#include <unordered_map>

#include "app/editor/core/editor_context.h"
#include "app/editor/events/ui_events.h"

namespace yaze {

class Rom;

namespace editor {

/**
 * @class StatusBar
 * @brief A session-aware status bar displayed at the bottom of the application
 *
 * The StatusBar sits outside the ImGui dockspace (like the sidebars) and displays:
 * - ROM filename and dirty status indicator
 * - Session number (when multiple ROMs open)
 * - Cursor position (context-aware based on active editor)
 * - Selection info (count, dimensions)
 * - Zoom level
 * - Current editor mode/tool
 *
 * Each editor can update its relevant segments by calling the Set* methods or 
 * publishing StatusUpdateEvents to the event bus.
 *
 * Usage:
 * ```cpp
 * status_bar_.Initialize(&editor_context);
 * // ... 
 * editor_context.GetEventBus().Publish(StatusUpdateEvent::Cursor(x, y));
 * ```
 */
class StatusBar {
 public:
  StatusBar() = default;
  ~StatusBar() = default;

  void Initialize(GlobalEditorContext* context);

  // ============================================================================
  // Configuration
  // ============================================================================

  /**
   * @brief Enable or disable the status bar
   */
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  bool IsEnabled() const { return enabled_; }

  /**
   * @brief Set the current ROM for dirty status and filename display
   */
  void SetRom(Rom* rom) { rom_ = rom; }

  /**
   * @brief Set session information
   * @param session_id Current session index (0-based)
   * @param total_sessions Total number of open sessions
   */
  void SetSessionInfo(size_t session_id, size_t total_sessions);

  // ============================================================================
  // Context Setters (called by active editor)
  // ============================================================================

  /**
   * @brief Set cursor/mouse position in editor coordinates
   * @param x X coordinate (tile, pixel, or editor-specific)
   * @param y Y coordinate (tile, pixel, or editor-specific)
   * @param label Optional label (e.g., "Tile", "Pos", "Map")
   */
  void SetCursorPosition(int x, int y, const char* label = "Pos");

  /**
   * @brief Clear cursor position (no cursor in editor)
   */
  void ClearCursorPosition();

  /**
   * @brief Set selection information
   * @param count Number of selected items
   * @param width Width of selection (optional, 0 to hide)
   * @param height Height of selection (optional, 0 to hide)
   */
  void SetSelection(int count, int width = 0, int height = 0);

  /**
   * @brief Clear selection info
   */
  void ClearSelection();

  /**
   * @brief Set current zoom level
   * @param level Zoom multiplier (e.g., 1.0, 2.0, 0.5)
   */
  void SetZoom(float level);

  /**
   * @brief Clear zoom display
   */
  void ClearZoom();

  /**
   * @brief Set the current editor mode or tool
   * @param mode Mode string (e.g., "Draw", "Select", "Entity")
   */
  void SetEditorMode(const std::string& mode);

  /**
   * @brief Clear editor mode display
   */
  void ClearEditorMode();

  /**
   * @brief Set a custom segment with key-value pair
   * @param key Segment identifier
   * @param value Value to display
   */
  void SetCustomSegment(const std::string& key, const std::string& value);

  /**
   * @brief Remove a custom segment
   */
  void ClearCustomSegment(const std::string& key);

  /**
   * @brief Clear all context (cursor, selection, zoom, mode, custom)
   */
  void ClearAllContext();

  // ============================================================================
  // Rendering
  // ============================================================================

  /**
   * @brief Draw the status bar
   *
   * Should be called each frame. The status bar positions itself at the
   * bottom of the viewport, outside the dockspace.
   */
  void Draw();

  /**
   * @brief Get the height of the status bar
   * @return Height in pixels (0 if disabled)
   */
  float GetHeight() const { return enabled_ ? kStatusBarHeight : 0.0f; }

  static constexpr float kStatusBarHeight = 24.0f;

 private:
  void HandleStatusUpdate(const StatusUpdateEvent& event);

  void DrawRomSegment();
  void DrawSessionSegment();
  void DrawCursorSegment();
  void DrawSelectionSegment();
  void DrawZoomSegment();
  void DrawModeSegment();
  void DrawCustomSegments();
  void DrawSeparator();

  GlobalEditorContext* context_ = nullptr;
  bool enabled_ = false;
  Rom* rom_ = nullptr;

  // Session info
  size_t session_id_ = 0;
  size_t total_sessions_ = 1;

  // Cursor position
  bool has_cursor_ = false;
  int cursor_x_ = 0;
  int cursor_y_ = 0;
  std::string cursor_label_ = "Pos";

  // Selection
  bool has_selection_ = false;
  int selection_count_ = 0;
  int selection_width_ = 0;
  int selection_height_ = 0;

  // Zoom
  bool has_zoom_ = false;
  float zoom_level_ = 1.0f;

  // Editor mode
  bool has_mode_ = false;
  std::string editor_mode_;

  // Custom segments
  std::unordered_map<std::string, std::string> custom_segments_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_STATUS_BAR_H_

