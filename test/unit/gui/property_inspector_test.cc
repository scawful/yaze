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

}  // namespace
}  // namespace gui
}  // namespace yaze
