#ifndef YAZE_APP_CORE_EDITOR_H
#define YAZE_APP_CORE_EDITOR_H

#include <array>

#include "absl/status/status.h"
#include "app/editor/system/command_manager.h"
#include "app/editor/system/constant_manager.h"
#include "app/editor/system/extension_manager.h"
#include "app/editor/system/history_manager.h"
#include "app/editor/system/resource_manager.h"

namespace yaze {

/**
 * @namespace yaze::editor
 * @brief Editors are the view controllers for the application.
 */
namespace editor {

struct EditorContext {
  ConstantManager constant_manager;
  CommandManager command_manager;
  ExtensionManager extension_manager;
  HistoryManager history_manager;
  ResourceManager resource_manager;
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

  virtual absl::Status Cut() = 0;
  virtual absl::Status Copy() = 0;
  virtual absl::Status Paste() = 0;

  virtual absl::Status Undo() = 0;
  virtual absl::Status Redo() = 0;

  virtual absl::Status Update() = 0;

  virtual absl::Status Find() = 0;

  EditorType type() const { return type_; }

 protected:
  EditorType type_;
  EditorContext context_;
};

/**
 * @brief Dynamic Editor Layout Parameters
 */
typedef struct EditorLayoutParams {
  bool v_split;
  bool h_split;
  int v_split_pos;
  int h_split_pos;
  Editor *editor = nullptr;
  EditorLayoutParams *left = nullptr;
  EditorLayoutParams *right = nullptr;
  EditorLayoutParams *top = nullptr;
  EditorLayoutParams *bottom = nullptr;

  EditorLayoutParams() {
    v_split = false;
    h_split = false;
    v_split_pos = 0;
    h_split_pos = 0;
  }
} EditorLayoutParams;

absl::Status DrawEditor(EditorLayoutParams *params);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_CORE_EDITOR_H
