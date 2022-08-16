#include <asardll.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "app/asm/script.h"
#include "app/core/constants.h"
#include "app/rom.h"

namespace yaze_test {
namespace asm_test {

using yaze::app::ROM;
using yaze::app::snes_asm::Script;

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Return;

class MockScript : public Script {
 public:
  MOCK_METHOD(absl::Status, ApplyPatchToROM, (ROM & rom));
  MOCK_METHOD(absl::Status, GenerateMosaicChangeAssembly,
              (ROM & rom, char mosaic_tiles[yaze::app::core::kNumOverworldMaps],
               int routine_offset, int hook_offset));
};

TEST(ASMTest, ApplyMosaicChangePatchOk) {
  ROM rom;
  MockScript script;
  char mosaic_tiles[yaze::app::core::kNumOverworldMaps];

  EXPECT_CALL(script, GenerateMosaicChangeAssembly(_, Eq(mosaic_tiles),
                                                   Eq(0x1301D0 + 0x138000), 0))
      .WillOnce(Return(absl::OkStatus()));

  EXPECT_CALL(script, ApplyPatchToROM(_)).WillOnce(Return(absl::OkStatus()));

  EXPECT_THAT(script.GenerateMosaicChangeAssembly(rom, mosaic_tiles,
                                                  0x1301D0 + 0x138000, 0),
              absl::OkStatus());
  EXPECT_THAT(script.ApplyPatchToROM(rom), absl::OkStatus());
}

TEST(ASMTest, NoPatchLoadedError) {
  ROM rom;
  MockScript script;
  EXPECT_CALL(script, ApplyPatchToROM(_))
      .WillOnce(Return(absl::InvalidArgumentError("No patch loaded!")));

  EXPECT_THAT(script.ApplyPatchToROM(rom),
              absl::InvalidArgumentError("No patch loaded!"));
}

}  // namespace asm_test
}  // namespace yaze_test