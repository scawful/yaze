#include "zelda3/dungeon/object_draw_code.h"

#include <algorithm>
#include <set>

#include "rom/rom.h"
#include "rom/snes.h"

namespace yaze::zelda3 {

namespace {

struct DispatchTable {
  uint32_t address;
  int first_object;
  int count;
};

constexpr DispatchTable kDispatchTables[] = {
    {0x018200, 0x000, 0xF8},
    {0x018470, 0x100, 0x40},
    {0x0185F0, 0xF80, 0x80},
};
constexpr uint32_t kMaxRoutineBytes = 0x100;
constexpr uint8_t kJsl = 0x22;
constexpr uint8_t kJml = 0x5C;
constexpr uint8_t kFirstExpandedBank = 0x20;

}  // namespace

std::vector<ObjectDrawCode> ReadObjectDrawCode(const Rom& rom) {
  const auto& data = rom.vector();
  auto byte_at = [&](uint32_t snes) -> int {
    const uint32_t pc = SnesToPc(snes);
    return pc < data.size() ? data[pc] : -1;
  };

  std::vector<ObjectDrawCode> out;
  for (const auto& table : kDispatchTables) {
    for (int i = 0; i < table.count; ++i) {
      const uint32_t entry = table.address + static_cast<uint32_t>(i * 2);
      const int lo = byte_at(entry);
      const int hi = byte_at(entry + 1);
      if (lo < 0 || hi < 0) {
        return {};
      }
      ObjectDrawCode code;
      code.object_id = table.first_object + i;
      code.routine_start =
          0x010000 | static_cast<uint32_t>(lo | (hi << 8));  // Bank $01.
      out.push_back(code);
    }
  }

  std::set<uint32_t> starts;
  for (const auto& code : out) {
    starts.insert(code.routine_start);
  }
  for (auto& code : out) {
    auto next = starts.upper_bound(code.routine_start);
    const uint32_t cap = code.routine_start + kMaxRoutineBytes;
    code.routine_end = next == starts.end() ? cap : std::min(*next, cap);

    const int opcode = byte_at(code.routine_start);
    const int bank = byte_at(code.routine_start + 3);
    if ((opcode == kJsl || opcode == kJml) && bank >= kFirstExpandedBank) {
      code.jumps_to_expanded_code = true;
      code.jump_target =
          static_cast<uint32_t>(byte_at(code.routine_start + 1)) |
          (static_cast<uint32_t>(byte_at(code.routine_start + 2)) << 8) |
          (static_cast<uint32_t>(bank) << 16);
    }
  }
  return out;
}

}  // namespace yaze::zelda3
