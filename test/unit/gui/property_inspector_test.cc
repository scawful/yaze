#include "app/gui/widgets/property_inspector.h"

#include <cstdint>
#include <functional>
#include <string>

#include <gtest/gtest.h>

#include "app/gui/core/color.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace {

class PropertyInspectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);
    ImGui::NewFrame();
  }

  void TearDown() override {
    ImGui::EndFrame();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(PropertyInspectorTest, IsInPropertyTableFalseOutsideTable) {
  EXPECT_FALSE(property_inspector_internal::IsInPropertyTable());
}

TEST_F(PropertyInspectorTest, ResolveWidgetIdOutsideTableIsPlain) {
  EXPECT_EQ(property_inspector_internal::ResolveWidgetId("Retention"),
            "Retention");
}

TEST_F(PropertyInspectorTest, ResolveWidgetIdInsideTableHidesLabel) {
  ASSERT_TRUE(ImGui::Begin("root"));
  ASSERT_TRUE(ImGui::BeginTable("grid", 2));
  ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120);
  ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableNextRow();

  EXPECT_TRUE(property_inspector_internal::IsInPropertyTable());
  EXPECT_EQ(property_inspector_internal::ResolveWidgetId("Retention"),
            "##Retention");

  ImGui::EndTable();
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyBoolWithoutInputReturnsFalse) {
  ASSERT_TRUE(ImGui::Begin("root"));
  bool value = true;
  EXPECT_FALSE(DrawProperty("Enabled", &value));
  EXPECT_TRUE(value);
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyReadOnlyReturnsFalse) {
  ASSERT_TRUE(ImGui::Begin("root"));
  bool value = true;
  EXPECT_FALSE(DrawProperty("Enabled", &value, {.read_only = true}));
  EXPECT_TRUE(value);
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyIntRendersWithBounds) {
  ASSERT_TRUE(ImGui::Begin("root"));
  int value = 42;
  EXPECT_FALSE(DrawProperty("Count", &value, {.min = 1, .max = 365}));
  EXPECT_EQ(value, 42);
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyFloatPreservesValue) {
  ASSERT_TRUE(ImGui::Begin("root"));
  float value = 3.14f;
  EXPECT_FALSE(DrawProperty("Pi", &value));
  EXPECT_FLOAT_EQ(value, 3.14f);
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyDoublePreservesValue) {
  ASSERT_TRUE(ImGui::Begin("root"));
  double value = 2.71828;
  EXPECT_FALSE(DrawProperty("E", &value));
  EXPECT_DOUBLE_EQ(value, 2.71828);
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyStringPreservesContent) {
  ASSERT_TRUE(ImGui::Begin("root"));
  std::string value = "hello world";
  EXPECT_FALSE(DrawProperty("Name", &value));
  EXPECT_EQ(value, "hello world");
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyColorPreservesRgba) {
  ASSERT_TRUE(ImGui::Begin("root"));
  Color c{0.25f, 0.5f, 0.75f, 1.0f};
  EXPECT_FALSE(DrawProperty("Color", &c));
  EXPECT_FLOAT_EQ(c.red, 0.25f);
  EXPECT_FLOAT_EQ(c.green, 0.5f);
  EXPECT_FLOAT_EQ(c.blue, 0.75f);
  EXPECT_FLOAT_EQ(c.alpha, 1.0f);
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyImVec4PreservesRgba) {
  ASSERT_TRUE(ImGui::Begin("root"));
  ImVec4 c{0.1f, 0.2f, 0.3f, 0.4f};
  EXPECT_FALSE(DrawProperty("Color", &c));
  EXPECT_FLOAT_EQ(c.x, 0.1f);
  EXPECT_FLOAT_EQ(c.y, 0.2f);
  EXPECT_FLOAT_EQ(c.z, 0.3f);
  EXPECT_FLOAT_EQ(c.w, 0.4f);
  ImGui::End();
}

TEST_F(PropertyInspectorTest, DrawPropertyHexPreservesBytes) {
  ASSERT_TRUE(ImGui::Begin("root"));
  std::uint8_t a = 0xAB;
  std::uint16_t b = 0xBEEF;
  std::uint32_t c = 0xCAFED00Du;
  EXPECT_FALSE(DrawPropertyHex("A", &a));
  EXPECT_FALSE(DrawPropertyHex("B", &b));
  EXPECT_FALSE(DrawPropertyHex("C", &c));
  EXPECT_EQ(a, 0xAB);
  EXPECT_EQ(b, 0xBEEF);
  EXPECT_EQ(c, 0xCAFED00Du);
  ImGui::End();
}

enum class TestRole { kReader, kWriter, kAdmin };

TEST_F(PropertyInspectorTest, DrawPropertyComboCompilesForEnum) {
  ASSERT_TRUE(ImGui::Begin("root"));
  static constexpr const char* kRoles[] = {"Reader", "Writer", "Admin",
                                           nullptr};
  TestRole role = TestRole::kWriter;
  EXPECT_FALSE(DrawPropertyCombo("Role", &role, kRoles));
  EXPECT_EQ(role, TestRole::kWriter);
  ImGui::End();
}

TEST_F(PropertyInspectorTest, MultiplePropertiesRenderInsideTableWithoutError) {
  ASSERT_TRUE(ImGui::Begin("root"));
  ASSERT_TRUE(ImGui::BeginTable("pgrid", 2));
  ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120);
  ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

  bool enabled = true;
  int count = 42;
  float ratio = 0.5f;
  std::string name = "test";
  std::uint8_t byte = 0x7F;
  Color tint{1.0f, 0.5f, 0.25f, 1.0f};

  DrawProperty("Enabled", &enabled);
  DrawProperty("Count", &count);
  DrawProperty("Ratio", &ratio);
  DrawProperty("Name", &name);
  DrawPropertyHex("Byte", &byte);
  DrawProperty("Tint", &tint);

  ImGui::EndTable();
  ImGui::End();
  SUCCEED();
}

class DeferredScalarInputTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  void RunFrame(const std::function<void(ImGuiIO&)>& inject_events = {}) {
    ImGuiIO& io = ImGui::GetIO();
    if (inject_events) {
      inject_events(io);
    }

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(240.0f, 120.0f), ImGuiCond_Always);
    ImGui::SetNextWindowFocus();
    ImGui::Begin(
        "DeferredScalarHost", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextItemWidth(100.0f);
    committed_ = InputScalarDeferred("Value", ImGuiDataType_S32, &value_, "%d",
                                     0, target_identity_);
    const ImVec2 input_min = ImGui::GetItemRectMin();
    const ImVec2 input_max = ImGui::GetItemRectMax();
    input_center_ = ImVec2((input_min.x + input_max.x) * 0.5f,
                           (input_min.y + input_max.y) * 0.5f);
    ImGui::End();
    ImGui::EndFrame();
  }

  void ActivateAndType(const char* text) {
    RunFrame();
    RunFrame([&](ImGuiIO& io) {
      io.AddMousePosEvent(input_center_.x, input_center_.y);
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    });
    RunFrame([](ImGuiIO& io) {
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    });
    RunFrame([&](ImGuiIO& io) { io.AddInputCharactersUTF8(text); });
  }

  void RunFrameWithoutInput() {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(240.0f, 120.0f), ImGuiCond_Always);
    ImGui::Begin(
        "DeferredScalarHost", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextUnformatted("Input hidden");
    ImGui::End();
    ImGui::EndFrame();
  }

  ImGuiContext* context_ = nullptr;
  ImVec2 input_center_{};
  int value_ = 42;
  bool committed_ = false;
  InputScalarTargetIdentity target_identity_{1, 0, 0};
};

TEST_F(DeferredScalarInputTest, CommitsOnlyAfterEnterDeactivatesItem) {
  ActivateAndType("99");
  EXPECT_EQ(value_, 42);
  EXPECT_FALSE(committed_);

  RunFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });

  EXPECT_EQ(value_, 99);
  EXPECT_TRUE(committed_);
}

TEST_F(DeferredScalarInputTest, EscapeDiscardsPendingCandidate) {
  ActivateAndType("99");
  EXPECT_EQ(value_, 42);
  EXPECT_FALSE(committed_);

  RunFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Escape, true); });

  EXPECT_EQ(value_, 42);
  EXPECT_FALSE(committed_);
}

TEST_F(DeferredScalarInputTest, DisappearingWidgetCannotCommitStaleCandidate) {
  ActivateAndType("99");
  EXPECT_EQ(value_, 42);

  RunFrameWithoutInput();
  RunFrame();

  EXPECT_EQ(value_, 42);
  EXPECT_FALSE(committed_);
}

TEST_F(DeferredScalarInputTest, ModelChangeCancelsPendingCandidate) {
  ActivateAndType("99");
  EXPECT_EQ(value_, 42);

  value_ = 77;
  RunFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });

  EXPECT_EQ(value_, 77);
  EXPECT_FALSE(committed_);
}

TEST_F(DeferredScalarInputTest,
       EqualValuedSemanticTargetChangeCancelsPendingCandidate) {
  ActivateAndType("99");
  EXPECT_EQ(value_, 42);

  target_identity_ = {2, 0, 0};
  RunFrame();
  EXPECT_EQ(value_, 42);
  EXPECT_FALSE(committed_);

  RunFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });

  EXPECT_EQ(value_, 42);
  EXPECT_FALSE(committed_);
}

// Drives the hex inputs through real frames so wheel, button, and text input
// reach them the way they do in the editor.
class HexInputTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override {
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  // Events are injected before NewFrame, so they target the item rect
  // recorded on the previous frame.
  void RunFrame(const std::function<void()>& draw,
                const std::function<void(ImGuiIO&)>& inject_events = {},
                bool focus_host = true) {
    ImGuiIO& io = ImGui::GetIO();
    if (inject_events) {
      inject_events(io);
    }

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 120.0f), ImGuiCond_Always);
    if (focus_host) {
      ImGui::SetNextWindowFocus();
    }
    ImGui::Begin(
        "HexInputHost", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    draw();
    item_min_ = ImGui::GetItemRectMin();
    item_max_ = ImGui::GetItemRectMax();
    frame_height_ = ImGui::GetFrameHeight();
    ImGui::End();
    ImGui::EndFrame();
  }

  float CenterY() const { return (item_min_.y + item_max_.y) * 0.5f; }

  void MoveMouse(const std::function<void()>& draw, float x, float y) {
    RunFrame(draw, [=](ImGuiIO& io) { io.AddMousePosEvent(x, y); });
  }

  void Click(const std::function<void()>& draw, float x, float y) {
    MoveMouse(draw, x, y);
    RunFrame(draw, [](ImGuiIO& io) {
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    });
    RunFrame(draw, [](ImGuiIO& io) {
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    });
  }

  void WheelBehindPopup(const std::function<void()>& draw_input,
                        ImGuiKey modifier = ImGuiMod_Ctrl) {
    bool show_popup = false;
    bool popup_open = false;
    bool hovered_normally = false;
    bool hovered_allowing_popup = false;
    auto draw = [&] {
      draw_input();
      hovered_normally = ImGui::IsItemHovered();
      hovered_allowing_popup =
          ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
      if (show_popup) {
        if (!ImGui::IsPopupOpen("WheelBlocker")) {
          ImGui::OpenPopup("WheelBlocker");
        }
        // Keep the popup away from the input: popup blocking, rather than
        // another window covering the item, must reject the wheel.
        ImGui::SetNextWindowPos(ImVec2(380.0f, 20.0f), ImGuiCond_Always);
        popup_open = ImGui::BeginPopup("WheelBlocker");
        if (popup_open) {
          ImGui::TextUnformatted("Popup blocks the underlying input");
          ImGui::EndPopup();
        }
      }
    };
    RunFrame(draw);
    MoveMouse(draw, (item_min_.x + item_max_.x) * 0.5f, CenterY());
    show_popup = true;
    RunFrame(draw);
    // Refocusing the host would close its popup before the wheel arrives.
    RunFrame(
        draw, [modifier](ImGuiIO& io) { io.AddKeyEvent(modifier, true); },
        /*focus_host=*/false);
    RunFrame(
        draw, [](ImGuiIO& io) { io.AddMouseWheelEvent(0.0f, 1.0f); },
        /*focus_host=*/false);
    RunFrame(draw, {}, /*focus_host=*/false);

    ASSERT_TRUE(popup_open);
    EXPECT_FALSE(hovered_normally);
    EXPECT_TRUE(hovered_allowing_popup);
  }

  ImGuiContext* context_ = nullptr;
  ImVec2 item_min_{};
  ImVec2 item_max_{};
  float frame_height_ = 0.0f;
};

// Hovering is not enough: a plain wheel over a field scrolls the panel.
// Callers apply on ShouldApply(), so a reported no-op change would reach
// them on every scroll tick.
TEST_F(HexInputTest, PlainWheelOverWordFieldReportsNoChange) {
  uint16_t value = 0x0010;
  bool reported = false;
  auto draw = [&] {
    reported |= InputHexWordEx("##Room", &value, 80.0f, true).changed;
  };
  RunFrame(draw);
  MoveMouse(draw, (item_min_.x + item_max_.x) * 0.5f, CenterY());
  RunFrame(draw, [](ImGuiIO& io) { io.AddMouseWheelEvent(0.0f, 1.0f); });
  RunFrame(draw);

  EXPECT_EQ(value, 0x0010);
  EXPECT_FALSE(reported);
}

TEST_F(HexInputTest, CtrlWheelStepsWordByOneAsImmediateChange) {
  uint16_t value = 0x0010;
  bool should_apply = false;
  auto draw = [&] {
    should_apply |= InputHexWordEx("##Room", &value, 80.0f, true).ShouldApply();
  };
  RunFrame(draw);
  MoveMouse(draw, (item_min_.x + item_max_.x) * 0.5f, CenterY());
  RunFrame(draw, [](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, true); });
  RunFrame(draw, [](ImGuiIO& io) { io.AddMouseWheelEvent(0.0f, 1.0f); });
  RunFrame(draw);

  EXPECT_EQ(value, 0x0011);
  EXPECT_TRUE(should_apply);
}

TEST_F(HexInputTest, CtrlWheelBehindPopupDoesNotChangeByteField) {
  uint8_t value = 0x10;
  bool reported = false;
  WheelBehindPopup([&] {
    reported |= InputHexByteEx("##Room", &value, 80.0f, true).ShouldApply();
  });

  EXPECT_EQ(value, 0x10);
  EXPECT_FALSE(reported);
}

TEST_F(HexInputTest, SuperWheelBehindPopupDoesNotChangeClampedByteField) {
  uint8_t value = 0x10;
  bool reported = false;
  WheelBehindPopup(
      [&] {
        reported |= InputHexByteEx("##Room", &value, uint8_t{0x3F}, 80.0f, true)
                        .ShouldApply();
      },
      ImGuiMod_Super);

  EXPECT_EQ(value, 0x10);
  EXPECT_FALSE(reported);
}

TEST_F(HexInputTest, CtrlWheelBehindPopupDoesNotChangeWordField) {
  uint16_t value = 0x0010;
  bool reported = false;
  WheelBehindPopup([&] {
    reported |= InputHexWordEx("##Room", &value, 80.0f, true).ShouldApply();
  });

  EXPECT_EQ(value, 0x0010);
  EXPECT_FALSE(reported);
}

// Regular hex inputs allowed popup-blocked hover before the upgrade. Keep
// that separate contract while restoring the extended inputs' stricter one.
TEST_F(HexInputTest, RegularWordFieldKeepsLegacyWheelBehaviorBehindPopup) {
  uint16_t value = 0x0010;
  bool reported = false;
  WheelBehindPopup(
      [&] { reported |= InputHexWord("##Room", &value, 80.0f, true); });

  EXPECT_EQ(value, 0x0011);
  EXPECT_TRUE(reported);
}

TEST_F(HexInputTest, U64StepButtonAddsOne) {
  uint64_t value = 0;
  auto draw = [&] {
    InputHex("##Offset", &value);
  };
  RunFrame(draw);
  // With a hidden label the group ends at the "+" button.
  Click(draw, item_max_.x - frame_height_ * 0.5f, CenterY());
  RunFrame(draw);

  EXPECT_EQ(value, 1u);
}

TEST_F(HexInputTest, U64TextEntryKeepsUpperBits) {
  uint64_t value = 0;
  auto draw = [&] {
    InputHex("##Offset", &value);
  };
  RunFrame(draw);
  Click(draw, item_min_.x + 8.0f, CenterY());
  RunFrame(draw, [](ImGuiIO& io) { io.AddInputCharactersUTF8("123456789A"); });
  RunFrame(draw, [](ImGuiIO& io) { io.AddKeyEvent(ImGuiKey_Enter, true); });
  RunFrame(draw);

  EXPECT_EQ(value, 0x123456789Aull);
}

}  // namespace
}  // namespace gui
}  // namespace yaze
