#include "app/editor/system/emulator_runtime_policy.h"

#include <gtest/gtest.h>

#include <vector>

namespace yaze::editor {
namespace {

TEST(EmulatorRuntimePolicyTest, HiddenTickRequiresExplicitBackgroundPref) {
  EXPECT_FALSE(ShouldTickEmulatorWhenHidden(false));
  EXPECT_TRUE(ShouldTickEmulatorWhenHidden(true));
}

TEST(EmulatorRuntimePolicyTest, PreferStartupCategoryHonorsSavedEmulator) {
  const std::vector<std::string> cats = {"Overworld", "Dungeon", "Emulator"};
  EXPECT_EQ(PreferStartupCategory("Emulator", cats), "Emulator");
  EXPECT_EQ(PreferStartupCategory("Dungeon", cats), "Dungeon");
}

TEST(EmulatorRuntimePolicyTest, PreferStartupCategoryNeverDefaultsToEmulator) {
  const std::vector<std::string> only_emu = {"Emulator"};
  EXPECT_TRUE(PreferStartupCategory("", only_emu).empty());

  const std::vector<std::string> cats = {"Emulator", "Overworld"};
  EXPECT_EQ(PreferStartupCategory("", cats), "Overworld");
  EXPECT_EQ(PreferStartupCategory("Missing", cats), "Overworld");
}

}  // namespace
}  // namespace yaze::editor
