#ifndef YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H
#define YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "app/editor/editor.h"
#include "app/editor/system/session/background_command_task.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/core/style.h"
#include "app/gui/widgets/text_editor.h"
#include "core/asar_wrapper.h"
#include "rom/rom.h"
#ifdef YAZE_WITH_Z3DK
#include "core/z3dk_wrapper.h"
#endif

namespace yaze::test {
struct AssemblyEditorGuiTestAccess;
}

namespace yaze {
namespace editor {

struct FolderItem {
  std::string name;
  std::vector<FolderItem> subfolders;
  std::vector<std::string> files;
};

struct AsmSymbolLocation {
  std::string file;
  int line = 0;    // 0-based
  int column = 0;  // 0-based
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
  absl::Status JumpToSymbolDefinition(const std::string& symbol);
  // Jump to a best-effort assembly reference.
  //
  // Supported formats:
  // - "MyLabel"                    (symbol)
  // - "path/to/file.asm:123"       (file + 1-based line)
  // - "path/to/file.asm:123:10"    (file + 1-based line + 1-based column)
  // - "path/to/file.asm#L123"      (file + 1-based line)
  //
  // This keeps story graph navigation resilient even when a planning JSON
  // still contains file:line references; stable symbols are still preferred.
  absl::Status JumpToReference(const std::string& reference);

  [[nodiscard]] std::string active_file_path() const;
  [[nodiscard]] TextEditor::Coordinates active_cursor_position() const;

  void Initialize() override;
  absl::Status Load() override;
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

  // Structured diagnostics from the last assemble/validate. Populated by
  // both backends; richer (file:line:column) when YAZE_WITH_Z3DK is active.
  const std::vector<core::AssemblyDiagnostic>& last_diagnostics() const {
    return last_diagnostics_;
  }

 private:
  friend struct ::yaze::test::AssemblyEditorGuiTestAccess;

  TextEditor* GetActiveEditor();
  const TextEditor* GetActiveEditor() const;
  bool HasActiveFile() const {
    return active_file_id_ != -1 &&
           active_file_id_ < static_cast<int>(open_files_.size());
  }

  // Panel content drawing (called by WindowContent instances)
  void DrawCodeEditor();
  void DrawFileBrowser();
  void DrawSymbolsContent();
  void DrawBuildOutput();
  void DrawDisassemblyContent();
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

  void ClearSymbolJumpCache();
  absl::Status NavigateDisassemblyQuery();
  absl::Status GenerateZ3Disassembly();
  void PollZ3DisassemblyTask();
  void RefreshZ3DisassemblyFiles();
  void LoadSelectedZ3DisassemblyFile();
  void RefreshSelectedZ3DisassemblyMetadata();
  absl::Status RunProjectGraphQueryInDrawer(
      const std::vector<std::string>& args, const std::string& title);
  std::string ResolveZ3DisasmCommand() const;
  std::string ResolveZ3DisasmOutputDir() const;
  std::string ResolveZ3DisasmRomPath() const;
  std::string BuildProjectGraphBankQuery() const;
  std::string BuildProjectGraphLookupQuery(uint32_t address) const;
  int SelectedZ3DisasmBankIndex() const;
  std::optional<uint32_t> CurrentDisassemblyBank() const;

#ifdef YAZE_WITH_Z3DK
  core::Z3dkAssembleOptions BuildZ3dkAssembleOptions() const;
  void ExportZ3dkArtifacts(const core::AsarPatchResult& result,
                           bool sync_mesen_symbols);
#endif

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
#ifdef YAZE_WITH_Z3DK
  core::Z3dkWrapper z3dk_;
#endif
  bool asar_initialized_ = false;
  bool show_symbol_panel_ = false;
  struct Z3DisasmSourceJump {
    uint32_t address = 0;
    std::string file;
    int line = 0;
  };
  struct Z3DisasmHookJump {
    uint32_t address = 0;
    int size = 0;
    std::string kind;
    std::string name;
    std::string source;
  };
  std::map<std::string, core::AsarSymbol> symbols_;
  std::vector<std::string> last_errors_;
  std::vector<std::string> last_warnings_;
  std::vector<core::AssemblyDiagnostic> last_diagnostics_;
  BackgroundCommandTask z3disasm_task_;
  bool z3disasm_all_banks_ = false;
  int z3disasm_bank_start_ = 0;
  int z3disasm_bank_end_ = 0;
  std::string z3disasm_output_dir_;
  std::vector<std::string> z3disasm_files_;
  int z3disasm_selected_index_ = -1;
  std::string z3disasm_selected_path_;
  std::string z3disasm_selected_contents_;
  std::vector<Z3DisasmSourceJump> z3disasm_source_jumps_;
  std::vector<Z3DisasmHookJump> z3disasm_hook_jumps_;
  std::string z3disasm_status_;
  bool z3disasm_task_acknowledged_ = true;
  std::string disasm_query_ = "0x008000";
  int disasm_instruction_count_ = 24;
  std::string disasm_status_;

  // Symbol jump cache (used by story graph navigation; avoids scanning the
  // entire code folder on repeated lookups).
  std::string symbol_jump_root_;
  absl::flat_hash_map<std::string, AsmSymbolLocation> symbol_jump_cache_;
  absl::flat_hash_set<std::string> symbol_jump_negative_cache_;
};

}  // namespace editor
}  // namespace yaze

#endif
