#include "app/emu/input/input_backend.h"

#include <gtest/gtest.h>

#include "app/platform/sdl_compat.h"

namespace yaze::emu::input {
namespace {

TEST(InputBackendTest, ControllerStateTracksButtons) {
  ControllerState state;

  EXPECT_FALSE(state.IsPressed(SnesButton::A));
  state.SetButton(SnesButton::A, true);
  EXPECT_TRUE(state.IsPressed(SnesButton::A));

  state.SetButton(SnesButton::A, false);
  EXPECT_FALSE(state.IsPressed(SnesButton::A));
}

TEST(InputBackendTest, ApplyDefaultKeyBindingsFillsUnsetKeys) {
  InputConfig config;
  config.key_a = 1234;

  ApplyDefaultKeyBindings(config);

  EXPECT_EQ(config.key_a, 1234);
  EXPECT_EQ(config.key_b, platform::kKeyZ);
  EXPECT_EQ(config.key_x, platform::kKeyS);
  EXPECT_EQ(config.key_y, platform::kKeyA);
  EXPECT_EQ(config.key_l, platform::kKeyD);
  EXPECT_EQ(config.key_r, platform::kKeyC);
  EXPECT_EQ(config.key_start, platform::kKeyReturn);
  EXPECT_EQ(config.key_select, platform::kKeyRShift);
  EXPECT_EQ(config.key_up, platform::kKeyUp);
  EXPECT_EQ(config.key_down, platform::kKeyDown);
  EXPECT_EQ(config.key_left, platform::kKeyLeft);
  EXPECT_EQ(config.key_right, platform::kKeyRight);
}

}  // namespace
}  // namespace yaze::emu::input
