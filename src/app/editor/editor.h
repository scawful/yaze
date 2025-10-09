#ifndef YAZE_APP_CORE_EDITOR_H
#define YAZE_APP_CORE_EDITOR_H

#include <array>
#include <vector>
#include <functional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/system/command_manager.h"
#include "app/editor/system/extension_manager.h"
#include "app/editor/system/history_manager.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/shortcut_manager.h"

namespace yaze {

/**
 * @namespace yaze::editor
 * @brief Editors are the view controllers for the application.
 */
namespace editor {

struct EditorContext {
  CommandManager command_manager;
  ExtensionManager extension_manager;
  HistoryManager history_manager;
  PopupManager* popup_manager = nullptr;
  ShortcutManager shortcut_manager;
  
  // Session identification for multi-session support
  // Used by child panels to create unique ImGui IDs
  size_t session_id = 0;
  
  // Cross-session shared clipboard for editor data transfers
  struct SharedClipboard {
    // Overworld tile16 selection payload
    bool has_overworld_tile16 = false;
    std::vector<int> overworld_tile16_ids;
    int overworld_width = 0;   // in tile16 units
    int overworld_height = 0;  // in tile16 units

    void Clear() {
      has_overworld_tile16 = false;
      overworld_tile16_ids.clear();
      overworld_width = 0;
      overworld_height = 0;
    }
  } shared_clipboard;
};

enum class EditorType {
  kUnknown,
  kAssembly,
  kDungeon,
  kEmulator,
  kGraphics,
  kMusic,
  kOverworld,
  kPalette,
  kScreen,
  kSprite,
  kMessage,
  kHex,
  kAgent,
  kSettings,
};

constexpr std::array<const char*, 14> kEditorNames = {
    "Unknown",
    "Assembly", "Dungeon", "Emulator", "Graphics", "Music", "Overworld",
    "Palette",  "Screen",  "Sprite",   "Message",  "Hex",   "Agent", "Settings",
};

/**
 * @class Editor
 * @brief Interface for editor classes.
 *
 * Provides basic editing operations that each editor should implement.
 */
class Editor {
 public:
  Editor() = default;
  virtual ~Editor() = default;

  // Initialization of the editor, no ROM assets.
  virtual void Initialize() = 0;

  // Initialization of ROM assets.
  virtual absl::Status Load() = 0;

  // Save the editor state.
  virtual absl::Status Save() = 0;

  // Update the editor state, ran every frame.
  virtual absl::Status Update() = 0;

  virtual absl::Status Cut() = 0;
  virtual absl::Status Copy() = 0;
  virtual absl::Status Paste() = 0;

  virtual absl::Status Undo() = 0;
  virtual absl::Status Redo() = 0;

  virtual absl::Status Find() = 0;

  virtual absl::Status Clear() { return absl::OkStatus(); }

  EditorType type() const { return type_; }

  void set_context(EditorContext* context) { context_ = context; }

  bool* active() { return &active_; }
  void set_active(bool active) { active_ = active; }

  // ROM loading state helpers (default implementations)
  virtual bool IsRomLoaded() const { return false; }
  virtual std::string GetRomStatus() const { return "ROM state not implemented"; }

 protected:
  bool active_ = false;
  EditorType type_;
  EditorContext* context_ = nullptr;

  // Helper method to create session-aware card titles for multi-session support
  std::string MakeCardTitle(const std::string& base_title) const {
    if (context_ && context_->session_id > 0) {
      return absl::StrFormat("%s [S%zu]", base_title, context_->session_id);
    }
    return base_title;
  }

  // Helper method for ROM access with safety check
  template<typename T>
  absl::StatusOr<T> SafeRomAccess(std::function<T()> accessor, const std::string& operation = "") const {
    if (!IsRomLoaded()) {
      return absl::FailedPreconditionError(
          operation.empty() ? "ROM not loaded" : 
          absl::StrFormat("%s: ROM not loaded", operation));
    }
    try {
      return accessor();
    } catch (const std::exception& e) {
      return absl::InternalError(absl::StrFormat(
          "%s: %s", operation.empty() ? "ROM access failed" : operation, e.what()));
    }
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_CORE_EDITOR_H
