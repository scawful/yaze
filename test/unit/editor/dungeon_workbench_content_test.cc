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
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/inspectors/door_editor_content.h"
#include "app/editor/dungeon/ui/window/custom_collision_panel.h"
#include "app/editor/dungeon/ui/window/dungeon_entrances_panel.h"
#include "app/editor/dungeon/ui/window/item_editor_panel.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/dungeon/ui/window/sprite_editor_panel.h"
#include "app/editor/dungeon/ui/window/water_fill_panel.h"
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
#include "rom/snes.h"
#include "test_utils/expanded_entrance_fixture.h"
#include "util/file_util.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/pit_damage_table.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {

class DungeonCanvasViewerTestPeer {
 public:
  static void BindViewer(DungeonEditorV2& editor, Rom* rom,
                         DungeonCanvasViewer* viewer, int room_id) {
    editor.SetRom(rom);
    viewer->RefreshRomBackedState(rom, nullptr, &editor.rooms(), room_id);
    viewer->object_interaction().SetCurrentRoom(&editor.rooms(), room_id);
    editor.ConfigureViewerUndoHooks(viewer);
  }

  static DungeonCanvasViewer& MakeOwnedOverviewViewer(
      DungeonEditorV2& editor, Rom* rom, zelda3::GameData* game_data,
      int room_id) {
    editor.SetRom(rom);
    editor.SetGameData(game_data);
    editor.workbench_viewer_ = std::make_unique<DungeonCanvasViewer>();
    auto& viewer = *editor.workbench_viewer_;
    viewer.RefreshRomBackedState(rom, game_data, &editor.rooms(), room_id);
    viewer.object_interaction().SetCurrentRoom(&editor.rooms(), room_id);
    editor.ConfigureViewerUndoHooks(&viewer);
    return viewer;
  }

  static int CachedOverviewRoomCount(DungeonCanvasViewer& viewer, int room_id) {
    viewer.GetConnectedContentSize(room_id);
    return viewer.connected_graph_cache_.room_count;
  }

  static std::function<void()> PitDamageChanged(DungeonEditorV2& editor) {
    return [&editor]() {
      editor.InvalidateConnectedRoomGraphs();
    };
  }

  static absl::Status ImportCollision(
      DungeonEditorV2& editor,
      const std::vector<zelda3::CustomCollisionRoomEntry>& entries) {
    return editor.ApplyCustomCollisionImportBatch(entries);
  }

  static void OpenRoom(DungeonEditorV2& editor, int room_id) {
    editor.OnRoomSelected(room_id, /*request_focus=*/false);
  }
};

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
  void Draw(bool*) override { ++draw_count; }
  int draw_count = 0;

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
    gui::WidgetIdRegistry::Instance().Clear();
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

  void ClickPitControl(DungeonWorkbenchContent& content, int room_id,
                       const char* control) {
    const auto label = gui::WidgetIdRegistry::NormalizeLabel(control);
    std::optional<DungeonWorkbenchTestRect> details;
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (path.starts_with("Dungeon/Workbench/") && widget.label == label) {
        ASSERT_TRUE(widget.visible);
        ASSERT_TRUE(widget.enabled);
        details = DungeonWorkbenchTestRect{
            widget.visible, widget.bounds.min_x, widget.bounds.min_y,
            widget.bounds.max_x, widget.bounds.max_y};
        break;
      }
    }
    ASSERT_TRUE(details.has_value());
    ClickRect(content, room_id, *details);
  }

  void ClickRect(DungeonWorkbenchContent& content, int room_id,
                 const DungeonWorkbenchTestRect& rect) {
    const float center_x = (rect.min_x + rect.max_x) * 0.5f;
    const float center_y = (rect.min_y + rect.max_y) * 0.5f;

    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(center_x, center_y);
    // Resolve the hovered window before pressing, as native pointer input does.
    DrawPitDamageFrame(content, room_id);
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

TEST(DungeonWorkbenchNavigatorUiTest,
     PlacementLibrariesStaySeparateFromEntrances) {
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
  std::array<zelda3::RoomEntrance, zelda3::kNumDungeonEntranceSlots> entrances;
  std::array<zelda3::DungeonSpawnPoint, zelda3::kNumDungeonSpawnPoints> spawns;
  DungeonEntrancesPanel entrance_panel(&entrances, &spawns, &entrance_slot,
                                       [](int) {});
  const std::deque<int> recent;
  auto content = MakeWorkbenchForToolStateTests(room_id, recent);
  ImVector<int> active_rooms;
  RoomMatrixContent matrix(&room_id, &active_rooms, [](int) {});
  FakeWorkbenchToolContent objects("dungeon.object_selector");
  FakeWorkbenchToolContent sprites("dungeon.sprite_editor");
  FakeWorkbenchToolContent doors("dungeon.door_editor");
  FakeWorkbenchToolContent items("dungeon.item_editor");
  content.SetNavigationPanels(&matrix, &entrance_panel);
  content.SetEmbeddedEditorPanels(&objects, &doors, &sprites, &items, nullptr,
                                  nullptr);
  float host_width = 280;
  const auto frame = [&]() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(host_width + 20, 640), ImGuiCond_Always);
    ImGui::Begin("NavigatorHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    DungeonWorkbenchContentTestPeer::DrawNavigator(content, host_width, 600);
    ImGui::End();
    ImGui::Render();
  };
  const auto widget = [](const char* label)
      -> std::optional<gui::WidgetIdRegistry::WidgetInfo> {
    for (const auto& [path, info] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets())
      if (info.label == gui::WidgetIdRegistry::NormalizeLabel(label))
        return info;
    return std::nullopt;
  };
  const auto click = [&](const char* label) {
    frame();
    const auto target = widget(label);
    ASSERT_TRUE(target.has_value()) << label;
    ASSERT_TRUE(target->visible);
    io.AddMousePosEvent((target->bounds.min_x + target->bounds.max_x) / 2,
                        (target->bounds.min_y + target->bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
  };
  for (float width : {260.0f, 320.0f}) {
    host_width = width;
    frame();
    frame();
    EXPECT_FALSE(widget("Graphics entrance").has_value());
    EXPECT_FALSE(widget("Navigator Entrances").has_value());
    for (const char* kind :
         {"Place Objects", "Place Sprites", "Place Doors", "Place Items"}) {
      const auto choice = widget(kind);
      ASSERT_TRUE(choice.has_value()) << kind;
      EXPECT_TRUE(choice->visible) << kind;
      EXPECT_EQ(choice->type, "button") << kind;
      const char* text = kind + std::string("Place ").size();
      EXPECT_GE(
          choice->bounds.max_x - choice->bounds.min_x,
          ImGui::CalcTextSize(text).x + ImGui::GetStyle().FramePadding.x * 2)
          << kind;
      EXPECT_LT(choice->bounds.max_x, 20 + host_width + 20) << kind;
      click(kind);
      EXPECT_FALSE(content.IsToolInspectorActiveForTesting());
    }
    EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "items");
    float tools_bottom = -1, matrix_top = -1;
    for (ImGuiWindow* window : context->Windows) {
      if (!window->Active)
        continue;
      const std::string name(window->Name);
      if (name.find("/##NavigatorTools_") != std::string::npos)
        tools_bottom = window->Pos.y + window->Size.y;
      if (name.find("/##NavigatorMatrix_") != std::string::npos)
        matrix_top = window->Pos.y;
    }
    EXPECT_GT(tools_bottom, 0);
    EXPECT_GE(matrix_top, tools_bottom);
  }
  EXPECT_GT(objects.draw_count, 0);
  EXPECT_GT(sprites.draw_count, 0);
  EXPECT_GT(doors.draw_count, 0);
  EXPECT_GT(items.draw_count, 0);
  int focused = 0;
  content.SetStandaloneToolCallbacks(
      [](const std::string& id) { return id == "dungeon.sprite_editor"; },
      [&](const std::string&) {
        ++focused;
        return true;
      });
  const int sprite_draws = sprites.draw_count;
  click("Place Sprites");
  EXPECT_EQ(focused, 1);
  EXPECT_EQ(sprites.draw_count, sprite_draws);
  content.FocusEntranceBrowser();
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "inspect");
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "items");
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
     NavigatorDefaultsToObjectsAndKeepsMatrixBelow) {
  int current_room_id = 0;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  EXPECT_TRUE(content.IsInspectorOnLeft());
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "objects");
  content.FocusEntranceBrowser();
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "objects");
  content.FocusRoomMatrix();
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "objects");
}

TEST(DungeonWorkbenchContentLayoutTest, FocusRoomMatrixExpandsCollapsedMatrix) {
  int current_room_id = 0;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);

  DungeonWorkbenchContentTestPeer::SetRoomMatrixVisible(content, false);
  content.FocusRoomMatrix();
  EXPECT_TRUE(DungeonWorkbenchContentTestPeer::IsRoomMatrixVisible(content));
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

class DungeonWorkbenchResetUiTest : public DungeonWorkbenchObjectSizeUiTest {
 protected:
  void SetUp() override {
    DungeonWorkbenchObjectSizeUiTest::SetUp();
    ImGui::GetIO().ConfigInputTrickleEventQueue = false;
    rooms_[0].SetLoaded(true);
    viewer_.RefreshRomBackedState(nullptr, nullptr, &rooms_, 0);
    viewer_.SetProject(&project_);
    project_.resource_labels["sprite"]["1"] = "Oracle test guard";
    doors_.SetRooms(&rooms_);
    doors_.SetCurrentRoom(room_id_);
    doors_.SetCanvasViewer(&viewer_);
    content_.SetEmbeddedEditorPanels(nullptr, &doors_, &sprites_, &items_,
                                     nullptr, &palette_);
    content_.SetEmbeddedToolPanels(nullptr, nullptr, nullptr, &tracks_);
  }

  void Frame() {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width_ + 20, 640), ImGuiCond_Always);
    ImGui::Begin("ResetHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::LogToBuffer();
    ImGui::GetCurrentContext()->LogDepthToExpand = 0;
    if (show_tools_toolbar_)
      DungeonWorkbenchContentTestPeer::DrawToolsToolbar(content_);
    if (standalone_content_)
      standalone_content_->Draw(nullptr);
    else if (show_standalone_sprites_)
      sprites_.Draw(nullptr);
    else if (show_library_)
      DungeonWorkbenchContentTestPeer::DrawNavigator(content_, width_, 600);
    else
      DungeonWorkbenchContentTestPeer::DrawInspectorPane(content_, viewer_,
                                                         width_, 600, true);
    text_ = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> Find(const char* name) {
    for (const auto& [path, widget] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets())
      if (widget.label == gui::WidgetIdRegistry::NormalizeLabel(name))
        return widget;
    return std::nullopt;
  }

  void Click(const char* name) {
    Frame();
    const auto widget = Find(name);
    ASSERT_TRUE(widget.has_value()) << name;
    ASSERT_TRUE(widget->visible) << name;
    ASSERT_TRUE(widget->enabled) << name;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((widget->bounds.min_x + widget->bounds.max_x) / 2,
                        (widget->bounds.min_y + widget->bounds.max_y) / 2);
    Frame();
    io.AddMouseButtonEvent(0, true);
    Frame();
    io.AddMouseButtonEvent(0, false);
    Frame();
  }

  int room_id_ = 0;
  std::deque<int> recent_;
  DungeonRoomStore rooms_;
  DungeonCanvasViewer viewer_;
  project::YazeProject project_;
  SpriteEditorPanel sprites_{&room_id_, &rooms_, &viewer_};
  DoorEditorContent doors_;
  ItemEditorPanel items_{&room_id_, &rooms_, &viewer_};
  WindowContent* standalone_content_ = nullptr;
  bool show_tools_toolbar_ = false;
  FakeWorkbenchToolContent palette_{"dungeon.palette_editor"};
  MinecartTrackEditorPanel tracks_;
  DungeonWorkbenchContent content_{
      nullptr,
      &room_id_,
      [](int) {},
      [](int, RoomSelectionIntent) {},
      [](int) {},
      [] {},
      [this] { return &viewer_; },
      [](int) -> DungeonCanvasViewer* { return nullptr; },
      [this]() -> const std::deque<int>& { return recent_; },
      [](int) {},
      [](bool) {}};
  float width_ = 260;
  bool show_library_ = false;
  bool show_standalone_sprites_ = false;
  std::string text_;
};

TEST_F(DungeonWorkbenchResetUiTest,
       SpriteLibraryUsesNamesAndRealPlacementWithoutStaleMode) {
  show_library_ = true;
  for (float width : {260.0f, 320.0f}) {
    width_ = width;
    Frame();
    Frame();
    Click("Place Sprites");
    EXPECT_FALSE(content_.IsToolInspectorActiveForTesting());
    EXPECT_NE(text_.find("Oracle test guard"), std::string::npos);
    auto search = Find("sprite_search");
    ASSERT_TRUE(search.has_value());
    EXPECT_TRUE(search->visible);
    EXPECT_LT(search->bounds.max_x, 20 + width + 20);
    if (width == 260.0f) {
      Click("sprite_search");
      ImGui::GetIO().AddInputCharactersUTF8("Oracle");
      Frame();
      EXPECT_TRUE(Find("place_sprite_01").has_value());
      EXPECT_FALSE(Find("place_sprite_00").has_value());
    }
    Click("place_sprite_01");
    auto& interaction = viewer_.object_interaction();
    ASSERT_TRUE(interaction.IsSpritePlacementActive());
    EXPECT_EQ(interaction.entity_coordinator().sprite_handler().GetSpriteId(),
              1);
    const size_t before = rooms_[0].GetSprites().size();
    EXPECT_TRUE(
        interaction.entity_coordinator().sprite_handler().HandleClick(32, 48));
    ASSERT_EQ(rooms_[0].GetSprites().size(), before + 1);
    EXPECT_EQ(rooms_[0].GetSprites().back().id(), 1);
    EXPECT_EQ(rooms_[0].GetSprites().back().x(), 2);
    EXPECT_EQ(rooms_[0].GetSprites().back().y(), 3);
    Click("finish_sprite_placement");
    EXPECT_FALSE(interaction.IsSpritePlacementActive());
    Frame();
    EXPECT_FALSE(Find("finish_sprite_placement").has_value());
    Click("place_sprite_01");
    interaction.CancelPlacement();  // Same cancellation used by canvas Escape.
    Frame();
    EXPECT_FALSE(Find("finish_sprite_placement").has_value());
    viewer_.SetHeaderReadOnly(true);
    Frame();
    auto choice = Find("place_sprite_01");
    ASSERT_TRUE(choice.has_value());
    EXPECT_FALSE(choice->enabled);
    viewer_.SetHeaderReadOnly(false);
  }
  EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
}

TEST_F(DungeonWorkbenchResetUiTest,
       DoorAndItemWorkbenchLibrariesExcludeStandaloneSelectedFields) {
  rooms_[0].GetDoors().push_back(zelda3::Room::Door::FromRomBytes(0x10, 0x00));
  rooms_[0].GetPotItems().push_back(zelda3::PotItem{0x2714, 0x09});
  for (float width : {260.0f, 320.0f}) {
    width_ = width;
    for (EntityType type : {EntityType::Door, EntityType::Item}) {
      SCOPED_TRACE(width);
      SCOPED_TRACE(static_cast<int>(type));
      const bool door = type == EntityType::Door;
      const char* picker = door ? "place_door_00" : "place_item_00";
      const char* raw =
          door ? "selected_door_raw_type" : "selected_item_raw_type";
      const char* row = door ? "door_row_0" : "item_row_0";
      viewer_.object_interaction().SelectEntity(type, 0);
      show_library_ = true;
      standalone_content_ = nullptr;
      Frame();
      Frame();
      Click(door ? "Place Doors" : "Place Items");
      ASSERT_TRUE(Find(picker).has_value());
      EXPECT_TRUE(Find(picker)->visible);
      EXPECT_TRUE(Find(picker)->enabled);
      for (const auto& [path, widget] :
           gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
        if (widget.visible &&
            widget.label.starts_with(door ? "place_door_" : "place_item_"))
          EXPECT_LE(widget.bounds.max_x, 20 + width + 20);
      }
      EXPECT_FALSE(Find(raw).has_value());
      EXPECT_FALSE(Find(row).has_value());
      Click(picker);
      EXPECT_TRUE(door ? viewer_.object_interaction().IsDoorPlacementActive()
                       : viewer_.object_interaction().IsItemPlacementActive());
      viewer_.object_interaction().CancelPlacement();
      viewer_.object_interaction().SelectEntity(type, 0);

      standalone_content_ = door ? static_cast<WindowContent*>(&doors_)
                                 : static_cast<WindowContent*>(&items_);
      Frame();
      Frame();
      ASSERT_TRUE(Find(raw).has_value());
      EXPECT_TRUE(Find(raw)->visible);
      EXPECT_TRUE(Find(raw)->enabled);
      EXPECT_TRUE(Find(row).has_value());
      EXPECT_LT(Find(raw)->bounds.max_x, 20 + width + 20);
      viewer_.SetHeaderReadOnly(true);
      Frame();
      ASSERT_TRUE(Find(raw).has_value());
      ASSERT_TRUE(Find(picker).has_value());
      EXPECT_FALSE(Find(raw)->enabled);
      EXPECT_FALSE(Find(picker)->enabled);
      viewer_.SetHeaderReadOnly(false);
      viewer_.SetObjectInteractionEnabled(false);
      Frame();
      ASSERT_TRUE(Find(raw).has_value());
      EXPECT_FALSE(Find(raw)->enabled);
      viewer_.SetObjectInteractionEnabled(true);

      // The panel must not reuse another canvas room's selected index.
      viewer_.object_interaction().SetCurrentRoom(&rooms_, 1);
      Frame();
      EXPECT_FALSE(Find(raw).has_value());
      ASSERT_TRUE(Find(picker).has_value());
      EXPECT_FALSE(Find(picker)->enabled);
      viewer_.object_interaction().SetCurrentRoom(&rooms_, 0);
    }
  }
  EXPECT_FALSE(rooms_[0].object_stream_dirty());
  EXPECT_FALSE(rooms_[0].pot_items_dirty());
  EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
}

TEST_F(DungeonWorkbenchResetUiTest,
       PotSaveNoticeFollowsSettingBeforeEditingInBothHosts) {
  auto& flags = core::FeatureFlags::get().dungeon;
  const bool previous_save = flags.kSavePotItems;
  absl::Cleanup cleanup = [&] {
    flags.kSavePotItems = previous_save;
  };
  rooms_[0].GetPotItems().push_back(zelda3::PotItem{0x2714, 0x09});
  viewer_.object_interaction().SelectEntity(EntityType::Item, 0);
  int mutations = 0;
  viewer_.object_interaction().SetMutationCallback([&] { ++mutations; });

  for (float width : {260.0f, 320.0f}) {
    width_ = width;
    for (bool standalone : {false, true}) {
      SCOPED_TRACE(width);
      SCOPED_TRACE(standalone);
      standalone_content_ = standalone ? &items_ : nullptr;
      for (bool save_enabled : {false, true, false}) {
        flags.kSavePotItems = save_enabled;
        Frame();
        Frame();
        const auto notice = text_.find("Pot-item saving is off.");
        EXPECT_EQ(notice != std::string::npos, !save_enabled);
        const auto editor =
            Find(standalone ? "selected_item_raw_type" : "ItemType");
        ASSERT_TRUE(editor.has_value());
        EXPECT_TRUE(editor->visible);
        EXPECT_TRUE(editor->enabled);
        EXPECT_GE(editor->bounds.min_x, 20);
        EXPECT_LE(editor->bounds.max_x, 20 + width + 20);
        if (!save_enabled) {
          const auto control =
              text_.find(standalone ? "Selected raw type" : "Type");
          ASSERT_NE(control, std::string::npos);
          EXPECT_LT(notice, control);
        }
        EXPECT_EQ(flags.kSavePotItems, save_enabled);
        EXPECT_EQ(rooms_[0].GetPotItems().front().item, 0x09);
        EXPECT_FALSE(rooms_[0].pot_items_dirty());
        EXPECT_EQ(mutations, 0);
      }
    }
  }
  EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
}

TEST_F(DungeonWorkbenchResetUiTest,
       ItemLibraryWarnsBeforePlacementWithoutChangingSaveSetting) {
  auto& flags = core::FeatureFlags::get().dungeon;
  const bool previous_save = flags.kSavePotItems;
  absl::Cleanup cleanup = [&] {
    flags.kSavePotItems = previous_save;
  };
  flags.kSavePotItems = false;
  show_library_ = true;
  int mutations = 0;
  viewer_.object_interaction().SetMutationCallback([&] { ++mutations; });
  Frame();
  Frame();
  Click("Place Items");
  const auto notice = text_.find("Pot-item saving is off.");
  const auto picker = Find("place_item_08");
  ASSERT_NE(notice, std::string::npos);
  ASSERT_TRUE(picker.has_value());
  const auto selector = text_.find("Select Item:");
  ASSERT_NE(selector, std::string::npos);
  EXPECT_LT(notice, selector);
  EXPECT_TRUE(picker->visible);
  EXPECT_TRUE(picker->enabled);
  EXPECT_FALSE(viewer_.object_interaction().IsItemPlacementActive());
  Click("place_item_08");
  EXPECT_TRUE(viewer_.object_interaction().IsItemPlacementActive());
  EXPECT_EQ(viewer_.object_interaction()
                .entity_coordinator()
                .item_handler()
                .GetItemId(),
            8);
  EXPECT_FALSE(flags.kSavePotItems);
  EXPECT_FALSE(rooms_[0].pot_items_dirty());
  EXPECT_EQ(mutations, 0);
  flags.kSavePotItems = true;
  Frame();
  EXPECT_EQ(text_.find("Pot-item saving is off."), std::string::npos);
  EXPECT_TRUE(viewer_.object_interaction().IsItemPlacementActive());
}

TEST_F(DungeonWorkbenchResetUiTest,
       StandaloneRawTypesKeepExistingMutationsAndRejectReadOnlyCommit) {
  rooms_[0].GetDoors().push_back(zelda3::Room::Door::FromRomBytes(0x10, 0x00));
  rooms_[0].GetPotItems().push_back(zelda3::PotItem{0x2714, 0x09});
  int mutations = 0;
  viewer_.object_interaction().SetMutationCallback([&] { ++mutations; });
  const auto enter = [&] {
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
    Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);
    Frame();
  };
  for (EntityType type : {EntityType::Door, EntityType::Item}) {
    const bool door = type == EntityType::Door;
    const char* raw =
        door ? "selected_door_raw_type" : "selected_item_raw_type";
    standalone_content_ = door ? static_cast<WindowContent*>(&doors_)
                               : static_cast<WindowContent*>(&items_);
    viewer_.object_interaction().SelectEntity(type, 0);
    Frame();
    Frame();
    const auto before_door = rooms_[0].GetDoors().front();
    const auto before_item = rooms_[0].GetPotItems().front();
    const int before_mutations = mutations;
    Click(raw);
    ImGui::GetIO().AddInputCharactersUTF8("0A");
    Frame();
    EXPECT_EQ(mutations, before_mutations);
    enter();
    EXPECT_EQ(mutations, before_mutations + 1);
    if (door) {
      EXPECT_EQ(static_cast<uint8_t>(rooms_[0].GetDoors().front().type), 0x0A);
      EXPECT_EQ(rooms_[0].GetDoors().front().direction, before_door.direction);
      EXPECT_EQ(rooms_[0].GetDoors().front().position, before_door.position);
    } else {
      EXPECT_EQ(rooms_[0].GetPotItems().front().item, 0x0A);
      EXPECT_EQ(rooms_[0].GetPotItems().front().position, before_item.position);
    }
    Click(raw);
    ImGui::GetIO().AddInputCharactersUTF8("02");
    Frame();
    viewer_.SetHeaderReadOnly(true);
    enter();
    EXPECT_EQ(mutations, before_mutations + 1);
    EXPECT_EQ(door ? static_cast<uint8_t>(rooms_[0].GetDoors().front().type)
                   : rooms_[0].GetPotItems().front().item,
              0x0A);
    viewer_.SetHeaderReadOnly(false);
  }
  viewer_.object_interaction().SetMutationCallback({});
}

TEST_F(DungeonWorkbenchResetUiTest,
       StandaloneAndWorkbenchShareSelectedSpriteFieldsAndMutationGuards) {
  rooms_[0].GetSprites().emplace_back(1, 2, 3, 2, 0);
  rooms_[0].GetSprites().front().set_key_drop(1);
  rooms_[0].GetSprites().emplace_back(2, 7, 8, 3, 1);
  auto& interaction = viewer_.object_interaction();
  interaction.SelectEntity(EntityType::Sprite, 0);
  int mutations = 0;
  int entity_changes = 0;
  interaction.SetMutationCallback([&] { ++mutations; });
  interaction.SetEntityChangedCallback([&] { ++entity_changes; });
  auto& handler = interaction.entity_coordinator().sprite_handler();

  for (float width : {260.0f, 320.0f}) {
    width_ = width;
    for (bool standalone : {false, true}) {
      SCOPED_TRACE(width);
      SCOPED_TRACE(standalone);
      show_standalone_sprites_ = standalone;
      Frame();
      Frame();
      EXPECT_NE(text_.find("Oracle test guard"), std::string::npos);
      for (const char* field :
           {"SpriteId", "SpriteX", "SpriteY", "SpriteSubtype", "SpriteLayer",
            "SpriteKeyDrop"}) {
        const auto widget = Find(field);
        ASSERT_TRUE(widget.has_value()) << field;
        EXPECT_TRUE(widget->visible) << field;
        EXPECT_TRUE(widget->enabled) << field;
        EXPECT_GE(widget->bounds.min_x, 20);
        EXPECT_LE(widget->bounds.max_x, 20 + width + 20);
        int count = 0;
        for (const auto& [path, registered] :
             gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
          if (registered.label ==
              gui::WidgetIdRegistry::NormalizeLabel(field)) {
            ++count;
            EXPECT_NE(path.find("Dungeon/EntityInspector/"), std::string::npos);
          }
        }
        EXPECT_EQ(count, 1) << field;
      }
      EXPECT_FALSE(Find("selected_sprite_x").has_value());
      EXPECT_FALSE(Find("selected_sprite_y").has_value());
      const int before_x = rooms_[0].GetSprites().front().x();
      const int before_mutations = mutations;
      Click("SpriteXIncrease");
      EXPECT_EQ(rooms_[0].GetSprites().front().x(), before_x + 1);
      EXPECT_EQ(rooms_[0].GetSprites().front().y(), 3);
      EXPECT_EQ(rooms_[0].GetSprites().front().id(), 1);
      EXPECT_EQ(rooms_[0].GetSprites().front().subtype(), 2);
      EXPECT_EQ(rooms_[0].GetSprites().front().layer(), 0);
      EXPECT_EQ(rooms_[0].GetSprites().front().key_drop(), 1);
      EXPECT_EQ(rooms_[0].GetSprites()[1].x(), 7);
      EXPECT_EQ(mutations, before_mutations + 1);
      EXPECT_EQ(entity_changes, mutations);
      EXPECT_TRUE(rooms_[0].sprites_dirty());

      // A rejected save/storage preflight must be identical in both hosts.
      int preflights = 0;
      handler.SetSpriteMutationPreflight(
          [&](int room, const std::vector<zelda3::Sprite>& proposed) {
            ++preflights;
            EXPECT_EQ(room, 0);
            EXPECT_EQ(proposed.front().x(), before_x + 2);
            return absl::FailedPreconditionError("fixture storage rejection");
          });
      Click("SpriteXIncrease");
      EXPECT_EQ(preflights, 1);
      EXPECT_EQ(rooms_[0].GetSprites().front().x(), before_x + 1);
      EXPECT_EQ(mutations, before_mutations + 1);
      EXPECT_FALSE(handler.mutation_status().ok());
      handler.SetSpriteMutationPreflight({});

      viewer_.SetHeaderReadOnly(true);
      Frame();
      for (const char* field :
           {"SpriteId", "SpriteX", "SpriteY", "SpriteSubtype", "SpriteLayer",
            "SpriteKeyDrop"}) {
        ASSERT_TRUE(Find(field).has_value());
        EXPECT_FALSE(Find(field)->enabled) << field;
      }
      viewer_.SetHeaderReadOnly(false);
    }
  }
  EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
}

TEST_F(DungeonWorkbenchResetUiTest,
       CompactInspectFollowsSelectionAndAlwaysDrawsRoomFields) {
  rooms_[0].GetSprites().emplace_back(1, 2, 3, 0, 0);
  for (float width : {260.0f, 320.0f}) {
    width_ = width;
    viewer_.object_interaction().SelectEntity(EntityType::Sprite, 0);
    Frame();
    Frame();
    ASSERT_TRUE(Find("SpriteX").has_value());
    EXPECT_TRUE(Find("SpriteX")->visible);
    for (const char* field : {"RoomHeaderTag1", "RoomHeaderTag2",
                              "RoomHeaderPalette", "RoomHeaderCollision"})
      EXPECT_TRUE(Find(field).has_value()) << field;
    for (const char* old_mode :
         {"mode_room", "mode_selection", "mode_tools", "room_id"})
      EXPECT_FALSE(Find(old_mode).has_value()) << old_mode;
    viewer_.object_interaction().ClearEntitySelection();
    Frame();
    EXPECT_FALSE(Find("SpriteX").has_value());
    ASSERT_TRUE(Find("RoomHeaderTag1").has_value());
    EXPECT_TRUE(Find("RoomHeaderTag1")->visible);
  }
  EXPECT_FALSE(rooms_[0].header_dirty());
  EXPECT_FALSE(rooms_[0].sprites_dirty());
}

TEST_F(DungeonWorkbenchResetUiTest, ToolsMenuAndBackReleaseRailEditing) {
  width_ = 720;
  show_tools_toolbar_ = true;
  // Opening the real tool audits materialized rooms through their lazy object
  // and sprite loaders. SetLoaded alone does not initialize those domains.
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  constexpr int kObjectTable = 0x0F8000;
  constexpr int kObjectStream = 0x100000;
  constexpr int kSpriteTable = 0x048000;
  constexpr int kSpriteStream = 0x049000;
  const auto write_long = [&](int pc, uint32_t value) {
    rom.mutable_data()[pc] = value & 0xFF;
    rom.mutable_data()[pc + 1] = (value >> 8) & 0xFF;
    rom.mutable_data()[pc + 2] = (value >> 16) & 0xFF;
  };
  write_long(zelda3::kRoomObjectPointer, PcToSnes(kObjectTable));
  ASSERT_TRUE(rom.WriteWord(zelda3::kRoomsSpritePointer,
                            static_cast<uint16_t>(PcToSnes(kSpriteTable)))
                  .ok());
  for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
    write_long(kObjectTable + room * 3, PcToSnes(kObjectStream));
    ASSERT_TRUE(rom.WriteWord(kSpriteTable + room * 2,
                              static_cast<uint16_t>(PcToSnes(kSpriteStream)))
                    .ok());
  }
  const auto track =
      zelda3::RoomObject(0x31, 10, 10, 0, 0).EncodeObjectToBytes();
  ASSERT_TRUE(rom.WriteVector(kObjectStream,
                              {0x00, 0x00, track.b1, track.b2, track.b3, 0xFF,
                               0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF})
                  .ok());
  ASSERT_TRUE(
      rom.WriteVector(kSpriteStream, {0x00, 0x05, 0x05, 0xA3, 0xFF}).ok());
  rooms_.SetRom(&rom);
  ASSERT_FALSE(rooms_[0].AreObjectsLoaded());
  ASSERT_FALSE(rooms_[0].AreSpritesLoaded());
  const auto root = MakeTempProjectRoot();
  std::filesystem::create_directories(root);
  absl::Cleanup cleanup = [&] {
    tracks_.DetachRuntimeContext();
    rooms_.SetRom(nullptr);
    std::error_code error;
    std::filesystem::remove_all(root, error);
  };
  project_.filepath = (root / "tracks.yaze").string();
  std::ofstream(project_.filepath) << "name=Track test\n";
  ASSERT_TRUE(project_.hack_manifest
                  .LoadFromString(R"json({
    "manifest_version": 3,
    "minecart_tracks": {"source": {"format": "yaze-minecart-track-table",
      "version": 1, "path": "tracks.asm"}}
  })json")
                  .ok());
  {
    std::ofstream source(root / "tracks.asm");
    for (const char* label :
         {".TrackStartingRooms", ".TrackStartingX", ".TrackStartingY"}) {
      source << label << "\n";
      for (int i = 0; i < 32; ++i)
        source << "  dw $0000\n";
    }
  }
  tracks_.SetRooms(&rooms_);
  ASSERT_TRUE(tracks_.SetProject(&project_).ok());
  Frame();
  Frame();
  Click("tool_picker");
  Click("tool_minecart");
  ASSERT_TRUE(content_.IsToolInspectorActiveForTesting());
  EXPECT_TRUE(tracks_.canvas_editing());
  EXPECT_TRUE(rooms_[0].AreObjectsLoaded());
  EXPECT_TRUE(rooms_[0].AreSpritesLoaded());
  ASSERT_EQ(rooms_[0].GetTileObjects().size(), 1u);
  EXPECT_EQ(rooms_[0].GetTileObjects().front().id_, 0x31);
  ASSERT_EQ(rooms_[0].GetSprites().size(), 1u);
  EXPECT_EQ(rooms_[0].GetSprites().front().id(), 0xA3);
  tracks_.set_collision_stamping(true);
  Click("back_to_inspect");
  Frame();
  EXPECT_FALSE(content_.IsToolInspectorActiveForTesting());
  EXPECT_FALSE(tracks_.canvas_editing());
  EXPECT_FALSE(tracks_.collision_stamping());
  ASSERT_TRUE(Find("RoomHeaderTag1").has_value());
  EXPECT_TRUE(Find("RoomHeaderTag1")->visible);
  EXPECT_EQ(ImGui::GetCurrentContext()->StyleVarStack.Size, 0);
  EXPECT_EQ(ImGui::GetCurrentContext()->ColorStack.Size, 0);
}

TEST_F(DungeonWorkbenchPlacementUiTest,
       DefaultPlacementSelectsResultAndPlaceAnotherRestoresPreviewControls) {
  auto& interaction = viewer_.object_interaction();
  DrawInspectorFrame();
  DrawInspectorFrame();
  ASSERT_TRUE(Widget("checkbox:repeat_placement").has_value());
  ASSERT_TRUE(Widget("combo:selected_object_width").has_value());
  EXPECT_EQ(interaction.GetPlacementPolicy(),
            DungeonObjectInteraction::PlacementPolicy::kOnce);
  ASSERT_NE(interaction.GetPlacementPreview(), nullptr);
  const auto preview = *interaction.GetPlacementPreview();
  ASSERT_TRUE(interaction.entity_coordinator().tile_handler().PlaceObjectAt(
      0, preview, 12, 14));
  DrawInspectorFrame();
  EXPECT_STREQ(content_.GetInspectorModeIdForTesting(), "inspect");
  ASSERT_EQ(interaction.GetSelectedObjectIndices(), std::vector<size_t>{0});
  EXPECT_EQ(interaction.GetPlacementPreview(), nullptr);
  ClickInspector("button:place_another");
  DrawInspectorFrame();
  EXPECT_STREQ(content_.GetInspectorModeIdForTesting(), "inspect");
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
  ClickInspector("checkbox:repeat_placement");
  EXPECT_EQ(interaction.GetPlacementPolicy(),
            DungeonObjectInteraction::PlacementPolicy::kRepeat);
  const auto preview = *interaction.GetPlacementPreview();
  auto& handler = interaction.entity_coordinator().tile_handler();
  ASSERT_TRUE(handler.PlaceObjectAt(0, preview, 12, 14));
  DrawInspectorFrame();
  ASSERT_TRUE(handler.PlaceObjectAt(0, preview, 16, 18));
  DrawInspectorFrame();
  EXPECT_STREQ(content_.GetInspectorModeIdForTesting(), "inspect");
  EXPECT_STREQ(content_.GetSidebarModeIdForTesting(), "objects");
  ASSERT_EQ(interaction.GetSelectedObjectIndices(), std::vector<size_t>{1});
  ASSERT_NE(interaction.GetPlacementPreview(), nullptr);
  ClickInspector("button:finish_placement");
  DrawInspectorFrame();
  EXPECT_STREQ(content_.GetInspectorModeIdForTesting(), "inspect");
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
  for (const char* name :
       {"selectable:object_variant_3", "selectable:object_variant_4"}) {
    const auto minish = Widget(name);
    ASSERT_TRUE(minish.has_value());
    EXPECT_FALSE(minish->enabled);
  }
  EXPECT_FALSE(Widget("selectable:object_variant_5").has_value());
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

  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "inspect");

  content.OpenDoorTool();
  EXPECT_FALSE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "inspect");
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "doors");
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "door");

  content.OpenPaletteTool();
  EXPECT_TRUE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "tools");
  EXPECT_STREQ(content.GetActiveToolIdForTesting(), "palette");
}

TEST(DungeonWorkbenchContentLayoutTest,
     SpecialtyToolsRemainTemporaryInspectorContexts) {
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
  FakeWorkbenchToolContent palette_editor("dungeon.palette_editor");
  content.SetEmbeddedEditorPanels(nullptr, nullptr, nullptr, nullptr, nullptr,
                                  &palette_editor);

  std::string opened_window_id;
  content.SetStandaloneToolCallbacks([](const std::string&) { return false; },
                                     [&](const std::string& window_id) {
                                       opened_window_id = window_id;
                                       return true;
                                     });

  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "inspect");
  content.OpenPaletteTool();
  ASSERT_TRUE(content.IsToolInspectorActiveForTesting());
  EXPECT_TRUE(content.PopOutActiveTool());
  EXPECT_EQ(opened_window_id, "dungeon.palette_editor");
  EXPECT_FALSE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "inspect");
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
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "inspect");
  content.OpenObjectSelectorTool();
  ASSERT_FALSE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetSidebarModeIdForTesting(), "objects");

  standalone_open = true;
  content.OpenObjectSelectorTool();
  EXPECT_FALSE(content.IsToolInspectorActiveForTesting());
  EXPECT_STREQ(content.GetInspectorModeIdForTesting(), "inspect");
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
       ButtonsRefreshCachedOverviewAfterPitDamageMembershipSwaps) {
  int current_room_id = 0x020;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x8000, 0)).ok());
  zelda3::GameData game_data;
  game_data.pit_damage_table = MakePitDamageTable({0x001, 0x010});
  auto& table = game_data.pit_damage_table;
  DungeonEditorV2 editor(&rom);
  auto& viewer = DungeonCanvasViewerTestPeer::MakeOwnedOverviewViewer(
      editor, &rom, &game_data, current_room_id);
  for (int id : {current_room_id, 0x030}) {
    editor.rooms()[id].SetLoaded(true);
  }
  editor.rooms()[current_room_id].SetHolewarp(0x030);
  ASSERT_TRUE(editor.rooms()[current_room_id]
                  .AddObject(zelda3::RoomObject(0x0A4, 8, 8, 0, 0))
                  .ok());
  content.SetPitDamageTableProvider(
      [&table]() { return &table; },
      DungeonCanvasViewerTestPeer::PitDamageChanged(editor));
  const auto room_count = [&]() {
    return DungeonCanvasViewerTestPeer::CachedOverviewRoomCount(
        viewer, current_room_id);
  };
  ASSERT_EQ(room_count(), 2);

  DrawPitDamageFrame(content, current_room_id);
  EXPECT_FALSE(
      content.GetPitDamageControlRectsForTesting().add_current.visible);
  ClickPitControl(content, current_room_id, "PitDamageEnabled");
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
  EXPECT_EQ(room_count(), 1);

  table.ClearDirty();
  DrawPitDamageFrame(content, current_room_id);
  EXPECT_FALSE(
      content.GetPitDamageControlRectsForTesting().replace_current.visible);
  ClickPitControl(content, current_room_id, "PitDamageEnabled");
  DrawPitDamageFrame(content, current_room_id);
  const auto replace_rect =
      content.GetPitDamageControlRectsForTesting().replace_current;
  ASSERT_TRUE(replace_rect.visible);
  ASSERT_LT(replace_rect.min_x, replace_rect.max_x);
  ASSERT_LT(replace_rect.min_y, replace_rect.max_y);

  ClickRect(content, current_room_id, replace_rect);
  EXPECT_FALSE(table.Contains(0x020));
  EXPECT_EQ(room_count(), 2);
  EXPECT_TRUE(table.Contains(0x000));
  EXPECT_TRUE(table.Contains(0x010));
  EXPECT_TRUE(table.dirty());
}

TEST_F(DungeonWorkbenchPitDamageUiTest,
       CachedOverviewRefreshesAfterCollisionPaintImportUndoRedo) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  ASSERT_TRUE(
      rom.WriteWord(SnesToPc(zelda3::kRoomLayoutPointers.front()), 0xFFFF)
          .ok());
  zelda3::GameData game_data;
  DungeonEditorV2 editor(&rom);
  constexpr int kRoom = 0x20;
  auto& viewer = DungeonCanvasViewerTestPeer::MakeOwnedOverviewViewer(
      editor, &rom, &game_data, kRoom);
  for (int id : {kRoom, 0x30}) {
    editor.rooms()[id].SetLoaded(true);
  }
  auto& room = editor.rooms()[kRoom];
  room.SetHolewarp(0x30);
  const auto room_count = [&]() {
    return DungeonCanvasViewerTestPeer::CachedOverviewRoomCount(viewer, kRoom);
  };
  ASSERT_EQ(room_count(), 1);

  // Exercise the same transaction hooks as a completed collision paint stroke.
  viewer.object_interaction().BeginCustomCollisionMutation();
  room.SetCollisionTile(8, 8, 0x4B);
  viewer.object_interaction().EndCustomCollisionMutation();
  EXPECT_EQ(room_count(), 2);
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(room_count(), 1);
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(room_count(), 2);

  // Import replaces the map atomically; batch undo/redo must refresh the cache.
  zelda3::CustomCollisionRoomEntry entry;
  entry.room_id = kRoom;
  ASSERT_TRUE(
      DungeonCanvasViewerTestPeer::ImportCollision(editor, {entry}).ok());
  EXPECT_EQ(room_count(), 1);
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(room_count(), 2);
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(room_count(), 1);
}

TEST_F(DungeonWorkbenchPitDamageUiTest,
       CheckboxCancelLeavesPitDamageMembershipUntouched) {
  int current_room_id = 0x020;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  auto table = MakePitDamageTable({0x001, 0x010});
  content.SetPitDamageTableProvider([&table]() { return &table; });

  DrawPitDamageFrame(content, current_room_id);
  ClickPitControl(content, current_room_id, "PitDamageEnabled");
  DrawPitDamageFrame(content, current_room_id);
  EXPECT_TRUE(content.GetPitDamageControlRectsForTesting().add_current.visible);
  EXPECT_FALSE(table.Contains(current_room_id));
  EXPECT_FALSE(table.dirty());
  ClickPitControl(content, current_room_id, "PitDamageCancel");
  DrawPitDamageFrame(content, current_room_id);
  EXPECT_FALSE(
      content.GetPitDamageControlRectsForTesting().add_current.visible);
  EXPECT_TRUE(table.Contains(0x001));
  EXPECT_TRUE(table.Contains(0x010));
  EXPECT_FALSE(table.Contains(current_room_id));
  EXPECT_FALSE(table.dirty());
  for (const auto& [path, widget] :
       gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
    EXPECT_NE(widget.label, "PitDamageDetails");
  }
}

TEST_F(DungeonWorkbenchPitDamageUiTest,
       RoomChangeDiscardsPendingPitDamageChoice) {
  int current_room_id = 0x020;
  const std::deque<int> recent_rooms;
  auto content = MakeWorkbenchForToolStateTests(current_room_id, recent_rooms);
  auto table = MakePitDamageTable({0x001, 0x010});
  content.SetPitDamageTableProvider([&table]() { return &table; });

  DrawPitDamageFrame(content, current_room_id);
  ClickPitControl(content, current_room_id, "PitDamageEnabled");
  DrawPitDamageFrame(content, current_room_id);
  ASSERT_TRUE(content.GetPitDamageControlRectsForTesting().add_current.visible);
  ASSERT_FALSE(ImGui::GetCurrentContext()->OpenPopupStack.empty());

  current_room_id = 0x030;
  DrawPitDamageFrame(content, current_room_id);
  EXPECT_TRUE(ImGui::GetCurrentContext()->OpenPopupStack.empty());
  EXPECT_FALSE(
      content.GetPitDamageControlRectsForTesting().add_current.visible);
  EXPECT_TRUE(table.Contains(0x001));
  EXPECT_TRUE(table.Contains(0x010));
  EXPECT_FALSE(table.Contains(0x020));
  EXPECT_FALSE(table.Contains(0x030));
  EXPECT_FALSE(table.dirty());
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

  EXPECT_FALSE(dungeon_project_labels::HasCustomRoomLabel(&project, 0x25));
  dungeon_project_labels::SetRoomLabel(&project, 0x25, "Flooded Hall");
  EXPECT_TRUE(dungeon_project_labels::HasCustomRoomLabel(&project, 0x25));
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Flooded Hall");
  // Reopening an Oracle project rewrites the registry "room" section; the
  // rename lives in its own section and must keep winning.
  project.resource_labels["room"]["37"] = "Water Grate";
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Flooded Hall");
  dungeon_project_labels::ClearRoomLabel(&project, 0x25);
  EXPECT_FALSE(project.resource_labels.contains(
      dungeon_project_labels::kRoomOverrideLabelType));
  EXPECT_EQ(dungeon_project_labels::GetRoomLabel(&project, 0x25),
            "Water Grate");

  std::filesystem::remove_all(root);
}

TEST(DungeonWorkbenchContentTest,
     ProposalIdentityTracksSuccessfulUiAndApiLoadsOnly) {
  const auto root = MakeTempProjectRoot();
  std::filesystem::create_directories(root);
  const auto cleanup =
      absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
  const std::string document =
      R"({"format":"yaze-dungeon-proposal-overlay","version":1,"rooms":[101],"layers":[]})";
  const auto a = root / "a.json";
  const auto b = root / "b.json";
  std::ofstream(a) << document;
  std::ofstream(b) << document;
  int room = 101;
  const std::deque<int> recent;
  auto workbench = MakeWorkbenchForToolStateTests(room, recent);
  ASSERT_TRUE(workbench.LoadProposalOverlay(a.string()));
  EXPECT_EQ(workbench.loaded_proposal_path(), a.string());
  ASSERT_TRUE(workbench.LoadProposalOverlay(b.string()));
  EXPECT_EQ(workbench.loaded_proposal_path(), b.string());
  EXPECT_FALSE(workbench.LoadProposalOverlay((root / "missing.json").string()));
  EXPECT_EQ(workbench.loaded_proposal_path(), b.string());
}

TEST(DungeonRoomStoreCollisionTest,
     MaterializedRoomsPreserveRoomIdAndExportWithoutMaterializingAllRooms) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  DungeonRoomStore store(&rom);
  EXPECT_EQ(store.GetIfMaterialized(0x65), nullptr);
  auto& room_65 = store[0x65];
  EXPECT_EQ(room_65.id(), 0x65);

  // Author a walkable floor (0x00) override in room 0x65 and save to ROM.
  room_65.SetCollisionTile(10, 5, 0x00);
  ASSERT_TRUE(room_65.HasCollisionOverride(10, 5));
  ASSERT_TRUE(zelda3::SaveAllCollision(
                  &rom, static_cast<int>(store.size()),
                  [&](int rid) { return store.GetIfMaterialized(rid); })
                  .ok());

  // Room 0x00 pointer must remain untouched; Room 0x65 pointer must be written.
  const int ptr_00 = zelda3::kCustomCollisionRoomPointers;
  const int ptr_65 = zelda3::kCustomCollisionRoomPointers + (0x65 * 3);
  EXPECT_EQ(
      rom.data()[ptr_00] | rom.data()[ptr_00 + 1] | rom.data()[ptr_00 + 2], 0);
  EXPECT_NE(
      rom.data()[ptr_65] | rom.data()[ptr_65 + 1] | rom.data()[ptr_65 + 2], 0);

  // In a fresh store where room 0x65 is NOT materialized in memory and room
  // 0x74 has an in-memory edit, CollectRoomEntries must export both rooms
  // without materializing the remaining 295 rooms.
  DungeonRoomStore fresh_store(&rom);
  CustomCollisionPanel::ApplyRoomEntry(
      zelda3::CustomCollisionRoomEntry{
          0x74, {zelda3::CustomCollisionTileEntry{8 * 64 + 12, 0x08}}},
      &fresh_store[0x74]);
  EXPECT_EQ(fresh_store.GetIfMaterialized(0x65), nullptr);

  const auto exported_or =
      CustomCollisionPanel::CollectRoomEntries(fresh_store);
  ASSERT_TRUE(exported_or.ok()) << exported_or.status();
  const auto& exported = *exported_or;
  EXPECT_EQ(fresh_store.GetIfMaterialized(0x65), nullptr);
  EXPECT_EQ(fresh_store.GetIfMaterialized(0x00), nullptr);
  ASSERT_EQ(exported.size(), 2u);
  EXPECT_EQ(exported[0].room_id, 0x65);
  ASSERT_EQ(exported[0].tiles.size(), 1u);
  EXPECT_EQ(exported[0].tiles[0].offset, 5 * 64 + 10);
  EXPECT_EQ(exported[0].tiles[0].value, 0x00);
  EXPECT_EQ(exported[1].room_id, 0x74);
  ASSERT_EQ(exported[1].tiles.size(), 1u);
  EXPECT_EQ(exported[1].tiles[0].offset, 8 * 64 + 12);
  EXPECT_EQ(exported[1].tiles[0].value, 0x08);

  // WaterFillPanel::CollectZones must also skip unmaterialized rooms.
  fresh_store[0x25].SetWaterFillTile(3, 4, true);
  fresh_store[0x25].set_water_fill_sram_bit_mask(0x04);
  const auto zones = WaterFillPanel::CollectZones(fresh_store);
  EXPECT_EQ(fresh_store.GetIfMaterialized(0x00), nullptr);
  ASSERT_EQ(zones.size(), 1u);
  EXPECT_EQ(zones[0].room_id, 0x25);
  EXPECT_EQ(zones[0].sram_bit_mask, 0x04);
  EXPECT_EQ(zones[0].fill_offsets, std::vector<uint16_t>{4 * 64 + 3});
}

TEST(DungeonRoomStoreCollisionTest,
     ExportFailsWhenUnvisitedRoomHasInvalidPointerOrUnterminatedStream) {
  const auto root = MakeTempProjectRoot();
  std::filesystem::create_directories(root);
  const auto cleanup =
      absl::MakeCleanup([&] { std::filesystem::remove_all(root); });
  const auto out_path = root / "exported_collision.json";

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  // Seed room 0x65 with valid collision data in ROM.
  {
    DungeonRoomStore seed_store(&rom);
    seed_store[0x65].SetCollisionTile(2, 2, 0x08);
    ASSERT_TRUE(zelda3::SaveAllCollision(
                    &rom, static_cast<int>(seed_store.size()),
                    [&](int rid) { return seed_store.GetIfMaterialized(rid); })
                    .ok());
  }

  // Case 1: Unvisited room 0x66 has an out-of-range pointer in the table.
  const int ptr_66 = zelda3::kCustomCollisionRoomPointers + (0x66 * 3);
  rom.mutable_data()[ptr_66 + 0] = 0x01;
  rom.mutable_data()[ptr_66 + 1] = 0x00;
  rom.mutable_data()[ptr_66 + 2] = 0x00;  // SNES $00:0001 -> invalid LoROM

  DungeonRoomStore store(&rom);
  DungeonCanvasViewer viewer;
  CustomCollisionPanel panel(&viewer, &viewer.object_interaction());
  {
    std::ofstream sentinel(out_path);
    sentinel << "SENTINEL_KEEP";
  }

  const auto bad_ptr_or = CustomCollisionPanel::CollectRoomEntries(store);
  EXPECT_FALSE(bad_ptr_or.ok());
  EXPECT_NE(std::string(bad_ptr_or.status().message()).find("Room 0x066"),
            std::string::npos);

  const auto export_ptr_status =
      panel.ExportCollisionToFile(store, out_path.string());
  EXPECT_FALSE(export_ptr_status.ok());
  EXPECT_NE(panel.last_io_error().find("Room 0x066"), std::string::npos);
  EXPECT_TRUE(panel.last_io_status().empty());
  EXPECT_EQ(util::LoadFile(out_path.string()), "SENTINEL_KEEP");

  // Case 2: Unvisited room 0x66 points to an unterminated stream inside the
  // custom collision data region.
  const int unterminated_pc = zelda3::kCustomCollisionDataPosition + 0x400;
  const uint32_t unterminated_snes = PcToSnes(unterminated_pc);
  rom.mutable_data()[ptr_66 + 0] = unterminated_snes & 0xFF;
  rom.mutable_data()[ptr_66 + 1] = (unterminated_snes >> 8) & 0xFF;
  rom.mutable_data()[ptr_66 + 2] = (unterminated_snes >> 16) & 0xFF;
  std::fill(rom.mutable_data() + unterminated_pc,
            rom.mutable_data() + zelda3::kCustomCollisionDataSoftEnd, 0x01);

  const auto bad_stream_or = CustomCollisionPanel::CollectRoomEntries(store);
  EXPECT_FALSE(bad_stream_or.ok());
  EXPECT_NE(std::string(bad_stream_or.status().message()).find("Room 0x066"),
            std::string::npos);

  const auto export_stream_status =
      panel.ExportCollisionToFile(store, out_path.string());
  EXPECT_FALSE(export_stream_status.ok());
  EXPECT_NE(panel.last_io_error().find("Room 0x066"), std::string::npos);
  EXPECT_TRUE(panel.last_io_status().empty());
  EXPECT_EQ(util::LoadFile(out_path.string()), "SENTINEL_KEEP");
}

TEST_F(DungeonWorkbenchResetUiTest,
       EntityInspectorActionsSupportPlaceAnotherAndDelete) {
  rooms_[0].GetSprites().emplace_back(0x09, 4, 5, 0, 0);
  rooms_[0].GetDoors().push_back(zelda3::Room::Door::FromRomBytes(0x10, 0x00));
  rooms_[0].GetPotItems().push_back(zelda3::PotItem{0x0714, 0x09});
  auto& interaction = viewer_.object_interaction();

  // Read-only guard disables both buttons.
  interaction.SelectEntity(EntityType::Sprite, 0);
  viewer_.SetHeaderReadOnly(true);
  Frame();
  Frame();
  ASSERT_TRUE(Find("EntityPlaceAnother").has_value());
  ASSERT_TRUE(Find("EntityDelete").has_value());
  EXPECT_FALSE(Find("EntityPlaceAnother")->enabled);
  EXPECT_FALSE(Find("EntityDelete")->enabled);
  viewer_.SetHeaderReadOnly(false);

  // 1. Sprite: PlaceAnother -> CancelPlacement -> EntityDelete WITHOUT
  // reselecting.
  Frame();
  Click("EntityPlaceAnother");
  EXPECT_TRUE(interaction.IsSpritePlacementActive());
  EXPECT_EQ(interaction.entity_coordinator().sprite_handler().GetSpriteId(),
            0x09);
  interaction.CancelPlacement();
  Frame();
  Click("EntityDelete");
  EXPECT_TRUE(rooms_[0].GetSprites().empty());
  EXPECT_TRUE(rooms_[0].sprites_dirty());

  // 2. Door: PlaceAnother -> CancelPlacement -> EntityDelete WITHOUT
  // reselecting.
  interaction.SelectEntity(EntityType::Door, 0);
  Frame();
  Frame();
  Click("EntityPlaceAnother");
  EXPECT_TRUE(interaction.IsDoorPlacementActive());
  interaction.CancelPlacement();
  Frame();
  Click("EntityDelete");
  EXPECT_TRUE(rooms_[0].GetDoors().empty());
  EXPECT_TRUE(rooms_[0].object_stream_dirty());

  // 3. Pot Item: PlaceAnother -> CancelPlacement -> EntityDelete WITHOUT
  // reselecting.
  interaction.SelectEntity(EntityType::Item, 0);
  Frame();
  Frame();
  Click("EntityPlaceAnother");
  EXPECT_TRUE(interaction.IsItemPlacementActive());
  interaction.CancelPlacement();
  Frame();
  Click("EntityDelete");
  EXPECT_TRUE(rooms_[0].GetPotItems().empty());
  EXPECT_TRUE(rooms_[0].pot_items_dirty());
}

TEST_F(DungeonWorkbenchResetUiTest,
       CustomCollisionPanelShowsFloorOverrideMetricsAndTogglesEraseMode) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  rooms_.SetRom(&rom);
  viewer_.RefreshRomBackedState(&rom, nullptr, &rooms_, 0);
  absl::Cleanup cleanup = [&] {
    standalone_content_ = nullptr;
    viewer_.RefreshRomBackedState(nullptr, nullptr, &rooms_, 0);
    rooms_.SetRom(nullptr);
  };

  rooms_[0].SetCollisionTile(2, 3, 0x00);
  rooms_[0].SetCollisionTile(4, 5, 0x08);
  CustomCollisionPanel collision(&viewer_, &viewer_.object_interaction());
  standalone_content_ = &collision;

  Frame();
  Frame();
  EXPECT_NE(text_.find("Override tiles: 2 (1 floor 00)"), std::string::npos);

  Click("collision_paint_mode");
  auto& mode_mgr = viewer_.object_interaction().mode_manager();
  ASSERT_EQ(mode_mgr.GetMode(), InteractionMode::PaintCollision);
  EXPECT_FALSE(mode_mgr.GetModeState().paint_collision_erase);

  Click("collision_erase_override");
  EXPECT_TRUE(mode_mgr.GetModeState().paint_collision_erase);
  ASSERT_TRUE(Find("collision_tile_type").has_value());
  EXPECT_FALSE(Find("collision_tile_type")->enabled);

  Click("quick_collision_floor_00");
  Frame();
  EXPECT_FALSE(mode_mgr.GetModeState().paint_collision_erase);
  EXPECT_EQ(mode_mgr.GetModeState().paint_collision_value, 0x00);
  // The combo is drawn before Quick Select, so its registered enabled state
  // still reflects erase mode during the button's release frame.
  Frame();
  ASSERT_TRUE(Find("collision_tile_type").has_value());
  EXPECT_TRUE(Find("collision_tile_type")->enabled);
}

TEST_F(
    DungeonWorkbenchResetUiTest,
    CustomCollisionPanelEditsAndImportsSupportEditorUndoRedoAndRomPersistence) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

  // First-open must execute the real room loader, with a normal object stream
  // to distinguish full loading from merely setting IsLoaded on the import.
  constexpr int kObjectTable = 0x0F8000;
  constexpr int kObjectStream = 0x100000;
  const auto write_pointer = [&](int pc, int target) {
    const uint32_t snes = PcToSnes(target);
    rom.mutable_data()[pc] = snes & 0xFF;
    rom.mutable_data()[pc + 1] = (snes >> 8) & 0xFF;
    rom.mutable_data()[pc + 2] = (snes >> 16) & 0xFF;
  };
  write_pointer(zelda3::kRoomObjectPointer, kObjectTable);
  write_pointer(kObjectTable + 0x12 * 3, kObjectStream);
  ASSERT_TRUE(
      rom.WriteWord(SnesToPc(zelda3::kRoomLayoutPointers.front()), 0xFFFF)
          .ok());
  const auto object =
      zelda3::RoomObject(0x31, 10, 10, 0, 0).EncodeObjectToBytes();
  ASSERT_TRUE(rom.WriteVector(kObjectStream,
                              {0x00, 0x00, object.b1, object.b2, object.b3,
                               0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF})
                  .ok());

  // Seed unvisited room 0x12 with a pre-existing custom collision tile in ROM.
  {
    DungeonRoomStore seed_store(&rom);
    seed_store[0x12].SetCollisionTile(3, 3, 0x1B);
    ASSERT_TRUE(zelda3::SaveAllCollision(
                    &rom, static_cast<int>(seed_store.size()),
                    [&](int rid) { return seed_store.GetIfMaterialized(rid); })
                    .ok());
  }

  DungeonEditorV2 editor(&rom);
  DungeonCanvasViewerTestPeer::BindViewer(editor, &rom, &viewer_, 0);
  CustomCollisionPanel collision(&viewer_, &viewer_.object_interaction());
  standalone_content_ = &collision;
  absl::Cleanup cleanup = [&] {
    standalone_content_ = nullptr;
    viewer_.RefreshRomBackedState(nullptr, nullptr, &rooms_, 0);
  };

  // 1. Enable / Disable Custom Collision enters editor undo/redo history.
  ASSERT_FALSE(editor.rooms()[0].has_custom_collision());
  Frame();
  Frame();
  Click("toggle_custom_collision");
  EXPECT_TRUE(editor.rooms()[0].has_custom_collision());
  ASSERT_TRUE(editor.CanUndo());
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_FALSE(editor.rooms()[0].has_custom_collision());
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_TRUE(editor.rooms()[0].has_custom_collision());

  // 2. Single-room & multi-room JSON import enters atomic undo/redo history.
  EXPECT_EQ(editor.rooms().GetIfMaterialized(0x12), nullptr);
  constexpr const char* kImportJson = R"json({
    "version": 1,
    "rooms": [
      {
        "room_id": "0x00",
        "tiles": [
          [260, 0],
          [325, 8]
        ]
      },
      {
        "room_id": "0x12",
        "tiles": [
          [390, 9],
          [455, 0]
        ]
      }
    ]
  })json";
  ASSERT_TRUE(collision.ImportCollisionJson(kImportJson, "test.json").ok());
  EXPECT_TRUE(editor.rooms()[0].HasCollisionOverride(4, 4));
  EXPECT_EQ(editor.rooms()[0].GetCollisionTile(4, 4), 0x00);
  EXPECT_TRUE(editor.rooms()[0].HasCollisionOverride(5, 5));
  EXPECT_EQ(editor.rooms()[0].GetCollisionTile(5, 5), 0x08);
  EXPECT_TRUE(editor.rooms()[0x12].HasCollisionOverride(6, 6));
  EXPECT_EQ(editor.rooms()[0x12].GetCollisionTile(6, 6), 0x09);
  EXPECT_FALSE(editor.rooms()[0x12].HasCollisionOverride(3, 3));
  ASSERT_FALSE(editor.rooms()[0x12].IsLoaded());
  ASSERT_TRUE(editor.rooms()[0x12].custom_collision_dirty());

  // Hovering the unopened room in the matrix previews it from a copy; it must
  // not reload the shared room and drop the pending import.
  {
    int matrix_room = 0;
    ImVector<int> matrix_active;
    RoomMatrixContent matrix(&matrix_room, &matrix_active, [](int) {});
    matrix.SetRooms(&editor.rooms());
    standalone_content_ = &matrix;
    Frame();
    const auto cell = Find("Room 0x012");
    ASSERT_TRUE(cell.has_value());
    ImGui::GetIO().AddMousePosEvent(
        (cell->bounds.min_x + cell->bounds.max_x) * 0.5f,
        (cell->bounds.min_y + cell->bounds.max_y) * 0.5f);
    Frame();
    Frame();
    standalone_content_ = &collision;
    ImGui::GetIO().AddMousePosEvent(-1.0f, -1.0f);
  }
  ASSERT_FALSE(editor.rooms()[0x12].IsLoaded());
  ASSERT_TRUE(editor.rooms()[0x12].custom_collision_dirty());
  EXPECT_EQ(editor.rooms()[0x12].GetCollisionTile(6, 6), 0x09);
  EXPECT_TRUE(editor.rooms()[0x12].HasCollisionOverride(7, 7));

  DungeonCanvasViewerTestPeer::OpenRoom(editor, 0x12);
  const auto& imported_room = editor.rooms()[0x12];
  ASSERT_TRUE(imported_room.IsLoaded());
  ASSERT_TRUE(imported_room.AreObjectsLoaded());
  ASSERT_EQ(imported_room.GetTileObjects().size(), 1u);
  EXPECT_EQ(imported_room.GetTileObjects()[0].id_, 0x31);
  EXPECT_TRUE(imported_room.custom_collision_dirty());
  EXPECT_TRUE(imported_room.HasCollisionOverride(6, 6));
  EXPECT_EQ(imported_room.GetCollisionTile(6, 6), 0x09);
  EXPECT_TRUE(imported_room.HasCollisionOverride(7, 7));
  EXPECT_EQ(imported_room.GetCollisionTile(7, 7), 0x00);
  EXPECT_FALSE(imported_room.HasCollisionOverride(3, 3));

  // Undo restores both Room 0x00 and Room 0x12 (including Room 0x12's prior ROM
  // collision tile) in a single step.
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_FALSE(editor.rooms()[0].HasCollisionOverride(4, 4));
  EXPECT_FALSE(editor.rooms()[0].HasCollisionOverride(5, 5));
  EXPECT_TRUE(editor.rooms()[0x12].HasCollisionOverride(3, 3));
  EXPECT_EQ(editor.rooms()[0x12].GetCollisionTile(3, 3), 0x1B);
  EXPECT_FALSE(editor.rooms()[0x12].HasCollisionOverride(6, 6));
  EXPECT_FALSE(editor.rooms()[0x12].HasCollisionOverride(7, 7));
  EXPECT_TRUE(editor.rooms()[0x12].custom_collision_dirty());

  // Redo re-applies the multi-room import across both rooms.
  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_TRUE(editor.rooms()[0].HasCollisionOverride(4, 4));
  EXPECT_EQ(editor.rooms()[0].GetCollisionTile(4, 4), 0x00);
  EXPECT_TRUE(editor.rooms()[0x12].HasCollisionOverride(6, 6));
  EXPECT_EQ(editor.rooms()[0x12].GetCollisionTile(6, 6), 0x09);
  EXPECT_TRUE(editor.rooms()[0x12].HasCollisionOverride(7, 7));
  EXPECT_EQ(editor.rooms()[0x12].GetCollisionTile(7, 7), 0x00);
  EXPECT_TRUE(editor.rooms()[0x12].custom_collision_dirty());

  // 3. Clear All Custom Collision enters editor undo/redo history.
  Frame();
  Frame();
  Click("clear_all_custom_collision");
  EXPECT_FALSE(editor.rooms()[0].has_custom_collision());
  EXPECT_FALSE(editor.rooms()[0].HasCollisionOverride(4, 4));
  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_TRUE(editor.rooms()[0].has_custom_collision());
  EXPECT_TRUE(editor.rooms()[0].HasCollisionOverride(4, 4));
  EXPECT_EQ(editor.rooms()[0].GetCollisionTile(4, 4), 0x00);
  EXPECT_TRUE(editor.rooms()[0].HasCollisionOverride(5, 5));
  EXPECT_EQ(editor.rooms()[0].GetCollisionTile(5, 5), 0x08);

  // 4. Save to disposable ROM and reopen into a fresh store to prove explicit
  // floor 0x00 override and multi-room import persist across save/reopen.
  ASSERT_TRUE(
      zelda3::SaveAllCollision(
          &rom, static_cast<int>(editor.rooms().size()),
          [&](int rid) { return editor.rooms().GetIfMaterialized(rid); })
          .ok());
  const auto root = MakeTempProjectRoot();
  std::filesystem::create_directories(root);
  absl::Cleanup remove_files = [&] {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  };
  const auto saved_path = root / "collision-first-open.sfc";
  const auto save = rom.SaveToFile({.filename = saved_path.string()});
  ASSERT_TRUE(save.ok()) << save;
  Rom reopened_rom;
  const auto reopen = reopened_rom.LoadFromFile(saved_path.string());
  ASSERT_TRUE(reopen.ok()) << reopen;
  const auto reopened_room0 = zelda3::LoadCustomCollisionMap(&reopened_rom, 0);
  ASSERT_TRUE(reopened_room0.ok());
  EXPECT_TRUE(reopened_room0->has_data);
  EXPECT_TRUE(reopened_room0->HasOverride(4 * 64 + 4));
  EXPECT_EQ(reopened_room0->tiles[4 * 64 + 4], 0x00);
  EXPECT_TRUE(reopened_room0->HasOverride(5 * 64 + 5));
  EXPECT_EQ(reopened_room0->tiles[5 * 64 + 5], 0x08);

  const auto reopened_room12 =
      zelda3::LoadCustomCollisionMap(&reopened_rom, 0x12);
  ASSERT_TRUE(reopened_room12.ok());
  EXPECT_TRUE(reopened_room12->has_data);
  EXPECT_TRUE(reopened_room12->HasOverride(6 * 64 + 6));
  EXPECT_EQ(reopened_room12->tiles[6 * 64 + 6], 0x09);
  EXPECT_TRUE(reopened_room12->HasOverride(7 * 64 + 7));
  EXPECT_EQ(reopened_room12->tiles[7 * 64 + 7], 0x00);
  EXPECT_FALSE(reopened_room12->HasOverride(3 * 64 + 3));
}

}  // namespace
}  // namespace yaze::editor
