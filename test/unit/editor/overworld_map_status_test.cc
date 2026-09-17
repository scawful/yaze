#include "app/editor/overworld/overworld_map_status.h"

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(OverworldMapStatusTest, FallsBackToSelectedWhenNotHovering) {
  EXPECT_EQ(FormatOverworldMapStatusSegment(0x05, -1), "LW #05");
  EXPECT_EQ(FormatOverworldMapStatusSegment(0x45, -1), "DW #45");
}

TEST(OverworldMapStatusTest, SameMapHoverUsesSelectedForm) {
  EXPECT_EQ(FormatOverworldMapStatusSegment(0x05, 0x05), "LW #05");
}

TEST(OverworldMapStatusTest, DistinctHoverSameWorldShowsBoth) {
  EXPECT_EQ(FormatOverworldMapStatusSegment(0x05, 0x12), "LW #12 · sel #05");
}

TEST(OverworldMapStatusTest, DistinctHoverCrossWorldShowsBothWorlds) {
  EXPECT_EQ(FormatOverworldMapStatusSegment(0x05, 0x45), "DW #45 · sel LW #05");
}

}  // namespace
}  // namespace yaze::editor
