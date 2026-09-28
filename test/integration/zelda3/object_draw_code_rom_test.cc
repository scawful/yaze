// The vanilla game never dispatches an object draw routine into expanded
// banks, so the "your ROM changed this object's draw code" marker in the
// Object Coverage window must stay silent on a vanilla ROM.

#include "zelda3/dungeon/object_draw_code.h"

#include <memory>

#include <gtest/gtest.h>

#include "app/editor/dungeon/object_coverage_model.h"
#include "rom/rom.h"
#include "test_utils.h"

namespace yaze::test {
namespace {

TEST(ObjectDrawCodeRomTest, VanillaRoutinesStayInsideTheOriginalRom) {
  YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla, "ObjectDrawCodeRomTest");
  auto rom = std::make_unique<Rom>();
  ASSERT_TRUE(
      rom->LoadFromFile(TestRomManager::GetRomPath(RomRole::kVanilla)).ok());

  const auto draw_code = zelda3::ReadObjectDrawCode(*rom);
  ASSERT_EQ(draw_code.size(), 0xF8u + 0x40u + 0x80u);
  for (const auto& code : draw_code) {
    EXPECT_FALSE(code.jumps_to_expanded_code)
        << "object " << std::hex << code.object_id;
    EXPECT_EQ(code.routine_start >> 16, 1u);
    EXPECT_GT(code.routine_end, code.routine_start);
  }
  // With no hack manifest, nothing is reported as custom.
  EXPECT_TRUE(editor::FindObjectsWithCustomDrawCode(draw_code, {}).empty());
}

}  // namespace
}  // namespace yaze::test
