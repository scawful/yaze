#ifndef YAZE_APP_CORE_EDITOR_H
#define YAZE_APP_CORE_EDITOR_H

#include <array>
#include <cstddef>
#include <vector>
#include <functional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/shortcut_manager.h"

namespace yaze {

// Forward declarations
class Rom;
namespace gfx {
class IRenderer;
}

/**
 * @namespace yaze::editor
 * @brief Editors are the view controllers for the application.
 */
namespace editor {

// Forward declarations
class EditorCardRegistry;
class ToastManager;
class UserSettings;

/**
 * @struct EditorDependencies
 * @brief Unified dependency container for all editor types
 * 
 * This struct encapsulates all dependencies that editors might need,
 * providing a clean interface for dependency injection. It supports
 * both standard editors and specialized ones (emulator, dungeon) that
 * need additional dependencies like renderers.
 * 
 * Design Philosophy:
 * - Single point of dependency management
 * - Type-safe for common dependencies
 * - Extensible via custom_data for editor-specific needs
 * - Session-aware for multi-session support
 * 
 * Usage:
 * ```cpp
 * EditorDependencies deps;
 * deps.rom = current_rom;
 * deps.card_registry = &card_registry_;
 * deps.session_id = session_index;
 * 
 * // Standard editor
 * OverworldEditor editor(deps);
 * 
 * // Specialized editor with renderer
 * deps.renderer = renderer_;
 * DungeonEditor dungeon_editor(deps);
 * ```
 */
struct EditorDependencies {
  struct SharedClipboard {
    bool has_overworld_tile16 = false;
    std::vector<int> overworld_tile16_ids;
    int overworld_width = 0;
    int overworld_height = 0;

    void Clear() {
      has_overworld_tile16 = false;
      overworld_tile16_ids.clear();
      overworld_width = 0;
      overworld_height = 0;
    }
  };

  Rom* rom = nullptr;
  EditorCardRegistry* card_registry = nullptr;
  ToastManager* toast_manager = nullptr;
  PopupManager* popup_manager = nullptr;
  ShortcutManager* shortcut_manager = nullptr;
  SharedClipboard* shared_clipboard = nullptr;
  UserSettings* user_settings = nullptr;
  size_t session_id = 0;

  gfx::IRenderer* renderer = nullptr;

  void* custom_data = nullptr;
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

  void SetDependencies(const EditorDependencies& deps) { dependencies_ = deps; }

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

  bool* active() { return &active_; }
  void set_active(bool active) { active_ = active; }
  void toggle_active() { active_ = !active_; }

  // ROM loading state helpers (default implementations)
  virtual bool IsRomLoaded() const { return false; }
  virtual std::string GetRomStatus() const { return "ROM state not implemented"; }

 protected:
  bool active_ = false;
  EditorType type_;
  EditorDependencies dependencies_;

  // Helper method to create session-aware card titles for multi-session support
  std::string MakeCardTitle(const std::string& base_title) const {
    if (dependencies_.session_id > 0) {
      return absl::StrFormat("%s [S%zu]", base_title, dependencies_.session_id);
    }
    return base_title;
  }
  
  // Helper method to create session-aware card IDs for multi-session support
  std::string MakeCardId(const std::string& base_id) const {
    if (dependencies_.session_id > 0) {
      return absl::StrFormat("s%zu.%s", dependencies_.session_id, base_id);
    }
    return base_id;
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
