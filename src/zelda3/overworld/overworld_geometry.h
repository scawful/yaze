#ifndef YAZE_ZELDA3_OVERWORLD_OVERWORLD_GEOMETRY_H_
#define YAZE_ZELDA3_OVERWORLD_OVERWORLD_GEOMETRY_H_

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {

// Compact topology and authoring values; no rendered buffers enter history.
struct OverworldAreaGeometry {
  int parent;
  AreaSizeEnum size;
  int quadrant;
  bool operator==(const OverworldAreaGeometry&) const = default;
};
struct OverworldAreaProfile {
  uint8_t graphics = 0;
  uint8_t palette = 0;
  std::array<uint8_t, 4> music{};
  std::array<uint8_t, 3> sprite_graphics{};
  std::array<uint8_t, 3> sprite_palettes{};
  bool operator==(const OverworldAreaProfile&) const = default;
};
struct OverworldSpriteArea {
  uint8_t owner;
  uint8_t x;
  uint8_t y;
  bool operator==(const OverworldSpriteArea&) const = default;
};
struct OverworldPageProperties {
  uint16_t background = 0;
  uint16_t overlay = 0;
  uint16_t message = 0;
  uint8_t main_palette = 0;
  uint8_t animated = 0;
  std::array<uint8_t, 8> graphics{};
  std::array<bool, 4> mosaic{};
  bool operator==(const OverworldPageProperties&) const = default;
};
struct OverworldAreaSnapshot {
  std::vector<OverworldAreaGeometry> geometry;
  std::vector<OverworldAreaProfile> profiles;
  std::vector<OverworldPageProperties> pages;
  std::array<std::vector<OverworldSpriteArea>, 3> sprites;
  std::array<std::vector<bool>, 3> loaded_sprite_maps;
  bool operator==(const OverworldAreaSnapshot&) const = default;
};

// One physical screen's area owner and derived navigation values. Word order
// matches native directory tables 8..15: screen changes 1..4, transition Y/X,
// and north/west scroll targets. This is a value plan, not a ROM writer.
struct OverworldGeometryRow {
  uint8_t parent;
  AreaSizeEnum size;
  std::array<uint16_t, 8> words{};
  bool operator==(const OverworldGeometryRow&) const = default;
};

// Plan all materialized screens together: a changed area's neighbors can need
// different edge transitions even when their own parent and size do not change.
absl::StatusOr<std::vector<OverworldGeometryRow>> PlanExpandedOverworldGeometry(
    std::span<const OverworldMap> maps);
absl::StatusOr<std::vector<OverworldGeometryRow>> PlanExpandedOverworldGeometry(
    std::span<const OverworldAreaGeometry> maps);

}  // namespace yaze::zelda3
#endif
