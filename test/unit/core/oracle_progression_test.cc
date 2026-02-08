#include "core/oracle_progression.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::core {

TEST(OracleProgressionStateTest, ParsesSRAMAndCrystalMapping) {
  std::vector<uint8_t> sram(0x400, 0);
  sram[OracleProgressionState::kCrystalOffset] =
      OracleProgressionState::kCrystalD1 | OracleProgressionState::kCrystalD4;
  sram[OracleProgressionState::kGameStateOffset] = 2;
  sram[OracleProgressionState::kOosProgOffset] = 0xAA;
  sram[OracleProgressionState::kOosProg2Offset] = 0xBB;
  sram[OracleProgressionState::kSideQuestOffset] = 0xCC;
  sram[OracleProgressionState::kPendantOffset] = 0xDD;

  const auto state =
      OracleProgressionState::ParseFromSRAM(sram.data(), sram.size());
  EXPECT_EQ(state.crystal_bitfield,
            OracleProgressionState::kCrystalD1 |
                OracleProgressionState::kCrystalD4);
  EXPECT_EQ(state.game_state, 2);
  EXPECT_EQ(state.oosprog, 0xAA);
  EXPECT_EQ(state.oosprog2, 0xBB);
  EXPECT_EQ(state.side_quest, 0xCC);
  EXPECT_EQ(state.pendants, 0xDD);

  EXPECT_EQ(state.GetCrystalCount(), 2);
  EXPECT_TRUE(state.IsDungeonComplete(1));
  EXPECT_TRUE(state.IsDungeonComplete(4));
  EXPECT_FALSE(state.IsDungeonComplete(2));
  EXPECT_FALSE(state.IsDungeonComplete(3));
  EXPECT_EQ(state.GetGameStateName(), "Kydrog Complete");
}

}  // namespace yaze::core

