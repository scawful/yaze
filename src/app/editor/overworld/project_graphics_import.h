#ifndef YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_IMPORT_H_
#define YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_IMPORT_H_

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "app/editor/overworld/painting/tile_brush.h"
#include "app/editor/overworld/tile16/tile16_edit_types.h"
#include "app/gfx/util/palette_manager.h"
#include "zelda3/graphics/resolved_graphics_context.h"
#include "zelda3/overworld/tile16_renderer.h"

namespace yaze::editor {

// Logical source art only. Neither IDs nor palette names allocate destinations.
struct ProjectImportPalette {
  std::string id;
  std::array<std::optional<uint16_t>, 16> colors;
  std::vector<uint8_t> characters;  // Native 4bpp, 32 bytes per logical CHR.
};
struct ProjectImportQuadrant {
  bool transparent = true;
  std::string palette;
  int character = 0;
  bool hflip = false;
  bool vflip = false;
};
struct ProjectImportTile16 {
  std::array<ProjectImportQuadrant, 4> quadrants;
};
struct ProjectGraphicsImportAsset {
  int width = 0;  // Tile16 cells.
  int height = 0;
  std::vector<ProjectImportPalette> palettes;
  std::vector<ProjectImportTile16> cells;  // Row-major, complete rectangle.
  std::string source_sha256;
};

// These ranges must already be owned by the caller. Unreferenced is not free.
struct ProjectImportPaletteBinding {
  std::string palette;
  int bg_row = -1;
  int first_character = -1;
  int owned_character_count = 0;  // Includes exact-background alpha variants.
};
struct ProjectGraphicsImportBindings {
  std::vector<uint8_t> target_resource;
  int resource_first_character = -1;
  std::vector<ProjectImportPaletteBinding> palettes;
  int first_tile16 = -1;
  int owned_tile16_count = 0;
  int anchor_x = 0;  // Tile16 coordinates in context.grid.
  int anchor_y = 0;
};

// Snapshot built from the actual resolved map, including unsaved source art.
// owner must remain alive and belongs to the active caller session.
struct ProjectGraphicsImportContext {
  const zelda3::GameData* owner = nullptr;
  std::vector<zelda3::Tile8PixelData> characters;
  std::array<uint16_t, 128> palette{};
  zelda3::PaletteAddressMap palette_addresses{};
  std::vector<gfx::Tile16> definitions;
  int grid_width = 0;
  int grid_height = 0;
  std::vector<uint16_t> grid;  // Row-major current Tile16 IDs.
};
struct ProjectImportReplacement {
  int character = -1;
  int definition_consumers = 0;
  int placed_consumers = 0;
};
struct ProjectGraphicsImportPlan {
  std::string source_sha256;
  std::vector<uint8_t> resource_before;
  std::vector<uint8_t> resource_after;
  std::vector<gfx::PaletteColorChange> palette_changes;
  std::vector<Tile16Commit> definitions;
  TileBrush stamp;  // Fully transparent cells retain their current Tile16 IDs.
  std::vector<ProjectImportReplacement> replacements;
  std::vector<std::string> notes;
};

// Reads a logical pack and its relative V2 catalog; verifies hashes and shapes.
// It never writes an asset, ROM, document, history, or project.
absl::StatusOr<ProjectGraphicsImportAsset> LoadProjectGraphicsImport(
    const std::filesystem::path& logical_pack);

absl::StatusOr<ProjectGraphicsImportContext>
CaptureProjectGraphicsImportContext(const zelda3::OverworldMap& map,
                                    const std::vector<gfx::Tile16>& definitions,
                                    const std::vector<uint16_t>& grid,
                                    int width, int height);

// Pure planning. Partial alpha is composed against the resolved current tile;
// exact background colors may use unused logical indices except 0/8. Refuses
// when palette/range capacity is insufficient or a placed consumer changes.
absl::StatusOr<ProjectGraphicsImportPlan> PlanProjectGraphicsImport(
    const ProjectGraphicsImportAsset& asset,
    const ProjectGraphicsImportBindings& bindings,
    const ProjectGraphicsImportContext& context);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PROJECT_GRAPHICS_IMPORT_H_
