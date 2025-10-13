#ifndef YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H
#define YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "app/editor/editor.h"
#include "app/gui/widgets/text_editor.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/app/editor_card_manager.h"
#include "app/gui/core/style.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

struct FolderItem {
  std::string name;
  std::vector<FolderItem> subfolders;
  std::vector<std::string> files;
};

/**
 * @class AssemblyEditor
 * @brief Text editor for modifying assembly code.
 */
class AssemblyEditor : public Editor {
 public:
  explicit AssemblyEditor(Rom* rom = nullptr) : rom_(rom) {
    text_editor_.SetPalette(TextEditor::GetDarkPalette());
    text_editor_.SetShowWhitespaces(false);
    type_ = EditorType::kAssembly;
  }
  void ChangeActiveFile(const std::string_view &filename);

  void Initialize() override;
  absl::Status Load() override;
  void Update(bool &is_loaded);
  void InlineUpdate();

  void UpdateCodeView();

  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;

  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Find() override { return absl::UnimplementedError("Find"); }

  absl::Status Update() override;

  absl::Status Save() override;

  void OpenFolder(const std::string &folder_path);

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
  void DrawFileMenu();
  void DrawEditMenu();
  void DrawCurrentFolder();
  void DrawFileTabView();
  void DrawToolset();

  bool file_is_loaded_ = false;
  int current_file_id_ = 0;
  int active_file_id_ = -1;

  std::vector<std::string> files_;
  std::vector<TextEditor> open_files_;
  ImVector<int> active_files_;

  std::string current_file_;
  FolderItem current_folder_;
  TextEditor text_editor_;

  Rom* rom_;
};

}  // namespace editor
}  // namespace yaze

#endif
