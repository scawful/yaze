#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_SYMBOLOGY_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_SYMBOLOGY_H

#include <cstdint>
#include <string>

namespace yaze {
namespace zelda3 {

struct DrawRoutineInfo;

// Compact selector symbology derived from DrawRoutineRegistry metadata.
struct DrawRoutineSymbology {
  std::string badge;   // Short 1-2 character badge for grid cells.
  std::string family;  // Human-readable routine family label.
  bool dual_layer = false;
  bool large_fixed = false;
};

DrawRoutineSymbology GetSymbologyForRoutine(const DrawRoutineInfo* info);
DrawRoutineSymbology GetSymbologyForObject(int16_t object_id);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_SYMBOLOGY_H
