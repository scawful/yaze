#ifndef YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H
#define YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H

#include <map>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "app/editor/editor.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/core/style.h"
#include "app/gui/widgets/text_editor.h"
#include "rom/rom.h"
#include "core/asar_wrapper.h"

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
  void ChangeActiveFile(const std::string_view& filename);

  void Initialize() override;
  absl::Status Load() override;
  void Update(bool& is_loaded);
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

  void OpenFolder(const std::string& folder_path);

  // Asar integration methods
  absl::Status ValidateCurrentFile();
  absl::Status ApplyPatchToRom();
  void UpdateErrorMarkers(const core::AsarPatchResult& result);
  void ClearErrorMarkers();

  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

  // Accessors for Asar state
  bool is_asar_initialized() const { return asar_initialized_; }
  const std::map<std::string, core::AsarSymbol>& symbols() const {
    return symbols_;
  }
  core::AsarWrapper* asar_wrapper() { return &asar_; }

 private:
  // Panel content drawing (called by EditorPanel instances)
  void DrawCodeEditor();
  void DrawFileBrowser();
  void DrawSymbolsContent();
  void DrawBuildOutput();
  void DrawToolbarContent();

  // Menu drawing
  void DrawFileMenu();
  void DrawEditMenu();
  void DrawAssembleMenu();

  // Helper drawing
  void DrawCurrentFolder();
  void DrawFileTabView();
  void DrawToolset();
  void DrawSymbolPanel();

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

  // Asar integration state
  core::AsarWrapper asar_;
  bool asar_initialized_ = false;
  bool show_symbol_panel_ = false;
  std::map<std::string, core::AsarSymbol> symbols_;
  std::vector<std::string> last_errors_;
  std::vector<std::string> last_warnings_;
};

}  // namespace editor
}  // namespace yaze

#endif
