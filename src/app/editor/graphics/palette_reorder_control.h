#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTE_REORDER_CONTROL_H_
#define YAZE_APP_EDITOR_GRAPHICS_PALETTE_REORDER_CONTROL_H_

#include <functional>
#include <map>
#include <optional>
#include <string>

#include "app/editor/graphics/palette_pixel_command.h"

namespace yaze::editor {

// Stages a source-scoped permutation without editing the model. The Graphics
// editor owns the mutation callback and its combined palette/pixel history.
class PaletteReorderControl {
 public:
  using Apply = std::function<absl::Status(const std::vector<size_t>&,
                                           const PalettePermutation&)>;
  void Draw(GraphicsEditorState& state, const Apply& apply, bool busy);

 private:
  void Begin(const GraphicsEditorState& state);
  bool SourceChanged(const GraphicsEditorState& state) const;

  std::optional<GraphicsEditContext> source_;
  uint64_t source_revision_ = 0;
  uint64_t palette_revision_ = 0;
  std::map<uint16_t, uint64_t> sheet_revisions_;
  // New slot -> old slot; converted to old -> new for the model command.
  PalettePermutation order_{0, 1, 2, 3, 4, 5, 6, 7};
  int selected_slot_ = -1;
  bool whole_pattern_ = false;
  std::string error_;
};

}  // namespace yaze::editor
#endif  // YAZE_APP_EDITOR_GRAPHICS_PALETTE_REORDER_CONTROL_H_
