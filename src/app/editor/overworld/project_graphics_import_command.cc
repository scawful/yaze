#include "app/editor/overworld/project_graphics_import_command.h"

#include <utility>

#include "absl/strings/str_cat.h"
#include "app/editor/overworld/project_graphics_source.h"
#include "app/gfx/util/palette_manager.h"
#include "util/macro.h"

namespace yaze::editor {
namespace {

class ProjectGraphicsImportAction final : public UndoAction {
 public:
  ProjectGraphicsImportAction(ProjectGraphicsImportResourceCallbacks resource,
                              const ProjectGraphicsImportPlan& plan,
                              std::unique_ptr<UndoAction> definitions,
                              const zelda3::GameData* owner,
                              ProjectGraphicsImportRefresh refresh,
                              std::string description)
      : resource_(std::move(resource)),
        before_(plan.resource_before),
        after_(plan.resource_after),
        palettes_(plan.palette_changes),
        definitions_(std::move(definitions)),
        owner_(owner),
        refresh_(std::move(refresh)),
        description_(std::move(description)) {}

  absl::Status Undo() override { return Apply(false); }
  absl::Status Redo() override { return Apply(true); }
  std::string Description() const override { return description_; }
  size_t MemoryUsage() const override {
    return before_.size() + after_.size() +
           palettes_.size() * sizeof(gfx::PaletteColorChange) +
           (definitions_ ? definitions_->MemoryUsage() : 0);
  }

 private:
  absl::Status Apply(bool forward) {
    if (poisoned_)
      return absl::FailedPreconditionError(
          "Import rollback failed; reload the document");
    if (forward == applied_)
      return absl::FailedPreconditionError(
          "Import action is already in that state");
    const auto& expected = forward ? before_ : after_;
    const auto& desired = forward ? after_ : before_;
    ASSIGN_OR_RETURN(const auto current, resource_.read());
    if (current != expected) {
      return absl::FailedPreconditionError(
          "Graphics resource changed since import planning");
    }
    auto changes = palettes_;
    if (!forward) {
      for (auto& change : changes)
        std::swap(change.original_color, change.new_color);
    }
    bool participated = false;
    bool resource_applied = false;
    bool definitions_applied = false;
    std::string rollback_errors;
    auto record_rollback = [&](const absl::Status& status, const char* phase) {
      if (!status.ok()) {
        absl::StrAppend(&rollback_errors, "; ", phase, ": ", status.message());
        poisoned_ = true;
      }
    };
    auto rollback = [&](const absl::Status& failure) -> absl::Status {
      if (definitions_applied) {
        record_rollback(forward ? definitions_->Undo() : definitions_->Redo(),
                        "definition rollback");
      }
      if (resource_applied)
        record_rollback(resource_.replace(expected), "resource rollback");
      return rollback_errors.empty() ? failure
                                     : absl::InternalError(absl::StrCat(
                                           failure.message(), rollback_errors));
    };
    const auto status = gfx::PaletteManager::Get().ApplyExternalChanges(
        owner_, changes, [&]() -> absl::Status {
          participated = true;
          if (expected != desired) {
            const auto replaced = resource_.replace(desired);
            if (!replaced.ok())
              return rollback(replaced);
            resource_applied = true;
          }
          const auto source_refreshed = refresh_();
          if (!source_refreshed.ok())
            return rollback(source_refreshed);
          if (definitions_) {
            const auto defined =
                forward ? definitions_->Redo() : definitions_->Undo();
            if (!defined.ok())
              return rollback(defined);
            definitions_applied = true;
          }
          const auto view_refreshed = refresh_();
          if (!view_refreshed.ok())
            return rollback(view_refreshed);
          return absl::OkStatus();
        });
    if (!status.ok()) {
      // ApplyExternalChanges has now restored palette values and tracking.
      // Refreshing inside its participant would retain the rejected colors.
      if (participated) {
        const auto restored = refresh_();
        if (!restored.ok()) {
          poisoned_ = true;
          return absl::InternalError(
              absl::StrCat(status.message(),
                           "; restored-view refresh: ", restored.message()));
        }
      }
      return status;
    }
    applied_ = forward;
    return absl::OkStatus();
  }

  ProjectGraphicsImportResourceCallbacks resource_;
  std::vector<uint8_t> before_, after_;
  std::vector<gfx::PaletteColorChange> palettes_;
  std::unique_ptr<UndoAction> definitions_;
  const zelda3::GameData* owner_;
  ProjectGraphicsImportRefresh refresh_;
  std::string description_;
  bool applied_ = false;
  bool poisoned_ = false;
};

}  // namespace

absl::StatusOr<std::unique_ptr<UndoAction>> CreateProjectGraphicsImportAction(
    ProjectGraphicsImportResourceCallbacks resource,
    const ProjectGraphicsImportPlan& plan,
    std::unique_ptr<UndoAction> prepared_definitions,
    const zelda3::GameData* owner, ProjectGraphicsImportRefresh refresh,
    std::string description) {
  if (!resource.read || !resource.replace || !refresh || !owner ||
      !gfx::PaletteManager::Get().IsManaging(owner)) {
    return absl::FailedPreconditionError(
        "Import document session is unavailable");
  }
  if (plan.resource_before.empty() ||
      plan.resource_before.size() != plan.resource_after.size() ||
      plan.resource_before.size() % 32 != 0) {
    return absl::InvalidArgumentError(
        "Import must preserve aligned resource size");
  }
  ASSIGN_OR_RETURN(const auto current, resource.read());
  if (current != plan.resource_before) {
    return absl::FailedPreconditionError(
        "Graphics resource changed since import planning");
  }
  return std::unique_ptr<UndoAction>(
      std::make_unique<ProjectGraphicsImportAction>(
          std::move(resource), plan, std::move(prepared_definitions), owner,
          std::move(refresh), std::move(description)));
}

absl::StatusOr<std::unique_ptr<UndoAction>> CreateProjectGraphicsImportAction(
    std::shared_ptr<ProjectGraphicsSource> source,
    ProjectGraphicsImportResourceResolver resolve_resource,
    const ProjectGraphicsImportPlan& plan,
    std::unique_ptr<UndoAction> prepared_definitions,
    const zelda3::GameData* owner, ProjectGraphicsImportRefresh refresh,
    std::string description) {
  if (!source || !source->loaded() || !resolve_resource)
    return absl::FailedPreconditionError(
        "Project graphics source is unavailable");
  const auto source_root = source->source_root();
  const auto catalog = source->portable_catalog();
  auto resolve = [source, resolve_resource, source_root,
                  catalog]() -> absl::StatusOr<std::string> {
    if (!source->loaded() || source->source_root() != source_root ||
        source->portable_catalog() != catalog) {
      return absl::FailedPreconditionError(
          "Project graphics source session changed");
    }
    return resolve_resource(*source);
  };
  ProjectGraphicsImportResourceCallbacks resource;
  resource.read = [source, resolve]() -> absl::StatusOr<std::vector<uint8_t>> {
    ASSIGN_OR_RETURN(const auto path, resolve());
    for (const auto& candidate : source->resources()) {
      if (candidate.source_path == path)
        return candidate.pending_bytes;
    }
    return absl::NotFoundError("Import resource binding is unavailable");
  };
  resource.replace =
      [source, resolve](const std::vector<uint8_t>& bytes) -> absl::Status {
    ASSIGN_OR_RETURN(const auto path, resolve());
    return source->ReplaceResourceBytes(path, bytes);
  };
  return CreateProjectGraphicsImportAction(
      std::move(resource), plan, std::move(prepared_definitions), owner,
      std::move(refresh), std::move(description));
}

}  // namespace yaze::editor
