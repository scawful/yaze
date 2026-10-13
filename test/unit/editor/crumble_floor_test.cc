#include "app/editor/dungeon/ui/crumble/crumble_floor_view.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/canvas/canvas_types.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

class CrumbleFloorViewTestPeer {
 public:
  static void Rehearse(CrumbleFloorView& view) { view.rehearsing_ = true; }
};

class DungeonCanvasCrumbleTestPeer {
 public:
  static CrumbleFloorView& View(DungeonCanvasViewer& viewer) {
    return viewer.crumble_floor_view_;
  }
  static bool Draw(DungeonCanvasViewer& viewer, const gui::CanvasRuntime& rt) {
    return viewer.DrawCrumbleCanvas(rt);
  }
};

namespace {

TEST(CrumbleFloorModelTest, ExactWordsRejectSimilarPaletteAndFlipVariants) {
  EXPECT_EQ(ClassifyCrumbleFloorWord(0x0C62), CrumbleFloorCell::kIntact);
  EXPECT_EQ(ClassifyCrumbleFloorWord(0x0C63), CrumbleFloorCell::kIntact);
  EXPECT_EQ(ClassifyCrumbleFloorWord(0x0CCC), CrumbleFloorCell::kCracked);
  for (uint16_t word : {0x0062, 0x4862, 0x4C62, 0x8CCC, 0x0CDC, 0x0CCD}) {
    EXPECT_EQ(ClassifyCrumbleFloorWord(word), CrumbleFloorCell::kOther) << word;
  }
}

TEST(CrumbleFloorModelTest, TwoDeparturesCollapseAndBackRestoresRoute) {
  CrumbleFloorModel::Words words{};
  words[0] = 0x0C62;
  words[1] = 0x0C63;
  words[2] = 0x0C62;
  CrumbleFloorModel model;
  model.Reset(words);
  ASSERT_TRUE(model.Step(0, 0).ok());
  for (int i = 0; i < 120; ++i)
    ASSERT_TRUE(model.Step(0, 0).ok());
  EXPECT_EQ(model.cell(0), CrumbleFloorCell::kIntact);
  ASSERT_TRUE(model.Step(1, 0).ok());
  EXPECT_EQ(model.cell(0), CrumbleFloorCell::kCracked);
  EXPECT_EQ(model.cell(1), CrumbleFloorCell::kIntact);
  ASSERT_TRUE(model.Step(0, 0).ok());
  ASSERT_TRUE(model.Step(1, 0).ok());
  EXPECT_EQ(model.cell(0), CrumbleFloorCell::kPit);
  const auto route = model.route();
  EXPECT_FALSE(model.Step(0, 0).ok());
  EXPECT_EQ(model.route(), route);
  EXPECT_EQ(model.cell(1), CrumbleFloorCell::kCracked);
  model.Back();
  EXPECT_EQ(model.current(), 0);
  EXPECT_EQ(model.cell(0), CrumbleFloorCell::kCracked);
  ASSERT_TRUE(model.Step(1, 0).ok());
  ASSERT_TRUE(model.Step(2, 0).ok());
  EXPECT_EQ(model.cell(1), CrumbleFloorCell::kPit);
  model.Reset(words);
  EXPECT_TRUE(model.route().empty());
  EXPECT_EQ(model.cell(0), CrumbleFloorCell::kIntact);
}

TEST(CrumbleFloorModelTest, InvalidMoveNeverWearsCurrentCell) {
  CrumbleFloorModel::Words words{};
  words[0] = 0x0CCC;
  words[2] = 0x0C62;
  words[32] = 0x01E9;
  CrumbleFloorModel model;
  model.Reset(words);
  ASSERT_TRUE(model.Step(0, 0).ok());
  for (const auto [x, y] :
       {std::pair{-1, 0}, {32, 0}, {2, 0}, {1, 0}, {0, 1}, {1, 1}}) {
    EXPECT_FALSE(model.Step(x, y).ok());
    EXPECT_EQ(model.route().size(), 1u);
    EXPECT_EQ(model.cell(0), CrumbleFloorCell::kCracked);
  }
}

TEST(CrumbleFloorModelTest, BoundIsAtomicAndDoesNotWrapRows) {
  CrumbleFloorModel::Words words;
  words.fill(0x0C62);
  CrumbleFloorModel model;
  model.Reset(words);
  ASSERT_TRUE(model.Step(31, 0).ok());
  EXPECT_FALSE(model.Step(0, 1).ok());
  model.Reset(words);
  for (int y = 0; y < 16; ++y) {
    for (int dx = 0; dx < 32; ++dx) {
      ASSERT_TRUE(model.Step(y % 2 ? 31 - dx : dx, y).ok());
    }
  }
  EXPECT_FALSE(model.Step(0, 16).ok());
  EXPECT_EQ(model.route().size(), 512u);
  EXPECT_EQ(model.cell(15 * 32), CrumbleFloorCell::kIntact);
}

TEST(CrumbleFloorModelTest, DesignSketchesHaveFloorModelSolutions) {
  using Cell = std::pair<int, int>;
  struct Sketch {
    const char* name;
    std::vector<Cell> route;
    std::vector<Cell> cracked;
  };
  const std::array<Sketch, 3> sketches = {{
      {"First crossing",
       {{0, 2}, {1, 2}, {2, 2}, {2, 1}, {3, 1}, {4, 1}, {4, 0}},
       {}},
      {"Return branch",
       {{0, 2},
        {1, 2},
        {2, 2},
        {2, 1},
        {3, 1},
        {4, 1},
        {4, 2},
        {4, 3},
        {3, 3},
        {2, 3},
        {2, 2},
        {1, 2},
        {1, 3},
        {1, 4}},
       {}},
      {"One-use bridge",
       {{0, 2},
        {1, 2},
        {2, 2},
        {3, 2},
        {4, 2},
        {4, 1},
        {3, 1},
        {2, 1},
        {1, 1},
        {0, 1}},
       {{2, 2}}},
  }};
  nlohmann::json output = nlohmann::json::array();
  for (const auto& sketch : sketches) {
    CrumbleFloorModel::Words words{};
    for (const auto [x, y] : sketch.route)
      words[y * 32 + x] = 0x0C62;
    for (const auto [x, y] : sketch.cracked)
      words[y * 32 + x] = 0x0CCC;
    CrumbleFloorModel model;
    model.Reset(words);
    for (const auto [x, y] : sketch.route) {
      ASSERT_TRUE(model.Step(x, y).ok())
          << sketch.name << " at " << x << "," << y;
    }
    ASSERT_EQ(model.route().size(), sketch.route.size());
    output.push_back(
        {{"name", sketch.name},
         {"route", sketch.route},
         {"initial_cracked", sketch.cracked},
         {"verified", "floor model only; not collision or gameplay"}});
  }
  if (const char* path = std::getenv("YAZE_CRUMBLE_SKETCHES_OUT")) {
    std::ofstream file(path);
    file << output.dump(2) << '\n';
    ASSERT_TRUE(file.good());
  }
}

TEST(CrumbleFloorModelTest, ObjectCoverageOverridesLayoutEvenWithZeroWord) {
  zelda3::Room room;
  room.bg1_buffer().SetTileAt(2, 4, 0x0C62);
  room.object_bg1_buffer().SetTileAt(2, 4, 0x0CCC);
  EXPECT_EQ(ReadCrumbleFloorWords(room)[2 * 32 + 1], 0x0C62);
  room.object_bg1_buffer().mutable_coverage_data()[32 * 512 + 16] = 1;
  EXPECT_EQ(ReadCrumbleFloorWords(room)[2 * 32 + 1], 0x0CCC);
  room.object_bg1_buffer().SetTileAt(2, 4, 0);
  EXPECT_EQ(ReadCrumbleFloorWords(room)[2 * 32 + 1], 0);
}

class CrumbleFloorViewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
    ASSERT_TRUE(rom.WriteWord(0xC27D + 0x34 * 2, 0xCC08).ok());
    ASSERT_TRUE(rom.WriteWord(0xC27D + 0x35 * 2, 0xCC0C).ok());
    ASSERT_TRUE(
        project.hack_manifest
            .LoadFromString(R"({"version":1,"hack_name":"Oracle of Secrets",
      "room_tags":{"tags":[{"tag_id":"0x35","address":"0x01CC08",
      "name":"Tag_0x35","source":"Dungeons/crumblefloor_tag.asm:4"}]}})")
            .ok());
    room.SetTag1(static_cast<zelda3::TagKey>(0x34));
    room.bg1_buffer().SetTileAt(0, 0, 0x0C62);
    room.bg1_buffer().SetTileAt(2, 0, 0x0C63);
    view.Sync(&rom, &project, room);
  }
  Rom rom;
  project::YazeProject project;
  zelda3::Room room;
  CrumbleFloorView view;
};

TEST_F(CrumbleFloorViewTest,
       RecognitionUsesRealDispatchNotIncorrectManifestId) {
  EXPECT_TRUE(HasOracleCrumbleTag(rom, &project, room));
  room.SetTag1(static_cast<zelda3::TagKey>(0x35));
  EXPECT_FALSE(HasOracleCrumbleTag(rom, &project, room));
  room.SetTag2(static_cast<zelda3::TagKey>(0x34));
  EXPECT_TRUE(HasOracleCrumbleTag(rom, &project, room));
  EXPECT_FALSE(HasOracleCrumbleTag(rom, nullptr, room));
  ASSERT_TRUE(rom.WriteWord(0xC27D + 0x34 * 2, 0xFFFF).ok());
  EXPECT_FALSE(HasOracleCrumbleTag(rom, &project, room));
}

TEST_F(CrumbleFloorViewTest, CorrectedManifestRetainsDispatchValidation) {
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(
                      R"({"version":1,"hack_name":"Oracle of Secrets",
      "room_tags":{"tags":[{"tag_id":"0x34","address":"0x01CC08",
      "name":"CrumbleFloor","enabled":true,
      "source":"Dungeons/crumblefloor_tag.asm:4"}]}})")
                  .ok());
  EXPECT_TRUE(HasOracleCrumbleTag(rom, &project, room));
  room.SetTag1(static_cast<zelda3::TagKey>(0x35));
  EXPECT_FALSE(HasOracleCrumbleTag(rom, &project, room));
  room.SetTag1(static_cast<zelda3::TagKey>(0x34));
  ASSERT_TRUE(rom.WriteWord(0xC27D + 0x34 * 2, 0xFFFF).ok());
  EXPECT_FALSE(HasOracleCrumbleTag(rom, &project, room));
}

TEST_F(CrumbleFloorViewTest, HeadlessCanvasClicksRehearseWithoutEditingRoom) {
  auto* previous = ImGui::GetCurrentContext();
  auto* context = ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = {900, 700};
  io.DeltaTime = 1.0f / 60;
  unsigned char* pixels;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  const auto before = ReadCrumbleFloorWords(room);
  const auto rom_before = rom.vector();
  const auto frame = [&] {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({850, 650});
    ImGui::Begin("Crumble test", nullptr, ImGuiWindowFlags_NoSavedSettings);
    view.Sync(&rom, &project, room);
    view.DrawControls(true);
    gui::CanvasRuntime rt{};
    rt.canvas_p0 = {100, 200};
    rt.scrolling = {20, 10};
    rt.canvas_sz = {650, 430};
    rt.scale = 2;
    rt.hovered = true;
    rt.draw_list = ImGui::GetWindowDrawList();
    view.DrawCanvas(rt, true, true);
    ImGui::End();
    ImGui::Render();
  };
  gui::WidgetIdRegistry::Instance().Clear();
  frame();
  const auto widgets = gui::WidgetIdRegistry::Instance().GetAllWidgets();
  auto target = std::find_if(
      widgets.begin(), widgets.end(),
      [](const auto& item) { return item.second.label == "Rehearse"; });
  EXPECT_NE(target, widgets.end());
  if (target != widgets.end()) {
    const auto& bounds = target->second.bounds;
    io.AddMousePosEvent((bounds.min_x + bounds.max_x) / 2,
                        (bounds.min_y + bounds.max_y) / 2);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
  }
  EXPECT_TRUE(view.rehearsing());
  for (int x : {0, 1, 0, 1}) {
    io.AddMousePosEvent(120 + x * 32 + 16, 226);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
  }
  EXPECT_EQ(view.model().route().size(), 4u);
  EXPECT_EQ(view.model().cell(0), CrumbleFloorCell::kPit);
  EXPECT_EQ(ReadCrumbleFloorWords(room), before);
  EXPECT_EQ(rom.vector(), rom_before);
  room.bg1_buffer().SetTileAt(0, 0, 0x0CCC);
  frame();
  EXPECT_TRUE(view.model().route().empty());
  EXPECT_FALSE(view.rehearsing());
  EXPECT_EQ(view.model().cell(0), CrumbleFloorCell::kCracked);
  // Switching to the read-only connected overview must release rehearsal
  // keyboard ownership, even when it cannot draw a room yet.
  DungeonCanvasViewer viewer;
  viewer.SetRom(&rom);
  auto& connected_preview = DungeonCanvasCrumbleTestPeer::View(viewer);
  connected_preview.Sync(&rom, &project, room);
  CrumbleFloorViewTestPeer::Rehearse(connected_preview);
  ImGui::NewFrame();
  ImGui::Begin("Connected overview");
  viewer.DrawConnectedRoomMatrix(0x98);
  EXPECT_FALSE(connected_preview.rehearsing());
  ImGui::End();
  ImGui::Render();
  gui::WidgetIdRegistry::Instance().Clear();
  ImGui::DestroyContext(context);
  ImGui::SetCurrentContext(previous);
}

TEST_F(CrumbleFloorViewTest, SameRomRebindClearsIdenticalFloorRehearsal) {
  DungeonCanvasViewer viewer;
  viewer.SetRom(&rom);
  auto& preview = DungeonCanvasCrumbleTestPeer::View(viewer);
  preview.Sync(&rom, &project, room);
  CrumbleFloorViewTestPeer::Rehearse(preview);
  ASSERT_TRUE(preview.rehearsing());
  viewer.SetRom(&rom);
  preview.Sync(&rom, &project, room);
  EXPECT_FALSE(preview.rehearsing());
  EXPECT_TRUE(preview.model().route().empty());
}

TEST_F(CrumbleFloorViewTest, RailStampTakesCanvasFromRehearsal) {
  DungeonCanvasViewer viewer;
  MinecartTrackEditorPanel tracks;
  viewer.SetMinecartTrackPanel(&tracks);
  auto& preview = DungeonCanvasCrumbleTestPeer::View(viewer);
  preview.Sync(&rom, &project, room);
  CrumbleFloorViewTestPeer::Rehearse(preview);
  tracks.set_collision_stamping(true);
  gui::CanvasRuntime rt{};
  EXPECT_FALSE(DungeonCanvasCrumbleTestPeer::Draw(viewer, rt));
  EXPECT_FALSE(preview.rehearsing());
}

TEST(CrumbleFloorOracleTest,
     Room098NeedsFloorPatternAndPreviewTracksUnsavedPlacement) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  const char* manifest = std::getenv("YAZE_TEST_OOS_MANIFEST");
  if (!path || !manifest)
    GTEST_SKIP() << "Provide disposable Oracle ROM and manifest";
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  const auto before = rom.vector();
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest.LoadFromFile(manifest).ok());
  zelda3::GameData data;
  ASSERT_TRUE(zelda3::LoadGameData(rom, data).ok());
  auto room = zelda3::LoadRoomFromRom(&rom, 0x98);
  room.SetGameData(&data);
  room.LoadRoomGraphics();
  room.RenderRoomGraphics();
  ASSERT_TRUE(HasOracleCrumbleTag(rom, &project, room));
  const auto words = ReadCrumbleFloorWords(room);
  int intact = 0, cracked = 0, pits = 0;
  for (auto word : words) {
    intact += ClassifyCrumbleFloorWord(word) == CrumbleFloorCell::kIntact;
    cracked += ClassifyCrumbleFloorWord(word) == CrumbleFloorCell::kCracked;
    pits += ClassifyCrumbleFloorWord(word) == CrumbleFloorCell::kPit;
  }
  nlohmann::json histogram = nlohmann::json::object();
  for (auto word : words) {
    const auto key = std::to_string(word);
    histogram[key] = histogram.value(key, 0) + 1;
  }
  RecordProperty("words", histogram.dump());
  RecordProperty("floor1", room.floor1());
  RecordProperty("floor2", room.floor2());
  // The current authored room has tag $34, but floors 9/8 and no $CA
  // placement. A tag alone must not manufacture eligible cells.
  EXPECT_EQ(room.floor1(), 9);
  EXPECT_EQ(room.floor2(), 8);
  EXPECT_EQ(intact + cracked, 0) << histogram.dump();
  EXPECT_TRUE(
      std::none_of(room.GetTileObjects().begin(), room.GetTileObjects().end(),
                   [](const auto& object) { return object.id_ == 0xCA; }));
  RecordProperty("intact", intact);
  RecordProperty("cracked", cracked);
  RecordProperty("pit_words", pits);
  if (const char* out = std::getenv("YAZE_CRUMBLE_GRID_OUT")) {
    std::ofstream file(out);
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        const auto cell = ClassifyCrumbleFloorWord(words[y * 32 + x]);
        file << (cell == CrumbleFloorCell::kIntact    ? 'o'
                 : cell == CrumbleFloorCell::kCracked ? 'c'
                 : cell == CrumbleFloorCell::kPit     ? 'X'
                                                      : '.');
      }
      file << '\n';
    }
  }
  CrumbleFloorView preview;
  preview.Sync(&rom, &project, room);
  const auto object_count = room.GetTileObjectCount();
  // In-memory fixture only: even 8px coordinates align to the tag's 16px grid.
  // $CA uses the existing ROM floor pattern; no hardcoded tilemap injection.
  ASSERT_TRUE(
      room.AddObject(zelda3::RoomObject(0xCA, 8, 8, 0,
                                        zelda3::RoomObject::kListBg2Overlay))
          .ok());
  room.RenderRoomGraphics();
  EXPECT_EQ(ReadCrumbleFloorWords(room), words);  // BG2 is not sampled.
  ASSERT_TRUE(room.RemoveObject(object_count).ok());
  ASSERT_TRUE(
      room.AddObject(zelda3::RoomObject(0xCA, 8, 8, 0,
                                        zelda3::RoomObject::kListPrimary))
          .ok());
  room.RenderRoomGraphics();
  preview.Sync(&rom, &project, room);
  const auto placed_words = ReadCrumbleFloorWords(room);
  EXPECT_EQ(placed_words[4 * 32 + 4], 0x0C62);
  EXPECT_EQ(preview.model().cell(4 * 32 + 4), CrumbleFloorCell::kIntact);
  EXPECT_EQ(preview.model().cell(4 * 32 + 5), CrumbleFloorCell::kIntact);
  ASSERT_TRUE(room.RemoveObject(object_count).ok());
  room.RenderRoomGraphics();
  preview.Sync(&rom, &project, room);
  EXPECT_EQ(ReadCrumbleFloorWords(room), words);
  EXPECT_EQ(rom.vector(), before);
}

}  // namespace
}  // namespace yaze::editor
