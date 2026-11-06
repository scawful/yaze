#include "zelda3/overworld/overworld_geometry.h"

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
namespace yaze::zelda3 {
namespace {

void SetWord(std::vector<OverworldGeometryRow>& rows, int table, int map,
             uint16_t value) {
  rows[map].words[table - 8] = value;
}

void PlanSmallAreaTransitions(std::span<const OverworldAreaGeometry> maps,
                              int i, int parent_x_pos, int parent_y_pos,
                              std::vector<OverworldGeometryRow>& rows) {
  // Set basic transition targets
  SetWord(rows, 14, i, (uint16_t)((parent_y_pos * 0x0200) - 0x00E0));
  SetWord(rows, 15, i, (uint16_t)((parent_x_pos * 0x0200) - 0x0100));

  SetWord(rows, 13, i, parent_x_pos * 0x0200);
  SetWord(rows, 12, i, parent_y_pos * 0x0200);

  // byScreen1 = Transitioning right
  uint16_t by_screen1_small = 0x0060;

  // Check west neighbor for transition adjustments
  if ((i % 0x40) - 1 >= 0) {
    auto& west_neighbor = maps[i - 1];

    // Transition from bottom right quadrant of large area to small area
    if (west_neighbor.size == AreaSizeEnum::LargeArea &&
        west_neighbor.quadrant == 3) {
      by_screen1_small = 0xF060;
    }
    // Transition from bottom quadrant of tall area to small area
    else if (west_neighbor.size == AreaSizeEnum::TallArea &&
             west_neighbor.quadrant == 2) {
      by_screen1_small = 0xF060;
    }
  }

  SetWord(rows, 8, i, by_screen1_small);

  // byScreen2 = Transitioning left
  uint16_t by_screen2_small = 0x0040;

  // Check east neighbor for transition adjustments
  if ((i % 0x40) + 1 < 0x40 && i + 1 < static_cast<int>(maps.size())) {
    auto& east_neighbor = maps[i + 1];

    // Transition from bottom left quadrant of large area to small area
    if (east_neighbor.size == AreaSizeEnum::LargeArea &&
        east_neighbor.quadrant == 2) {
      by_screen2_small = 0xF040;
    }
    // Transition from bottom quadrant of tall area to small area
    else if (east_neighbor.size == AreaSizeEnum::TallArea &&
             east_neighbor.quadrant == 2) {
      by_screen2_small = 0xF040;
    }
  }

  SetWord(rows, 9, i, by_screen2_small);

  // byScreen3 = Transitioning down
  uint16_t by_screen3_small = 0x1800;

  // Check north neighbor for transition adjustments
  if ((i % 0x40) - 8 >= 0) {
    auto& north_neighbor = maps[i - 8];

    // Transition from bottom right quadrant of large area to small area
    if (north_neighbor.size == AreaSizeEnum::LargeArea &&
        north_neighbor.quadrant == 3) {
      by_screen3_small = 0x17C0;
    }
    // Transition from right quadrant of wide area to small area
    else if (north_neighbor.size == AreaSizeEnum::WideArea &&
             north_neighbor.quadrant == 1) {
      by_screen3_small = 0x17C0;
    }
  }

  SetWord(rows, 10, i, by_screen3_small);

  // byScreen4 = Transitioning up
  uint16_t by_screen4_small = 0x1000;

  // Check south neighbor for transition adjustments
  if ((i % 0x40) + 8 < 0x40 && i + 8 < static_cast<int>(maps.size())) {
    auto& south_neighbor = maps[i + 8];

    // Transition from top right quadrant of large area to small area
    if (south_neighbor.size == AreaSizeEnum::LargeArea &&
        south_neighbor.quadrant == 1) {
      by_screen4_small = 0x0FC0;
    }
    // Transition from right quadrant of wide area to small area
    else if (south_neighbor.size == AreaSizeEnum::WideArea &&
             south_neighbor.quadrant == 1) {
      by_screen4_small = 0x0FC0;
    }
  }

  SetWord(rows, 11, i, by_screen4_small);
}

void PlanLargeAreaTransitions(std::span<const OverworldAreaGeometry> maps,
                              int i, int parent_x_pos, int parent_y_pos,
                              std::vector<OverworldGeometryRow>& rows) {
  // Set transition targets for all 4 quadrants
  const uint16_t offsets[] = {0, 2, 16, 18};
  for (auto offset : offsets) {
    SetWord(rows, 14, i + offset / 2,
            (uint16_t)((parent_y_pos * 0x0200) - 0x00E0));
    SetWord(rows, 15, i + offset / 2,
            (uint16_t)((parent_x_pos * 0x0200) - 0x0100));
    SetWord(rows, 13, i + offset / 2, parent_x_pos * 0x0200);
    SetWord(rows, 12, i + offset / 2, parent_y_pos * 0x0200);
  }

  // Complex neighbor-aware transition calculations for large areas
  // byScreen1 = Transitioning right
  std::array<uint16_t, 4> by_screen1_large = {0x0060, 0x0060, 0x1060, 0x1060};

  // Check west neighbor
  if ((i % 0x40) - 1 >= 0) {
    auto& west_neighbor = maps[i - 1];

    if (west_neighbor.size == AreaSizeEnum::LargeArea) {
      switch (west_neighbor.quadrant) {
        case 1:  // From bottom right to bottom left of large area
          by_screen1_large[2] = 0x0060;
          break;
        case 3:  // From bottom right to top left of large area
          by_screen1_large[0] = 0xF060;
          break;
      }
    } else if (west_neighbor.size == AreaSizeEnum::TallArea) {
      switch (west_neighbor.quadrant) {
        case 0:  // From bottom of tall to bottom left of large
          by_screen1_large[2] = 0x0060;
          break;
        case 2:  // From bottom of tall to top left of large
          by_screen1_large[0] = 0xF060;
          break;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    SetWord(rows, 8, i + offsets[j] / 2, by_screen1_large[j]);
  }

  // byScreen2 = Transitioning left
  std::array<uint16_t, 4> by_screen2_large = {0x0080, 0x0080, 0x1080, 0x1080};

  // Check east neighbor
  if ((i % 0x40) + 2 < 0x40 && i + 2 < static_cast<int>(maps.size())) {
    auto& east_neighbor = maps[i + 2];

    if (east_neighbor.size == AreaSizeEnum::LargeArea) {
      switch (east_neighbor.quadrant) {
        case 0:  // From bottom left to bottom right of large area
          by_screen2_large[3] = 0x0080;
          break;
        case 2:  // From bottom left to top right of large area
          by_screen2_large[1] = 0xF080;
          break;
      }
    } else if (east_neighbor.size == AreaSizeEnum::TallArea) {
      switch (east_neighbor.quadrant) {
        case 0:  // From bottom of tall to bottom right of large
          by_screen2_large[3] = 0x0080;
          break;
        case 2:  // From bottom of tall to top right of large
          by_screen2_large[1] = 0xF080;
          break;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    SetWord(rows, 9, i + offsets[j] / 2, by_screen2_large[j]);
  }

  // byScreen3 = Transitioning down
  std::array<uint16_t, 4> by_screen3_large = {0x1800, 0x1840, 0x1800, 0x1840};

  // Check north neighbor
  if ((i % 0x40) - 8 >= 0) {
    auto& north_neighbor = maps[i - 8];

    if (north_neighbor.size == AreaSizeEnum::LargeArea) {
      switch (north_neighbor.quadrant) {
        case 2:  // From bottom right to top right of large area
          by_screen3_large[1] = 0x1800;
          break;
        case 3:  // From bottom right to top left of large area
          by_screen3_large[0] = 0x17C0;
          break;
      }
    } else if (north_neighbor.size == AreaSizeEnum::WideArea) {
      switch (north_neighbor.quadrant) {
        case 0:  // From right of wide to top right of large
          by_screen3_large[1] = 0x1800;
          break;
        case 1:  // From right of wide to top left of large
          by_screen3_large[0] = 0x17C0;
          break;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    SetWord(rows, 10, i + offsets[j] / 2, by_screen3_large[j]);
  }

  // byScreen4 = Transitioning up
  std::array<uint16_t, 4> by_screen4_large = {0x2000, 0x2040, 0x2000, 0x2040};

  // Check south neighbor
  if ((i % 0x40) + 16 < 0x40 && i + 16 < static_cast<int>(maps.size())) {
    auto& south_neighbor = maps[i + 16];

    if (south_neighbor.size == AreaSizeEnum::LargeArea) {
      switch (south_neighbor.quadrant) {
        case 0:  // From top right to bottom right of large area
          by_screen4_large[3] = 0x2000;
          break;
        case 1:  // From top right to bottom left of large area
          by_screen4_large[2] = 0x1FC0;
          break;
      }
    } else if (south_neighbor.size == AreaSizeEnum::WideArea) {
      switch (south_neighbor.quadrant) {
        case 0:  // From right of wide to bottom right of large
          by_screen4_large[3] = 0x2000;
          break;
        case 1:  // From right of wide to bottom left of large
          by_screen4_large[2] = 0x1FC0;
          break;
      }
    }
  }

  for (int j = 0; j < 4; j++) {
    SetWord(rows, 11, i + offsets[j] / 2, by_screen4_large[j]);
  }
}

void PlanWideAreaTransitions(std::span<const OverworldAreaGeometry> maps, int i,
                             int parent_x_pos, int parent_y_pos,
                             std::vector<OverworldGeometryRow>& rows) {
  // Set transition targets for both quadrants
  const uint16_t offsets[] = {0, 2};
  for (auto offset : offsets) {
    SetWord(rows, 14, i + offset / 2,
            (uint16_t)((parent_y_pos * 0x0200) - 0x00E0));
    SetWord(rows, 15, i + offset / 2,
            (uint16_t)((parent_x_pos * 0x0200) - 0x0100));
    SetWord(rows, 13, i + offset / 2, parent_x_pos * 0x0200);
    SetWord(rows, 12, i + offset / 2, parent_y_pos * 0x0200);
  }

  // byScreen1 = Transitioning right
  std::array<uint16_t, 2> by_screen1_wide = {0x0060, 0x0060};

  // Check west neighbor
  if ((i % 0x40) - 1 >= 0) {
    auto& west_neighbor = maps[i - 1];

    // From bottom right of large to left of wide
    if (west_neighbor.size == AreaSizeEnum::LargeArea &&
        west_neighbor.quadrant == 3) {
      by_screen1_wide[0] = 0xF060;
    }
    // From bottom of tall to left of wide
    else if (west_neighbor.size == AreaSizeEnum::TallArea &&
             west_neighbor.quadrant == 2) {
      by_screen1_wide[0] = 0xF060;
    }
  }

  for (int j = 0; j < 2; j++) {
    SetWord(rows, 8, i + offsets[j] / 2, by_screen1_wide[j]);
  }

  // byScreen2 = Transitioning left
  std::array<uint16_t, 2> by_screen2_wide = {0x0080, 0x0080};

  // Check east neighbor
  if ((i % 0x40) + 2 < 0x40 && i + 2 < static_cast<int>(maps.size())) {
    auto& east_neighbor = maps[i + 2];

    // From bottom left of large to right of wide
    if (east_neighbor.size == AreaSizeEnum::LargeArea &&
        east_neighbor.quadrant == 2) {
      by_screen2_wide[1] = 0xF080;
    }
    // From bottom of tall to right of wide
    else if (east_neighbor.size == AreaSizeEnum::TallArea &&
             east_neighbor.quadrant == 2) {
      by_screen2_wide[1] = 0xF080;
    }
  }

  for (int j = 0; j < 2; j++) {
    SetWord(rows, 9, i + offsets[j] / 2, by_screen2_wide[j]);
  }

  // byScreen3 = Transitioning down
  std::array<uint16_t, 2> by_screen3_wide = {0x1800, 0x1840};

  // Check north neighbor
  if ((i % 0x40) - 8 >= 0) {
    auto& north_neighbor = maps[i - 8];

    if (north_neighbor.size == AreaSizeEnum::LargeArea) {
      switch (north_neighbor.quadrant) {
        case 2:  // From bottom right of large to right of wide
          by_screen3_wide[1] = 0x1800;
          break;
        case 3:  // From bottom right of large to left of wide
          by_screen3_wide[0] = 0x17C0;
          break;
      }
    } else if (north_neighbor.size == AreaSizeEnum::WideArea) {
      switch (north_neighbor.quadrant) {
        case 0:  // From right of wide to right of wide
          by_screen3_wide[1] = 0x1800;
          break;
        case 1:  // From right of wide to left of wide
          by_screen3_wide[0] = 0x17C0;
          break;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    SetWord(rows, 10, i + offsets[j] / 2, by_screen3_wide[j]);
  }

  // byScreen4 = Transitioning up
  std::array<uint16_t, 2> by_screen4_wide = {0x1000, 0x1040};

  // Check south neighbor
  if ((i % 0x40) + 8 < 0x40 && i + 8 < static_cast<int>(maps.size())) {
    auto& south_neighbor = maps[i + 8];

    if (south_neighbor.size == AreaSizeEnum::LargeArea) {
      switch (south_neighbor.quadrant) {
        case 0:  // From top right of large to right of wide
          by_screen4_wide[1] = 0x1000;
          break;
        case 1:  // From top right of large to left of wide
          by_screen4_wide[0] = 0x0FC0;
          break;
      }
    } else if (south_neighbor.size == AreaSizeEnum::WideArea) {
      if (south_neighbor.quadrant == 1) {
        by_screen4_wide[0] = 0x0FC0;
      }
      switch (south_neighbor.quadrant) {
        case 0:  // From right of wide to right of wide
          by_screen4_wide[1] = 0x1000;
          break;
        case 1:  // From right of wide to left of wide
          by_screen4_wide[0] = 0x0FC0;
          break;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    SetWord(rows, 11, i + offsets[j] / 2, by_screen4_wide[j]);
  }
}

void PlanTallAreaTransitions(std::span<const OverworldAreaGeometry> maps, int i,
                             int parent_x_pos, int parent_y_pos,
                             std::vector<OverworldGeometryRow>& rows) {
  // Set transition targets for both quadrants
  const uint16_t offsets[] = {0, 16};
  for (auto offset : offsets) {
    SetWord(rows, 14, i + offset / 2,
            (uint16_t)((parent_y_pos * 0x0200) - 0x00E0));
    SetWord(rows, 15, i + offset / 2,
            (uint16_t)((parent_x_pos * 0x0200) - 0x0100));
    SetWord(rows, 13, i + offset / 2, parent_x_pos * 0x0200);
    SetWord(rows, 12, i + offset / 2, parent_y_pos * 0x0200);
  }

  // byScreen1 = Transitioning right
  std::array<uint16_t, 2> by_screen1_tall = {0x0060, 0x1060};

  // Check west neighbor
  if ((i % 0x40) - 1 >= 0) {
    auto& west_neighbor = maps[i - 1];

    if (west_neighbor.size == AreaSizeEnum::LargeArea) {
      switch (west_neighbor.quadrant) {
        case 1:  // From bottom right of large to bottom of tall
          by_screen1_tall[1] = 0x0060;
          break;
        case 3:  // From bottom right of large to top of tall
          by_screen1_tall[0] = 0xF060;
          break;
      }
    } else if (west_neighbor.size == AreaSizeEnum::TallArea) {
      switch (west_neighbor.quadrant) {
        case 0:  // From bottom of tall to bottom of tall
          by_screen1_tall[1] = 0x0060;
          break;
        case 2:  // From bottom of tall to top of tall
          by_screen1_tall[0] = 0xF060;
          break;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    SetWord(rows, 8, i + offsets[j] / 2, by_screen1_tall[j]);
  }

  // byScreen2 = Transitioning left
  std::array<uint16_t, 2> by_screen2_tall = {0x0040, 0x1040};

  // Check east neighbor
  if ((i % 0x40) + 1 < 0x40 && i + 1 < static_cast<int>(maps.size())) {
    auto& east_neighbor = maps[i + 1];

    if (east_neighbor.size == AreaSizeEnum::LargeArea) {
      switch (east_neighbor.quadrant) {
        case 0:  // From bottom left of large to bottom of tall
          by_screen2_tall[1] = 0x0040;
          break;
        case 2:  // From bottom left of large to top of tall
          by_screen2_tall[0] = 0xF040;
          break;
      }
    } else if (east_neighbor.size == AreaSizeEnum::TallArea) {
      switch (east_neighbor.quadrant) {
        case 0:  // From bottom of tall to bottom of tall
          by_screen2_tall[1] = 0x0040;
          break;
        case 2:  // From bottom of tall to top of tall
          by_screen2_tall[0] = 0xF040;
          break;
      }
    }
  }

  for (int j = 0; j < 2; j++) {
    SetWord(rows, 9, i + offsets[j] / 2, by_screen2_tall[j]);
  }

  // byScreen3 = Transitioning down
  std::array<uint16_t, 2> by_screen3_tall = {0x1800, 0x1800};

  // Check north neighbor
  if ((i % 0x40) - 8 >= 0) {
    auto& north_neighbor = maps[i - 8];

    // From bottom right of large to top of tall
    if (north_neighbor.size == AreaSizeEnum::LargeArea &&
        north_neighbor.quadrant == 3) {
      by_screen3_tall[0] = 0x17C0;
    }
    // From right of wide to top of tall
    else if (north_neighbor.size == AreaSizeEnum::WideArea &&
             north_neighbor.quadrant == 1) {
      by_screen3_tall[0] = 0x17C0;
    }
  }

  for (int j = 0; j < 2; j++) {
    SetWord(rows, 10, i + offsets[j] / 2, by_screen3_tall[j]);
  }

  // byScreen4 = Transitioning up
  std::array<uint16_t, 2> by_screen4_tall = {0x2000, 0x2000};

  // Check south neighbor
  if ((i % 0x40) + 16 < 0x40 && i + 16 < static_cast<int>(maps.size())) {
    auto& south_neighbor = maps[i + 16];

    // From top right of large to bottom of tall
    if (south_neighbor.size == AreaSizeEnum::LargeArea &&
        south_neighbor.quadrant == 1) {
      by_screen4_tall[1] = 0x1FC0;
    }
    // From right of wide to bottom of tall
    else if (south_neighbor.size == AreaSizeEnum::WideArea &&
             south_neighbor.quadrant == 1) {
      by_screen4_tall[1] = 0x1FC0;
    }
  }

  for (int j = 0; j < 2; j++) {
    SetWord(rows, 11, i + offsets[j] / 2, by_screen4_tall[j]);
  }
}

}  // namespace

absl::StatusOr<std::vector<OverworldGeometryRow>> PlanExpandedOverworldGeometry(
    std::span<const OverworldMap> maps) {
  std::vector<OverworldAreaGeometry> geometry;
  geometry.reserve(maps.size());
  for (size_t i = 0; i < maps.size(); ++i) {
    if (maps[i].index() != static_cast<int>(i))
      return absl::FailedPreconditionError("Overworld map identity changed");
    geometry.push_back(
        {maps[i].parent(), maps[i].area_size(), maps[i].large_index()});
  }
  return PlanExpandedOverworldGeometry(geometry);
}

absl::StatusOr<std::vector<OverworldGeometryRow>> PlanExpandedOverworldGeometry(
    std::span<const OverworldAreaGeometry> maps) {
  if (maps.empty() || maps.size() > 192)
    return absl::InvalidArgumentError("Invalid expanded overworld map count");
  std::vector<OverworldGeometryRow> rows;
  rows.reserve(maps.size());
  // Validate the complete footprint before calculating or publishing any row.
  for (size_t i = 0; i < maps.size(); ++i) {
    const auto& map = maps[i];
    const int parent = map.parent;
    const int size = static_cast<int>(map.size);
    const int width = size == 1 || size == 2 ? 2 : 1;
    const int height = size == 1 || size == 3 ? 2 : 1;
    if (parent < 0 || parent >= static_cast<int>(maps.size()) || size < 0 ||
        size > 3 || parent / 64 != static_cast<int>(i) / 64 ||
        parent % 8 + width > 8 || parent % 64 / 8 + height > 8 ||
        static_cast<int>(i) < parent ||
        static_cast<int>(i) - parent >= height * 8 ||
        (static_cast<int>(i) - parent) % 8 >= width)
      return absl::FailedPreconditionError(
          absl::StrFormat("Invalid overworld area footprint at map 0x%02X", i));
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const int child = parent + y * 8 + x;
        if (child >= static_cast<int>(maps.size()) ||
            maps[child].parent != parent || maps[child].size != map.size ||
            maps[child].quadrant != y * 2 + x)
          return absl::FailedPreconditionError(absl::StrFormat(
              "Inconsistent overworld area child at map 0x%02X", child));
      }
    }
    rows.push_back({static_cast<uint8_t>(parent), map.size});
  }
  for (size_t i = 0; i < maps.size(); ++i) {
    if (maps[i].parent != static_cast<int>(i))
      continue;
    const int x = (i % 64) % 8;
    const int y = (i % 64) / 8;
    switch (maps[i].size) {
      case AreaSizeEnum::SmallArea:
        PlanSmallAreaTransitions(maps, i, x, y, rows);
        break;
      case AreaSizeEnum::LargeArea:
        PlanLargeAreaTransitions(maps, i, x, y, rows);
        break;
      case AreaSizeEnum::WideArea:
        PlanWideAreaTransitions(maps, i, x, y, rows);
        break;
      case AreaSizeEnum::TallArea:
        PlanTallAreaTransitions(maps, i, x, y, rows);
        break;
    }
  }
  return rows;
}

}  // namespace yaze::zelda3
