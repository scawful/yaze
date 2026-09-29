#ifndef YAZE_TEST_UNIT_ZELDA3_OVERWORLD_ALLOCATION_TEST_ROM_H
#define YAZE_TEST_UNIT_ZELDA3_OVERWORLD_ALLOCATION_TEST_ROM_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace yaze::test {

// Synthetic ROMs for overworld-allocation tests. No bytes come from a real
// ROM: map streams, tables and entities are written here.
//
// Vanilla-shaped: 1 MB, ASM version byte absent (reads 0xFF).
// ZS v3-shaped: 2 MB, ASM version 3 at $28:8145, Pool parent/overlay/sprite
// tables, vanilla map-loader operands.
//
// Areas $00-$7F and the special-world IDs in `kSpecialContentAreas` get a
// map with 256 distinct tile32; every other ID $00-$9F gets a blank map.
inline constexpr uint32_t kTestBlankStreamPc = 0x058000;
inline constexpr uint32_t kTestContentLowStreamPc = 0x058010;
inline const std::set<int> kSpecialContentAreas = {0x80, 0x81, 0x82,
                                                   0x88, 0x89, 0x8A};

inline uint32_t TestPcToSnes(uint32_t pc) {
  return ((pc << 1) & 0x7F0000) | (pc & 0x7FFF) | 0x8000;
}

inline void TestWriteWord(std::vector<uint8_t>& d, uint32_t pc,
                          uint16_t value) {
  d[pc] = static_cast<uint8_t>(value & 0xFF);
  d[pc + 1] = static_cast<uint8_t>(value >> 8);
}

inline void TestWriteLong(std::vector<uint8_t>& d, uint32_t pc,
                          uint32_t value) {
  d[pc] = static_cast<uint8_t>(value & 0xFF);
  d[pc + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  d[pc + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
}

// LDA.l $02F94D,X (and friends) at the eight sites TailMapExpansion patches.
inline void TestWriteVanillaMapLoaderOperands(std::vector<uint8_t>& d) {
  const std::array<std::pair<uint32_t, uint32_t>, 8> operands = {{
      {0x1759E, 0x02F94D},
      {0x175A4, 0x02F94E},
      {0x175C9, 0x02FB2D},
      {0x175CF, 0x02FB2E},
      {0x177E4, 0x02F94D},
      {0x177EA, 0x02F94E},
      {0x1780F, 0x02FB2D},
      {0x17815, 0x02FB2E},
  }};
  for (const auto& [pc, target] : operands) {
    d[pc - 1] = 0xBF;
    TestWriteLong(d, pc, target);
  }
}

inline std::vector<uint8_t> BuildOverworldAllocationTestRom(bool zs_v3) {
  std::vector<uint8_t> d(zs_v3 ? 0x200000 : 0x100000, 0x00);

  // Map streams: 256 x $00 (byte fill), and 256 bytes counting 0..255.
  const std::array<uint8_t, 4> blank = {0xE4, 0xFF, 0x00, 0xFF};
  const std::array<uint8_t, 4> counting = {0xEC, 0xFF, 0x00, 0xFF};
  std::copy(blank.begin(), blank.end(), d.begin() + kTestBlankStreamPc);
  std::copy(counting.begin(), counting.end(),
            d.begin() + kTestContentLowStreamPc);
  for (int id = 0; id < 0xA0; ++id) {
    const bool content = id < 0x80 || kSpecialContentAreas.count(id) > 0;
    TestWriteLong(d, 0x1794D + 3 * id, TestPcToSnes(kTestBlankStreamPc));
    TestWriteLong(
        d, 0x17B2D + 3 * id,
        TestPcToSnes(content ? kTestContentLowStreamPc : kTestBlankStreamPc));
  }

  // Entities: vanilla entrance tables, every slot deleted.
  d[0xDB895] = 0xB8;
  for (int i = 0; i < 129; ++i) {
    TestWriteWord(d, 0xDBA71 + 2 * i, 0xFFFF);
  }
  for (int i = 0; i < 0x13; ++i) {
    TestWriteWord(d, 0xDB826 + 2 * i, 0xFFFF);  // Hole area: no area.
  }
  for (int i = 0; i < 0x4F; ++i) {
    TestWriteWord(d, 0x16051 + 2 * i, 0xFFFF);  // Exit player Y.
    TestWriteWord(d, 0x160EF + 2 * i, 0xFFFF);  // Exit player X.
  }
  // One entrance on $81 and one exit to $80.
  TestWriteWord(d, 0xDB96F, 0x0081);
  TestWriteWord(d, 0xDBA71, 0x0100);
  d[0x15E28] = 0x80;
  TestWriteWord(d, 0x16051, 0x0000);
  TestWriteWord(d, 0x160EF, 0x0000);

  // Light-world large area $00/$01/$08/$09.
  if (zs_v3) {
    d[0x140145] = 0x03;
    for (int id = 0; id < 0xA0; ++id) {
      d[0x140998 + id] = static_cast<uint8_t>(id);
      TestWriteWord(d, 0x140340 + 2 * id, 0x00FF);
    }
    for (int id : {0x01, 0x08, 0x09}) {
      d[0x140998 + id] = 0x00;
    }
    for (int id : {0x82, 0x89, 0x8A}) {
      d[0x140998 + id] = 0x81;
    }
    for (int id : {0x00, 0x01, 0x08, 0x09, 0x81, 0x82, 0x89, 0x8A}) {
      d[0x1788D + id] = 0x01;
    }
    // $80's fog overlay is $97. A child's entry ($82 -> $9B) is never read.
    TestWriteWord(d, 0x140340 + 2 * 0x80, 0x0097);
    TestWriteWord(d, 0x140340 + 2 * 0x82, 0x009B);
    // Sprites: every list is the empty list at $09:CB41, except $83 phase 0.
    d[0x4CB41] = 0xFF;
    const std::array<uint8_t, 7> list = {0x10, 0x10, 0x01, 0x12,
                                         0x14, 0x02, 0xFF};
    std::copy(list.begin(), list.end(), d.begin() + 0x4CB50);
    for (uint32_t table : {0x141438u, 0x141578u, 0x1416B8u}) {
      for (int id = 0; id < 0xA0; ++id) {
        TestWriteWord(d, table + 2 * id, 0xCB41);
      }
    }
    TestWriteWord(d, 0x141438 + 2 * 0x83, 0xCB50);
    // ZS function space: code from $28:9940 to $28:9950.
    for (uint32_t pc = 0x141940; pc <= 0x141950; ++pc) {
      d[pc] = 0xEA;
    }
    TestWriteVanillaMapLoaderOperands(d);
  } else {
    // Vanilla: parent table covers the light world; 0 = large, 1 = small.
    for (int id = 0; id < 0x40; ++id) {
      d[0x125EC + id] = static_cast<uint8_t>(id);
      d[0x1788D + id] = 0x01;
      d[0x1788D + 0x40 + id] = 0x01;
    }
    for (int id : {0x01, 0x08, 0x09}) {
      d[0x125EC + id] = 0x00;
    }
    for (int id : {0x00, 0x01, 0x08, 0x09}) {
      d[0x1788D + id] = 0x00;
      d[0x1788D + 0x40 + id] = 0x00;
    }
    TestWriteVanillaMapLoaderOperands(d);
  }
  return d;
}

}  // namespace yaze::test

#endif  // YAZE_TEST_UNIT_ZELDA3_OVERWORLD_ALLOCATION_TEST_ROM_H
