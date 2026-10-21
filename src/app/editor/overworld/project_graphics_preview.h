#ifndef YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_PREVIEW_H_
#define YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_PREVIEW_H_

#include <cstdint>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda3/overworld/graphics_preview.h"

namespace yaze::zelda3 {
class Overworld;
}

namespace yaze::editor {

// The project graphics catalog applied to one overworld model. The catalog is
// read from disk only when its path is bound or on an explicit reload.
class ProjectGraphicsPreviewSource {
 public:
  // A fully validated catalog read from the bound path. It is not applied
  // until Commit, and only against the catalog it was compared with.
  struct Replacement {
    zelda3::OverworldGraphicsPreview preview;
    // Parent maps whose resolved preview tiles differ from the applied ones.
    std::set<int> changed_parents;
    std::string path;
    uint64_t generation = 0;
  };

  const std::string& path() const { return path_; }

  // Binds `path` and applies its catalog. The previous catalog is dropped
  // before loading, so an invalid catalog leaves no preview rather than art
  // from another project. `changed_parents` receives every parent whose
  // applied tiles changed, including when the load fails.
  absl::Status Bind(const std::string& path, zelda3::Overworld& overworld,
                    std::set<int>* changed_parents);

  // Reads and validates the bound catalog again without changing the model.
  absl::StatusOr<Replacement> LoadReplacement() const;

  // Applies `replacement` and returns its inverse. Committing the inverse
  // before any other commit restores the previously applied catalog.
  absl::StatusOr<Replacement> Commit(Replacement replacement,
                                     zelda3::Overworld& overworld);

  static std::set<int> ChangedParents(
      const zelda3::OverworldGraphicsPreview& before,
      const zelda3::OverworldGraphicsPreview& after);

 private:
  void Apply(zelda3::OverworldGraphicsPreview next,
             zelda3::Overworld& overworld);

  std::string path_;
  zelda3::OverworldGraphicsPreview applied_;
  uint64_t generation_ = 0;
};

}  // namespace yaze::editor
#endif
