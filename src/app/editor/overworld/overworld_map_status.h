#ifndef YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_MAP_STATUS_H_
#define YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_MAP_STATUS_H_

#include <string>

#include "absl/strings/str_format.h"

namespace yaze::editor {

// Absolute overworld map IDs pack world as map / 0x40.
inline const char* OverworldWorldLabelFromMap(int map_id) {
  switch (map_id / 0x40) {
    case 0:
      return "LW";
    case 1:
      return "DW";
    case 2:
      return "SW";
    default:
      return "??";
  }
}

// Status-bar Map segment: prefer hovered identity while the canvas is hovered.
// When hover differs from selection, keep both visible and distinct.
inline std::string FormatOverworldMapStatusSegment(int current_map,
                                                   int hovered_map) {
  if (hovered_map < 0 || hovered_map == current_map) {
    return absl::StrFormat("%s #%02X", OverworldWorldLabelFromMap(current_map),
                           current_map & 0xFF);
  }

  const char* hover_world = OverworldWorldLabelFromMap(hovered_map);
  const char* sel_world = OverworldWorldLabelFromMap(current_map);
  if (std::strcmp(hover_world, sel_world) == 0) {
    return absl::StrFormat("%s #%02X · sel #%02X", hover_world,
                           hovered_map & 0xFF, current_map & 0xFF);
  }
  return absl::StrFormat("%s #%02X · sel %s #%02X", hover_world,
                         hovered_map & 0xFF, sel_world, current_map & 0xFF);
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_OVERWORLD_MAP_STATUS_H_
