#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/ui/window/dungeon_entrances_panel.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/dungeon/workspace/dungeon_pit_damage_view_model.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_layout.h"
#include "app/editor/dungeon/workspace/room_matrix_content.h"
#include "app/editor/system/session/user_settings.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "core/features.h"
#include "core/project.h"
#include "dungeon_workbench_test_peer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "test_utils/expanded_entrance_fixture.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/pit_damage_table.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {

namespace {

constexpr float kMinCanvasWidth = 420.0f;
constexpr float kMinSidebarWidth = 320.0f;
constexpr float kSplitterWidth = 8.0f;
constexpr float kCompactLeftWidth = 230.4f;
constexpr float kCompactRightWidth = 272.0f;

std::filesystem::path MakeTempProjectRoot() {
  return std::filesystem::temp_directory_path() /
         ("yaze_dungeon_project_labels_" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
}

DungeonWorkbenchContent MakeWorkbenchForToolStateTests(
    int& current_room_id, const std::deque<int>& recent_rooms) {
  return DungeonWorkbenchContent(
      nullptr, &current_room_id, [](int) {}, [](int, RoomSelectionIntent) {},
      [](int) {}, []() {}, []() -> DungeonCanvasViewer* { return nullptr; },
      [](int) -> DungeonCanvasViewer* { return nullptr; },
      [&recent_rooms]() -> const std::deque<int>& { return recent_rooms; },
      [](int) {}, [](bool) {});
}

TEST(DungeonWorkbenchEntranceBadgeTest, ResolvedBoundsAndSpawnPrecedence) {
  Rom rom;
  std::vector<uint8_t> data(0x200000, 0);
  yaze::test::InstallExpandedEntranceLoader(data);
  std::fill_n(data.begin() + 0x78000, 2 * zelda3::kNumRegularDungeonEntrances,
              0xFF);
  std::fill_n(data.begin() + zelda3::kStartingEntranceroom,
              2 * zelda3::kNumDungeonSpawnPoints, 0xFF);
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  ASSERT_TRUE(rom.WriteWord(0x78000 + 0x84 * 2, 0x122).ok());
  ASSERT_TRUE(rom.WriteByte(0x79800 + 0x84, 0x1F).ok());
  ASSERT_TRUE(rom.WriteWord(0x78000, 0x42).ok());
  ASSERT_TRUE(rom.WriteByte(0x79800, 2).ok());
  ASSERT_TRUE(rom.WriteWord(zelda3::kStartingEntranceroom, 0x42).ok());
  ASSERT_TRUE(rom.WriteByte(zelda3::kStartingEntranceDungeon, 3).ok());
  ASSERT_TRUE(rom.WriteWord(zelda3::kStartingEntranceroom + 6 * 2, 0x123).ok());
  ASSERT_TRUE(rom.WriteByte(zelda3::kStartingEntranceDungeon + 6, 4).ok());
  int current = 0;
  const std::deque<int> recent;
  auto workbench = MakeWorkbenchForToolStateTests(current, recent);
  workbench.SetRom(&rom);
  const auto& cache =
      DungeonWorkbenchContentTestPeer::RebuildDungeonBadgeCache(workbench);
  ASSERT_EQ(cache.size(), 3u);
  EXPECT_EQ(cache.at(0x122), "Dungeon 1F");
  EXPECT_EQ(cache.at(0x42), "Eastern");
  EXPECT_EQ(cache.at(0x123), "A-Tower");
  EXPECT_FALSE(cache.contains(-1));
  EXPECT_FALSE(cache.contains(0xFFFF));
  rom.mutable_data()[0x7F00F] ^= 1;
  EXPECT_TRUE(
      DungeonWorkbenchContentTestPeer::RebuildDungeonBadgeCache(workbench)
          .empty());
  Rom unloaded;
  workbench.SetRom(&unloaded);
  EXPECT_TRUE(
      DungeonWorkbenchContentTestPeer::RebuildDungeonBadgeCache(workbench)
          .empty());
}

class FakeWorkbenchToolContent final : public WindowContent {
 public:
  explicit FakeWorkbenchToolContent(std::string id) : id_(std::move(id)) {}

  std::string GetId() const override { return id_; }
  std::string GetDisplayName() const override { return "Fake Tool"; }
  std::string GetIcon() const override { return ""; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  void Draw(bool*) override {}

 private:
  std::string id_;
};

zelda3::PitDamageTable MakePitDamageTable(
    std::initializer_list<uint16_t> room_ids) {
  zelda3::PitDamageTable table;
  table.SetRoomIds(std::vector<uint16_t>(room_ids));
  table.ClearDirty();
  return table;
}

class DungeonWorkbenchPitDamageUiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int atlas_width = 0;
    int atlas_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &atlas_width, &atlas_height);
    io.AddMousePosEvent(-1000.0f, -1000.0f);
    io.AddMouseButtonEvent(0, false);
  }

  void TearDown() override {
    if (context_ != nullptr) {
      ImGui::DestroyContext(context_);
      context_ = nullptr;
    }
  }

  void DrawPitDamageFrame(DungeonWorkbenchContent& content, int room_id) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(640.0f, 480.0f);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 320.0f), ImGuiCond_Always);
    ImGui::Begin(
        "PitDamageHost", nullptr,
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);
    content.DrawPitDamageControlsForTesting(room_id);
    ImGui::End();
    ImGui::Render();
  }

  void ClickRect(DungeonWorkbenchContent& content, int room_id,
                 const DungeonWorkbenchTestRect& rect) {
    const float center_x = (rect.min_x + rect.max_x) * 0.5f;
    const float center_y = (rect.min_y + rect.max_y) * 0.5f;

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(center_x, center_y);
    io.AddMouseButtonEvent(0, true);
    DrawPitDamageFrame(content, room_id);

    io.AddMousePosEvent(center_x, center_y);
    io.AddMouseButtonEvent(0, false);
    DrawPitDamageFrame(content, room_id);
  }

 private:
  ImGuiContext* context_ = nullptr;
};

TEST(DungeonWorkbenchContentLayoutTest, KeepsNavigationWhenOnlyOnePaneFits) {
  const auto full = ResolveDungeonWorkbenchResponsiveLayout(
      1076.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(full.show_left);
  EXPECT_TRUE(full.show_right);
  EXPECT_FALSE(full.compact_left);
  EXPECT_FALSE(full.compact_right);

  const auto compact_left = ResolveDungeonWorkbenchResponsiveLayout(
      1075.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(compact_left.show_left);
  EXPECT_TRUE(compact_left.show_right);
  EXPECT_TRUE(compact_left.compact_left);
  EXPECT_FALSE(compact_left.compact_right);

  const auto both_compact = ResolveDungeonWorkbenchResponsiveLayout(
      979.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(both_compact.show_left);
  EXPECT_TRUE(both_compact.show_right);
  EXPECT_TRUE(both_compact.compact_left);
  EXPECT_TRUE(both_compact.compact_right);

  const auto hide_inspector = ResolveDungeonWorkbenchResponsiveLayout(
      931.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_TRUE(hide_inspector.show_left);
  EXPECT_FALSE(hide_inspector.show_right);
  EXPECT_TRUE(hide_inspector.compact_left);
  EXPECT_FALSE(hide_inspector.compact_right);

  const auto hide_both = ResolveDungeonWorkbenchResponsiveLayout(
      657.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, true, true);
  EXPECT_FALSE(hide_both.show_left);
  EXPECT_FALSE(hide_both.show_right);
}

TEST(DungeonWorkbenchNavigatorUiTest, EntranceSelectorAboveToolsAndMatrix) {
  auto* previous_context = ImGui::GetCurrentContext();
  auto* context = ImGui::CreateContext();
  absl::Cleanup cleanup = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context);
    ImGui::SetCurrentContext(previous_context);
  };
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(800, 800);
  io.DeltaTime = 1.0f / 60.0f;
  io.ConfigInputTrickleEventQueue = false;
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  int room_id = 0;
  int entrance_slot = zelda3::kNumDungeonSpawnPoints;
  int selected_slot = -1;
  std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots> entrances;
  std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints> spawns;
  entrances[entrance_slot].blockset_ = 4;
  entrances[entrance_slot + 1].blockset_ = 10;
  DungeonEntrancesPanel entrance_panel(&entrances, &spawns, &entrance_slot,
                                       [&](int slot) { selected_slot = slot; });
  const std::deque<int> recent;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  DungeonRoomSelector room_selector(&rom);
  DungeonWorkbenchContent content(
      &room_selector, &room_id, [](int) {}, [](int, RoomSelectionIntent) {},
      [](int) {}, [] {}, []() -> DungeonCanvasViewer* { return nullptr; },
      [](int) -> DungeonCanvasViewer* { return nullptr; },
      [&]() -> const std::deque<int>& { return recent; }, [](int) {},
      [](bool) {}, &rom);
  ImVector<int> active_rooms;
  RoomMatrixContent matrix(&room_id, &active_rooms, [](int) {});
  FakeWorkbenchToolContent browser("dungeon.object_selector");
  content.SetNavigationPanels(&matrix, &entrance_panel);
  content.SetEmbeddedEditorPanels(&browser, nullptr, nullptr, nullptr, nullptr,
                                  nullptr);
  WorkspaceWindowManager windows;
  windows.RegisterSession(0);
  windows.SetActiveSession(0);
  bool matrix_visible = true;
  WindowDescriptor descriptor;
  descriptor.card_id = "dungeon.room_matrix";
  descriptor.category = "Dungeon";
  descriptor.visibility_flag = &matrix_visible;
  windows.RegisterWindow(0, descriptor);
  UserSettings settings;
  settings.prefs().panel_layout_defaults_revision = 28;
  settings.prefs().panel_visibility_state["Dungeon"] = {
      {"dungeon.workbench", true}, {"dungeon.room_matrix", true}};
  settings.ApplyPanelLayoutDefaultsRevision(
      UserSettings::kLatestPanelLayoutDefaultsRevision);
  windows.RestoreVisibilityState(
      0, settings.prefs().panel_visibility_state["Dungeon"]);
  EXPECT_FALSE(matrix_visible);
  content.SetStandaloneToolCallbacks(
      [&](const std::string& id) { return windows.IsWindowOpen(0, id); },
      [](const std::string&) { return false; });
  const auto frame = [&]() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 720), ImGuiCond_Always);
    ImGui::Begin("NavigatorHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    DungeonWorkbenchContentTestPeer::DrawNavigator(content, 280, 660);
    ImGui::End();
    ImGui::Render();
  };
  frame();
  frame();
  const auto find_widget = [](const std::string& label)
      -> std::optional<gui::WidgetIdRegistry::WidgetInfo> {
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets())
      if (widget.label == gui::WidgetIdRegistry::NormalizeLabel(label))
        return widget;
    return std::nullopt;
  };
  // The production combo remains visible while both child panes are drawn.
  auto combo = find_widget("Graphics entrance");
  ASSERT_TRUE(combo.has_value());
  const auto click = [&](const auto& widget) {
    ASSERT_TRUE(widget.has_value());
    const std::string label = widget->label;
    io.AddMousePosEvent((widget->bounds.min_x + widget->bounds.max_x) * 0.5f,
                        (widget->bounds.min_y + widget->bounds.max_y) * 0.5f);
    frame();
    // A newly opened combo can scroll its selected entrance into view on the
    // next frame. Use the resulting input rectangle rather than stale bounds.
    const auto settled = find_widget(label);
    ASSERT_TRUE(settled.has_value());
    io.AddMousePosEvent((settled->bounds.min_x + settled->bounds.max_x) * 0.5f,
                        (settled->bounds.min_y + settled->bounds.max_y) * 0.5f);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
  };
  click(combo);
  frame();  // Let the newly opened popup settle before using its item bounds.
  auto choice = find_widget("Entrance 01");
  ASSERT_TRUE(choice.has_value());
  click(choice);
  EXPECT_EQ(selected_slot, zelda3::kNumDungeonSpawnPoints + 1);
  EXPECT_EQ(entrance_slot, selected_slot);
  EXPECT_EQ(entrances[zelda3::kNumDungeonSpawnPoints].blockset_, 4);
  EXPECT_EQ(entrances[zelda3::kNumDungeonSpawnPoints + 1].blockset_, 10);
  bool automatic = false;
  entrance_panel.SetGraphicsPreview(
      [&]() {
        DungeonRenderContext render;
        render.room_id = room_id;
        render.owner_name = "Test dungeon";
        render.entrance_slot = entrance_slot;
        render.entrance_blockset = entrances[entrance_slot].blockset_;
        render.source = automatic
                            ? DungeonRenderContextSource::kInferredOwnerEntrance
                            : DungeonRenderContextSource::kSelectedEntrance;
        return render;
      },
      [&]() { return automatic; },
      [&](int slot) {
        automatic = slot == -1;
        entrance_slot = automatic ? zelda3::kNumDungeonSpawnPoints : slot;
      });
  frame();
  click(find_widget("Graphics entrance"));
  frame();
  const auto auto_choice = find_widget("Auto (follow room)");
  ASSERT_TRUE(auto_choice.has_value());
  click(auto_choice);
  EXPECT_TRUE(automatic);
  EXPECT_EQ(entrance_slot, zelda3::kNumDungeonSpawnPoints);
  click(find_widget("Graphics entrance"));
  frame();
  click(find_widget("Entrance 01"));
  EXPECT_FALSE(automatic);
  EXPECT_EQ(entrance_slot, zelda3::kNumDungeonSpawnPoints + 1);
  EXPECT_EQ(room_id, 0);
  float tools_top = -1, matrix_top = -1, tools_bottom = -1;
  for (ImGuiWindow* window : context->Windows) {
    if (!window->Active)
      continue;
    const std::string name(window->Name);
    if (name.find("/##NavigatorTools_") != std::string::npos) {
      tools_top = window->Pos.y;
      tools_bottom = window->Pos.y + window->Size.y;
    }
    if (name.find("/##NavigatorMatrix_") != std::string::npos) {
      matrix_top = window->Pos.y;
      EXPECT_EQ(window->ScrollMax.y, 0);
      EXPECT_EQ(window->ScrollMax.x, 0);
    }
  }
  EXPECT_GT(tools_top, combo->bounds.max_y);
  EXPECT_GE(matrix_top, tools_bottom);
  EXPECT_GT(tools_bottom, tools_top);
  click(find_widget("Navigator Entrances"));
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "entrances");
  click(find_widget("Navigator Objects"));
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "objects");
  content.FocusEntranceBrowser();
  frame();
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "entrances");
  for (const float host_width : {1200.0f, 800.0f}) {
    io.DisplaySize = ImVec2(host_width + 40, 800);
    for (int settle = 0; settle < 2; ++settle) {
      ImGui::NewFrame();
      ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(host_width, 720), ImGuiCond_Always);
      ImGui::Begin("WholeWorkbench", nullptr, ImGuiWindowFlags_NoSavedSettings);
      content.Draw(nullptr);
      ImGui::End();
      ImGui::Render();
    }
    float canvas_right = -1, navigator_left = -1;
    for (ImGuiWindow* window : context->Windows) {
      if (!window->Active)
        continue;
      const std::string name(window->Name);
      if (name.find("/##DungeonWorkbenchCanvas_") != std::string::npos)
        canvas_right = window->Pos.x + window->Size.x;
      if (name.find("/##DungeonWorkbenchSidebar_") != std::string::npos)
        navigator_left = window->Pos.x;
    }
    EXPECT_GT(canvas_right, 0) << host_width;
    EXPECT_GE(navigator_left, canvas_right) << host_width;
  }
}

// Exercise the real matrix drawing and its registered input rectangles. This
// catches extra layout extent even when every cell happens to look on-screen.
TEST(DungeonWorkbenchNavigatorUiTest, MatrixFitsAndBottomRoomRemainsClickable) {
  auto* previous = ImGui::GetCurrentContext();
  auto* context = ImGui::CreateContext();
  absl::Cleanup cleanup = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context);
    ImGui::SetCurrentContext(previous);
  };
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1000, 900);
  io.ConfigInputTrickleEventQueue = false;
  unsigned char* pixels;
  int w, h;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
  int current = 0;
  ImVector<int> active;
  RoomMatrixContent matrix(&current, &active,
                           [&](int room) { current = room; });
  const auto frame = [&](ImVec2 size) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("MatrixHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    matrix.Draw(nullptr);
    ImGui::End();
    ImGui::Render();
  };
  for (const auto size :
       {ImVec2(240, 300), ImVec2(272, 380), ImVec2(340, 540)}) {
    frame(size);
    frame(size);
    auto* window = ImGui::FindWindowByName("MatrixHost");
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->ScrollMax.y, 0) << size.y;
    EXPECT_EQ(window->ScrollMax.x, 0) << size.x;
    std::optional<gui::WidgetIdRegistry::WidgetInfo> last;
    int cells = 0;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label.find("Room 0x") == 0) {
        ++cells;
        EXPECT_GE(widget.bounds.min_y, window->InnerClipRect.Min.y);
        EXPECT_LE(widget.bounds.max_y, window->InnerClipRect.Max.y);
        EXPECT_GE(widget.bounds.min_x, window->InnerClipRect.Min.x);
        EXPECT_LE(widget.bounds.max_x, window->InnerClipRect.Max.x);
      }
      if (widget.label == "Room 0x127")
        last = widget;
    }
    EXPECT_EQ(cells, 296);
    ASSERT_TRUE(last.has_value());
    io.AddMousePosEvent((last->bounds.min_x + last->bounds.max_x) * 0.5f,
                        (last->bounds.min_y + last->bounds.max_y) * 0.5f);
    frame(size);
    io.AddMouseButtonEvent(0, true);
    frame(size);
    io.AddMouseButtonEvent(0, false);
    frame(size);
    EXPECT_EQ(current, 0x127);
    current = 0;
  }
  const auto click_label = [&](const std::string& label) {
    std::optional<gui::WidgetIdRegistry::WidgetInfo> target;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets())
      if (widget.label == label)
        target = widget;
    ASSERT_TRUE(target.has_value()) << label;
    io.AddMousePosEvent((target->bounds.min_x + target->bounds.max_x) * 0.5f,
                        (target->bounds.min_y + target->bounds.max_y) * 0.5f);
    frame(ImVec2(340, 540));
    io.AddMouseButtonEvent(0, true);
    frame(ImVec2(340, 540));
    io.AddMouseButtonEvent(0, false);
    frame(ImVec2(340, 540));
    frame(ImVec2(340, 540));
  };
  click_label("Room matrix options");
  click_label("Census overlay");
  EXPECT_TRUE(matrix.census_overlay().enabled());
  click_label("Census overlay");
  EXPECT_FALSE(matrix.census_overlay().enabled());
}

TEST(DungeonWorkbenchNavigatorUiTest,
     OraclePaletteColorsBeforeOpeningAndAfterEdits) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to the Oracle base ROM";
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  const std::vector<uint8_t> initial(rom.begin(), rom.end());
  zelda3::GameData data(&rom);
  ASSERT_TRUE(zelda3::LoadGfxGroups(rom, data).ok());
  ASSERT_TRUE(zelda3::LoadPalettes(rom, data).ok());
  DungeonRoomStore rooms(&rom, &data);
  auto* previous = ImGui::GetCurrentContext();
  auto* context = ImGui::CreateContext();
  absl::Cleanup cleanup = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::DestroyContext(context);
    ImGui::SetCurrentContext(previous);
  };
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(800, 800);
  unsigned char* pixels;
  int w, h;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
  io.AddMousePosEvent(-1000, -1000);
  int current = 0;
  ImVector<int> active;
  RoomMatrixContent matrix(&current, &active, [](int) {}, nullptr, &rooms);
  const auto frame = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 660), ImGuiCond_Always);
    ImGui::Begin("PaletteMatrix", nullptr, ImGuiWindowFlags_NoSavedSettings);
    matrix.Draw(nullptr);
    ImGui::End();
    ImGui::Render();
  };
  const auto fill = [&](int id) -> ImU32 {
    const std::string label = absl::StrFormat("Room 0x%03X", id);
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (widget.label != label)
        continue;
      auto* window = ImGui::FindWindowByName("PaletteMatrix");
      for (const auto& vertex : window->DrawList->VtxBuffer)
        if (vertex.pos.x == widget.bounds.min_x &&
            vertex.pos.y == widget.bounds.min_y)
          return vertex.col;
    }
    ADD_FAILURE() << "Missing rendered room fill " << label;
    return 0;
  };
  frame();
  frame();
  // Expected color independently follows ZScream's palette-set/pointer lookup,
  // using raw ROM bytes and its CGRAM [4,2] swatch (stored color 3).
  for (int id = 1; id < 296; ++id) {
    const auto header = zelda3::LoadRoomHeaderFromRom(&rom, id);
    const auto offset =
        rom.ReadByte(zelda3::kPalettesetIdsAddress + header.palette() * 4);
    ASSERT_TRUE(offset.ok());
    const auto pointer =
        rom.ReadWord(zelda3::kDungeonPalettePointerTable + *offset);
    ASSERT_TRUE(pointer.ok());
    const auto value = rom.ReadWord(gfx::kDungeonMainPalettes + *pointer + 6);
    ASSERT_TRUE(value.ok());
    const auto color = gfx::ConvertSnesToRgb(*value);
    EXPECT_EQ(fill(id), IM_COL32(color.red, color.green, color.blue, 255))
        << id;
    EXPECT_EQ(rooms.GetIfMaterialized(id), nullptr);
  }
  const ImU32 original = fill(0x89);
  rooms[0x89] = zelda3::LoadRoomHeaderFromRom(&rom, 0x89);
  rooms[0x89].SetGameData(&data);
  rooms[0x89].SetLoaded(true);
  frame();
  const ImU32 visited = fill(0x89);
  active.push_back(0x89);
  frame();
  EXPECT_EQ(fill(0x89),
            visited);  // Tabs and workbench use the same open state.
  const auto before = ImGui::ColorConvertU32ToFloat4(original);
  const auto after = ImGui::ColorConvertU32ToFloat4(fill(0x89));
  EXPECT_GT(after.x + after.y + after.z, before.x + before.y + before.z);
  float before_h, before_s, before_v, after_h, after_s, after_v;
  ImGui::ColorConvertRGBtoHSV(before.x, before.y, before.z, before_h, before_s,
                              before_v);
  ImGui::ColorConvertRGBtoHSV(after.x, after.y, after.z, after_h, after_s,
                              after_v);
  EXPECT_NEAR(before_h, after_h, 0.01f);
  EXPECT_NEAR(before_s, after_s, 0.01f);
  active.clear();
  // Follow the same brightness treatment after an in-memory palette change.
  const auto open_color = [](ImVec4 color) {
    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
    ImGui::ColorConvertHSVtoRGB(h, s, v + (1 - v) * 0.22f, color.x, color.y,
                                color.z);
    return ImGui::ColorConvertFloat4ToU32(color);
  };
  const int palette_id = rooms[0x89].ResolveDungeonPaletteId();
  data.palette_groups.dungeon_main.mutable_palette(palette_id)
      ->UpdateColor(3, gfx::SnesColor(80, 120, 160));
  frame();
  EXPECT_EQ(fill(0x89),
            open_color(ImVec4(80 / 255.0f, 120 / 255.0f, 160 / 255.0f, 1)));
  rooms[0x89].SetPalette(0);
  frame();
  const auto& next = data.palette_groups.dungeon_main
                         .palette_ref(rooms[0x89].ResolveDungeonPaletteId())[3]
                         .rgb();
  EXPECT_EQ(fill(0x89),
            open_color(ImVec4(next.x / 255, next.y / 255, next.z / 255, 1)));
  EXPECT_EQ(std::vector<uint8_t>(rom.begin(), rom.end()), initial);
}

TEST(DungeonWorkbenchApplyScopeTest, SelectAllAndNoneIncludeEntrances) {
  auto& flags = core::FeatureFlags::get().dungeon;
  const auto previous = flags;
  int current_room_id = 0;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  flags.kSaveWaterFillZones = true;
  content.SetAllSaveFlagsForTesting(false);
  EXPECT_FALSE(flags.kSaveObjects);
  EXPECT_FALSE(flags.kSaveSprites);
  EXPECT_FALSE(flags.kSaveRoomHeaders);
  EXPECT_FALSE(flags.kSaveChests);
  EXPECT_FALSE(flags.kSavePotItems);
  EXPECT_FALSE(flags.kSaveEntrances);
  EXPECT_FALSE(flags.kSavePalettes);
  EXPECT_FALSE(flags.kSaveCollision);
  EXPECT_FALSE(flags.kSaveBlocks);
  EXPECT_FALSE(flags.kSaveTorches);
  EXPECT_FALSE(flags.kSavePits);
  EXPECT_TRUE(flags.kSaveWaterFillZones);

  flags.kSaveWaterFillZones = false;
  content.SetAllSaveFlagsForTesting(true);
  EXPECT_TRUE(flags.kSaveEntrances);
  EXPECT_TRUE(flags.kSaveObjects);
  EXPECT_FALSE(flags.kSaveWaterFillZones);

  flags = previous;
}

TEST(DungeonWorkbenchContentLayoutTest,
     CompactInspectorDetailRequestResetsOutsideCompactLayout) {
  EXPECT_FALSE(ResolveCompactInspectorDetailRequest(false, false));
  EXPECT_FALSE(ResolveCompactInspectorDetailRequest(true, false));
  EXPECT_TRUE(ResolveCompactInspectorDetailRequest(true, true));
  EXPECT_FALSE(ResolveCompactInspectorDetailRequest(false, true));
}

TEST(DungeonWorkbenchContentLayoutTest,
     NavigatorDefaultsToObjectsAndKeepsMatrixBelow) {
  int current_room_id = 0;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  EXPECT_TRUE(content.IsInspectorOnLeft());
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "objects");
  content.FocusEntranceBrowser();
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "entrances");
  content.FocusRoomMatrix();
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "objects");
}

TEST(DungeonWorkbenchContentObjectSizeTest,
     SizeControlsEnableOnlyWhenSelectionContainsEditableObject) {
  const std::vector<zelda3::RoomObject> objects = {
      zelda3::RoomObject(0x100, 5, 5, 0, 0),
      zelda3::RoomObject(0xF99, 10, 10, 0x06, 0),
      zelda3::RoomObject(0x001, 15, 15, 3, 0),
  };
  const std::vector<size_t> fixed_only = {0, 1};
  const std::vector<size_t> mixed = {0, 1, 2};
  const std::vector<size_t> invalid = {objects.size()};

  EXPECT_FALSE(workbench::HasEditableRoomObjectSize(objects, fixed_only));
  EXPECT_TRUE(workbench::HasEditableRoomObjectSize(objects, mixed));
  EXPECT_FALSE(workbench::HasEditableRoomObjectSize(objects, invalid));
}

class DungeonWorkbenchObjectSizeUiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_custom_ = core::FeatureFlags::get().kEnableCustomObjects;
    previous_manager_ = zelda3::CustomObjectManager::Get().SnapshotState();
    core::FeatureFlags::get().kEnableCustomObjects = false;
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override {
    ImGui::DestroyContext();
    gui::WidgetIdRegistry::Instance().Clear();
    core::FeatureFlags::get().kEnableCustomObjects = previous_custom_;
    zelda3::CustomObjectManager::Get().RestoreState(previous_manager_);
  }

  void DrawFrame(float width = 240.0f) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 240), ImGuiCond_Always);
    ImGui::Begin("ObjectSizeHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    if (ImGui::BeginTable("SizeProperties", 2)) {
      ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 56);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      uint8_t requested = object_.size_;
      if (workbench::DrawObjectSizeControls(object_, &requested)) {
        object_.set_size(requested);
        ++changes_;
      }
      ImGui::EndTable();
    }
    logged_text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    controls_bottom_ = ImGui::GetCursorPosY();
    ImGui::End();
    ImGui::Render();
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> Widget(const char* name) {
    return gui::WidgetIdRegistry::Instance().GetWidgetInfo(
        std::string("Dungeon/Workbench/") + name);
  }

  void Click(const char* name) {
    const auto widget = Widget(name);
    ASSERT_TRUE(widget.has_value()) << name;
    ASSERT_TRUE(widget->enabled) << name;
    const auto& rect = widget->bounds;
    ASSERT_TRUE(rect.valid);
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((rect.min_x + rect.max_x) / 2,
                        (rect.min_y + rect.max_y) / 2);
    DrawFrame();
    io.AddMouseButtonEvent(0, true);
    DrawFrame();
    io.AddMouseButtonEvent(0, false);
    DrawFrame();
    DrawFrame();
  }

  zelda3::RoomObject object_{0xD1, 8, 9, 3, 1};
  int changes_ = 0;
  float controls_bottom_ = 0;
  std::string logged_text_;
  bool previous_custom_ = false;
  zelda3::CustomObjectManager::State previous_manager_;
};

TEST_F(DungeonWorkbenchObjectSizeUiTest, WidthAndHeightChangeIndependently) {
  DrawFrame();
  DrawFrame();
  const float before = controls_bottom_;
  Click("combo:selected_object_width");
  EXPECT_FLOAT_EQ(controls_bottom_, before);
  Click("selectable:object_width_2");
  EXPECT_EQ(object_.size_, 0x0B);  // 12 x 16 tiles, height preserved.
  EXPECT_EQ(changes_, 1);
  EXPECT_FLOAT_EQ(controls_bottom_, before);
  Click("combo:selected_object_height");
  Click("selectable:object_height_0");
  EXPECT_EQ(object_.size_, 0x08);  // 12 x 4 tiles, width preserved.
  EXPECT_EQ(changes_, 2);
  EXPECT_EQ(object_.x_, 8);
  EXPECT_EQ(object_.y_, 9);
  EXPECT_EQ(object_.GetLayerValue(), 1);
}

class DungeonWorkbenchPlacementUiTest
    : public DungeonWorkbenchObjectSizeUiTest {
 protected:
  void SetUp() override {
    DungeonWorkbenchObjectSizeUiTest::SetUp();
    rooms_[0].SetLoaded(true);
    viewer_.RefreshRomBackedState(nullptr, nullptr, &rooms_, 0);
    content_.SetEmbeddedEditorPanels(&browser_, nullptr, nullptr, nullptr,
                                     nullptr, nullptr);
    content_.OpenObjectSelectorTool();
    viewer_.object_interaction().SetPreviewObject(object_, true);
  }

  void DrawInspectorFrame() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(272, 560), ImGuiCond_Always);
    ImGui::Begin("PlacementHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    DungeonWorkbenchContentTestPeer::DrawInspector(content_, viewer_);
    auto* window = ImGui::GetCurrentWindow();
    EXPECT_LE(window->DC.CursorMaxPos.x, window->WorkRect.Max.x + 1);
    ImGui::End();
    ImGui::Render();
  }

  void ClickInspector(const char* id) {
    auto widget = Widget(id);
    ASSERT_TRUE(widget.has_value()) << id;
    const auto bounds = widget->bounds;
    ASSERT_TRUE(bounds.valid);
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((bounds.min_x + bounds.max_x) / 2,
                        (bounds.min_y + bounds.max_y) / 2);
    DrawInspectorFrame();
    io.AddMouseButtonEvent(0, true);
    DrawInspectorFrame();
    io.AddMouseButtonEvent(0, false);
    DrawInspectorFrame();
  }

  int room_id_ = 0;
  std::deque<int> recent_;
  DungeonRoomStore rooms_;
  DungeonCanvasViewer viewer_;
  FakeWorkbenchToolContent browser_{"dungeon.object_selector"};
  DungeonWorkbenchContent content_ =
      MakeWorkbenchForToolStateTests(room_id_, recent_);
};

TEST_F(DungeonWorkbenchPlacementUiTest,
       PlaceOnceSelectsResultAndPlaceAnotherRestoresPreviewControls) {
  auto& interaction = viewer_.object_interaction();
  DrawInspectorFrame();
  DrawInspectorFrame();
  ASSERT_TRUE(Widget("checkbox:repeat_placement").has_value());
  ASSERT_TRUE(Widget("combo:selected_object_width").has_value());
  ClickInspector("checkbox:repeat_placement");
  EXPECT_EQ(interaction.GetPlacementPolicy(),
            DungeonObjectInteraction::PlacementPolicy::kOnce);
  ASSERT_NE(interaction.GetPlacementPreview(), nullptr);
  const auto preview = *interaction.GetPlacementPreview();
  ASSERT_TRUE(interaction.entity_coordinator().tile_handler().PlaceObjectAt(
      0, preview, 12, 14));
  DrawInspectorFrame();
  EXPECT_STREQ(content_.GetInspectorModeIdForTesting(), "selection");
  ASSERT_EQ(interaction.GetSelectedObjectIndices(), std::vector<size_t>{0});
  EXPECT_EQ(interaction.GetPlacementPreview(), nullptr);
  ClickInspector("button:place_another");
  DrawInspectorFrame();
  EXPECT_STREQ(content_.GetInspectorModeIdForTesting(), "selection");
  EXPECT_STREQ(content_.GetSidebarModeIdForTesting(), "objects");
  ASSERT_NE(interaction.GetPlacementPreview(), nullptr);
  EXPECT_EQ(interaction.GetPlacementPreview()->size_, preview.size_);
  EXPECT_EQ(interaction.GetPlacementPreview()->GetLayerValue(),
            preview.GetLayerValue());
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 1);
  EXPECT_TRUE(Widget("checkbox:repeat_placement").has_value());
}

TEST_F(DungeonWorkbenchPlacementUiTest,
       RepeatKeepsBrowserAndDoneSwitchesToSelectionWithoutStackLeaks) {
  auto& interaction = viewer_.object_interaction();
  DrawInspectorFrame();
  DrawInspectorFrame();
  const auto preview = *interaction.GetPlacementPreview();
  auto& handler = interaction.entity_coordinator().tile_handler();
  ASSERT_TRUE(handler.PlaceObjectAt(0, preview, 12, 14));
  DrawInspectorFrame();
  ASSERT_TRUE(handler.PlaceObjectAt(0, preview, 16, 18));
  DrawInspectorFrame();
  EXPECT_STREQ(content_.GetInspectorModeIdForTesting(), "selection");
  EXPECT_STREQ(content_.GetSidebarModeIdForTesting(), "objects");
  ASSERT_EQ(interaction.GetSelectedObjectIndices(), std::vector<size_t>{1});
  ASSERT_NE(interaction.GetPlacementPreview(), nullptr);
  ClickInspector("button:finish_placement");
  DrawInspectorFrame();
  EXPECT_STREQ(content_.GetInspectorModeIdForTesting(), "selection");
  EXPECT_EQ(interaction.GetPlacementPreview(), nullptr);
  EXPECT_EQ(rooms_[0].GetTileObjects().size(), 2);
  EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
}

TEST_F(DungeonWorkbenchPlacementUiTest,
       ObjectDeleteButtonPreservesSelectedDoorsSpritesAndItems) {
  auto& interaction = viewer_.object_interaction();
  interaction.CancelPlacement();
  rooms_[0].SetTileObjects({object_});
  rooms_[0].GetDoors().push_back(zelda3::Room::Door::FromRomBytes(0x63, 0));
  rooms_[0].GetSprites().emplace_back(9, 6, 8, 0, 0);
  rooms_[0].GetPotItems().push_back({0x0A20, 6});
  interaction.SetSelectedObjects({0});
  const std::vector<SelectedEntity> entities{
      {EntityType::Door, 0}, {EntityType::Sprite, 0}, {EntityType::Item, 0}};
  interaction.entity_coordinator().SetSelectedEntities(entities);
  auto draw = [&] {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Always);
    ImGui::Begin("ObjectActionsHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    DungeonWorkbenchContentTestPeer::DrawSelectedObjectActions(content_,
                                                               viewer_, 0);
    ImGui::End();
    ImGui::Render();
  };
  draw();
  draw();
  const auto widget = Widget("button:delete_object");
  ASSERT_TRUE(widget.has_value());
  ASSERT_TRUE(widget->bounds.valid);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent((widget->bounds.min_x + widget->bounds.max_x) / 2,
                      (widget->bounds.min_y + widget->bounds.max_y) / 2);
  draw();
  io.AddMouseButtonEvent(0, true);
  draw();
  io.AddMouseButtonEvent(0, false);
  draw();
  EXPECT_TRUE(rooms_[0].GetTileObjects().empty());
  EXPECT_EQ(rooms_[0].GetDoors().size(), 1u);
  EXPECT_EQ(rooms_[0].GetSprites().size(), 1u);
  EXPECT_EQ(rooms_[0].GetPotItems().size(), 1u);
  EXPECT_EQ(interaction.entity_coordinator().SelectedEntitiesForEdit(),
            entities);
}

class DungeonWorkbenchEntityInspectorUiTest
    : public DungeonWorkbenchObjectSizeUiTest {
 protected:
  void SetUp() override {
    DungeonWorkbenchObjectSizeUiTest::SetUp();
    rooms_[0].SetLoaded(true);
    rooms_[0].GetSprites().emplace_back(0x42, 5, 6, 2, 0);
    rooms_[0].GetSprites().front().set_key_drop(1);
    rooms_[0].GetSprites().emplace_back(0x43, 7, 8, 3, 1);
    rooms_[0].GetDoors().push_back(
        zelda3::Room::Door::FromRomBytes(0x10, 0x00));
    rooms_[0].GetPotItems().push_back(zelda3::PotItem{0x0714, 0x09});
    viewer_.RefreshRomBackedState(nullptr, nullptr, &rooms_, 0);
    viewer_.object_interaction().SetMutationCallback([this] { ++mutations_; });
    viewer_.object_interaction().SetEntityChangedCallback(
        [this] { ++entity_changes_; });
    content_.FocusSelectionInspector();
  }

  void DrawEntityInspectorFrame() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(272, 560), ImGuiCond_Always);
    ImGui::Begin("EntityInspectorHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    DungeonWorkbenchContentTestPeer::DrawInspector(content_, viewer_);
    auto* window = ImGui::GetCurrentWindow();
    EXPECT_LE(window->DC.CursorMaxPos.x, window->WorkRect.Max.x + 1);
    ImGui::End();
    ImGui::Render();
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> EntityWidget(
      const char* name) {
    const std::string scope = "Dungeon/EntityInspector/";
    const auto normalized_label = gui::WidgetIdRegistry::NormalizeLabel(name);
    // AutoRegisterLastItem includes the widget type in its full path. Resolve
    // the explicit label within this inspector so a same-named control in
    // another panel cannot satisfy these tests.
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (path.starts_with(scope) && widget.label == normalized_label) {
        return widget;
      }
    }
    return std::nullopt;
  }

  void ClickEntityWidget(const char* name) {
    const auto widget = EntityWidget(name);
    ASSERT_TRUE(widget.has_value()) << name;
    ASSERT_TRUE(widget->enabled) << name;
    ASSERT_TRUE(widget->visible) << name;
    const auto bounds = widget->bounds;
    ASSERT_TRUE(bounds.valid) << name;
    const float x = (bounds.min_x + bounds.max_x) / 2.0f;
    const float y = (bounds.min_y + bounds.max_y) / 2.0f;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    DrawEntityInspectorFrame();
    io.AddMouseButtonEvent(0, true);
    DrawEntityInspectorFrame();
    io.AddMouseButtonEvent(0, false);
    DrawEntityInspectorFrame();
    DrawEntityInspectorFrame();
  }

  void SelectEntity(EntityType type, size_t index = 0) {
    viewer_.object_interaction().SelectEntity(type, index);
    DrawEntityInspectorFrame();
    DrawEntityInspectorFrame();
    mutations_ = 0;
    entity_changes_ = 0;
  }

  int room_id_ = 0;
  int mutations_ = 0;
  int entity_changes_ = 0;
  std::deque<int> recent_;
  DungeonRoomStore rooms_;
  DungeonCanvasViewer viewer_;
  DungeonWorkbenchContent content_ =
      MakeWorkbenchForToolStateTests(room_id_, recent_);
};

TEST_F(DungeonWorkbenchEntityInspectorUiTest,
       RendersEditablePropertiesForEachSelectedDomainWithoutMutating) {
  struct DomainCase {
    EntityType type;
    std::vector<const char*> fields;
  };
  const std::vector<DomainCase> cases = {
      {EntityType::Sprite,
       {"SpriteId", "SpriteX", "SpriteY", "SpriteSubtype", "SpriteLayer",
        "SpriteKeyDrop"}},
      {EntityType::Door, {"DoorType", "DoorDirection", "DoorPosition"}},
      {EntityType::Item, {"ItemType", "ItemX", "ItemY"}},
  };
  for (const auto& test_case : cases) {
    SCOPED_TRACE(static_cast<int>(test_case.type));
    SelectEntity(test_case.type);
    for (const char* field : test_case.fields) {
      SCOPED_TRACE(field);
      const auto widget = EntityWidget(field);
      ASSERT_TRUE(widget.has_value());
      EXPECT_TRUE(widget->visible);
      EXPECT_TRUE(widget->enabled);
      EXPECT_TRUE(widget->bounds.valid);
    }
    DrawEntityInspectorFrame();
    EXPECT_EQ(mutations_, 0);
    EXPECT_EQ(entity_changes_, 0);
  }
  EXPECT_FALSE(rooms_[0].sprites_dirty());
  EXPECT_FALSE(rooms_[0].pot_items_dirty());
  EXPECT_FALSE(rooms_[0].object_stream_dirty());
  EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
}

TEST_F(DungeonWorkbenchEntityInspectorUiTest,
       SpriteLayerComboChangesOnlySelectedSpriteThroughMutationHook) {
  SelectEntity(EntityType::Sprite);
  ClickEntityWidget("SpriteLayer");
  ASSERT_TRUE(EntityWidget("SpriteLayer/Upper").has_value());
  ASSERT_TRUE(EntityWidget("SpriteLayer/Lower").has_value());
  ClickEntityWidget("SpriteLayer/Lower");

  const auto& sprite = rooms_[0].GetSprites().front();
  EXPECT_EQ(sprite.layer(), 1);
  EXPECT_EQ(sprite.id(), 0x42);
  EXPECT_EQ(sprite.x(), 5);
  EXPECT_EQ(sprite.y(), 6);
  EXPECT_EQ(sprite.subtype(), 2);
  EXPECT_EQ(sprite.key_drop(), 1);
  EXPECT_EQ(rooms_[0].GetSprites()[1].layer(), 1);
  EXPECT_EQ(mutations_, 1);
  EXPECT_EQ(entity_changes_, 1);
  EXPECT_TRUE(rooms_[0].sprites_dirty());
  EXPECT_FALSE(rooms_[0].pot_items_dirty());
  EXPECT_FALSE(rooms_[0].object_stream_dirty());

  // Returning to Upper exercises both legal layer values. There is no third
  // layer in the dungeon sprite encoding.
  ClickEntityWidget("SpriteLayer");
  ClickEntityWidget("SpriteLayer/Upper");
  EXPECT_EQ(rooms_[0].GetSprites().front().layer(), 0);
  EXPECT_EQ(rooms_[0].GetSprites()[1].layer(), 1);
  EXPECT_EQ(mutations_, 2);
  EXPECT_EQ(entity_changes_, 2);
}

TEST_F(DungeonWorkbenchEntityInspectorUiTest,
       ItemXIncrementPreservesYAndItemAndUsesMutationHook) {
  SelectEntity(EntityType::Item);
  const auto before = rooms_[0].GetPotItems().front();
  ClickEntityWidget("ItemXIncrease");

  const auto& item = rooms_[0].GetPotItems().front();
  EXPECT_EQ(item.GetPixelX(), before.GetPixelX() + 8);
  EXPECT_EQ(item.GetPixelY(), before.GetPixelY());
  EXPECT_EQ(item.item, before.item);
  EXPECT_EQ(mutations_, 1);
  EXPECT_EQ(entity_changes_, 1);
  EXPECT_TRUE(rooms_[0].pot_items_dirty());
  EXPECT_FALSE(rooms_[0].sprites_dirty());
  EXPECT_FALSE(rooms_[0].object_stream_dirty());
}

TEST_F(DungeonWorkbenchEntityInspectorUiTest,
       ItemTypeChoicePreservesFlaggedVanillaPosition) {
  rooms_[0].GetPotItems().front() = {0x2660, 1};
  rooms_[0].ClearSaveDirtyState();
  SelectEntity(EntityType::Item);
  ClickEntityWidget("ItemType");
  ClickEntityWidget("ItemType/6");

  const auto& item = rooms_[0].GetPotItems().front();
  EXPECT_EQ(item.position, 0x2660);
  EXPECT_EQ(item.item, 6);
  EXPECT_EQ(mutations_, 1);
  EXPECT_EQ(entity_changes_, 1);
  EXPECT_TRUE(rooms_[0].pot_items_dirty());
  EXPECT_FALSE(rooms_[0].object_stream_dirty());
}

TEST_F(DungeonWorkbenchEntityInspectorUiTest,
       MultipleAndMixedSelectionsDoNotExposeFirstEntityProperties) {
  auto& interaction = viewer_.object_interaction();
  auto& coordinator = interaction.entity_coordinator();
  const std::vector<std::vector<SelectedEntity>> selections = {
      {{EntityType::Sprite, 0}, {EntityType::Sprite, 1}},
      {{EntityType::Sprite, 0}, {EntityType::Item, 0}},
      {{EntityType::Door, 0}, {EntityType::Sprite, 0}},
  };
  for (const auto& selected : selections) {
    coordinator.SetSelectedEntities(selected);
    mutations_ = 0;
    entity_changes_ = 0;
    DrawEntityInspectorFrame();
    DrawEntityInspectorFrame();
    EXPECT_FALSE(EntityWidget("SpriteId").has_value());
    EXPECT_FALSE(EntityWidget("SpriteLayer").has_value());
    EXPECT_FALSE(EntityWidget("DoorType").has_value());
    EXPECT_FALSE(EntityWidget("ItemType").has_value());
    EXPECT_EQ(mutations_, 0);
    EXPECT_EQ(entity_changes_, 0);
  }
  EXPECT_EQ(rooms_[0].GetSprites().front().layer(), 0);
  EXPECT_EQ(rooms_[0].GetPotItems().front().position, 0x0714);
  EXPECT_EQ(rooms_[0].GetDoors().front().type, zelda3::DoorType::NormalDoor);
}

TEST_F(DungeonWorkbenchEntityInspectorUiTest,
       MismatchedRoomContextHidesPropertiesWithoutMutationOrHistoryCapture) {
  rooms_[1].SetLoaded(true);
  rooms_[1].GetSprites().emplace_back(0x49, 2, 3, 4, 1);
  rooms_[1].GetPotItems().push_back(zelda3::PotItem{0x0310, 0x04});
  rooms_[1].GetDoors().push_back(zelda3::Room::Door::FromRomBytes(0x20, 0x02));
  rooms_[1].MarkHeaderDirty();  // Existing dirty state must also survive.
  auto& interaction = viewer_.object_interaction();
  struct DomainCase {
    EntityType type;
    const char* control;
  };
  const DomainCase cases[] = {
      {EntityType::Sprite, "SpriteId"},
      {EntityType::Door, "DoorType"},
      {EntityType::Item, "ItemType"},
  };
  for (const auto& test_case : cases) {
    SCOPED_TRACE(static_cast<int>(test_case.type));
    interaction.SetCurrentRoom(&rooms_, 0);
    SelectEntity(test_case.type);
    ASSERT_TRUE(EntityWidget(test_case.control).has_value());

    // The viewer still displays room 0, while handler mutations now target
    // room 1. Both rooms have index 0, so an accidental write would be valid.
    interaction.SetCurrentRoom(&rooms_, 1);
    ASSERT_FALSE(interaction.HasEntitySelection());
    // Room changes now clear selection. Deliberately select in the newly bound
    // room so this still exercises the inspector's independent context guard.
    interaction.SelectEntity(test_case.type, 0);
    entity_changes_ = 0;  // Selection itself emits a presentation notification.
    ASSERT_EQ(viewer_.current_room_id(), 0);
    ASSERT_EQ(interaction.entity_coordinator()
                  .tile_handler()
                  .context()
                  ->current_room_id,
              1);
    DrawEntityInspectorFrame();
    DrawEntityInspectorFrame();
    for (const auto& domain : cases) {
      EXPECT_FALSE(EntityWidget(domain.control).has_value());
    }
    EXPECT_FALSE(EntityWidget("SpriteXIncrease").has_value());
    EXPECT_FALSE(EntityWidget("ItemXIncrease").has_value());
    EXPECT_EQ(mutations_, 0);  // No new undo snapshot may be captured.
    EXPECT_EQ(entity_changes_, 0);
    EXPECT_EQ(interaction.GetSelectedEntity(),
              (SelectedEntity{test_case.type, 0}));
  }

  ASSERT_EQ(rooms_[0].GetSprites().size(), 2u);
  EXPECT_EQ(rooms_[0].GetSprites().front().id(), 0x42);
  EXPECT_EQ(rooms_[0].GetSprites().front().x(), 5);
  EXPECT_EQ(rooms_[0].GetSprites().front().layer(), 0);
  EXPECT_EQ(rooms_[0].GetPotItems().front().position, 0x0714);
  EXPECT_EQ(rooms_[0].GetPotItems().front().item, 0x09);
  EXPECT_EQ(rooms_[0].GetDoors().front().EncodeBytes(),
            (std::pair<uint8_t, uint8_t>{0x10, 0x00}));
  ASSERT_EQ(rooms_[1].GetSprites().size(), 1u);
  EXPECT_EQ(rooms_[1].GetSprites().front().id(), 0x49);
  EXPECT_EQ(rooms_[1].GetSprites().front().x(), 2);
  EXPECT_EQ(rooms_[1].GetSprites().front().layer(), 1);
  EXPECT_EQ(rooms_[1].GetPotItems().front().position, 0x0310);
  EXPECT_EQ(rooms_[1].GetPotItems().front().item, 0x04);
  EXPECT_EQ(rooms_[1].GetDoors().front().EncodeBytes(),
            (std::pair<uint8_t, uint8_t>{0x20, 0x02}));
  EXPECT_FALSE(rooms_[0].header_dirty());
  EXPECT_TRUE(rooms_[1].header_dirty());
  for (int room_id : {0, 1}) {
    EXPECT_FALSE(rooms_[room_id].sprites_dirty());
    EXPECT_FALSE(rooms_[room_id].pot_items_dirty());
    EXPECT_FALSE(rooms_[room_id].object_stream_dirty());
  }
}

TEST_F(DungeonWorkbenchObjectSizeUiTest,
       PlatformControlsShowPhysicalSizesAndPreserveOtherAxis) {
  struct SizeCase {
    int id;
    int base_width;
    int base_height;
  };
  for (const auto& test_case :
       {SizeCase{0xC1, 14, 8}, SizeCase{0xDC, 10, 7}, SizeCase{0xDD, 4, 4}}) {
    SCOPED_TRACE(test_case.id);
    object_ = zelda3::RoomObject(test_case.id, 8, 9, 0, 1);
    changes_ = 0;
    DrawFrame();
    DrawFrame();
    const auto expect_tiles = [&](int tiles) {
      EXPECT_NE(logged_text_.find(std::to_string(tiles) + " tiles"),
                std::string::npos)
          << logged_text_;
    };
    expect_tiles(test_case.base_width);
    expect_tiles(test_case.base_height);
    const float before = controls_bottom_;

    Click("combo:selected_object_width");
    for (int axis = 0; axis < 4; ++axis) {
      expect_tiles(test_case.base_width + axis * 2);
    }
    Click("selectable:object_width_2");
    EXPECT_EQ(object_.size_, 0x08);
    expect_tiles(test_case.base_width + 4);
    expect_tiles(test_case.base_height);

    Click("combo:selected_object_height");
    for (int axis = 0; axis < 4; ++axis) {
      expect_tiles(test_case.base_height + axis * 2);
    }
    Click("selectable:object_height_3");
    EXPECT_EQ(object_.size_, 0x0B);
    expect_tiles(test_case.base_width + 4);
    expect_tiles(test_case.base_height + 6);
    EXPECT_EQ(changes_, 2);
    EXPECT_EQ(object_.x_, 8);
    EXPECT_EQ(object_.y_, 9);
    EXPECT_EQ(object_.GetLayerValue(), 1);
    EXPECT_FLOAT_EQ(controls_bottom_, before);
  }
}

TEST_F(DungeonWorkbenchObjectSizeUiTest,
       PackedRectangleControlsFitNarrowAndWideInspectors) {
  for (int id : {0xD1, 0xC1, 0xDC, 0xDD}) {
    SCOPED_TRACE(id);
    object_ = zelda3::RoomObject(id, 8, 9, 3, 1);
    for (float scale : {1.0f, 1.5f}) {
      ImGui::GetIO().FontGlobalScale = scale;
      for (float width : {200.0f, 320.0f, 500.0f}) {
        DrawFrame(width);
        DrawFrame(width);
        for (const auto* name :
             {"combo:selected_object_width", "combo:selected_object_height"}) {
          const auto widget = Widget(name);
          ASSERT_TRUE(widget.has_value());
          EXPECT_GT(widget->bounds.max_x - widget->bounds.min_x, 30);
          EXPECT_LE(widget->bounds.max_x, 20 + width);
        }
      }
    }
  }
  EXPECT_EQ(changes_, 0);
  EXPECT_EQ(object_.size_, 3);
}

TEST_F(DungeonWorkbenchObjectSizeUiTest,
       ConfiguredPlatformsOfferVariantsInsteadOfAxes) {
  core::FeatureFlags::get().kEnableCustomObjects = true;
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0xC1, {"closed_a.bin", "closed_b.bin"}},
       {0xDC, {"open_a.bin", "open_b.bin"}},
       {0xDD, {"rock_a.bin", "rock_b.bin"}}});
  for (int id : {0xC1, 0xDC, 0xDD}) {
    SCOPED_TRACE(id);
    object_ = zelda3::RoomObject(id, 8, 9, 1, 1);
    DrawFrame();
    DrawFrame();
    EXPECT_FALSE(Widget("combo:selected_object_width").has_value());
    EXPECT_FALSE(Widget("combo:selected_object_height").has_value());
    Click("combo:selected_object_variant");
    Click("selectable:object_variant_0");
    EXPECT_EQ(object_.size_, 0);
  }
  EXPECT_EQ(changes_, 3);
}

TEST_F(DungeonWorkbenchObjectSizeUiTest,
       ComboRegistrationRemainsStableWhilePopupsAreOpen) {
  struct ComboCase {
    int object_id;
    bool custom;
    const char* combo;
    const char* selected_option;
  };
  const ComboCase cases[] = {
      {0xD1, false, "combo:selected_object_width", "selectable:object_width_0"},
      {0xD1, false, "combo:selected_object_height",
       "selectable:object_height_0"},
      {0x32, true, "combo:selected_object_variant",
       "selectable:object_variant_0"},
  };
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x32, {"furnace.bin", "", "ice_chair.bin"}}});
  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.combo);
    core::FeatureFlags::get().kEnableCustomObjects = test_case.custom;
    object_ = zelda3::RoomObject(test_case.object_id, 8, 9, 0, 1);
    DrawFrame();
    DrawFrame();
    const auto closed = Widget(test_case.combo);
    ASSERT_TRUE(closed.has_value());
    ASSERT_TRUE(closed->bounds.valid);
    ASSERT_GT(closed->bounds.max_y, closed->bounds.min_y);

    Click(test_case.combo);
    ASSERT_TRUE(Widget(test_case.selected_option).has_value());
    const auto open = Widget(test_case.combo);
    ASSERT_TRUE(open.has_value());
    EXPECT_EQ(open->imgui_id, closed->imgui_id);
    EXPECT_EQ(open->window_name, closed->window_name);
    EXPECT_EQ(open->visible, closed->visible);
    EXPECT_EQ(open->enabled, closed->enabled);
    EXPECT_TRUE(open->bounds.valid);
    EXPECT_FLOAT_EQ(open->bounds.min_x, closed->bounds.min_x);
    EXPECT_FLOAT_EQ(open->bounds.min_y, closed->bounds.min_y);
    EXPECT_FLOAT_EQ(open->bounds.max_x, closed->bounds.max_x);
    EXPECT_FLOAT_EQ(open->bounds.max_y, closed->bounds.max_y);
    Click(test_case.selected_option);
  }
  EXPECT_EQ(changes_, 0);
}

TEST_F(DungeonWorkbenchObjectSizeUiTest, IcePropsOfferOnlyMappedVariants) {
  core::FeatureFlags::get().kEnableCustomObjects = true;
  zelda3::CustomObjectManager::Get().SetObjectFileMap(
      {{0x32, {"furnace.bin", "", "ice_chair.bin"}}});
  object_ = zelda3::RoomObject(0x32, 8, 9, 2, 1);
  DrawFrame();
  DrawFrame();
  EXPECT_FALSE(Widget("combo:selected_object_width").has_value());
  Click("combo:selected_object_variant");
  EXPECT_TRUE(Widget("selectable:object_variant_0").has_value());
  EXPECT_TRUE(Widget("selectable:object_variant_2").has_value());
  EXPECT_FALSE(Widget("selectable:object_variant_3").has_value());
  const auto unmapped = Widget("selectable:object_variant_1");
  ASSERT_TRUE(unmapped.has_value());
  EXPECT_FALSE(unmapped->enabled);
  EXPECT_FALSE(workbench::HasEditableRoomObjectSize(
      std::vector<zelda3::RoomObject>{object_}, std::vector<size_t>{0}));
  Click("selectable:object_variant_0");
  EXPECT_EQ(object_.size_, 0);
  EXPECT_EQ(changes_, 1);
}

TEST(DungeonWorkbenchContentLayoutTest,
     PaneLayoutUsesSharedCompactWidthsAtResponsiveThresholds) {
  const auto compact_both = ResolveDungeonWorkbenchPaneLayout(
      938.4f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 280.0f, 320.0f,
      true, true);
  EXPECT_TRUE(compact_both.responsive.show_left);
  EXPECT_TRUE(compact_both.responsive.show_right);
  EXPECT_TRUE(compact_both.responsive.compact_left);
  EXPECT_TRUE(compact_both.responsive.compact_right);
  EXPECT_NEAR(compact_both.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(compact_both.right_width, kCompactRightWidth, 0.001f);
  EXPECT_NEAR(compact_both.center_width, 420.0f, 0.001f);

  const auto hide_left = ResolveDungeonWorkbenchPaneLayout(
      931.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 280.0f, 320.0f,
      true, true);
  EXPECT_TRUE(hide_left.responsive.show_left);
  EXPECT_FALSE(hide_left.responsive.show_right);
  EXPECT_NEAR(hide_left.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(hide_left.right_width, 0.0f, 0.001f);
  EXPECT_NEAR(hide_left.center_width,
              931.0f - kCompactLeftWidth - kSplitterWidth, 0.001f);
}

TEST(DungeonWorkbenchContentLayoutTest,
     OversizedPaneMemoryStillProtectsMinimumCanvasWidth) {
  const auto layout = ResolveDungeonWorkbenchPaneLayout(
      1000.0f, kMinCanvasWidth, kMinSidebarWidth, kSplitterWidth, 500.0f,
      360.0f, true, true);
  EXPECT_TRUE(layout.responsive.show_left);
  EXPECT_TRUE(layout.responsive.show_right);
  EXPECT_TRUE(layout.responsive.compact_left);
  EXPECT_FALSE(layout.responsive.compact_right);
  EXPECT_NEAR(layout.left_width, kCompactLeftWidth, 0.001f);
  EXPECT_NEAR(layout.right_width, 333.6f, 0.001f);
  EXPECT_GE(layout.center_width, kMinCanvasWidth);
}

TEST(DungeonWorkbenchContentLayoutTest,
     ExistingStandaloneToolOwnsThePresentation) {
  EXPECT_EQ(ResolveDungeonWorkbenchToolRequestTarget(false),
            DungeonWorkbenchToolRequestTarget::kEmbeddedInspector);
  EXPECT_EQ(ResolveDungeonWorkbenchToolRequestTarget(true),
            DungeonWorkbenchToolRequestTarget::kStandaloneWindow);
}

TEST(DungeonWorkbenchContentLayoutTest, SwitchingToolsReleasesRailStampInput) {
  int current_room_id = 0x89;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  MinecartTrackEditorPanel panel;
  content.SetEmbeddedToolPanels(nullptr, nullptr, nullptr, &panel);
  panel.set_collision_stamping(true);
  ASSERT_TRUE(panel.collision_stamping());
  content.OpenPaletteTool();
  EXPECT_FALSE(panel.collision_stamping());
  EXPECT_FALSE(panel.canvas_editing());
}

TEST(DungeonWorkbenchContentLayoutTest, ToolRequestsOpenEmbeddedInspector) {
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "room");

  content.OpenDoorTool();
  EXPECT_TRUE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "tools");
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "door");

  content.OpenPaletteTool();
  EXPECT_TRUE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "tools");
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "palette");
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolPickerCyclesAllToolsWithoutLeavingInspector) {
  // The sidebar's compact picker calls Open*Tool() rapidly as users hop
  // between tools. Switching active tools must keep the inspector in Tools
  // mode, and re-opening the same tool must remain a no-op on mode.
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  struct Step {
    void (DungeonWorkbenchContent::*open)();
    const char* expected_id;
  };
  const Step steps[] = {
      {&DungeonWorkbenchContent::OpenDoorTool, "door"},
      {&DungeonWorkbenchContent::OpenSpriteTool, "sprite"},
      {&DungeonWorkbenchContent::OpenItemTool, "item"},
      {&DungeonWorkbenchContent::OpenPaletteTool, "palette"},
      {&DungeonWorkbenchContent::OpenRoomGraphicsTool, "room_graphics"},
      {&DungeonWorkbenchContent::OpenRoomTagsTool, "room_tags"},
      {&DungeonWorkbenchContent::OpenCustomCollisionTool, "custom_collision"},
      {&DungeonWorkbenchContent::OpenWaterFillTool, "water_fill"},
      {&DungeonWorkbenchContent::OpenMinecartTool, "minecart"},
      {&DungeonWorkbenchContent::OpenObjectCoverageTool, "object_coverage"},
  };

  for (const auto& step : steps) {
    (content.*step.open)();
    EXPECT_TRUE(content.IsToolInspectorActiveForTesting()) << step.expected_id;
    EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "tools")
        << step.expected_id;
    EXPECT_STREQ(content.GetActiveToolIdForTesting(), step.expected_id);
  }

  // Re-opening the active tool must keep the inspector on the same tool.
  content.OpenMinecartTool();
  EXPECT_TRUE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "tools");
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "minecart");
}

TEST(DungeonWorkbenchContentLayoutTest,
     PopOutUsesEmbeddedWindowIdentityAndRestoresRoomInspector) {
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  FakeWorkbenchToolContent door_editor("dungeon.door_editor");
  content.SetEmbeddedEditorPanels(nullptr, &door_editor, nullptr, nullptr,
                                  nullptr, nullptr);

  std::string opened_window_id;
  content.SetStandaloneToolCallbacks([](const std::string&) { return false; },
                                     [&](const std::string& window_id) {
                                       opened_window_id = window_id;
                                       return true;
                                     });

  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "room");
  content.OpenDoorTool();
  ASSERT_TRUE(content.IsToolInspectorActiveForTesting());
  EXPECT_TRUE(content.PopOutActiveTool());
  EXPECT_EQ(opened_window_id, "dungeon.door_editor");
  EXPECT_FALSE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "room");
}

TEST(DungeonWorkbenchContentLayoutTest,
     OpenToolRestoresSelectionAndFocusesExistingStandalone) {
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  FakeWorkbenchToolContent object_selector("dungeon.object_selector");
  content.SetEmbeddedEditorPanels(&object_selector, nullptr, nullptr, nullptr,
                                  nullptr, nullptr);

  int focus_count = 0;
  bool standalone_open = false;
  std::string focused_window_id;
  content.SetStandaloneToolCallbacks(
      [&](const std::string& window_id) {
        return standalone_open && window_id == "dungeon.object_selector";
      },
      [&](const std::string& window_id) {
        ++focus_count;
        focused_window_id = window_id;
        return true;
      });

  content.FocusSelectionInspector();
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "selection");
  content.OpenObjectSelectorTool();
  ASSERT_FALSE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "objects");

  standalone_open = true;
  content.OpenObjectSelectorTool();
  EXPECT_FALSE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "selection");
  EXPECT_EQ(focus_count, 1);
  EXPECT_EQ(focused_window_id, "dungeon.object_selector");
}

TEST(DungeonWorkbenchContentLayoutTest,
     ToolInspectorSelectionSurvivesRoomChanges) {
  int current_room_id = 0x011;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  content.OpenCustomCollisionTool();
  current_room_id = 0x012;
  content.NotifyRoomChanged(0x011);

  EXPECT_TRUE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "tools");
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "custom_collision");
}

TEST(DungeonWorkbenchPitDamageTest, BuildsStateWithSafeReplacementDefaults) {
  auto table = MakePitDamageTable({0x001, 0x010});

  auto state = BuildPitDamageMembershipState(&table, 0x001, 0x020, 0x010);

  EXPECT_TRUE(state.table_available);
  EXPECT_TRUE(state.room_valid);
  EXPECT_EQ(state.room_id, 0x001);
  EXPECT_TRUE(state.deals_damage);
  EXPECT_FALSE(state.dirty);
  ASSERT_TRUE(state.suggested_replacement_room.has_value());
  EXPECT_EQ(*state.suggested_replacement_room, 0x020);
  ASSERT_TRUE(state.suggested_victim_room.has_value());
  EXPECT_EQ(*state.suggested_victim_room, 0x010);
}

TEST(DungeonWorkbenchPitDamageTest,
     RepairsDuplicateReplacementAndNonMemberVictimDefaults) {
  auto table = MakePitDamageTable({0x001, 0x010});

  auto state = BuildPitDamageMembershipState(&table, 0x020, 0x010, 0x099);

  EXPECT_TRUE(state.table_available);
  EXPECT_TRUE(state.room_valid);
  EXPECT_FALSE(state.deals_damage);
  ASSERT_TRUE(state.suggested_replacement_room.has_value());
  EXPECT_EQ(*state.suggested_replacement_room, 0x000);
  ASSERT_TRUE(state.suggested_victim_room.has_value());
  EXPECT_EQ(*state.suggested_victim_room, 0x001);
}

TEST(DungeonWorkbenchPitDamageTest,
     AddAndRemoveSwapMembershipWithoutChangingCapacity) {
  auto table = MakePitDamageTable({0x001, 0x010});

  auto add_status = AddCurrentRoomToPitDamage(&table, 0x020, 0x010);
  ASSERT_TRUE(add_status.ok()) << add_status;
  EXPECT_TRUE(table.Contains(0x020));
  EXPECT_FALSE(table.Contains(0x010));
  EXPECT_EQ(table.room_ids().size(), 2u);
  EXPECT_TRUE(table.dirty());

  table.ClearDirty();
  auto remove_status = RemoveCurrentRoomFromPitDamage(&table, 0x020, 0x030);
  ASSERT_TRUE(remove_status.ok()) << remove_status;
  EXPECT_TRUE(table.Contains(0x030));
  EXPECT_FALSE(table.Contains(0x020));
  EXPECT_EQ(table.room_ids().size(), 2u);
  EXPECT_TRUE(table.dirty());
}

TEST(DungeonWorkbenchPitDamageTest, RejectsInvalidMembershipSwaps) {
  auto table = MakePitDamageTable({0x001, 0x010});

  EXPECT_EQ(AddCurrentRoomToPitDamage(nullptr, 0x020, 0x010).code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(AddCurrentRoomToPitDamage(&table, 0x001, 0x010).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(AddCurrentRoomToPitDamage(&table, 0x020, 0x030).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(RemoveCurrentRoomFromPitDamage(&table, 0x001, 0x010).code(),
            absl::StatusCode::kFailedPrecondition);
}

TEST_F(DungeonWorkbenchPitDamageUiTest,
       ButtonsClickThroughToPitDamageMembershipSwaps) {
  int current_room_id = 0x020;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  auto table = MakePitDamageTable({0x001, 0x010});
  content.SetPitDamageTableProvider([&table]() { return &table; });

  DrawPitDamageFrame(content, current_room_id);
  const auto add_rect =
      content.GetPitDamageControlRectsForTesting().add_current;
  ASSERT_TRUE(add_rect.visible);
  ASSERT_LT(add_rect.min_x, add_rect.max_x);
  ASSERT_LT(add_rect.min_y, add_rect.max_y);

  ClickRect(content, current_room_id, add_rect);
  EXPECT_TRUE(table.Contains(0x020));
  EXPECT_FALSE(table.Contains(0x001));
  EXPECT_TRUE(table.Contains(0x010));
  EXPECT_TRUE(table.dirty());

  table.ClearDirty();
  DrawPitDamageFrame(content, current_room_id);
  const auto replace_rect =
      content.GetPitDamageControlRectsForTesting().replace_current;
  ASSERT_TRUE(replace_rect.visible);
  ASSERT_LT(replace_rect.min_x, replace_rect.max_x);
  ASSERT_LT(replace_rect.min_y, replace_rect.max_y);

  ClickRect(content, current_room_id, replace_rect);
  EXPECT_FALSE(table.Contains(0x020));
  EXPECT_TRUE(table.Contains(0x000));
  EXPECT_TRUE(table.Contains(0x010));
  EXPECT_TRUE(table.dirty());
}

TEST(DungeonWorkbenchProjectLabelsTest,
     UsesOracleRegistryDungeonAndRoomNamesWhenProjectIsOpen) {
  const std::filesystem::path root = MakeTempProjectRoot();
  const std::filesystem::path planning = root / "Docs" / "Dev" / "Planning";
  ASSERT_TRUE(std::filesystem::create_directories(planning));
  {
    std::ofstream out(planning / "dungeons.json");
    out << R"json({
      "dungeons": [
        {
          "id": "D4",
          "name": "Zora Temple",
          "vanilla_name": "Thieves' Town",
          "rooms": [
            {
              "id": "0x25",
              "name": "Water Grate",
              "grid_row": 1,
              "grid_col": 2,
              "type": "connector",
              "palette": 1,
              "blockset": 6,
              "spriteset": 3,
              "tag1": 0,
              "tag2": 0
            }
          ]
        }
      ]
    })json";
  }

  project::YazeProject project;
  project.name = "Oracle of Secrets";
  project.filepath = (root / "Oracle-of-Secrets.yaze").string();
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json({
                    "manifest_version": 1,
                    "hack_name": "Oracle of Secrets"
                  })json")
                  .ok());
  ASSERT_TRUE(project.hack_manifest.LoadProjectRegistry(root.string()).ok());

  EXPECT_EQ(dungeon_project_labels::GetDungeonNameForRoom(&project, 0x25),
            "D4 Zora Temple");
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Water Grate");

  zelda3::ResourceLabelProvider::ProjectLabels stale_labels;
  stale_labels["room"]["37"] = "Thieves' Town";
  zelda3::GetResourceLabels().SetProjectLabels(&stale_labels);
  zelda3::GetResourceLabels().SetHackManifest(&project.hack_manifest);
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Water Grate");
  EXPECT_EQ(zelda3::GetRoomLabel(0x25), "Water Grate");
  zelda3::GetResourceLabels().SetHackManifest(nullptr);
  zelda3::GetResourceLabels().SetProjectLabels(nullptr);

  std::filesystem::remove_all(root);
}

}  // namespace
}  // namespace yaze::editor
