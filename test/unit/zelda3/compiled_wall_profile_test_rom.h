#ifndef YAZE_TEST_UNIT_ZELDA3_COMPILED_WALL_PROFILE_TEST_ROM_H_
#define YAZE_TEST_UNIT_ZELDA3_COMPILED_WALL_PROFILE_TEST_ROM_H_
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
namespace yaze::zelda3::test {
inline std::vector<uint8_t> CompiledWallProfileRom() {
  std::vector<uint8_t> b(0x240000, 0);
  const auto pc = [](uint32_t a) {
    return (a >> 16) * 0x8000 + (a & 0x7FFF);
  };
  const auto put = [&](uint32_t a, uint32_t v, int n) {
    for (int i = 0; i < n; ++i)
      b[pc(a) + i] = (v >> (8 * i)) & 255;
  };
  put(0x00E636, 0x44E7005C, 4);
  put(0x44E700, 0x0880, 2);
  std::copy_n("WKPM", 4, b.begin() + pc(0x44E702));
  put(0x44E706, 1, 1);
  put(0x44E707, 0x44E600, 3);
  std::copy_n("WSR1", 4, b.begin() + pc(0x44E600));
  put(0x44E604, 0x128, 2);
  put(0x44E606, 30, 1);
  put(0x44E607, 1, 1);
  put(0x44E608, 0x17, 1);
  put(0x44E609, 1, 1);
  const std::array<uint32_t, 10> ptr = {0x44E000, 0x44E200, 0x44E300, 0x44E340,
                                        0x44E350, 0x44E390, 0x44E400, 0x44E440,
                                        0x44E450, 0x44E500};
  for (int i = 0; i < 10; ++i)
    put(0x44E60A + i * 3, ptr[i], 3);
  put(0x44E0BD, 1, 1);
  std::fill_n(b.begin() + pc(0x44E200), 0xF8, 255);
  std::fill_n(b.begin() + pc(0x44E300), 0x40, 255);
  std::fill_n(b.begin() + pc(0x44E350), 60, 255);
  std::fill_n(b.begin() + pc(0x44E390), 60, 255);
  put(0x44E201, 0, 1);
  put(0x44E350, 14, 2);
  put(0x44E390, 14, 2);
  for (int i = 0; i < 8; ++i)
    put(0x44E400 + i * 2, 0x1580 + (i & 1), 2);
  put(0x44E440, 0x17, 1);
  put(0x44E450, 0x26, 1);
  put(0x44E500, 0x0101, 2);
  return b;
}
}  // namespace yaze::zelda3::test
#endif
