#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_OBJECT_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_OBJECT_EDITOR_PANEL_H_

#include <string>

#include "app/editor/dungeon/object_editor_card.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonObjectEditorPanel
 * @brief EditorPanel wrapper for ObjectEditorCard
 *
 * This panel wraps the existing ObjectEditorCard component for unified
 * object editing (tiles, sprites, chests, etc.) and integrates it with
 * the central panel drawing system.
 *
 * @see ObjectEditorCard - The underlying component
 * @see EditorPanel - Base interface
 */
class DungeonObjectEditorPanel : public EditorPanel {
 public:
  explicit DungeonObjectEditorPanel(ObjectEditorCard* object_editor)
      : object_editor_(object_editor) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.object_editor"; }
  std::string GetDisplayName() const override { return "Object Editor"; }
  std::string GetIcon() const override { return ICON_MD_CONSTRUCTION; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 60; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!object_editor_) return;
    object_editor_->Draw(p_open);
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  ObjectEditorCard* object_editor() const { return object_editor_; }

 private:
  ObjectEditorCard* object_editor_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_DUNGEON_OBJECT_EDITOR_PANEL_H_
