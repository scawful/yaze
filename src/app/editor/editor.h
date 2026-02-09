#ifndef YAZE_APP_CORE_EDITOR_H
#define YAZE_APP_CORE_EDITOR_H

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/system/shortcut_manager.h"

// Forward declaration in yaze::core namespace
namespace yaze {
namespace core {
class VersionManager;
}
}

namespace yaze {

// Forward declarations
class Rom;
namespace gfx {
class IRenderer;
}
namespace emu {
class Emulator;
}
namespace project {
struct YazeProject;
}  // namespace project
namespace zelda3 {
struct GameData;
}  // namespace zelda3

/**
 * @namespace yaze::editor
 * @brief Editors are the view controllers for the application.
 */
namespace editor {

// Forward declarations
class PanelManager;
class ToastManager;
class UserSettings;
class StatusBar;

/**
 * @struct EditorContext
 * @brief Lightweight view into the essential runtime context (Rom + GameData)
 *
 * This struct provides a bundled view of the two primary dependencies
 * for Zelda3 editing operations. It can be passed by value and is designed
 * to replace the pattern of passing rom_ and game_data_ separately.
 *
 * Usage:
 * ```cpp
 * void SomeComponent::DoWork(EditorContext ctx) {
 *   if (!ctx.IsValid()) return;
 *   auto data = ctx.rom->ReadByte(0x1234);
 *   auto& palettes = ctx.game_data->palette_groups;
 * }
 * ```
 */
struct EditorContext {
  Rom* rom = nullptr;
  zelda3::GameData* game_data = nullptr;

  // Check if context is valid for operations
  bool IsValid() const { return rom != nullptr && game_data != nullptr; }
  bool HasRom() const { return rom != nullptr; }
  bool HasGameData() const { return game_data != nullptr; }

  // Implicit conversion to bool for quick validity checks
  explicit operator bool() const { return IsValid(); }
};

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
 * deps.game_data = game_data;
 * deps.panel_manager = &panel_manager_;
 * deps.session_id = session_index;
 *
 * // Standard editor
 * OverworldEditor editor(deps);
 *
 * // Get lightweight context for passing to sub-components
 * auto ctx = deps.context();
 * sub_component.Initialize(ctx);
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
  zelda3::GameData* game_data = nullptr;  // Zelda3-specific game state
  PanelManager* panel_manager = nullptr;
  ToastManager* toast_manager = nullptr;
  PopupManager* popup_manager = nullptr;
  ShortcutManager* shortcut_manager = nullptr;
  SharedClipboard* shared_clipboard = nullptr;
  UserSettings* user_settings = nullptr;
  project::YazeProject* project = nullptr;
  core::VersionManager* version_manager = nullptr;
  StatusBar* status_bar = nullptr;
  size_t session_id = 0;

  gfx::IRenderer* renderer = nullptr;
  emu::Emulator* emulator = nullptr;

  void* custom_data = nullptr;

  // Get lightweight context for passing to sub-components
  EditorContext context() const { return {rom, game_data}; }

  // Check if essential context is available
  bool HasContext() const { return rom != nullptr && game_data != nullptr; }
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
    "Unknown", "Assembly",  "Dungeon", "Emulator", "Graphics",
    "Music",   "Overworld", "Palette", "Screen",   "Sprite",
    "Message", "Hex",       "Agent",   "Settings",
};

constexpr size_t kEditorTypeCount =
    static_cast<size_t>(EditorType::kSettings) + 1;

inline size_t EditorTypeIndex(EditorType type) {
  return static_cast<size_t>(type);
}

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

  virtual void SetDependencies(const EditorDependencies& deps) {
    dependencies_ = deps;
  }

  // Set GameData for Zelda3-specific data access
  virtual void SetGameData(zelda3::GameData* game_data) {
    dependencies_.game_data = game_data;
  }

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
  virtual std::string GetRomStatus() const {
    return "ROM state not implemented";
  }

  // Accessors for common dependencies
  Rom* rom() const { return dependencies_.rom; }
  zelda3::GameData* game_data() const { return dependencies_.game_data; }

  // Get bundled context for sub-components
  EditorContext context() const { return dependencies_.context(); }
  bool HasContext() const { return dependencies_.HasContext(); }

 protected:
  bool active_ = false;
  EditorType type_;
  EditorDependencies dependencies_;

  // Helper method to create session-aware card titles for multi-session support
  std::string MakePanelTitle(const std::string& base_title) const {
    if (dependencies_.session_id > 0) {
      return absl::StrFormat("%s [S%zu]", base_title, dependencies_.session_id);
    }
    return base_title;
  }

  // Helper method to create session-aware card IDs for multi-session support
  std::string MakePanelId(const std::string& base_id) const {
    if (dependencies_.session_id > 0) {
      return absl::StrFormat("s%zu.%s", dependencies_.session_id, base_id);
    }
    return base_id;
  }

  // Helper method for ROM access with safety check
  template <typename T>
  absl::StatusOr<T> SafeRomAccess(std::function<T()> accessor,
                                  const std::string& operation = "") const {
    if (!IsRomLoaded()) {
      return absl::FailedPreconditionError(
          operation.empty() ? "ROM not loaded"
                            : absl::StrFormat("%s: ROM not loaded", operation));
    }
    try {
      return accessor();
    } catch (const std::exception& e) {
      return absl::InternalError(absl::StrFormat(
          "%s: %s", operation.empty() ? "ROM access failed" : operation,
          e.what()));
    }
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_CORE_EDITOR_H
