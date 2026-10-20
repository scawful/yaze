#ifndef YAZE_APP_EDITOR_OVERWORLD_LYRA_TERRAIN_CANVAS_H_
#define YAZE_APP_EDITOR_OVERWORLD_LYRA_TERRAIN_CANVAS_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/painting/tile_brush.h"
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
  // Rendering only. TilePaintingManager owns the shared Overworld gestures.
  void DrawSpecialWorld(const gui::CanvasRuntime& canvas);
  // Coordinates are document-relative Tile16 cells; footprints clip at its
  // edge. The entire brush is validated before any cells are changed.
  absl::Status PaintPattern(const TileBrush& brush, int x, int y, int width,
                            int height, bool finalize, UndoManager& history);
  void FinishStroke(UndoManager& history);
  void Reset();
  const core::LyraTerrainDocument* document() const { return document_.get(); }
  const gfx::Bitmap& atlas() const { return atlas_; }
  gfx::Bitmap& atlas() { return atlas_; }
  bool stroke_active() const { return stroke_active_; }
  // All edits share the project's history and atomic sidecar save path.
  absl::Status EditAuthoring(core::LyraAuthoringState after,
                             zelda3::Overworld& overworld,
                             UndoManager& history);
  absl::Status PrepareMapAssets(zelda3::Overworld& overworld, int state);
  gfx::Bitmap& MapAtlas(int map_id);
  gfx::Bitmap* MapGraphics(int map_id);
  void DrawProperties(int map_id, int& state, zelda3::Overworld& overworld,
                      UndoManager& history);
  bool entity_placement_active() const {
    return document_ && placement_kind_ != 0;
  }
  void DrawSpritePreview(const gui::CanvasRuntime& canvas, int map_id,
                         const core::LyraSpritePlacement& sprite);
  void DrawEntities(const gui::CanvasRuntime& canvas, int world, int state);
  void HandleEntityInput(const gui::CanvasRuntime& canvas, int world,
                         zelda3::Overworld& overworld, UndoManager& history);

 private:
  absl::Status ApplyAuthoring(core::LyraAuthoringState after,
                              zelda3::Overworld& overworld,
                              UndoManager& history);
  struct MapAssets {
    gfx::Bitmap atlas;
    gfx::Bitmap graphics;
    std::string binding;
    std::unordered_map<int, std::unique_ptr<gfx::Bitmap>> sprites;
    std::unordered_map<int, SDL_Rect> sprite_bounds;
    ~MapAssets();
  };
  absl::StatusOr<std::unique_ptr<MapAssets>> ResolveMapAssets(
      zelda3::Overworld& overworld, const core::LyraMapProperties& properties,
      int state, bool textures);
  // Returns `candidate` with the native area parent's graphics selectors only
  // after every field and Lyra state validates. Never mutates the source map.
  absl::StatusOr<core::LyraMapProperties> CopyNativeGraphicsSettings(
      zelda3::Overworld& overworld, int source_map,
      core::LyraMapProperties candidate);
  absl::Status CheckAssetSources(zelda3::Overworld& overworld);
  absl::Status ValidateMapAssets(zelda3::Overworld& overworld,
                                 const core::LyraAuthoringState& authoring);
  absl::Status BindSource(zelda3::Overworld& overworld,
                          core::LyraTerrainProfile* profile, bool make_atlas);
  void DrawTile(ImDrawList* draw, const gfx::Bitmap& atlas, int tile,
                ImVec2 min, float size) const;
  void PaintStamp(int x, int y);
  void PaintLine(int x, int y);

  std::shared_ptr<core::LyraTerrainDocument> document_;
  gfx::Bitmap atlas_;
  std::string source_path_;
  std::optional<std::string> source_bytes_;
  bool unpublished_ = false;
  bool pending_project_commit_ = false;
  bool stroke_active_ = false;
  std::optional<ImVec2> last_cell_;
  TileBrush brush_;
  int pattern_width_ = 0;
  int pattern_height_ = 0;
  absl::Status status_;
  std::unordered_map<std::string, std::unique_ptr<MapAssets>> map_assets_;
  std::array<MapAssets*, 32> displayed_assets_{};
  int asset_state_ = 0;
  gfx::Bitmap unavailable_atlas_;
  std::vector<uint8_t> source_rom_, source_graphics_;
  std::string source_metadata_;
  absl::Status source_status_;
  absl::Status authoring_assets_status_;
  std::optional<core::LyraAuthoringState> validated_authoring_;
  bool assets_valid_ = false;
  int property_map_ = -1;
  std::optional<core::LyraMapProperties> property_draft_;
  std::optional<core::LyraMapProperties> property_baseline_;
  int copy_source_map_ = 0x3A;
  // Cleared whenever the destination map changes.
  std::string copy_message_;
  bool copy_failed_ = false;
  int placement_kind_ = 0;
  int placement_type_ = 0;
  int placement_state_ = 0;
  // Independent of placement_type_, which other placement modes overwrite.
  int new_sprite_type_ = 0;
  std::array<char, 64> sprite_filter_{};
  uint32_t selected_entity_ = 0;
  int selected_kind_ = 0;
  std::optional<core::LyraAuthoringState> drag_before_;
  int drag_map_ = -1;
};

}  // namespace yaze::editor
#endif
