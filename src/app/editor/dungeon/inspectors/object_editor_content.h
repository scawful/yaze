#ifndef YAZE_APP_EDITOR_DUNGEON_INSPECTORS_OBJECT_EDITOR_CONTENT_H_
#define YAZE_APP_EDITOR_DUNGEON_INSPECTORS_OBJECT_EDITOR_CONTENT_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/system/workspace/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/dungeon/dungeon_object_editor.h"

namespace yaze::editor {

class ObjectEditorContent : public WindowContent {
 public:
  explicit ObjectEditorContent(
      std::shared_ptr<zelda3::DungeonObjectEditor> object_editor = nullptr);

  std::string GetId() const override { return "dungeon.object_editor"; }
  std::string GetDisplayName() const override { return "Object Editor"; }
  std::string GetIcon() const override { return ICON_MD_TUNE; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 61; }
  float GetPreferredWidth() const override { return 460.0f; }

  void Draw(bool* p_open) override;

  void SetCurrentRoom(int room_id) { current_room_id_ = room_id; }
  void SetCanvasViewerProvider(std::function<DungeonCanvasViewer*()> provider) {
    canvas_viewer_provider_ = std::move(provider);
  }
  void SetCanvasViewer(DungeonCanvasViewer* viewer);

  void CycleObjectSelection(int direction);
  void SelectAllObjects();
  void DeleteSelectedObjects();
  void CopySelectedObjects();
  void PasteObjects();

 private:
  void SetupSelectionCallbacks();
  void OnSelectionChanged();
  DungeonCanvasViewer* ResolveCanvasViewer();

  void DrawSelectionSummary();
  void DrawSelectionActions();
  void DrawSelectedObjectInfo();
  void DrawKeyboardShortcutHelp();

  void HandleKeyboardShortcuts();
  void DeselectAllObjects();
  void DuplicateSelectedObjects();
  void NudgeSelectedObjects(int dx, int dy);
  void ScrollToObject(size_t index);

  DungeonCanvasViewer* canvas_viewer_ = nullptr;
  std::function<DungeonCanvasViewer*()> canvas_viewer_provider_;
  int current_room_id_ = 0;
  std::shared_ptr<zelda3::DungeonObjectEditor> object_editor_;

  size_t cached_selection_count_ = 0;
  bool selection_callbacks_setup_ = false;
  bool show_shortcut_help_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_INSPECTORS_OBJECT_EDITOR_CONTENT_H_
