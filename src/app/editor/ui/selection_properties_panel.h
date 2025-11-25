#ifndef YAZE_APP_EDITOR_UI_SELECTION_PROPERTIES_PANEL_H_
#define YAZE_APP_EDITOR_UI_SELECTION_PROPERTIES_PANEL_H_

#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {

class Rom;

namespace zelda3 {
struct DungeonObject;
struct Sprite;
struct Entrance;
class OverworldMap;
}  // namespace zelda3

namespace editor {

/**
 * @enum SelectionType
 * @brief Types of entities that can be selected and edited
 */
enum class SelectionType {
  kNone,
  kDungeonRoom,
  kDungeonObject,
  kDungeonSprite,
  kDungeonEntrance,
  kOverworldMap,
  kOverworldTile,
  kOverworldSprite,
  kOverworldEntrance,
  kOverworldExit,
  kOverworldItem,
  kGraphicsSheet,
  kPalette
};

/**
 * @struct SelectionContext
 * @brief Holds information about the current selection
 */
struct SelectionContext {
  SelectionType type = SelectionType::kNone;
  int id = -1;                  // Primary identifier
  int secondary_id = -1;        // Secondary identifier (e.g., room for objects)
  void* data = nullptr;         // Pointer to the actual data structure
  std::string display_name;     // Human-readable name for the selection
  bool read_only = false;       // Whether editing is allowed
};

/**
 * @class SelectionPropertiesPanel
 * @brief Full-editing properties panel for selected entities
 *
 * This panel displays and allows editing of properties for whatever entity
 * is currently selected in the active editor. It adapts its UI based on the
 * selection type and provides appropriate editing controls.
 *
 * Usage:
 * ```cpp
 * SelectionPropertiesPanel panel;
 * panel.SetRom(rom);
 *
 * // When selection changes:
 * SelectionContext ctx;
 * ctx.type = SelectionType::kDungeonObject;
 * ctx.id = object_id;
 * ctx.data = &object;
 * panel.SetSelection(ctx);
 *
 * // In render loop:
 * panel.Draw();
 * ```
 */
class SelectionPropertiesPanel {
 public:
  using ChangeCallback = std::function<void(const SelectionContext&)>;

  SelectionPropertiesPanel() = default;
  ~SelectionPropertiesPanel() = default;

  // Non-copyable
  SelectionPropertiesPanel(const SelectionPropertiesPanel&) = delete;
  SelectionPropertiesPanel& operator=(const SelectionPropertiesPanel&) = delete;

  // ============================================================================
  // Configuration
  // ============================================================================

  void SetRom(Rom* rom) { rom_ = rom; }
  void SetChangeCallback(ChangeCallback callback) {
    on_change_ = std::move(callback);
  }

  // ============================================================================
  // Selection Management
  // ============================================================================

  /**
   * @brief Set the current selection to display/edit
   */
  void SetSelection(const SelectionContext& context);

  /**
   * @brief Clear the current selection
   */
  void ClearSelection();

  /**
   * @brief Get the current selection context
   */
  const SelectionContext& GetSelection() const { return selection_; }

  /**
   * @brief Check if there's an active selection
   */
  bool HasSelection() const { return selection_.type != SelectionType::kNone; }

  // ============================================================================
  // Rendering
  // ============================================================================

  /**
   * @brief Draw the properties panel content
   *
   * Should be called from within an ImGui context (e.g., inside a window).
   */
  void Draw();

 private:
  // Type-specific drawing methods
  void DrawNoSelection();
  void DrawDungeonRoomProperties();
  void DrawDungeonObjectProperties();
  void DrawDungeonSpriteProperties();
  void DrawDungeonEntranceProperties();
  void DrawOverworldMapProperties();
  void DrawOverworldTileProperties();
  void DrawOverworldSpriteProperties();
  void DrawOverworldEntranceProperties();
  void DrawOverworldExitProperties();
  void DrawOverworldItemProperties();
  void DrawGraphicsSheetProperties();
  void DrawPaletteProperties();

  // Helper methods
  void DrawPropertyHeader(const char* icon, const char* title);
  bool DrawPositionEditor(const char* label, int* x, int* y,
                          int min_val = 0, int max_val = 512);
  bool DrawSizeEditor(const char* label, int* width, int* height);
  bool DrawByteProperty(const char* label, uint8_t* value,
                        const char* tooltip = nullptr);
  bool DrawWordProperty(const char* label, uint16_t* value,
                        const char* tooltip = nullptr);
  bool DrawComboProperty(const char* label, int* current_item,
                         const char* const items[], int items_count);
  bool DrawFlagsProperty(const char* label, uint8_t* flags,
                         const char* const flag_names[], int flag_count);
  void DrawReadOnlyText(const char* label, const char* value);
  void DrawReadOnlyHex(const char* label, uint32_t value, int digits = 4);

  void NotifyChange();

  // State
  SelectionContext selection_;
  Rom* rom_ = nullptr;
  ChangeCallback on_change_;

  // UI state
  bool show_advanced_ = false;
  bool show_raw_data_ = false;
};

/**
 * @brief Get a human-readable name for a selection type
 */
const char* GetSelectionTypeName(SelectionType type);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_SELECTION_PROPERTIES_PANEL_H_
