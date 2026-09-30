#ifndef YAZE_ZELDA3_GRAPHICS_RESOLVED_GRAPHICS_CONTEXT_H_
#define YAZE_ZELDA3_GRAPHICS_RESOLVED_GRAPHICS_CONTEXT_H_

#include <cstdint>
#include <string>
#include <vector>

namespace yaze {
namespace core {
class TerrainCatalog;
}  // namespace core

namespace zelda3 {

class OverworldMap;
class Room;

// Caller-supplied project facts. Sheet and palette facts come from the loader.
struct GraphicsContextIdentity {
  std::string project_name;
  const core::TerrainCatalog* terrain_catalog = nullptr;
};

// One of the 16 slots a map or room actually loaded.
struct ResolvedGraphicsSlot {
  int slot = 0;
  std::string role;
  std::string palette_group;
  int sheet_id = 0;
  std::string source;
  int inherited_sheet = -1;
  bool loader_agrees = false;
  uint64_t store_revision = 0;
  int width = 0;
  int height = 0;
  bool composite = false;
  int composite_upper_sheet = -1;
  std::string composite_upper_source;
  uint64_t composite_upper_revision = 0;
};

struct ResolvedPaletteSource {
  std::string name;
  std::string source;
  int selector = -1;
  bool inherited = false;
};

// A palette context that applies beside the loaded one, or an explicit gap.
struct ResolvedAlternateContext {
  std::string name;
  std::string state;
  std::string detail;
};

// Read-only graphics facts for one overworld map or one dungeon room.
struct ResolvedGraphicsContext {
  std::string schema = "yaze.graphics_context.v1";
  std::string surface;
  bool graphics_loaded = false;
  int id = -1;
  int parent_id = -1;
  std::string project_name;
  std::string rom_sha256;
  uint64_t graphics_revision = 0;
  std::string graphics_fingerprint;
  std::string terrain_catalog = "not_attached";
  int terrain_adapter_entries = 0;
  bool palette_colors_resolved = false;
  bool loader_agrees = false;
  std::vector<ResolvedGraphicsSlot> slots;
  std::vector<ResolvedPaletteSource> palettes;
  std::vector<ResolvedAlternateContext> alternates;

  std::string ToJson() const;
};

// Requires LoadAreaGraphics() on this map. LoadPalette() fills palette colors.
ResolvedGraphicsContext DescribeOverworldGraphicsContext(
    const OverworldMap& map, const GraphicsContextIdentity& identity = {});

// Requires LoadRoomGraphics() on this room.
ResolvedGraphicsContext DescribeDungeonGraphicsContext(
    const Room& room, const GraphicsContextIdentity& identity = {});

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_GRAPHICS_RESOLVED_GRAPHICS_CONTEXT_H_
