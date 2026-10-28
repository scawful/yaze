// Pins the dashboard's recent-editors parsing rules.
//
// The dashboard (the editor chooser reached on ROM load and via Ctrl+E) had no
// tests at all, and its recents parser carried two sentinel/bound bugs plus a
// failure mode where one malformed line truncated the rest of the list. These
// rules are cheap to get wrong again, so they are pinned here rather than left
// to a visual check — the parser is deliberately pure and static so none of
// this touches the user's real config directory.

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "app/editor/editor.h"
#include "app/editor/shell/windows/dashboard_panel.h"
#include "app/editor/system/commands/shortcut_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/core/platform_keys.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {

class DashboardPanelTestPeer {
 public:
  static void DrawRoles(DashboardPanel& panel, bool has_rom = true) {
    panel.has_rom_ = has_rom;
    panel.DrawRoleChoices();
  }
  static void DrawHint(DashboardPanel& panel) {
    panel.DrawCommandPaletteHint();
  }
};

namespace {

class DashboardRolePickerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = ImVec2(1000, 800);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  void TearDown() override {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }
  std::string Frame(const std::function<void()>& draw) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_Always);
    ImGui::Begin("RolePickerTest", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    draw();
    const std::string text = context_->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    return text;
  }
  void ClickRole(DashboardPanel& panel, const std::string& label) {
    const auto draw = [&] {
      DashboardPanelTestPeer::DrawRoles(panel);
    };
    Frame(draw);
    Frame(draw);
    const auto widget = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
        "RolePicker/button:" + label);
    ASSERT_TRUE(widget.has_value());
    ASSERT_TRUE(widget->bounds.valid);
    const ImVec2 center((widget->bounds.min_x + widget->bounds.max_x) / 2,
                        (widget->bounds.min_y + widget->bounds.max_y) / 2);
    ImGui::GetIO().AddMousePosEvent(center.x, center.y);
    Frame(draw);
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Frame(draw);
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    Frame(draw);
  }
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
};

TEST_F(DashboardRolePickerTest, EachRoleSelectsItsEditorAndPresetOnce) {
  struct ExpectedRole {
    const char* label;
    EditorType editor;
    const char* preset;
  };
  const std::array<ExpectedRole, 5> roles = {{
      {"Write dialogue", EditorType::kMessage, "Writer"},
      {"Build maps", EditorType::kOverworld, "Overworld Artist"},
      {"Build dungeons", EditorType::kDungeon, "Dungeon Master"},
      {"Edit graphics", EditorType::kGraphics, "Designer"},
      {"Write ASM", EditorType::kAssembly, "Developer"},
  }};
  for (const auto& role : roles) {
    SCOPED_TRACE(role.label);
    DashboardPanel panel(nullptr);
    int calls = 0;
    panel.SetRoleSelectionCallback(
        [&](EditorType editor, const std::string& preset) {
          ++calls;
          EXPECT_EQ(editor, role.editor);
          EXPECT_EQ(preset, role.preset);
          return absl::OkStatus();
        });
    ClickRole(panel, role.label);
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(panel.IsVisible());
  }
}

TEST_F(DashboardRolePickerTest, FailedSelectionKeepsPickerAndShowsError) {
  DashboardPanel panel(nullptr);
  panel.SetRoleSelectionCallback([](EditorType, const std::string&) {
    return absl::FailedPreconditionError("Could not save workspace preference");
  });
  ClickRole(panel, "Write dialogue");
  EXPECT_TRUE(panel.IsVisible());
  const auto text = Frame([&] { DashboardPanelTestPeer::DrawRoles(panel); });
  EXPECT_NE(text.find("Could not save workspace preference"),
            std::string::npos);
}

TEST_F(DashboardRolePickerTest, RomRolesAreDisabledUntilRomIsLoaded) {
  DashboardPanel panel(nullptr);
  Frame([&] { DashboardPanelTestPeer::DrawRoles(panel, false); });
  for (const auto& label : {"Write dialogue", "Build maps", "Build dungeons",
                            "Edit graphics", "Write ASM"}) {
    const auto widget = gui::WidgetIdRegistry::Instance().GetWidgetInfo(
        std::string("RolePicker/button:") + label);
    ASSERT_TRUE(widget.has_value()) << label;
    EXPECT_EQ(widget->enabled, std::string(label) == "Write ASM");
  }
}

TEST_F(DashboardRolePickerTest, PaletteHintUsesLiveBindingAndHandlesUnbound) {
  DashboardPanel panel(nullptr);
  ShortcutManager shortcuts;
  shortcuts.RegisterShortcut(
      "Command Palette", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_P}, [] {});
  panel.SetShortcutManager(&shortcuts);
  const auto draw = [&] {
    DashboardPanelTestPeer::DrawHint(panel);
  };
  EXPECT_NE(Frame(draw).find(shortcuts.GetDisplayString("Command Palette") +
                             ": search every command"),
            std::string::npos);
  ASSERT_TRUE(shortcuts.UpdateShortcutKeys("Command Palette", {ImGuiKey_F7}));
  EXPECT_NE(Frame(draw).find("F7: search every command"), std::string::npos);
  ASSERT_TRUE(shortcuts.UpdateShortcutKeys("Command Palette", {}));
  const auto text = Frame(draw);
  EXPECT_NE(text.find("Command Palette: search every command"),
            std::string::npos);
  EXPECT_EQ(text.find(gui::FormatCtrlShiftShortcut(ImGuiKey_P)),
            std::string::npos);
}

constexpr size_t kMax = 5;

std::vector<EditorType> Parse(const std::string& body, size_t max = kMax) {
  return DashboardPanel::ParseRecentEditors(body, max);
}

std::string Line(EditorType type) {
  return std::to_string(static_cast<int>(type)) + "\n";
}

TEST(DashboardRecentEditorsTest, RoundTripsInFileOrder) {
  const auto parsed =
      Parse(Line(EditorType::kOverworld) + Line(EditorType::kDungeon) +
            Line(EditorType::kGraphics));
  ASSERT_EQ(parsed.size(), 3u);
  EXPECT_EQ(parsed[0], EditorType::kOverworld);
  EXPECT_EQ(parsed[1], EditorType::kDungeon);
  EXPECT_EQ(parsed[2], EditorType::kGraphics);
}

// kUnknown is the sentinel the layout system keys on, not a selectable editor.
// The old lower bound was `type_int >= 0`, which admitted it.
TEST(DashboardRecentEditorsTest, RejectsTheUnknownSentinel) {
  const auto parsed =
      Parse(Line(EditorType::kUnknown) + Line(EditorType::kDungeon));
  ASSERT_EQ(parsed.size(), 1u);
  EXPECT_EQ(parsed[0], EditorType::kDungeon);
}

// kSettings is a real editor. The old upper bound was `type_int < kSettings`,
// which excluded it: Settings could never become a recent no matter how often
// it was opened.
TEST(DashboardRecentEditorsTest, AcceptsTheLastEditorType) {
  for (EditorType type : {EditorType::kSettings, kLastEditorType}) {
    const auto parsed = Parse(Line(type));
    ASSERT_EQ(parsed.size(), 1u);
    EXPECT_EQ(parsed[0], type);
  }
}

TEST(DashboardRecentEditorsTest, RejectsOutOfRangeValues) {
  const std::string body =
      "-1\n999\n" + std::to_string(static_cast<int>(kLastEditorType) + 1) +
      "\n" + Line(EditorType::kMusic);
  const auto parsed = Parse(body);
  ASSERT_EQ(parsed.size(), 1u);
  EXPECT_EQ(parsed[0], EditorType::kMusic);
}

// One bad line used to abandon every entry after it: std::stoi threw and a
// single try/catch around the whole loop swallowed the rest of the file.
TEST(DashboardRecentEditorsTest, AMalformedLineDropsOnlyThatLine) {
  const auto parsed = Parse(Line(EditorType::kOverworld) + "not-a-number\n" +
                            "\n" + Line(EditorType::kSprite));
  ASSERT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0], EditorType::kOverworld);
  EXPECT_EQ(parsed[1], EditorType::kSprite);
}

// Recents are a set. A duplicated line must not consume two of the five slots.
TEST(DashboardRecentEditorsTest, DeduplicatesRepeatedEntries) {
  const auto parsed =
      Parse(Line(EditorType::kDungeon) + Line(EditorType::kDungeon) +
            Line(EditorType::kPalette));
  ASSERT_EQ(parsed.size(), 2u);
  EXPECT_EQ(parsed[0], EditorType::kDungeon);
  EXPECT_EQ(parsed[1], EditorType::kPalette);
}

TEST(DashboardRecentEditorsTest, StopsAtTheEntryCap) {
  const std::string body =
      Line(EditorType::kAssembly) + Line(EditorType::kDungeon) +
      Line(EditorType::kEmulator) + Line(EditorType::kGraphics) +
      Line(EditorType::kMusic) + Line(EditorType::kOverworld);
  const auto parsed = Parse(body, /*max=*/kMax);
  EXPECT_EQ(parsed.size(), kMax);
  EXPECT_EQ(parsed.back(), EditorType::kMusic);
}

TEST(DashboardRecentEditorsTest, EmptyBodyYieldsNoRecents) {
  EXPECT_TRUE(Parse("").empty());
  EXPECT_TRUE(Parse("\n\n\n").empty());
}

}  // namespace
}  // namespace yaze::editor
