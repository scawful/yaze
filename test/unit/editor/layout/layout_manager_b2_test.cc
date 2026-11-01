// B2 layout: one dockspace per editor inside an editor host, plus one shared
// pin node split off the outer dockspace. Frames mirror Controller order:
// outer DockSpace window, then LayoutManager::DrawEditorHost(), then panels.

#include <gtest/gtest.h>

#include <functional>
#include <set>
#include <string>

#include "app/editor/editor.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {
namespace {

constexpr float kWidth = 1280.0f;
constexpr float kHeight = 720.0f;
constexpr char kPaletteId[] = "test.palette";
constexpr char kDungeonPanelId[] = "test.dungeon_objects";

ImGuiID RootOf(const char* window_name) {
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  if (!window || !window->DockNode) {
    return 0;
  }
  return ImGui::DockNodeGetRootNode(window->DockNode)->ID;
}

ImGuiID DockIdOf(const char* window_name) {
  ImGuiWindow* window = ImGui::FindWindowByName(window_name);
  return window ? window->DockId : 0;
}

class LayoutManagerB2Test : public ::testing::Test {
 protected:
  void SetUp() override {
    CreateContext();

    window_manager_.RegisterSession(0);
    window_manager_.SetActiveSession(0);

    WindowDescriptor palette;
    palette.card_id = kPaletteId;
    palette.display_name = "Palette";
    palette.icon = "ICON_DEMO";
    palette.category = "Palette";
    palette.visibility_flag = &palette_visible_;
    palette.priority = 1;
    window_manager_.RegisterWindow(0, palette);

    WindowDescriptor objects = palette;
    objects.card_id = kDungeonPanelId;
    objects.display_name = "Objects";
    objects.category = "Dungeon";
    objects.visibility_flag = &objects_visible_;
    window_manager_.RegisterWindow(0, objects);

    window_manager_.SetActiveCategory("Dungeon", /*notify=*/false);
    layout_manager_.SetWindowManager(&window_manager_);
    layout_manager_.RegisterLayoutIniHandler();

    palette_title_ = window_manager_.GetWorkspaceWindowName(0, kPaletteId);
    objects_title_ = window_manager_.GetWorkspaceWindowName(0, kDungeonPanelId);
  }

  void TearDown() override {
    if (ctx_) {
      ImGui::DestroyContext(ctx_);
      ctx_ = nullptr;
    }
  }

  void CreateContext() {
    ctx_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(kWidth, kHeight);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  std::string SaveIni() {
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    return std::string(data, size);
  }

  // Loads `ini` into a fresh context: the path an app restart takes.
  void RestartWithIni(const std::string& ini) {
    ImGui::DestroyContext(ctx_);
    CreateContext();
    layout_manager_.RegisterLayoutIniHandler();
    ImGui::LoadIniSettingsFromMemory(ini.c_str(), ini.size());
  }

  void RunFrames(int count, const std::function<void()>& panels) {
    for (int i = 0; i < count; ++i) {
      ImGui::NewFrame();
      ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(ImVec2(kWidth, kHeight));
      ImGui::Begin("Test DockSpaceWindow", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                       ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoBringToFrontOnFocus);
      ImGui::DockSpace(LayoutManager::OuterDockspaceId(), ImVec2(0.0f, 0.0f),
                       ImGuiDockNodeFlags_PassthruCentralNode |
                           ImGuiDockNodeFlags_NoDockingSplit);
      ImGui::End();
      layout_manager_.DrawEditorHost();
      panels();
      ImGui::Render();
    }
  }

  // First use docks into `dockspace_id`; later frames keep wherever the
  // window was moved to.
  static void SubmitPanel(const std::string& title, ImGuiID dockspace_id) {
    ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
    ImGui::Begin(title.c_str());
    ImGui::End();
  }

  // Dungeon is active and the pinned palette has moved to the pin node.
  void BuildPinnedPalette() {
    layout_manager_.SetActiveEditorType(EditorType::kDungeon);
    const ImGuiID dungeon =
        LayoutManager::EditorDockspaceId(EditorType::kDungeon);
    const auto panels = [&] {
      SubmitPanel(palette_title_, dungeon);
      SubmitPanel(objects_title_, dungeon);
    };
    RunFrames(2, panels);
    ASSERT_EQ(RootOf(palette_title_.c_str()), dungeon);
    ASSERT_EQ(layout_manager_.FindPinNode(), 0u);

    window_manager_.SetWindowPinned(0, kPaletteId, true);
    RunFrames(3, panels);
  }

  ImGuiContext* ctx_ = nullptr;
  WorkspaceWindowManager window_manager_;
  LayoutManager layout_manager_;
  bool palette_visible_ = true;
  bool objects_visible_ = true;
  std::string palette_title_;
  std::string objects_title_;
};

TEST(LayoutManagerB2IdsTest, DockspaceIdsAreStableAndDistinct) {
  EXPECT_EQ(LayoutManager::OuterDockspaceId(), ImHashStr("yaze.MainDockSpace"));
  EXPECT_EQ(LayoutManager::EditorDockspaceId(EditorType::kDungeon),
            ImHashStr("yaze.EditorDock/Dungeon"));

  std::set<ImGuiID> ids = {LayoutManager::OuterDockspaceId()};
  for (size_t index = 1; index < kEditorTypeCount; ++index) {
    ids.insert(
        LayoutManager::EditorDockspaceId(static_cast<EditorType>(index)));
  }
  EXPECT_EQ(ids.size(), kEditorTypeCount);
}

TEST_F(LayoutManagerB2Test, MainDockspaceIdFollowsActiveEditor) {
  layout_manager_.SetMainDockspaceId(LayoutManager::OuterDockspaceId());
  EXPECT_EQ(layout_manager_.GetMainDockspaceId(),
            LayoutManager::OuterDockspaceId());

  layout_manager_.SetActiveEditorType(EditorType::kOverworld);
  EXPECT_EQ(layout_manager_.GetMainDockspaceId(),
            LayoutManager::EditorDockspaceId(EditorType::kOverworld));
}

TEST_F(LayoutManagerB2Test, PinnedPanelMovesToSharedPinNodeAndStaysOnSwitch) {
  BuildPinnedPalette();
  const ImGuiID pin = layout_manager_.FindPinNode();
  ASSERT_NE(pin, 0u);
  EXPECT_EQ(DockIdOf(palette_title_.c_str()), pin);
  EXPECT_EQ(RootOf(palette_title_.c_str()), LayoutManager::OuterDockspaceId());

  // Overworld becomes active; only the pinned palette is still drawn.
  const ImGuiID dungeon =
      LayoutManager::EditorDockspaceId(EditorType::kDungeon);
  layout_manager_.SetActiveEditorType(EditorType::kOverworld);
  window_manager_.SetActiveCategory("Overworld", /*notify=*/false);
  RunFrames(3, [&] { SubmitPanel(palette_title_, dungeon); });

  EXPECT_EQ(layout_manager_.FindPinNode(), pin);
  EXPECT_EQ(DockIdOf(palette_title_.c_str()), pin);
  EXPECT_NE(ImGui::DockBuilderGetNode(dungeon), nullptr);
  EXPECT_NE(ImGui::DockBuilderGetNode(
                LayoutManager::EditorDockspaceId(EditorType::kOverworld)),
            nullptr);

  // Back to Dungeon: its undrawn panel still belongs to the Dungeon tree.
  layout_manager_.SetActiveEditorType(EditorType::kDungeon);
  window_manager_.SetActiveCategory("Dungeon", /*notify=*/false);
  RunFrames(2, [&] {
    SubmitPanel(palette_title_, dungeon);
    SubmitPanel(objects_title_, dungeon);
  });
  EXPECT_EQ(RootOf(objects_title_.c_str()), dungeon);
  EXPECT_EQ(DockIdOf(palette_title_.c_str()), pin);
}

TEST_F(LayoutManagerB2Test, UnpinnedPanelDroppedOnPinNodeReturnsHome) {
  BuildPinnedPalette();
  const ImGuiID pin = layout_manager_.FindPinNode();
  ASSERT_NE(pin, 0u);

  // Stands in for a user drag of an unpinned Dungeon panel onto the pin node.
  ImGui::DockBuilderDockWindow(objects_title_.c_str(), pin);
  const ImGuiID dungeon =
      LayoutManager::EditorDockspaceId(EditorType::kDungeon);
  RunFrames(3, [&] {
    SubmitPanel(palette_title_, dungeon);
    SubmitPanel(objects_title_, dungeon);
  });

  EXPECT_EQ(RootOf(objects_title_.c_str()), dungeon);
  EXPECT_EQ(DockIdOf(palette_title_.c_str()), pin);
}

TEST_F(LayoutManagerB2Test, IniRoundTripKeepsInitializedEditorTrees) {
  layout_manager_.SetActiveEditorType(EditorType::kDungeon);
  layout_manager_.MarkLayoutInitialized(EditorType::kDungeon);
  layout_manager_.MarkLayoutInitialized(EditorType::kOverworld);
  const ImGuiID dungeon =
      LayoutManager::EditorDockspaceId(EditorType::kDungeon);
  RunFrames(3, [&] { SubmitPanel(objects_title_, dungeon); });
  ASSERT_EQ(RootOf(objects_title_.c_str()), dungeon);

  const std::string ini = SaveIni();
  EXPECT_NE(ini.find("[YazeLayout][Data]"), std::string::npos);
  EXPECT_NE(ini.find("Revision=1"), std::string::npos);
  EXPECT_NE(ini.find("Initialized=Dungeon"), std::string::npos);
  // Overworld was marked but never drew a tree, so it rebuilds next launch.
  EXPECT_EQ(ini.find("Initialized=Overworld"), std::string::npos);

  RestartWithIni(ini);
  EXPECT_TRUE(layout_manager_.IsLayoutInitialized(EditorType::kDungeon));
  EXPECT_FALSE(layout_manager_.IsLayoutInitialized(EditorType::kOverworld));
  EXPECT_NE(ImGui::DockBuilderGetNode(dungeon), nullptr);

  RunFrames(2, [&] { SubmitPanel(objects_title_, dungeon); });
  EXPECT_EQ(RootOf(objects_title_.c_str()), dungeon);
}

TEST_F(LayoutManagerB2Test, PreB2IniDropsDockTrees) {
  layout_manager_.SetActiveEditorType(EditorType::kDungeon);
  layout_manager_.MarkLayoutInitialized(EditorType::kDungeon);
  const ImGuiID dungeon =
      LayoutManager::EditorDockspaceId(EditorType::kDungeon);
  RunFrames(3, [&] { SubmitPanel(objects_title_, dungeon); });
  ASSERT_EQ(RootOf(objects_title_.c_str()), dungeon);

  // A pre-B2 imgui.ini has dock trees but no YazeLayout section.
  std::string ini = SaveIni();
  const size_t begin = ini.find("[YazeLayout][Data]");
  ASSERT_NE(begin, std::string::npos);
  const size_t end = ini.find("\n\n", begin);
  ASSERT_NE(end, std::string::npos);
  ini.erase(begin, end + 2 - begin);
  ASSERT_NE(ini.find("[Docking][Data]"), std::string::npos);

  RestartWithIni(ini);
  EXPECT_FALSE(layout_manager_.IsLayoutInitialized(EditorType::kDungeon));
  EXPECT_EQ(ImGui::DockBuilderGetNode(dungeon), nullptr);
  EXPECT_EQ(ImGui::DockBuilderGetNode(LayoutManager::OuterDockspaceId()),
            nullptr);
}

}  // namespace
}  // namespace yaze::editor
