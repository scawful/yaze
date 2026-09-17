#ifndef YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_SYMBOLOGY_H
#define YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_SYMBOLOGY_H

#include <cstdint>
#include <string>

namespace yaze {
namespace zelda3 {

struct DrawRoutineInfo;

/**
 * @brief Compact, human-readable identity for an object's draw routine.
 *
 * Derived entirely from DrawRoutineRegistry metadata — the routine's
 * category, its BothBG flag and its base pattern size. Nothing here inspects
 * the routine's NAME to infer behaviour: names encode repetition (the
 * `_1to16` suffix) and tile patterns ("4x4") in prose, and reading behaviour
 * out of prose gets it wrong in both directions. `BigHole4x4_1to16` repeats
 * despite naming a 4x4 pattern, and `Corner4x4` is a corner despite naming
 * one too.
 *
 * The four named special cases below are matched by name deliberately: they
 * are distinct object semantics (a chest is a chest), not inferred geometry.
 */
struct DrawRoutineSymbology {
  std::string badge;        // One or two characters, for a grid cell.
  std::string family;       // Human-readable routine family.
  bool dual_layer = false;  // Routine writes to BG1 and BG2.
  int base_width = 0;       // Base pattern width in tiles (0 = unknown).
  int base_height = 0;      // Base pattern height in tiles (0 = unknown).
};

// Returns the symbology for `info`, or an "unknown" symbology when null.
DrawRoutineSymbology GetSymbologyForRoutine(const DrawRoutineInfo* info);

// Returns the symbology for whichever routine draws `object_id`, or an
// "unmapped" symbology when the registry has no routine for it.
DrawRoutineSymbology GetSymbologyForObject(int16_t object_id);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DRAW_ROUTINES_DRAW_ROUTINE_SYMBOLOGY_H
