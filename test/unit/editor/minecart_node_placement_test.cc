#include "app/editor/dungeon/ui/minecart/minecart_node_tool.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/ui/minecart/minecart_canvas_tool.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::editor {

class DungeonEditorV2MinecartNodeTestPeer {
 public:
  static zelda3::Room& Bind(DungeonEditorV2& editor,
                            MinecartTrackEditorPanel& panel, Rom& rom) {
    editor.minecart_track_editor_panel_ = &panel;
    panel.SetRooms(&editor.rooms_);
    auto& room = editor.rooms_[0x98];
    room = zelda3::Room(0x98, &rom);
    room.LoadSprites();
    room.SetLoaded(true);
    return room;
  }
  static void Detach(DungeonEditorV2& editor) {
    editor.minecart_track_editor_panel_ = nullptr;
  }
  static absl::Status Place(DungeonEditorV2& editor, int x, int y,
                            const MinecartNodeBrush& brush) {
    return editor.PlaceMinecartNode(0x98, x, y, brush);
  }
  static size_t UndoCount(const DungeonEditorV2& editor) {
    return editor.undo_manager_.UndoStackSize();
  }
  static void AdvanceGeneration(DungeonEditorV2& editor) {
    ++editor.minecart_collision_generation_;
  }
  static void BeginSpriteGesture(DungeonEditorV2& editor) {
    editor.BeginEntityUndoSnapshot(0x98, MutationDomain::kSprites);
  }
  static zelda3::Room& OtherRoom(DungeonEditorV2& editor, Rom& rom) {
    auto& room = editor.rooms_[0x99];
    room = zelda3::Room(0x99, &rom);
    room.LoadSprites();
    room.SetLoaded(true);
    return room;
  }
};

namespace {

std::vector<MinecartTrack> ReservedTracks() {
  std::vector<MinecartTrack> tracks;
  for (int i = 0; i < 32; ++i)
    tracks.push_back({i, 0, 0, 0});
  return tracks;
}

TEST(MinecartNodePreviewTest, CartUsesWorldCoordinatesAndOneSampledCell) {
  zelda3::Room room(0x98, nullptr);
  const auto before = room.custom_collision();
  const auto tracks = ReservedTracks();
  const auto result = PreviewMinecartNodePlacement(
      room, 50, 48, {MinecartNodeKind::kCartStart, 17, 0xBA}, tracks);
  ASSERT_TRUE(result.ok()) << result.status();
  ASSERT_EQ(result->sprites.size(), 1u);
  EXPECT_EQ(result->sprites[0].id(), 0xA3);
  EXPECT_EQ(result->sprites[0].subtype(), 17);
  EXPECT_EQ(result->sprites[0].x(), 25);
  EXPECT_EQ(result->sprites[0].y(), 24);
  EXPECT_EQ(result->sprites[0].layer(), 0);
  ASSERT_TRUE(result->track_before);
  ASSERT_TRUE(result->track_after);
  EXPECT_EQ(*result->track_after, (MinecartTrack{17, 0x98, 0x1190, 0x1380}));
  EXPECT_TRUE(result->collision.HasOverride(48 * 64 + 50));
  EXPECT_EQ(result->collision.tiles[48 * 64 + 50], 0xBA);
  EXPECT_FALSE(result->collision.HasOverride(48 * 64 + 51));
  EXPECT_FALSE(result->collision.HasOverride(49 * 64 + 50));
  EXPECT_EQ(room.custom_collision(), before);
  EXPECT_TRUE(room.GetSprites().empty());
}

TEST(MinecartNodePreviewTest, RejectsInvalidIdentityLayerAndSpriteGrid) {
  zelda3::Room room(0x98, nullptr);
  auto tracks = ReservedTracks();
  const MinecartNodeBrush cart{MinecartNodeKind::kCartStart, 17, 0xB7};
  for (const auto [x, y] : {std::pair{-2, 8}, {64, 8}, {8, 64}, {9, 8}, {8, 9}})
    EXPECT_FALSE(PreviewMinecartNodePlacement(room, x, y, cart, tracks).ok());
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 8, 8, cart, tracks, 1).ok());
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 8, 8,
                                            {MinecartNodeKind::kSwitch, 32, 0})
                   .ok());
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 8, 8,
                                            {MinecartNodeKind::kSwitch, -1, 0})
                   .ok());
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 8, 8, cart).ok());
  tracks[17].room_id = 0x98;
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 8, 8, cart, tracks).ok());
}

TEST(MinecartNodePreviewTest, RejectsNonRailAndProtectedNodeCollisions) {
  zelda3::Room room(0x98, nullptr);
  const auto tracks = ReservedTracks();
  const MinecartNodeBrush cart{MinecartNodeKind::kCartStart, 17, 0xB7};
  for (uint8_t value : {0x00, 0x01, 0x20, 0xB8, 0xD0}) {
    room.custom_collision().SetOverride(8 * 64 + 8, value);
    EXPECT_FALSE(PreviewMinecartNodePlacement(room, 8, 8, cart, tracks).ok());
  }
  room.custom_collision().SetOverride(8 * 64 + 8, 0xB1);
  EXPECT_TRUE(PreviewMinecartNodePlacement(room, 8, 8, cart, tracks).ok());
  room.custom_collision().SetOverride(8 * 64 + 8, 0xB7);
  EXPECT_TRUE(PreviewMinecartNodePlacement(room, 8, 8, cart, tracks).ok());
}

TEST(MinecartNodePreviewTest, SwitchTrackPairsChannelWithExactOrientationCell) {
  zelda3::Room room(0x77, nullptr);
  room.GetSprites().emplace_back(0xAF, 14, 19, 3, 0);
  for (uint8_t collision = 0xD0; collision <= 0xD3; ++collision) {
    const auto result = PreviewMinecartNodePlacement(
        room, 44, 38, {MinecartNodeKind::kSwitchTrack, 3, collision});
    ASSERT_TRUE(result.ok()) << result.status();
    EXPECT_EQ(result->sprites.back().id(), 0xB0);
    EXPECT_EQ(result->sprites.back().subtype(), 3);
    EXPECT_EQ(result->collision.tiles[38 * 64 + 44], collision);
    EXPECT_FALSE(result->collision.HasOverride(38 * 64 + 45));
    EXPECT_FALSE(result->track_before);
  }
  EXPECT_FALSE(PreviewMinecartNodePlacement(
                   room, 44, 38, {MinecartNodeKind::kSwitchTrack, 3, 0xB2})
                   .ok());
}

TEST(MinecartNodePreviewTest,
     LeverPreservesCollisionAndPreventsDuplicateLever) {
  zelda3::Room room(0x77, nullptr);
  room.custom_collision().SetOverride(38 * 64 + 28, 0);
  const auto collision = room.custom_collision();
  auto result = PreviewMinecartNodePlacement(room, 28, 38,
                                             {MinecartNodeKind::kSwitch, 3});
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->sprites.back().id(), 0xAF);
  EXPECT_EQ(result->collision, collision);
  room.GetSprites() = result->sprites;
  EXPECT_FALSE(
      PreviewMinecartNodePlacement(room, 30, 38, {MinecartNodeKind::kSwitch, 3})
          .ok());
  EXPECT_FALSE(PreviewMinecartNodePlacement(
                   room, 28, 38, {MinecartNodeKind::kSwitchTrack, 4, 0xD0})
                   .ok());
}

class MinecartNodePlacementTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_ = ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    constexpr int table = 0x048000;
    constexpr int empty = 0x049000;
    constexpr int target = 0x049100;
    constexpr int next = 0x049200;
    ASSERT_TRUE(
        rom_.WriteWord(zelda3::kRoomsSpritePointer, PcToSnes(table) & 0xFFFF)
            .ok());
    for (int id = 0; id < zelda3::kNumberOfRooms; ++id)
      ASSERT_TRUE(
          rom_.WriteWord(table + id * 2, PcToSnes(empty) & 0xFFFF).ok());
    ASSERT_TRUE(
        rom_.WriteWord(table + 0x98 * 2, PcToSnes(target) & 0xFFFF).ok());
    ASSERT_TRUE(rom_.WriteWord(table + 0x99 * 2, PcToSnes(next) & 0xFFFF).ok());
    for (int address : {empty, target, next})
      ASSERT_TRUE(rom_.WriteVector(address, {0, 0xFF}).ok());
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("yaze_minecart_nodes_" + std::to_string(nonce));
    std::filesystem::create_directories(root_ / "Sprites/Objects/data");
    source_ = root_ / "Sprites/Objects/data/minecart_tracks.asm";
    std::ostringstream source;
    for (const char* label :
         {".TrackStartingRooms", ".TrackStartingX", ".TrackStartingY"}) {
      source << label << "\n  dw ";
      for (int i = 0; i < 32; ++i)
        source << (i ? ", " : "") << "$0000";
      source << '\n';
    }
    original_source_ = source.str();
    std::ofstream(source_) << original_source_;
    project_.name = "Minecart Node Test";
    project_.filepath = (root_ / "test.yaze").string();
    project_.code_folder = (root_ / "Core").string();
    std::ofstream(project_.filepath) << "name=Minecart Node Test\n";
    ASSERT_TRUE(project_.hack_manifest
                    .LoadFromString(R"json({
      "manifest_version":3,
      "minecart_tracks":{"source":{"format":"yaze-minecart-track-table",
      "version":1,"path":"Sprites/Objects/data/minecart_tracks.asm"}}
    })json")
                    .ok());
    ASSERT_TRUE(panel_.SetProject(&project_).ok());
    ASSERT_TRUE(panel_.ReloadTracks().ok());
    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    room_ = &DungeonEditorV2MinecartNodeTestPeer::Bind(*editor_, panel_, rom_);
    original_rom_ = rom_.vector();
  }
  void TearDown() override {
    if (editor_)
      DungeonEditorV2MinecartNodeTestPeer::Detach(*editor_);
    editor_.reset();
    ImGui::DestroyContext(imgui_);
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }
  std::string ReadSource() const {
    std::ifstream stream(source_);
    return std::string(std::istreambuf_iterator<char>(stream), {});
  }
  absl::Status Place(const MinecartNodeBrush& brush) {
    return DungeonEditorV2MinecartNodeTestPeer::Place(*editor_, 50, 48, brush);
  }
  ImGuiContext* imgui_ = nullptr;
  Rom rom_;
  project::YazeProject project_;
  MinecartTrackEditorPanel panel_;
  std::unique_ptr<DungeonEditorV2> editor_;
  zelda3::Room* room_ = nullptr;
  std::filesystem::path root_, source_;
  std::string original_source_;
  std::vector<uint8_t> original_rom_;
};

TEST_F(MinecartNodePlacementTest, CartIsOneUndoWithDraftAndNoImmediateWrites) {
  ASSERT_TRUE(Place({MinecartNodeKind::kCartStart, 17, 0xBA}).ok());
  EXPECT_EQ(DungeonEditorV2MinecartNodeTestPeer::UndoCount(*editor_), 1u);
  EXPECT_EQ(panel_.GetTracks().size(), 32u);
  EXPECT_EQ(panel_.GetTracks()[17], (MinecartTrack{17, 0x98, 0x1190, 0x1380}));
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(room_->GetSprites()[0].subtype(), 17);
  EXPECT_EQ(room_->custom_collision().tiles[48 * 64 + 50], 0xBA);
  EXPECT_TRUE(room_->sprites_dirty());
  EXPECT_TRUE(room_->custom_collision_dirty());
  EXPECT_TRUE(panel_.HasPairedTrackDraft());
  EXPECT_FALSE(panel_.DiscardUnpublishedChanges().ok());
  EXPECT_EQ(ReadSource(), original_source_);
  EXPECT_EQ(rom_.vector(), original_rom_);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_EQ(panel_.GetTracks()[17].room_id, 0);
  EXPECT_FALSE(room_->custom_collision().HasOverride(48 * 64 + 50));
  EXPECT_FALSE(panel_.HasUnpublishedChanges());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(panel_.GetTracks()[17].room_id, 0x98);
  ASSERT_TRUE(panel_.SaveTracks().ok());
  const auto parsed = MinecartTrackSourceDocument::Parse(ReadSource());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->tracks().size(), 32u);
  EXPECT_EQ(parsed->tracks()[17], panel_.GetTracks()[17]);
  EXPECT_EQ(rom_.vector(), original_rom_);
}

TEST_F(MinecartNodePlacementTest, SwitchTrackUndoRestoresPriorRail) {
  room_->custom_collision().SetOverride(48 * 64 + 50, 0xB2);
  ASSERT_TRUE(Place({MinecartNodeKind::kSwitchTrack, 3, 0xD0}).ok());
  EXPECT_EQ(room_->GetSprites().back().id(), 0xB0);
  EXPECT_EQ(room_->custom_collision().tiles[48 * 64 + 50], 0xD0);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_EQ(room_->custom_collision().tiles[48 * 64 + 50], 0xB2);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->custom_collision().tiles[48 * 64 + 50], 0xD0);
  EXPECT_EQ(ReadSource(), original_source_);
}

TEST_F(MinecartNodePlacementTest,
       OverviewSnapshotsFollowUnsavedCartAndRotatingTrackUndoRedo) {
  using Kind = MinecartOverviewNodeKind;
  auto snapshot = [&]() {
    return CollectMinecartOverviewNodes(*room_, panel_.GetTracks());
  };
  EXPECT_TRUE(snapshot().empty());
  ASSERT_TRUE(Place({MinecartNodeKind::kCartStart, 17, 0xB8}).ok());
  auto nodes = snapshot();
  ASSERT_EQ(nodes.size(), 2u);  // Paired cart/start and sampled stop.
  EXPECT_EQ(nodes.front().kind, Kind::kCartStart);
  EXPECT_TRUE(nodes.front().paired_actor);
  EXPECT_EQ(nodes.front().pixel_x, 400);
  EXPECT_EQ(nodes.front().pixel_y, 384);
  EXPECT_EQ(panel_.GetTracks()[17].room_id, 0x98);
  EXPECT_EQ(ReadSource(), original_source_);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(snapshot().empty());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(snapshot().size(), 2u);
  ASSERT_TRUE(editor_->Undo().ok());

  ASSERT_TRUE(Place({MinecartNodeKind::kSwitchTrack, 9, 0xD2}).ok());
  nodes = snapshot();
  ASSERT_EQ(nodes.size(), 1u);
  EXPECT_EQ(nodes.front().kind, Kind::kRotatingTrack);
  EXPECT_EQ(nodes.front().id, 9);
  EXPECT_TRUE(nodes.front().has_collision);
  EXPECT_EQ(nodes.front().collision, 0xD2);
  EXPECT_TRUE(IsMinecartOverviewNodeSelected(nodes.front(), nodes, {-1, 9}));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(snapshot().empty());
  ASSERT_TRUE(editor_->Redo().ok());
  nodes = snapshot();
  ASSERT_FALSE(nodes.empty());
  EXPECT_EQ(nodes.front().collision, 0xD2);
  EXPECT_EQ(ReadSource(), original_source_);
  EXPECT_EQ(rom_.vector(), original_rom_);
}

TEST_F(MinecartNodePlacementTest,
       CollisionCasFailureLeavesSpriteAndDraftAlone) {
  ASSERT_TRUE(Place({MinecartNodeKind::kCartStart, 17, 0xB7}).ok());
  room_->custom_collision().SetOverride(0, 0x20);
  EXPECT_FALSE(editor_->Undo().ok());
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(panel_.GetTracks()[17].room_id, 0x98);
  EXPECT_EQ(room_->custom_collision().tiles[0], 0x20);
}

TEST_F(MinecartNodePlacementTest, SpriteCasAndProjectGenerationRejectReplay) {
  ASSERT_TRUE(Place({MinecartNodeKind::kSwitchTrack, 3, 0xD2}).ok());
  room_->GetSprites()[0].set_key_drop(1);
  EXPECT_FALSE(editor_->Undo().ok());
  EXPECT_EQ(room_->custom_collision().tiles[48 * 64 + 50], 0xD2);
  room_->GetSprites()[0].set_key_drop(0);
  DungeonEditorV2MinecartNodeTestPeer::AdvanceGeneration(*editor_);
  EXPECT_FALSE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites().size(), 1u);
}

TEST_F(MinecartNodePlacementTest, ExistingGlobalCartIdentityRejectsUnusedSlot) {
  auto& other = DungeonEditorV2MinecartNodeTestPeer::OtherRoom(*editor_, rom_);
  other.GetSprites().emplace_back(0xA3, 2, 2, 17, 0);
  EXPECT_FALSE(Place({MinecartNodeKind::kCartStart, 17, 0xB7}).ok());
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_EQ(panel_.GetTracks()[17].room_id, 0);
  EXPECT_FALSE(room_->custom_collision().HasOverrides());
  EXPECT_EQ(DungeonEditorV2MinecartNodeTestPeer::UndoCount(*editor_), 0u);
}

TEST_F(MinecartNodePlacementTest, RedoRechecksIdentityInOtherRoomDraft) {
  ASSERT_TRUE(Place({MinecartNodeKind::kCartStart, 17, 0xB7}).ok());
  ASSERT_TRUE(editor_->Undo().ok());
  auto& other = DungeonEditorV2MinecartNodeTestPeer::OtherRoom(*editor_, rom_);
  other.GetSprites().emplace_back(0xA3, 2, 2, 17, 0);
  EXPECT_FALSE(editor_->Redo().ok());
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_EQ(panel_.GetTracks()[17].room_id, 0);
  EXPECT_FALSE(room_->custom_collision().HasOverrides());
}

TEST_F(MinecartNodePlacementTest, StreamPreflightFailureChangesNoDraftOrNode) {
  ASSERT_TRUE(rom_.WriteWord(0x048000 + 0x98 * 2, 0xFFFF).ok());
  const auto malformed_rom = rom_.vector();
  EXPECT_FALSE(Place({MinecartNodeKind::kCartStart, 17, 0xB7}).ok());
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_EQ(panel_.GetTracks()[17].room_id, 0);
  EXPECT_FALSE(room_->custom_collision().HasOverrides());
  EXPECT_EQ(DungeonEditorV2MinecartNodeTestPeer::UndoCount(*editor_), 0u);
  EXPECT_EQ(rom_.vector(), malformed_rom);
  EXPECT_EQ(ReadSource(), original_source_);
}

TEST_F(MinecartNodePlacementTest, FinalizesPriorSpriteGestureBeforeNodePlan) {
  DungeonEditorV2MinecartNodeTestPeer::BeginSpriteGesture(*editor_);
  room_->GetSprites().emplace_back(0xAF, 2, 2, 1, 0);
  room_->MarkSpritesDirty();
  ASSERT_TRUE(Place({MinecartNodeKind::kSwitchTrack, 3, 0xD0}).ok());
  EXPECT_EQ(DungeonEditorV2MinecartNodeTestPeer::UndoCount(*editor_), 2u);
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(room_->GetSprites()[0].id(), 0xAF);
  EXPECT_EQ(room_->GetSprites()[0].subtype(), 1);
  EXPECT_FALSE(room_->custom_collision().HasOverrides());
}

MinecartNodeBrush MoveBrush(const zelda3::Room& room, MinecartNodeKind kind,
                            int channel, uint8_t collision, int x, int y,
                            uint8_t key_drop = 0) {
  MinecartNodeBrush brush{kind, channel, collision};
  brush.source_x = x;
  brush.source_y = y;
  brush.source_key_drop = key_drop;
  brush.expected_collision = room.custom_collision();
  return brush;
}

TEST(MinecartNodePreviewTest, ExistingTrackMovePreservesEveryOrientation) {
  for (uint8_t orientation = 0xD0; orientation <= 0xD3; ++orientation) {
    zelda3::Room room(0x77, nullptr);
    room.GetSprites().emplace_back(0xB0, 22, 19, 3, 0);
    room.GetSprites()[0].set_key_drop(2);
    room.custom_collision().SetOverride(38 * 64 + 44, orientation);
    room.custom_collision().SetOverride(38 * 64 + 46, 0xB0);
    const auto brush = MoveBrush(room, MinecartNodeKind::kSwitchTrack, 3,
                                 orientation, 44, 38, 2);
    const auto result = PreviewMinecartNodePlacement(room, 46, 38, brush);
    ASSERT_TRUE(result.ok()) << result.status();
    ASSERT_EQ(result->sprites.size(), 1u);
    EXPECT_EQ(result->sprites[0].x(), 23);
    EXPECT_EQ(result->sprites[0].y(), 19);
    EXPECT_EQ(result->sprites[0].subtype(), 3);
    EXPECT_EQ(result->sprites[0].key_drop(), 2);
    EXPECT_FALSE(result->collision.HasOverride(38 * 64 + 44));
    EXPECT_EQ(result->collision.tiles[38 * 64 + 46], orientation);
    EXPECT_EQ(room.GetSprites()[0].x(), 22);
    EXPECT_EQ(room.custom_collision().tiles[38 * 64 + 44], orientation);
  }
}

TEST(MinecartNodePreviewTest, MoveRejectsChangedSourceTokensAndCollisions) {
  zelda3::Room room(0x77, nullptr);
  room.GetSprites().emplace_back(0xB0, 22, 19, 3, 0);
  room.custom_collision().SetOverride(38 * 64 + 44, 0xD0);
  auto brush = MoveBrush(room, MinecartNodeKind::kSwitchTrack, 3, 0xD0, 44, 38);
  auto token = brush;
  token.id = 4;
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, token).ok());
  token = brush;
  token.source_key_drop = 1;
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, token).ok());
  token = brush;
  token.source_x = 45;
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, token).ok());
  token = brush;
  token.expected_collision.reset();
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, token).ok());
  room.GetSprites()[0].set_layer(1);
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, brush).ok());
  room.GetSprites()[0].set_layer(0);
  room.custom_collision().SetOverride(0, 0x20);
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, brush).ok());
  brush.expected_collision = room.custom_collision();
  room.custom_collision().SetOverride(38 * 64 + 44, 0xD1);
  brush.expected_collision = room.custom_collision();
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, brush).ok());
}

TEST(MinecartNodePreviewTest, MoveRejectsDuplicateOccupiedAndNonRailTargets) {
  zelda3::Room room(0x77, nullptr);
  room.GetSprites().emplace_back(0xB0, 22, 19, 3, 0);
  room.custom_collision().SetOverride(38 * 64 + 44, 0xD0);
  auto brush = MoveBrush(room, MinecartNodeKind::kSwitchTrack, 3, 0xD0, 44, 38);
  room.GetSprites().emplace_back(0xB0, 22, 19, 3, 0);
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, brush).ok());
  room.GetSprites().pop_back();
  room.GetSprites().emplace_back(0x01, 23, 19, 0, 0);
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, brush).ok());
  room.GetSprites().pop_back();
  room.custom_collision().SetOverride(38 * 64 + 46, 0x20);
  brush.expected_collision = room.custom_collision();
  EXPECT_FALSE(PreviewMinecartNodePlacement(room, 46, 38, brush).ok());
}

TEST_F(MinecartNodePlacementTest, ExistingLeverMoveIsOneUndoAndKeepsCollision) {
  room_->GetSprites().emplace_back(0xAF, 22, 19, 3, 0);
  room_->GetSprites()[0].set_key_drop(1);
  const auto collision = room_->custom_collision();
  const auto brush =
      MoveBrush(*room_, MinecartNodeKind::kSwitch, 3, 0xB7, 44, 38, 1);
  ASSERT_TRUE(Place(brush).ok());
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(room_->GetSprites()[0].x(), 25);
  EXPECT_EQ(room_->GetSprites()[0].y(), 24);
  EXPECT_EQ(room_->GetSprites()[0].key_drop(), 1);
  EXPECT_EQ(room_->custom_collision(), collision);
  EXPECT_EQ(DungeonEditorV2MinecartNodeTestPeer::UndoCount(*editor_), 1u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 22);
  EXPECT_EQ(room_->GetSprites()[0].y(), 19);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 25);
  EXPECT_EQ(room_->GetSprites()[0].key_drop(), 1);
  EXPECT_EQ(room_->custom_collision(), collision);
  EXPECT_EQ(ReadSource(), original_source_);
  EXPECT_EQ(rom_.vector(), original_rom_);
}

TEST_F(MinecartNodePlacementTest,
       ExistingTrackMovePairsSpriteAndCollisionUndo) {
  room_->GetSprites().emplace_back(0xB0, 22, 19, 3, 0);
  room_->GetSprites()[0].set_key_drop(2);
  room_->custom_collision().SetOverride(38 * 64 + 44, 0xD3);
  const auto original = room_->custom_collision();
  const auto brush =
      MoveBrush(*room_, MinecartNodeKind::kSwitchTrack, 3, 0xD3, 44, 38, 2);
  ASSERT_TRUE(Place(brush).ok());
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(room_->GetSprites()[0].x(), 25);
  EXPECT_EQ(room_->GetSprites()[0].y(), 24);
  EXPECT_EQ(room_->GetSprites()[0].key_drop(), 2);
  EXPECT_FALSE(room_->custom_collision().HasOverride(38 * 64 + 44));
  EXPECT_EQ(room_->custom_collision().tiles[48 * 64 + 50], 0xD3);
  EXPECT_EQ(DungeonEditorV2MinecartNodeTestPeer::UndoCount(*editor_), 1u);
  const auto moved = room_->custom_collision();
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 22);
  EXPECT_EQ(room_->GetSprites()[0].y(), 19);
  EXPECT_EQ(room_->custom_collision(), original);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 25);
  EXPECT_EQ(room_->custom_collision(), moved);
  EXPECT_EQ(ReadSource(), original_source_);
  EXPECT_EQ(rom_.vector(), original_rom_);
}

TEST_F(MinecartNodePlacementTest, StaleMoveFailsBeforeEitherPlaneChanges) {
  room_->GetSprites().emplace_back(0xB0, 22, 19, 3, 0);
  room_->custom_collision().SetOverride(38 * 64 + 44, 0xD0);
  const auto brush =
      MoveBrush(*room_, MinecartNodeKind::kSwitchTrack, 3, 0xD0, 44, 38);
  room_->custom_collision().SetOverride(0, 0x20);
  const auto current = room_->custom_collision();
  EXPECT_FALSE(Place(brush).ok());
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(room_->GetSprites()[0].x(), 22);
  EXPECT_EQ(room_->custom_collision(), current);
  EXPECT_EQ(DungeonEditorV2MinecartNodeTestPeer::UndoCount(*editor_), 0u);
}

TEST(MinecartNodePreviewTest, CartMoveUsesTheExistingPairedStartOperation) {
  zelda3::Room room(0x98, nullptr);
  auto brush = MoveBrush(room, MinecartNodeKind::kCartStart, 17, 0xB7, 44, 38);
  EXPECT_FALSE(
      PreviewMinecartNodePlacement(room, 46, 38, brush, ReservedTracks()).ok());
}

}  // namespace
}  // namespace yaze::editor
