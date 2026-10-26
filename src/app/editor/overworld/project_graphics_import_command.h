#ifndef YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_IMPORT_COMMAND_H_
#define YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_IMPORT_COMMAND_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/overworld/project_graphics_import.h"
#include "app/editor/registry/undo_action.h"

namespace yaze::editor {

class ProjectGraphicsSource;

// Access one resource in the active document, resolving its current binding on
// every call. A replacement must either succeed completely or leave it alone.
// These callbacks never publish source files or write ROM bytes.
struct ProjectGraphicsImportResourceCallbacks {
  std::function<absl::StatusOr<std::vector<uint8_t>>()> read;
  std::function<absl::Status(const std::vector<uint8_t>&)> replace;
};

using ProjectGraphicsImportRefresh = std::function<absl::Status()>;
using ProjectGraphicsImportResourceResolver =
    std::function<absl::StatusOr<std::string>(const ProjectGraphicsSource&)>;

// Returns an unapplied action; only its caller pushes the parent history step.
// The prepared definitions participant must be unapplied and internally atomic.
// Refresh synchronizes pending characters/palettes before definitions render and
// refreshes the complete view afterward. Palette notifications follow success.
// On failure all completed participants are reversed, then the restored view is
// refreshed after PaletteManager restores its tracking and values. A rollback
// failure is reported and disables further replay of this action.
// owner and the definitions session must outlive the action.
absl::StatusOr<std::unique_ptr<UndoAction>> CreateProjectGraphicsImportAction(
    ProjectGraphicsImportResourceCallbacks resource,
    const ProjectGraphicsImportPlan& plan,
    std::unique_ptr<UndoAction> prepared_definitions,
    const zelda3::GameData* owner, ProjectGraphicsImportRefresh refresh,
    std::string description = "Import project graphics");

// Normal ProjectGraphicsSource adapter. The resolver belongs to the caller and
// must identify the same consumer binding, not capture a source path/hash: a
// successful Publish may change both. Reads and replacements resolve afresh.
absl::StatusOr<std::unique_ptr<UndoAction>> CreateProjectGraphicsImportAction(
    std::shared_ptr<ProjectGraphicsSource> source,
    ProjectGraphicsImportResourceResolver resolve_resource,
    const ProjectGraphicsImportPlan& plan,
    std::unique_ptr<UndoAction> prepared_definitions,
    const zelda3::GameData* owner, ProjectGraphicsImportRefresh refresh,
    std::string description = "Import project graphics");

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_IMPORT_COMMAND_H_
