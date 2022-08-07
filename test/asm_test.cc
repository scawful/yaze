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
using ::testing::Return;
using ::testing::TypedEq;

class MockScript : public Script {
 public:
  MOCK_METHOD(absl::Status, ApplyPatchToROM, (ROM & rom));
};

TEST(ASMTest, NoPatchLoadedError) {
  ROM rom;
  MockScript script;
  EXPECT_CALL(script, ApplyPatchToROM(_))
      .WillOnce(Return(absl::InvalidArgumentError("No patch loaded!")));

  EXPECT_THAT(script.ApplyPatchToROM(rom),
              absl::InvalidArgumentError("No patch loaded!"));
}

TEST(ASMTest, ApplyPatchOk) {}

}  // namespace asm_test
}  // namespace yaze_test