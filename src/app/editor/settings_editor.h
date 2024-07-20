#ifndef YAZE_APP_EDITOR_SETTINGS_EDITOR_H
#define YAZE_APP_EDITOR_SETTINGS_EDITOR_H

#include "absl/status/status.h"
#include "app/editor/utils/editor.h"

namespace yaze {
namespace app {
namespace editor {

class SettingsEditor : public Editor {
 public:
  SettingsEditor() : Editor() { type_ = EditorType::kSettings; }

  absl::Status Update() override;

  absl::Status Undo() override { return absl::UnimplementedError("Undo"); }
  absl::Status Redo() override { return absl::UnimplementedError("Redo"); }
  absl::Status Cut() override { return absl::UnimplementedError("Cut"); }
  absl::Status Copy() override { return absl::UnimplementedError("Copy"); }
  absl::Status Paste() override { return absl::UnimplementedError("Paste"); }
  absl::Status Find() override { return absl::UnimplementedError("Find"); }

 private:
  void DrawGeneralSettings();

  absl::Status DrawKeyboardShortcuts();
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SETTINGS_EDITOR_H_