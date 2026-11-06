#ifndef YAZE_ZELDA3_OVERWORLD_OVERWORLD_GEOMETRY_H_
#define YAZE_ZELDA3_OVERWORLD_OVERWORLD_GEOMETRY_H_

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {

// One physical screen's area owner and derived navigation values. Word order
// matches native directory tables 8..15: screen changes 1..4, transition Y/X,
// and north/west scroll targets. This is a value plan, not a ROM writer.
struct OverworldGeometryRow {
  uint8_t parent;
  AreaSizeEnum size;
  std::array<uint16_t, 8> words{};
};

// Plan all materialized screens together: a changed area's neighbors can need
// different edge transitions even when their own parent and size do not change.
absl::StatusOr<std::vector<OverworldGeometryRow>> PlanExpandedOverworldGeometry(
    std::span<const OverworldMap> maps);

}  // namespace yaze::zelda3
#endif
