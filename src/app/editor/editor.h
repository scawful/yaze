#ifndef YAZE_APP_CORE_EDITOR_H
#define YAZE_APP_CORE_EDITOR_H

#include <array>

#include "absl/status/status.h"
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
  PopupManager popup_manager;
  ShortcutManager shortcut_manager;
};

enum class EditorType {
  kAssembly,
  kDungeon,
  kGraphics,
  kMusic,
  kOverworld,
  kPalette,
  kScreen,
  kSprite,
  kMessage,
  kSettings,
};

constexpr std::array<const char *, 10> kEditorNames = {
    "Assembly", "Dungeon", "Graphics", "Music",   "Overworld",
    "Palette",  "Screen",  "Sprite",   "Message", "Settings",
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

  // Update the editor state, ran every frame.
  virtual absl::Status Update() = 0;

  virtual absl::Status Cut() = 0;
  virtual absl::Status Copy() = 0;
  virtual absl::Status Paste() = 0;

  virtual absl::Status Undo() = 0;
  virtual absl::Status Redo() = 0;

  virtual absl::Status Find() = 0;

  EditorType type() const { return type_; }

 protected:
  EditorType type_;
  EditorContext context_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_CORE_EDITOR_H
