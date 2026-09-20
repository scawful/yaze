#include "zelda3/dungeon/object_draw_code.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "rom/snes.h"

namespace yaze::zelda3 {
namespace {

void WriteWord(std::vector<uint8_t>& data, uint32_t snes, uint16_t word) {
  const uint32_t pc = SnesToPc(snes);
  data[pc] = static_cast<uint8_t>(word & 0xFF);
  data[pc + 1] = static_cast<uint8_t>(word >> 8);
}

const ObjectDrawCode* Find(const std::vector<ObjectDrawCode>& all, int id) {
  auto it = std::find_if(all.begin(), all.end(), [id](const auto& code) {
    return code.object_id == id;
  });
  return it == all.end() ? nullptr : &*it;
}

TEST(ObjectDrawCodeTest, ReadsDispatchTablesAndSpotsExpandedJumps) {
  std::vector<uint8_t> data(0x400000, 0x00);
  // Object 0x031 -> $01B53C, the address Oracle of Secrets uses.
  WriteWord(data, 0x018200 + 2 * 0x31, 0xB53C);
  // Object 0x032 -> $01B541, and 0xF81 -> $019000 (a plain routine).
  WriteWord(data, 0x018200 + 2 * 0x32, 0xB541);
  WriteWord(data, 0x0185F0 + 2 * 1, 0x9000);
  // $01B53C: JSL $2C8000 (into expanded code); $01B541: RTS.
  const uint32_t stub = SnesToPc(0x01B53C);
  data[stub] = 0x22;
  data[stub + 1] = 0x00;
  data[stub + 2] = 0x80;
  data[stub + 3] = 0x2C;
  data[SnesToPc(0x01B541)] = 0x60;

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  const auto all = ReadObjectDrawCode(rom);
  ASSERT_FALSE(all.empty());

  const auto* replaced = Find(all, 0x031);
  ASSERT_NE(replaced, nullptr);
  EXPECT_EQ(replaced->routine_start, 0x01B53Cu);
  EXPECT_TRUE(replaced->jumps_to_expanded_code);
  EXPECT_EQ(replaced->jump_target, 0x2C8000u);
  // The routine ends where the next dispatched routine starts.
  EXPECT_EQ(replaced->routine_end, 0x01B541u);

  const auto* plain = Find(all, 0x032);
  ASSERT_NE(plain, nullptr);
  EXPECT_FALSE(plain->jumps_to_expanded_code);
  // Nothing is dispatched after it, so the estimate caps at 256 bytes.
  EXPECT_EQ(plain->routine_end, 0x01B641u);

  const auto* subtype3 = Find(all, 0xF81);
  ASSERT_NE(subtype3, nullptr);
  EXPECT_EQ(subtype3->routine_start, 0x019000u);
  EXPECT_FALSE(subtype3->jumps_to_expanded_code);

  // A JSL that stays inside the original 1 MB is vanilla code, not a hack.
  data[stub + 3] = 0x01;
  Rom vanilla_style;
  ASSERT_TRUE(vanilla_style.LoadFromData(data).ok());
  const auto* same = Find(ReadObjectDrawCode(vanilla_style), 0x031);
  ASSERT_NE(same, nullptr);
  EXPECT_FALSE(same->jumps_to_expanded_code);
}

}  // namespace
}  // namespace yaze::zelda3
