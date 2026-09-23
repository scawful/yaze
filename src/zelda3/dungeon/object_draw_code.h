#ifndef YAZE_ZELDA3_DUNGEON_OBJECT_DRAW_CODE_H
#define YAZE_ZELDA3_DUNGEON_OBJECT_DRAW_CODE_H

#include <cstdint>
#include <vector>

namespace yaze {
class Rom;
}

namespace yaze::zelda3 {

/**
 * @brief Where the loaded ROM's code draws one room object.
 *
 * Read from the game's dispatch tables ($018200 for 0x000-0x0F7, $018470
 * for 0x100-0x13F, $0185F0 for 0xF80-0xFFF). Addresses are SNES LoROM.
 */
struct ObjectDrawCode {
  int object_id = -1;
  uint32_t routine_start = 0;
  // Exclusive estimate: the next dispatched routine's start, at most 256
  // bytes on.
  uint32_t routine_end = 0;
  // The routine starts with JSL/JML into bank $20 or above, i.e. into code
  // outside the original 1 MB game (a ROM hack's replacement).
  bool jumps_to_expanded_code = false;
  uint32_t jump_target = 0;
};

std::vector<ObjectDrawCode> ReadObjectDrawCode(const Rom& rom);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_OBJECT_DRAW_CODE_H
