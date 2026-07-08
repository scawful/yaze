#include "app/gui/widgets/font_picker.h"

#include <cstring>
#include <string_view>

#include <gtest/gtest.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace {

class FontPickerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;

    // Register a few fonts with named configs to test name resolution.
    ImFontConfig cfg_a;
    std::strncpy(cfg_a.Name, "TestAlpha", sizeof(cfg_a.Name) - 1);
    io.Fonts->AddFontDefault(&cfg_a);

    ImFontConfig cfg_b;
    std::strncpy(cfg_b.Name, "TestBeta", sizeof(cfg_b.Name) - 1);
    io.Fonts->AddFontDefault(&cfg_b);

    // Third font without a name — should fall back to "Font #2".
    io.Fonts->AddFontDefault();

    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    ImGui::NewFrame();
  }

  void TearDown() override {
    ImGui::EndFrame();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(FontPickerTest, RegisteredFontCountReturnsThree) {
  EXPECT_EQ(font_picker_internal::RegisteredFontCount(), 3);
}

TEST_F(FontPickerTest, FontNameAtReturnsConfiguredName) {
  EXPECT_EQ(std::string_view(font_picker_internal::FontNameAt(0)).substr(0, 9),
            "TestAlpha");
  EXPECT_EQ(std::string_view(font_picker_internal::FontNameAt(1)).substr(0, 8),
            "TestBeta");
}

TEST_F(FontPickerTest, FontNameAtOutOfRangeFallsBack) {
  EXPECT_STREQ(font_picker_internal::FontNameAt(-1), "Font #-1");
  EXPECT_STREQ(font_picker_internal::FontNameAt(999), "Font #999");
}

TEST_F(FontPickerTest, FontPickerPreservesIndexWithoutInput) {
  ASSERT_TRUE(ImGui::Begin("root"));
  int index = 1;
  EXPECT_FALSE(FontPicker("##font", &index));
  EXPECT_EQ(index, 1);
  ImGui::End();
}

TEST_F(FontPickerTest, FontPickerClampsOutOfRangeIndexInPreview) {
  ASSERT_TRUE(ImGui::Begin("root"));
  int index = 99;  // Out of range.
  EXPECT_FALSE(FontPicker("##font", &index));
  // Value pointer is left untouched (no commit happened). The preview text
  // uses index 0 as a fallback; the commit contract doesn't mutate a value
  // the user didn't select.
  EXPECT_EQ(index, 99);
  ImGui::End();
}

TEST_F(FontPickerTest, FontPickerNullIndexReturnsFalse) {
  ASSERT_TRUE(ImGui::Begin("root"));
  EXPECT_FALSE(FontPicker("##font", nullptr));
  ImGui::End();
}

}  // namespace
}  // namespace gui
}  // namespace yaze
