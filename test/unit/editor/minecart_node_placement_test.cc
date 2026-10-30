#include "app/editor/dungeon/ui/minecart/minecart_node_tool.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/ui/minecart/minecart_assembly.h"
#include "app/editor/dungeon/ui/minecart/minecart_canvas_tool.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "core/project.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::editor {

class DungeonEditorV2MinecartNodeTestPeer {
 public:
  static absl::StatusOr<MinecartAssemblyPlan> PreviewAssembly(
      DungeonEditorV2& editor, int room,
      const DungeonSelectionClipboard& clipboard, int x, int y,
      const MinecartAssemblyMapping& mapping = {}) {
    return editor.PreviewMinecartAssembly(room, clipboard, x, y, mapping);
  }
  static absl::Status ApplyAssembly(DungeonEditorV2& editor,
                                    const MinecartAssemblyPlan& plan) {
    return editor.ApplyMinecartAssembly(plan);
  }
  static const auto& Ownership(DungeonEditorV2& editor) {
    return editor.minecart_collision_ownership_;
  }
  static uint64_t Generation(DungeonEditorV2& editor) {
    return editor.minecart_collision_generation_;
  }
  static DungeonCanvasViewer& WorkbenchSelection(DungeonEditorV2& editor,
                                                 Rom& rom, int room) {
    editor.current_room_id_ = room;
    editor.workbench_viewer_ = std::make_unique<DungeonCanvasViewer>();
    editor.workbench_viewer_->RefreshRomBackedState(&rom, nullptr,
                                                    &editor.rooms_, room);
    editor.workbench_viewer_->object_interaction().SetSelectedObjects({0});
    return *editor.workbench_viewer_;
  }
  static absl::StatusOr<DungeonSelectionClipboard> CaptureAssembly(
      DungeonEditorV2& editor, int room) {
    return editor.CaptureSelectedMinecartAssembly(room);
  }
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

TEST(MinecartAssemblyPlanTest, PathsUseAdmittedShapesForBothProfilesAndStyles) {
  for (auto profile : {zelda3::MinecartRailProfile::kLegacy,
                       zelda3::MinecartRailProfile::kExpanded})
    for (auto style :
         {zelda3::MinecartRailStyle::kFloor, zelda3::MinecartRailStyle::kPit}) {
      const std::vector<std::pair<int, int>> points{
          {8, 8}, {12, 8}, {12, 12}, {8, 12}, {8, 8}};
      auto path = BuildMinecartRailPath(points, style, profile);
      ASSERT_TRUE(path.ok()) << path.status();
      ASSERT_EQ(path->objects.size(), 8u);
      std::set<zelda3::MinecartRailShape> shapes;
      for (const auto& object : path->objects) {
        const auto* descriptor = zelda3::FindMinecartRailDescriptor(
            object.id_, object.size_, profile);
        ASSERT_NE(descriptor, nullptr);
        EXPECT_EQ(descriptor->style, style);
        EXPECT_TRUE(descriptor->collision_verified);
        shapes.insert(descriptor->shape);
      }
      EXPECT_EQ(shapes.size(),
                6u);  // Four corners and both straight directions.
      zelda3::Room room(0x98, nullptr);
      auto plan = PlanMinecartAssembly(room, *path, 8, 8, {}, ReservedTracks(),
                                       {}, profile);
      ASSERT_TRUE(plan.ok()) << plan.status();
      EXPECT_EQ(plan->selection.after.objects.size(), 8u);
      EXPECT_TRUE(room.GetTileObjects().empty());
      EXPECT_FALSE(room.custom_collision().HasOverrides());
    }
}

TEST(MinecartAssemblyPlanTest,
     RejectsCrossingsBoundsLayersAndAuthoredConflicts) {
  const auto profile = zelda3::MinecartRailProfile::kLegacy;
  const auto style = zelda3::MinecartRailStyle::kPit;
  for (const std::vector<std::pair<int, int>>& points :
       {std::vector<std::pair<int, int>>{{8, 8}, {10, 10}},
        {{8, 8}, {12, 8}, {8, 8}},
        {{8, 8}, {64, 8}},
        {{9, 8}, {12, 8}},
        {{8, 8}, {12, 8}, {12, 10}, {8, 10}}})
    EXPECT_FALSE(BuildMinecartRailPath(points, style, profile).ok());
  auto path = BuildMinecartRailPath(
      std::vector<std::pair<int, int>>{{8, 8}, {12, 8}}, style, profile);
  ASSERT_TRUE(path.ok());
  zelda3::Room room(0x98, nullptr);
  room.custom_collision().SetOverride(8 * 64 + 8, 0x20);
  EXPECT_FALSE(
      PlanMinecartAssembly(room, *path, 8, 8, {}, ReservedTracks(), {}, profile)
          .ok());
  room.custom_collision() = {};
  room.custom_collision().SetOverride(8 * 64 + 8, 0xB7);
  auto preserved = PlanMinecartAssembly(room, *path, 8, 8, {}, ReservedTracks(),
                                        {}, profile);
  ASSERT_TRUE(preserved.ok()) << preserved.status();
  EXPECT_EQ(preserved->collision_after.tiles[8 * 64 + 8], 0xB7);
  path->objects[0].layer_ = zelda3::RoomObject::LayerType::BG2;
  EXPECT_FALSE(
      PlanMinecartAssembly(room, *path, 8, 8, {}, ReservedTracks(), {}, profile)
          .ok());
}

class MinecartAssemblyEditorTest : public MinecartNodePlacementTest {
 protected:
  void SetUp() override {
    MinecartNodePlacementTest::SetUp();
    old_assets_ = zelda3::CustomObjectManager::Get().SnapshotState();
    old_custom_ = core::FeatureFlags::get().kEnableCustomObjects;
    old_flags_ = core::FeatureFlags::get().dungeon;
    auto& assets = zelda3::CustomObjectManager::Get();
    std::filesystem::create_directories(root_ / "assets");
    assets.Initialize((root_ / "assets").string());
    assets.ClearObjectFileMap();
    assets.SetMinecartRailProfile(zelda3::MinecartRailProfile::kLegacy);
    for (const auto& descriptor : zelda3::MinecartRailDescriptors()) {
      auto encoded = zelda3::EncodeCustomObjectBinary(zelda3::CustomObject{
          .tiles = {
              {0, 0, 0x100}, {1, 0, 0x101}, {0, 1, 0x102}, {1, 1, 0x103}}});
      ASSERT_TRUE(encoded.ok());
      std::ofstream out(root_ / "assets" / descriptor.filename,
                        std::ios::binary);
      out.write(reinterpret_cast<const char*>(encoded->data()),
                encoded->size());
    }
    core::FeatureFlags::get().kEnableCustomObjects = true;
    auto write_pointer = [&](int slot, int pc) {
      const auto snes = PcToSnes(pc);
      for (int n = 0; n < 3; ++n)
        rom_.mutable_data()[slot + n] = (snes >> (n * 8)) & 255;
    };
    write_pointer(zelda3::kRoomObjectPointer, 0xF8000);
    for (int id = 0; id < zelda3::kNumberOfRooms; ++id) {
      const int pc = 0x138000 + id * 0x40;
      write_pointer(0xF8000 + id * 3, pc);
      ASSERT_TRUE(
          rom_.WriteVector(pc, {0, 0, 255, 255, 255, 255, 0xF0, 255, 255, 255})
              .ok());
    }
    room_->SetTileObjects({});
    room_->ClearSaveDirtyState();
    original_rom_ = rom_.vector();
    auto& flags = core::FeatureFlags::get().dungeon;
    flags.kSaveObjects = flags.kSaveSprites = flags.kSaveCollision = true;
    flags.kSaveRoomHeaders = flags.kSaveChests = flags.kSavePotItems = false;
    flags.kSaveTorches = flags.kSaveBlocks = flags.kSavePits =
        flags.kSaveWaterFillZones = false;
    flags.kSaveEntrances = flags.kSavePalettes = false;
  }
  void TearDown() override {
    core::FeatureFlags::get().dungeon = old_flags_;
    core::FeatureFlags::get().kEnableCustomObjects = old_custom_;
    zelda3::CustomObjectManager::Get().RestoreState(old_assets_);
    MinecartNodePlacementTest::TearDown();
    gfx::Arena::Get().ClearTextureQueue();
  }
  absl::StatusOr<MinecartAssemblyPlan> Path() {
    auto path = BuildMinecartRailPath(
        std::vector<std::pair<int, int>>{{8, 8}, {12, 8}, {12, 12}},
        zelda3::MinecartRailStyle::kPit, zelda3::MinecartRailProfile::kLegacy);
    if (!path.ok())
      return path.status();
    return DungeonEditorV2MinecartNodeTestPeer::PreviewAssembly(*editor_, 0x98,
                                                                *path, 8, 8);
  }
  absl::Status Apply(const MinecartAssemblyPlan& plan) {
    return DungeonEditorV2MinecartNodeTestPeer::ApplyAssembly(*editor_, plan);
  }
  zelda3::CustomObjectManager::State old_assets_;
  bool old_custom_;
  decltype(core::FeatureFlags::get().dungeon) old_flags_{};
};

TEST_F(MinecartAssemblyEditorTest, PathPreviewCancelAndApplyAreOneAtomicUndo) {
  const auto before = room_->custom_collision();
  auto plan = Path();
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(room_->GetTileObjects().empty());
  EXPECT_EQ(room_->custom_collision(), before);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), 0u);
  EXPECT_EQ(rom_.vector(), original_rom_);
  panel_.BeginRailPath(true);
  panel_.CancelAssemblyPreview();
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), 0u);
  ASSERT_TRUE(Apply(*plan).ok());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), 1u);
  EXPECT_EQ(room_->GetTileObjects().size(), 5u);
  const auto collision = room_->custom_collision();
  const auto ownership =
      DungeonEditorV2MinecartNodeTestPeer::Ownership(*editor_);
  EXPECT_FALSE(ownership.empty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(room_->GetTileObjects().empty());
  EXPECT_EQ(room_->custom_collision(), before);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->custom_collision(), collision);
  EXPECT_EQ(DungeonEditorV2MinecartNodeTestPeer::Ownership(*editor_),
            ownership);
  EXPECT_EQ(rom_.vector(), original_rom_);
}

TEST_F(MinecartAssemblyEditorTest,
       StaleRoomSourceGenerationAndBatchFailureDoNotMutate) {
  auto plan = Path();
  ASSERT_TRUE(plan.ok()) << plan.status();
  room_->custom_collision().SetOverride(0, 0x20);
  EXPECT_FALSE(Apply(*plan).ok());
  EXPECT_TRUE(room_->GetTileObjects().empty());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), 0u);
  room_->custom_collision() = {};
  auto changed = panel_.GetTracks();
  changed[4] = {4, 0x77, 0xE00, 0xE00};
  ASSERT_TRUE(panel_
                  .RestoreTrackDraftBatch(panel_.track_generation(),
                                          panel_.GetTracks(), changed)
                  .ok());
  EXPECT_FALSE(Apply(*plan).ok());
  EXPECT_TRUE(room_->GetTileObjects().empty());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), 0u);
  EXPECT_EQ(panel_.GetTracks(), changed);
  DungeonEditorV2MinecartNodeTestPeer::AdvanceGeneration(*editor_);
  EXPECT_FALSE(Apply(*plan).ok());
  EXPECT_EQ(rom_.vector(), original_rom_);
  // Failed batch validates all expected records before the first can change.
  auto expected = changed;
  expected[5].room_id = 1;
  auto replacement = changed;
  replacement[4] = {4, 0, 0, 0};
  EXPECT_FALSE(panel_
                   .RestoreTrackDraftBatch(panel_.track_generation(), expected,
                                           replacement)
                   .ok());
  EXPECT_EQ(panel_.GetTracks(), changed);
}

TEST_F(MinecartAssemblyEditorTest,
       ApplyRechecksEncodedCapacityWithoutPublishingAnyPlane) {
  auto plan = Path();
  ASSERT_TRUE(plan.ok()) << plan.status();
  const int boundary = 0x138000 + 0x98 * 0x40 + 11;
  const auto snes = PcToSnes(boundary);
  for (int b = 0; b < 3; ++b)
    rom_.mutable_data()[0xF8000 + 0x97 * 3 + b] = (snes >> (8 * b)) & 255;
  ASSERT_TRUE(rom_.WriteVector(boundary,
                               {0, 0, 255, 255, 255, 255, 0xF0, 255, 255, 255})
                  .ok());
  const auto bytes = rom_.vector();
  const auto tracks = panel_.GetTracks();
  EXPECT_FALSE(Apply(*plan).ok());
  EXPECT_TRUE(room_->GetTileObjects().empty());
  EXPECT_FALSE(room_->custom_collision().HasOverrides());
  EXPECT_EQ(panel_.GetTracks(), tracks);
  EXPECT_EQ(rom_.vector(), bytes);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), 0u);
}

TEST_F(MinecartAssemblyEditorTest,
       CaptureUsesTheSelectedWorkbenchRoomAndRejectsAStaleRoomRequest) {
  auto plan = Path();
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(Apply(*plan).ok());
  core::FeatureFlags::get().dungeon.kUseWorkbench = true;
  panel_.SetActiveRoom(0x98);
  auto& viewer = DungeonEditorV2MinecartNodeTestPeer::WorkbenchSelection(
      *editor_, rom_, 0x98);
  auto copied =
      DungeonEditorV2MinecartNodeTestPeer::CaptureAssembly(*editor_, 0x98);
  ASSERT_TRUE(copied.ok()) << copied.status();
  EXPECT_EQ(copied->objects.size(), 1u);
  auto& other = DungeonEditorV2MinecartNodeTestPeer::OtherRoom(*editor_, rom_);
  other.SetTileObjects({});
  const auto history = editor_->undo_manager().UndoStackSize();
  EXPECT_FALSE(
      DungeonEditorV2MinecartNodeTestPeer::CaptureAssembly(*editor_, 0x99)
          .ok());
  EXPECT_EQ(viewer.current_room_id(), 0x98);
  EXPECT_TRUE(other.GetTileObjects().empty());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), history);
}

TEST_F(MinecartAssemblyEditorTest,
       TranslatedCopyKeepsSignedOffsetsAndRejectsMalformedCollision) {
  auto plan = Path();
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(Apply(*plan).ok());
  auto copy =
      CaptureMinecartAssembly(*room_, {0, 1, 2, 3, 4}, {}, panel_.GetTracks(),
                              zelda3::MinecartRailProfile::kLegacy);
  ASSERT_TRUE(copy.ok()) << copy.status();
  zelda3::Room target(0x99, nullptr);
  auto shifted =
      PlanMinecartAssembly(target, *copy, 0, 0, {}, panel_.GetTracks(), {},
                           zelda3::MinecartRailProfile::kLegacy);
  ASSERT_TRUE(shifted.ok()) << shifted.status();
  EXPECT_TRUE(shifted->collision_after.HasOverride(0));
  EXPECT_EQ(shifted->selection.after.objects[0].x_, 0);
  EXPECT_EQ(shifted->selection.after.objects[0].y_, 0);
  copy->minecart->collision[4096] = 0xB0;
  EXPECT_FALSE(PlanMinecartAssembly(target, *copy, 0, 0, {}, panel_.GetTracks(),
                                    {}, zelda3::MinecartRailProfile::kLegacy)
                   .ok());
  EXPECT_TRUE(target.GetTileObjects().empty());
  EXPECT_FALSE(target.custom_collision().HasOverrides());
}

TEST_F(MinecartAssemblyEditorTest,
       CopyMapsCartAndChannelsAndRoundTripsExistingWriters) {
  auto plan = Path();
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(Apply(*plan).ok());
  // Authored stop and node dependencies are captured with their selection.
  room_->custom_collision().SetOverride(8 * 64 + 8, 0xB9);
  room_->GetSprites().emplace_back(0xA3, 4, 4, 17, 0);
  room_->GetSprites().emplace_back(0xAF, 8, 6, 3, 0);
  room_->GetSprites().emplace_back(0xB0, 6, 6, 3, 0);
  room_->custom_collision().SetOverride(12 * 64 + 12, 0xD2);
  auto tracks = panel_.GetTracks();
  tracks[17] = {17, 0x98, 8 * 512 + 64, 9 * 512 + 64};
  ASSERT_TRUE(panel_
                  .RestoreTrackDraftBatch(panel_.track_generation(),
                                          panel_.GetTracks(), tracks)
                  .ok());
  auto copied =
      CaptureMinecartAssembly(*room_, {0, 1, 2, 3, 4},
                              {{EntityType::Sprite, 0},
                               {EntityType::Sprite, 1},
                               {EntityType::Sprite, 2}},
                              tracks, zelda3::MinecartRailProfile::kLegacy);
  ASSERT_TRUE(copied.ok()) << copied.status();
  auto& target = DungeonEditorV2MinecartNodeTestPeer::OtherRoom(*editor_, rom_);
  target.SetTileObjects({});
  target.ClearSaveDirtyState();
  auto missing = PlanMinecartAssembly(target, *copied, 20, 20, {}, tracks, {},
                                      zelda3::MinecartRailProfile::kLegacy);
  EXPECT_FALSE(missing.ok());
  MinecartAssemblyMapping mapping{{{17, 18}}, {{3, 4}}};
  auto translated =
      PlanMinecartAssembly(target, *copied, 20, 20, mapping, tracks, {},
                           zelda3::MinecartRailProfile::kLegacy);
  ASSERT_TRUE(translated.ok()) << translated.status();
  // The editor supplies session pins on its normal preview path.
  copied->minecart->source_rom = &rom_;
  copied->minecart->session_generation =
      DungeonEditorV2MinecartNodeTestPeer::Generation(*editor_);
  auto candidate = DungeonEditorV2MinecartNodeTestPeer::PreviewAssembly(
      *editor_, 0x99, *copied, 20, 20, mapping);
  ASSERT_TRUE(candidate.ok()) << candidate.status();
  const auto undo_before = editor_->undo_manager().UndoStackSize();
  ASSERT_TRUE(Apply(*candidate).ok());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), undo_before + 1);
  EXPECT_EQ(target.GetSprites()[0].subtype(), 18);
  EXPECT_EQ(target.GetSprites()[1].subtype(), 4);
  EXPECT_EQ(target.GetSprites()[2].subtype(), 4);
  EXPECT_EQ(panel_.GetTracks()[18].room_id, 0x99);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(target.GetTileObjects().empty());
  EXPECT_EQ(panel_.GetTracks()[18].room_id, 0);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_FALSE(
      editor_->SaveRoom(0x99).ok());  // Existing paired draft save gate.
  ASSERT_TRUE(panel_.SaveTracks().ok());
  ASSERT_TRUE(editor_->SaveRoom(0x99).ok());
  const auto file = root_ / "assembly.sfc";
  ASSERT_TRUE(rom_.SaveToFile({.filename = file.string()}).ok());
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(file.string()).ok());
  zelda3::Room decoded(0x99, &reopened);
  decoded.LoadObjects();
  decoded.LoadSprites();
  EXPECT_EQ(decoded.GetTileObjects().size(), target.GetTileObjects().size());
  ASSERT_EQ(decoded.GetSprites().size(), 3u);
  EXPECT_EQ(decoded.GetSprites()[0].subtype(), 18);
  auto collision = zelda3::LoadCustomCollisionMap(&reopened, 0x99);
  ASSERT_TRUE(collision.ok()) << collision.status();
  EXPECT_EQ(*collision, target.custom_collision());
  auto document = MinecartTrackSourceDocument::Parse(ReadSource());
  ASSERT_TRUE(document.ok());
  EXPECT_EQ(document->tracks(), panel_.GetTracks());
}

TEST_F(MinecartAssemblyEditorTest,
       CopyRejectsPartialChannelsAndGlobalIdentityConflicts) {
  room_->GetSprites().emplace_back(0xAF, 4, 4, 3, 0);
  room_->GetSprites().emplace_back(0xB0, 6, 4, 3, 0);
  room_->custom_collision().SetOverride(8 * 64 + 12, 0xD2);
  EXPECT_FALSE(CaptureMinecartAssembly(*room_, {}, {{EntityType::Sprite, 1}},
                                       panel_.GetTracks(),
                                       zelda3::MinecartRailProfile::kLegacy)
                   .ok());
  auto copy = CaptureMinecartAssembly(
      *room_, {}, {{EntityType::Sprite, 0}, {EntityType::Sprite, 1}},
      panel_.GetTracks(), zelda3::MinecartRailProfile::kLegacy);
  ASSERT_TRUE(copy.ok()) << copy.status();
  copy->minecart->source_rom = &rom_;
  copy->minecart->session_generation =
      DungeonEditorV2MinecartNodeTestPeer::Generation(*editor_);
  auto& target = DungeonEditorV2MinecartNodeTestPeer::OtherRoom(*editor_, rom_);
  target.SetTileObjects({});
  MinecartAssemblyMapping reused{{}, {{3, 3}}};
  EXPECT_FALSE(DungeonEditorV2MinecartNodeTestPeer::PreviewAssembly(
                   *editor_, 0x99, *copy, 20, 20, reused)
                   .ok());
  MinecartAssemblyMapping mapping{{}, {{3, 4}}};
  auto preview = DungeonEditorV2MinecartNodeTestPeer::PreviewAssembly(
      *editor_, 0x99, *copy, 20, 20, mapping);
  ASSERT_TRUE(preview.ok()) << preview.status();
  room_->GetSprites().emplace_back(0xAF, 9, 9, 4, 0);
  EXPECT_FALSE(Apply(*preview).ok());
  EXPECT_TRUE(target.GetSprites().empty());
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), 0u);
}

}  // namespace
}  // namespace yaze::editor
