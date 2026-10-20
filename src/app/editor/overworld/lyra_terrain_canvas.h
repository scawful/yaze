#ifndef YAZE_APP_EDITOR_OVERWORLD_LYRA_TERRAIN_CANVAS_H_
#define YAZE_APP_EDITOR_OVERWORLD_LYRA_TERRAIN_CANVAS_H_

#include <memory>
#include <optional>
#include <string>

#include "app/editor/core/undo_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gui/canvas/canvas.h"
#include "core/lyra_terrain_document.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

// Publishes a validated sidecar while retaining the exact loaded bytes as the
// compare-and-swap baseline. The caller keeps project draft state until the
// project reference itself is saved successfully. Returns the published bytes.
absl::StatusOr<std::string> PublishLyraTerrainSource(
    const project::YazeProject& project, const std::string& source_path,
    const std::optional<std::string>& source_bytes,
    const core::LyraTerrainDocument& document);

// Project-owned terrain. This view never routes edits through the ROM map model.
class LyraTerrainCanvas {
 public:
  LyraTerrainCanvas() = default;
  ~LyraTerrainCanvas();
  LyraTerrainCanvas(const LyraTerrainCanvas&) = delete;
  LyraTerrainCanvas& operator=(const LyraTerrainCanvas&) = delete;
  absl::Status Load(project::YazeProject* project,
                    zelda3::Overworld& overworld);
  absl::Status Create(project::YazeProject* project,
                      zelda3::Overworld& overworld);
  absl::Status Save(project::YazeProject* project,
                    zelda3::Overworld& overworld);
  bool dirty() const;
  void ProjectSaveCommitted() { pending_project_commit_ = false; }
  bool loaded() const { return document_ != nullptr; }
  // Draws the lower four rows of the existing Special World canvas. The
  // owning editor supplies its normal tile selection and drawing-tool state.
  void DrawSpecialWorld(const gui::CanvasRuntime& canvas, UndoManager& history,
                        int& selected_tile, bool paint_enabled,
                        bool fill_enabled);
  void FinishStroke(UndoManager& history);
  void Reset();
  const core::LyraTerrainDocument* document() const { return document_.get(); }
  const gfx::Bitmap& atlas() const { return atlas_; }
  gfx::Bitmap& atlas() { return atlas_; }
  bool stroke_active() const { return stroke_active_; }

 private:
  absl::Status BindSource(zelda3::Overworld& overworld,
                          core::LyraTerrainProfile* profile, bool make_atlas);
  void DrawTile(ImDrawList* draw, int tile, ImVec2 min, float size) const;
  void PaintLine(int x, int y);

  std::shared_ptr<core::LyraTerrainDocument> document_;
  gfx::Bitmap atlas_;
  std::string source_path_;
  std::optional<std::string> source_bytes_;
  bool unpublished_ = false;
  bool pending_project_commit_ = false;
  bool stroke_active_ = false;
  std::optional<ImVec2> last_cell_;
  int brush_ = 0;
  absl::Status status_;
};

}  // namespace yaze::editor
#endif
