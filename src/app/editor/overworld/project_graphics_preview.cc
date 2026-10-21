#include "app/editor/overworld/project_graphics_preview.h"

#include <utility>

#include "core/overworld_graphics_preview.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

absl::Status ProjectGraphicsPreviewSource::Bind(
    const std::string& path, zelda3::Overworld& overworld,
    std::set<int>* changed_parents) {
  path_ = path;
  auto loaded = core::LoadOverworldGraphicsPreview(path);
  zelda3::OverworldGraphicsPreview next;
  if (loaded.ok())
    next = std::move(*loaded);
  auto changed = ChangedParents(applied_, next);
  Apply(std::move(next), overworld);
  if (changed_parents)
    *changed_parents = std::move(changed);
  return loaded.status();
}

absl::StatusOr<ProjectGraphicsPreviewSource::Replacement>
ProjectGraphicsPreviewSource::LoadReplacement() const {
  if (path_.empty()) {
    return absl::FailedPreconditionError(
        "The project has no overworld graphics catalog");
  }
  auto loaded = core::LoadOverworldGraphicsPreview(path_);
  if (!loaded.ok())
    return loaded.status();
  Replacement replacement;
  replacement.changed_parents = ChangedParents(applied_, *loaded);
  replacement.preview = std::move(*loaded);
  replacement.path = path_;
  replacement.generation = generation_;
  return replacement;
}

absl::StatusOr<ProjectGraphicsPreviewSource::Replacement>
ProjectGraphicsPreviewSource::Commit(Replacement replacement,
                                     zelda3::Overworld& overworld) {
  if (replacement.path != path_ || replacement.generation != generation_) {
    return absl::FailedPreconditionError(
        "Project graphics changed while the catalog was reloading");
  }
  Replacement inverse{applied_, replacement.changed_parents, path_, 0};
  if (!replacement.changed_parents.empty())
    Apply(std::move(replacement.preview), overworld);
  inverse.generation = generation_;
  return inverse;
}

std::set<int> ProjectGraphicsPreviewSource::ChangedParents(
    const zelda3::OverworldGraphicsPreview& before,
    const zelda3::OverworldGraphicsPreview& after) {
  std::set<int> parents;
  for (const auto& [parent, tiles] : before) {
    const auto match = after.find(parent);
    if (match == after.end() || match->second != tiles)
      parents.insert(parent);
  }
  for (const auto& [parent, tiles] : after) {
    if (!before.contains(parent))
      parents.insert(parent);
  }
  return parents;
}

void ProjectGraphicsPreviewSource::Apply(zelda3::OverworldGraphicsPreview next,
                                         zelda3::Overworld& overworld) {
  applied_ = std::move(next);
  ++generation_;
  overworld.SetGraphicsPreview(applied_);
}

}  // namespace yaze::editor
