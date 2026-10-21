#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/palette_manager.h"
#include "core/features.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/room_object_batch.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

class DungeonEditorV2AgentBatchTestPeer {
 public:
  static DungeonCanvasViewer* Viewer(DungeonEditorV2& editor, int room_id) {
    return editor.GetViewerForRoom(room_id);
  }

  static void BeginObjectEdit(DungeonEditorV2& editor, int room_id) {
    editor.BeginUndoSnapshot(room_id);
  }
  static void FinalizeObjectEdit(DungeonEditorV2& editor, int room_id) {
    editor.FinalizeUndoAction(room_id);
  }
  static bool HasPendingObjectEdit(const DungeonEditorV2& editor) {
    return editor.has_pending_undo_;
  }

  static void BeginCollisionEdit(DungeonEditorV2& editor, int room_id) {
    editor.BeginCollisionUndoSnapshot(room_id);
  }
  static void FinalizeCollisionEdit(DungeonEditorV2& editor, int room_id) {
    editor.FinalizeCollisionUndoAction(room_id);
  }
  static int PendingCollisionRoom(const DungeonEditorV2& editor) {
    return editor.pending_collision_undo_.room_id;
  }

  static void BeginWaterFillEdit(DungeonEditorV2& editor, int room_id) {
    editor.BeginWaterFillUndoSnapshot(room_id);
  }
  static void FinalizeWaterFillEdit(DungeonEditorV2& editor, int room_id) {
    editor.FinalizeWaterFillUndoAction(room_id);
  }
  static int PendingWaterFillRoom(const DungeonEditorV2& editor) {
    return editor.pending_water_fill_undo_.room_id;
  }
};

namespace {

using Peer = DungeonEditorV2AgentBatchTestPeer;
using ::testing::ElementsAre;

constexpr int kRoomId = 0;
constexpr int kUnmaterializedRoomId = 1;
constexpr int kMaterializedUnloadedRoomId = 2;

constexpr int kRoomTiles = 64;
constexpr int kRoomPixels = 512;
constexpr int kTilePixels = 8;

constexpr int kObjectPointerTablePc = 0x0F8000;
constexpr std::array<int, 3> kObjectStreamPcs = {0x100000, 0x100100, 0x100200};
constexpr int kHeaderTablePc = 0x0F6000;
constexpr std::array<int, 3> kHeaderPcs = {0x114000, 0x114020, 0x114040};

constexpr uint8_t kObjectTilePalette = 2;

struct FixtureObject {
  int16_t id;
  uint8_t x;
  uint8_t y;
  uint8_t size;
  uint8_t stored_list;
};

// One object per stored list. Object 0 (0x33, size 2) is a 12-tile-wide
// repeated 4x4 block, so it can be origin-valid while its footprint is not.
constexpr std::array<FixtureObject, 3> kRomObjects = {{
    {0x33, 4, 6, 0x02, 0},
    {0x34, 20, 20, 0x00, 1},
    {0x21, 30, 40, 0x01, 2},
}};

struct ObjectRecord {
  int id;
  int x;
  int y;
  int size;
  int stored_list;
  int options;
  bool all_bgs;
  uint8_t b1;
  uint8_t b2;
  uint8_t b3;
  size_t tile_count;

  bool operator==(const ObjectRecord&) const = default;
};

void PrintTo(const ObjectRecord& record, std::ostream* os) {
  *os << "{id=0x" << std::hex << record.id << std::dec << " x=" << record.x
      << " y=" << record.y << " size=" << record.size
      << " list=" << record.stored_list << " options=" << record.options
      << " tiles=" << record.tile_count << "}";
}

std::vector<ObjectRecord> Records(
    const std::vector<zelda3::RoomObject>& objects) {
  std::vector<ObjectRecord> records;
  records.reserve(objects.size());
  for (const auto& object : objects) {
    const auto bytes = object.EncodeObjectToBytes();
    records.push_back(ObjectRecord{
        .id = object.id_,
        .x = object.x(),
        .y = object.y(),
        .size = object.size(),
        .stored_list = object.GetLayerValue(),
        .options = static_cast<int>(object.options()),
        .all_bgs = object.all_bgs_,
        .b1 = bytes.b1,
        .b2 = bytes.b2,
        .b3 = bytes.b3,
        .tile_count = object.tiles_.size(),
    });
  }
  return records;
}

using DirtyFlags = std::tuple<bool, bool, bool, bool, bool, bool, bool, bool>;

DirtyFlags CaptureDirtyFlags(const zelda3::Room& room) {
  return {room.HasUnsavedChanges(),      room.header_dirty(),
          room.object_stream_dirty(),    room.object_stream_header_dirty(),
          room.torches_dirty(),          room.blocks_dirty(),
          room.custom_collision_dirty(), room.water_fill_dirty()};
}

struct RenderFingerprint {
  std::vector<uint8_t> object_layers;
  std::vector<uint8_t> composite;

  bool operator==(const RenderFingerprint&) const = default;
};

struct Observation {
  bool composite_dirty = false;
  std::vector<ObjectRecord> objects;
  std::vector<size_t> selection;
  size_t undo_size = 0;
  size_t redo_size = 0;
  DirtyFlags dirty;
  RenderFingerprint render;
};

void WriteLongPointer(Rom& rom, int pc, uint32_t snes_address) {
  rom.mutable_data()[pc + 0] = snes_address & 0xFF;
  rom.mutable_data()[pc + 1] = (snes_address >> 8) & 0xFF;
  rom.mutable_data()[pc + 2] = (snes_address >> 16) & 0xFF;
}

void SeedPaletteGroup(gfx::PaletteGroup* group, int palette_count,
                      int color_count, uint16_t seed) {
  ASSERT_NE(group, nullptr);
  group->clear();
  for (int palette_index = 0; palette_index < palette_count; ++palette_index) {
    gfx::SnesPalette palette;
    for (int color_index = 0; color_index < color_count; ++color_index) {
      palette.AddColor(gfx::SnesColor(static_cast<uint16_t>(
          (seed + palette_index * color_count + color_index) & 0x7FFF)));
    }
    group->AddPalette(palette);
  }
}

void InjectVisibleTiles(zelda3::RoomObject& object) {
  object.mutable_tiles().assign(
      64, gfx::TileInfo(/*id=*/0, kObjectTilePalette, /*v=*/false,
                        /*h=*/false, /*o=*/false));
  object.tiles_loaded_ = true;
}

int RightEdgePixels(const zelda3::RoomObject& object) {
  const auto [x, y, width, height] =
      zelda3::DimensionService::Get().GetHitTestBounds(object);
  (void)y;
  (void)height;
  return (x + width) * kTilePixels;
}

int BottomEdgePixels(const zelda3::RoomObject& object) {
  const auto [x, y, width, height] =
      zelda3::DimensionService::Get().GetHitTestBounds(object);
  (void)x;
  (void)width;
  return (y + height) * kTilePixels;
}

zelda3::RoomObjectMove Move(size_t index, int x_tile, int y_tile) {
  return zelda3::RoomObjectMove{
      .object_index = index, .x_tile = x_tile, .y_tile = y_tile};
}

class DungeonAgentBatchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    gfx::Arena::Get().Initialize(nullptr);
    gfx::PaletteManager::Get().ResetForTesting();
    previous_workbench_ = core::FeatureFlags::get().dungeon.kUseWorkbench;
    core::FeatureFlags::get().dungeon.kUseWorkbench = false;

    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    ASSERT_NO_FATAL_FAILURE(SetupRomFixture());
    ASSERT_NO_FATAL_FAILURE(SetupGameData());
    gfx::PaletteManager::Get().Initialize(&game_data_);
    rom_.ClearDirty();
    rom_before_ = rom_.vector();

    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    editor_->SetGameData(&game_data_);
    editor_->add_room(kRoomId);

    room_ = editor_->rooms().GetIfLoaded(kRoomId);
    ASSERT_NE(room_, nullptr) << "add_room must load the room through the "
                                 "editor's DungeonRoomLoader";
    ASSERT_EQ(room_->GetTileObjects().size(), kRomObjects.size());
    for (size_t i = 0; i < kRomObjects.size(); ++i) {
      SCOPED_TRACE(i);
      const auto& object = room_->GetTileObjects()[i];
      ASSERT_EQ(object.id_, kRomObjects[i].id);
      ASSERT_EQ(object.x(), kRomObjects[i].x);
      ASSERT_EQ(object.y(), kRomObjects[i].y);
      ASSERT_EQ(object.size(), kRomObjects[i].size);
      ASSERT_EQ(object.GetLayerValue(), kRomObjects[i].stored_list);
    }
    for (auto& object : room_->GetTileObjects()) {
      InjectVisibleTiles(object);
    }

    viewer_ = Peer::Viewer(*editor_, kRoomId);
    ASSERT_NE(viewer_, nullptr);
    viewer_->object_interaction().SetCurrentRoom(&editor_->rooms(), kRoomId);

    ASSERT_NO_FATAL_FAILURE(RenderToCleanState());
    const auto has_coverage = [](const std::vector<uint8_t>& coverage) {
      return std::any_of(coverage.begin(), coverage.end(),
                         [](uint8_t value) { return value != 0; });
    };
    ASSERT_TRUE(has_coverage(room_->object_bg1_buffer().coverage_data()) ||
                has_coverage(room_->object_bg2_buffer().coverage_data()))
        << "Fixture objects must draw into the room object buffers";

    ASSERT_FALSE(room_->HasUnsavedChanges());
    ASSERT_FALSE(editor_->undo_manager().CanUndo());
    ASSERT_FALSE(editor_->undo_manager().CanRedo());
  }

  void TearDown() override {
    editor_.reset();
    core::FeatureFlags::get().dungeon.kUseWorkbench = previous_workbench_;
    gfx::Arena::Get().ClearTextureQueue();
    gfx::Arena::Get().Initialize(nullptr);
    gfx::PaletteManager::Get().ResetForTesting();
  }

  void SetupRomFixture() {
    WriteLongPointer(rom_, zelda3::kRoomObjectPointer,
                     PcToSnes(kObjectPointerTablePc));
    for (size_t room = 0; room < kObjectStreamPcs.size(); ++room) {
      WriteLongPointer(rom_, kObjectPointerTablePc + static_cast<int>(room) * 3,
                       PcToSnes(kObjectStreamPcs[room]));
    }

    std::vector<uint8_t> stream = {0x00, 0x00};
    for (const auto& fixture : kRomObjects) {
      const auto bytes = zelda3::RoomObject(fixture.id, fixture.x, fixture.y,
                                            fixture.size, fixture.stored_list)
                             .EncodeObjectToBytes();
      stream.insert(stream.end(), {bytes.b1, bytes.b2, bytes.b3, 0xFF, 0xFF});
    }
    ASSERT_TRUE(rom_.WriteVector(kObjectStreamPcs[kRoomId], stream).ok());

    const std::vector<uint8_t> empty_stream = {0x00, 0x00, 0xFF, 0xFF, 0xFF,
                                               0xFF, 0xF0, 0xFF, 0xFF, 0xFF};
    for (int room : {kUnmaterializedRoomId, kMaterializedUnloadedRoomId}) {
      ASSERT_TRUE(rom_.WriteVector(kObjectStreamPcs[room], empty_stream).ok());
    }

    WriteLongPointer(rom_, zelda3::kRoomHeaderPointer,
                     PcToSnes(kHeaderTablePc));
    rom_.mutable_data()[zelda3::kRoomHeaderPointerBank] =
        (PcToSnes(kHeaderPcs[0]) >> 16) & 0xFF;
    for (size_t room = 0; room < kHeaderPcs.size(); ++room) {
      const uint16_t pointer =
          static_cast<uint16_t>(PcToSnes(kHeaderPcs[room]));
      const int slot = kHeaderTablePc + static_cast<int>(room) * 2;
      rom_.mutable_data()[slot] = pointer & 0xFF;
      rom_.mutable_data()[slot + 1] = (pointer >> 8) & 0xFF;
    }

    // Layout 0 is an empty (terminated) layout stream.
    const int layout_pc = SnesToPc(zelda3::kRoomLayoutPointers.front());
    ASSERT_GE(layout_pc, 0);
    ASSERT_LT(layout_pc + 1, static_cast<int>(rom_.size()));
    rom_.mutable_data()[layout_pc] = 0xFF;
    rom_.mutable_data()[layout_pc + 1] = 0xFF;
  }

  void SetupGameData() {
    game_data_.graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 0);
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        game_data_.graphics_buffer[y * 128 + x] = 1;
      }
    }
    for (auto& ids : game_data_.main_blockset_ids) {
      ids.fill(0);
    }
    for (auto& ids : game_data_.room_blockset_ids) {
      ids.fill(0);
    }
    for (auto& ids : game_data_.spriteset_ids) {
      ids.fill(0);
    }
    for (auto& ids : game_data_.paletteset_ids) {
      ids.fill(0);
    }
    ASSERT_NO_FATAL_FAILURE(SeedPaletteGroup(&game_data_.palette_groups.hud,
                                             /*palette_count=*/1,
                                             /*color_count=*/32,
                                             /*seed=*/0x0100));
    ASSERT_NO_FATAL_FAILURE(
        SeedPaletteGroup(&game_data_.palette_groups.dungeon_main,
                         /*palette_count=*/4, /*color_count=*/90,
                         /*seed=*/0x0200));
  }

  void RenderToCleanState() {
    room_->RenderRoomGraphics();
    room_->GetCompositeBitmap(layer_manager_);
    ASSERT_FALSE(room_->IsCompositeDirty());
  }

  // Replaces the loaded model with an unsaved, non-undo edit so the in-memory
  // room no longer matches the ROM fixture.
  void MakePendingEdit() {
    std::vector<zelda3::RoomObject> pending = room_->GetTileObjects();
    for (auto& object : pending) {
      object.set_x(object.x() + 2);
      object.set_y(object.y() + 2);
    }
    room_->SetTileObjects(pending);
    ASSERT_NO_FATAL_FAILURE(RenderToCleanState());
    ASSERT_TRUE(room_->object_stream_dirty());
    ASSERT_FALSE(editor_->undo_manager().CanUndo());

    const zelda3::Room rom_room = zelda3::LoadRoomFromRom(&rom_, kRoomId);
    ASSERT_EQ(rom_room.GetTileObjects().size(), pending.size());
    for (size_t i = 0; i < pending.size(); ++i) {
      ASSERT_NE(rom_room.GetTileObjects()[i].x(), pending[i].x());
      ASSERT_NE(rom_room.GetTileObjects()[i].y(), pending[i].y());
    }
  }

  RenderFingerprint Fingerprint() {
    RenderFingerprint fingerprint;
    for (auto* buffer :
         {&room_->object_bg1_buffer(), &room_->object_bg2_buffer()}) {
      const auto& pixels = buffer->bitmap().vector();
      fingerprint.object_layers.insert(fingerprint.object_layers.end(),
                                       pixels.begin(), pixels.end());
      const auto& coverage = buffer->coverage_data();
      fingerprint.object_layers.insert(fingerprint.object_layers.end(),
                                       coverage.begin(), coverage.end());
    }
    fingerprint.composite = room_->GetCompositeBitmap(layer_manager_).vector();
    return fingerprint;
  }

  Observation Observe() {
    Observation observation;
    observation.composite_dirty = room_->IsCompositeDirty();
    observation.objects = Records(room_->GetTileObjects());
    observation.selection =
        viewer_->object_interaction().GetSelectedObjectIndices();
    observation.undo_size = editor_->undo_manager().UndoStackSize();
    observation.redo_size = editor_->undo_manager().RedoStackSize();
    observation.dirty = CaptureDirtyFlags(*room_);
    observation.render = Fingerprint();
    return observation;
  }

  zelda3::RoomObjectBatch CurrentBatch(
      std::vector<zelda3::RoomObjectMove> moves) const {
    return zelda3::RoomObjectBatch{
        .room_id = kRoomId,
        .expected_objects =
            zelda3::CaptureRoomObjectGuards(room_->GetTileObjects()),
        .moves = std::move(moves),
    };
  }

  std::vector<zelda3::RoomObject> WithMoves(
      std::vector<zelda3::RoomObject> objects,
      const std::vector<zelda3::RoomObjectMove>& moves) const {
    for (const auto& move : moves) {
      objects[move.object_index].set_x(static_cast<uint8_t>(move.x_tile));
      objects[move.object_index].set_y(static_cast<uint8_t>(move.y_tile));
    }
    return objects;
  }

  void ExpectRomUnchanged() const {
    EXPECT_TRUE(rom_.vector() == rom_before_)
        << "Batch edits must never write source ROM bytes";
    EXPECT_FALSE(rom_.dirty());
  }

  void ExpectUnchangedSince(const Observation& before) {
    const Observation after = Observe();
    EXPECT_FALSE(after.composite_dirty)
        << "Room must not be re-rendered or invalidated";
    EXPECT_EQ(after.objects, before.objects);
    EXPECT_EQ(after.selection, before.selection);
    EXPECT_EQ(after.undo_size, before.undo_size);
    EXPECT_EQ(after.redo_size, before.redo_size);
    EXPECT_EQ(after.dirty, before.dirty);
    EXPECT_TRUE(after.render == before.render)
        << "Room buffers or composite changed";
    EXPECT_EQ(editor_->rooms().GetIfLoaded(kRoomId), room_);
    ExpectRomUnchanged();
  }

  void ExpectRejectedAtomically(const zelda3::RoomObjectBatch& batch) {
    const Observation before = Observe();
    ASSERT_FALSE(before.composite_dirty);
    const auto result = editor_->ApplyRoomObjectBatch(batch);
    EXPECT_FALSE(result.ok())
        << "Batch unexpectedly applied; changed_object_count="
        << result->changed_object_count;
    ExpectUnchangedSince(before);
  }

  Rom rom_;
  zelda3::GameData game_data_{&rom_};
  zelda3::RoomLayerManager layer_manager_;
  std::unique_ptr<DungeonEditorV2> editor_;
  zelda3::Room* room_ = nullptr;
  DungeonCanvasViewer* viewer_ = nullptr;
  std::vector<uint8_t> rom_before_;
  bool previous_workbench_ = false;
};

TEST_F(DungeonAgentBatchTest,
       MoveAppliesAsOneUndoableRenderedEditToPendingLoadedRoom) {
  ASSERT_NO_FATAL_FAILURE(MakePendingEdit());
  auto& interaction = viewer_->object_interaction();
  interaction.SetSelectedObjects({0, 2});

  const Observation pending = Observe();
  const std::vector<zelda3::RoomObject> pending_objects =
      room_->GetTileObjects();
  const zelda3::Room* room_address = room_;
  const std::vector<zelda3::RoomObjectMove> moves = {
      Move(0, 6, 6), Move(1, 26, 22), Move(2, 32, 38)};
  const auto expected = Records(WithMoves(pending_objects, moves));

  const auto result = editor_->ApplyRoomObjectBatch(CurrentBatch(moves));

  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->changed_object_count, 3u);
  EXPECT_EQ(editor_->rooms().GetIfLoaded(kRoomId), room_address)
      << "The loaded room must be edited in place, not reloaded";
  const Observation applied = Observe();
  EXPECT_TRUE(applied.composite_dirty)
      << "Applying the batch must re-render the room";
  EXPECT_EQ(applied.objects, expected);
  EXPECT_THAT(applied.selection, ElementsAre(0u, 2u));
  EXPECT_EQ(applied.undo_size, 1u);
  EXPECT_EQ(applied.redo_size, 0u);
  EXPECT_TRUE(room_->object_stream_dirty());
  EXPECT_FALSE(applied.render.object_layers == pending.render.object_layers)
      << "Room object buffers must reflect the moved objects";
  EXPECT_FALSE(applied.render.composite == pending.render.composite)
      << "Room composite must reflect the moved objects";
  ExpectRomUnchanged();

  interaction.SetSelectedObjects({1});
  ASSERT_TRUE(editor_->Undo().ok());
  const Observation undone = Observe();
  EXPECT_TRUE(undone.composite_dirty);
  EXPECT_EQ(undone.objects, pending.objects)
      << "One Undo must restore the whole pending (unsaved) model";
  EXPECT_THAT(undone.selection, ElementsAre(0u, 2u));
  EXPECT_EQ(undone.undo_size, 0u);
  EXPECT_EQ(undone.redo_size, 1u);
  EXPECT_TRUE(undone.render == pending.render);

  EXPECT_FALSE(editor_->Undo().ok())
      << "The batch must occupy exactly one undo entry";
  EXPECT_EQ(Records(room_->GetTileObjects()), pending.objects);
  EXPECT_FALSE(room_->IsCompositeDirty());

  ASSERT_TRUE(editor_->Redo().ok());
  const Observation redone = Observe();
  EXPECT_TRUE(redone.composite_dirty);
  EXPECT_EQ(redone.objects, expected);
  EXPECT_THAT(redone.selection, ElementsAre(0u, 2u));
  EXPECT_EQ(redone.undo_size, 1u);
  EXPECT_EQ(redone.redo_size, 0u);
  EXPECT_TRUE(redone.render == applied.render);
  EXPECT_EQ(editor_->rooms().GetIfLoaded(kRoomId), room_address);
  ExpectRomUnchanged();
}

TEST_F(DungeonAgentBatchTest, ChangedCountExcludesMovesToCurrentPosition) {
  ASSERT_NO_FATAL_FAILURE(MakePendingEdit());
  const std::vector<zelda3::RoomObject> pending_objects =
      room_->GetTileObjects();
  const std::vector<zelda3::RoomObjectMove> moves = {
      Move(0, pending_objects[0].x(), pending_objects[0].y()), Move(1, 26, 22),
      Move(2, 32, 38)};

  const auto result = editor_->ApplyRoomObjectBatch(CurrentBatch(moves));

  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->changed_object_count, 2u);
  EXPECT_EQ(Records(room_->GetTileObjects()),
            Records(WithMoves(pending_objects, moves)));
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), 1u);

  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(Records(room_->GetTileObjects()), Records(pending_objects));
  EXPECT_FALSE(editor_->undo_manager().CanUndo());
  ExpectRomUnchanged();
}

TEST_F(DungeonAgentBatchTest, NoOpBatchCreatesNoUndoDirtyStateOrRender) {
  viewer_->object_interaction().SetSelectedObjects({1});
  const Observation before = Observe();
  ASSERT_FALSE(std::get<0>(before.dirty));
  const auto& objects = room_->GetTileObjects();
  std::vector<zelda3::RoomObjectMove> moves;
  for (size_t i = 0; i < objects.size(); ++i) {
    moves.push_back(Move(i, objects[i].x(), objects[i].y()));
  }

  const auto result = editor_->ApplyRoomObjectBatch(CurrentBatch(moves));

  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->changed_object_count, 0u);
  ExpectUnchangedSince(before);
  EXPECT_FALSE(room_->HasUnsavedChanges());
}

TEST_F(DungeonAgentBatchTest, StaleOrIncompleteGuardsRejectAtomically) {
  ASSERT_NO_FATAL_FAILURE(MakePendingEdit());
  viewer_->object_interaction().SetSelectedObjects({0});
  const std::vector<zelda3::RoomObjectMove> moves = {Move(0, 6, 6)};

  {
    SCOPED_TRACE("guards captured from the ROM instead of the live model");
    const zelda3::Room rom_room = zelda3::LoadRoomFromRom(&rom_, kRoomId);
    auto batch = CurrentBatch(moves);
    batch.expected_objects =
        zelda3::CaptureRoomObjectGuards(rom_room.GetTileObjects());
    ExpectRejectedAtomically(batch);
  }

  using GuardMutation =
      std::function<void(std::vector<zelda3::RoomObjectGuard>&)>;
  const std::vector<std::pair<std::string, GuardMutation>> mutations = {
      {"object_index out of source order",
       [](auto& guards) { guards[1].object_index = 2; }},
      {"guards reordered",
       [](auto& guards) { std::swap(guards[0], guards[1]); }},
      {"object_id", [](auto& guards) { guards[1].object_id += 1; }},
      {"x_tile", [](auto& guards) { guards[1].x_tile += 1; }},
      {"y_tile", [](auto& guards) { guards[1].y_tile += 1; }},
      {"size", [](auto& guards) { guards[1].size += 1; }},
      {"stored_list",
       [](auto& guards) {
         guards[1].stored_list = (guards[1].stored_list + 1) % 3;
       }},
      {"missing trailing guard", [](auto& guards) { guards.pop_back(); }},
      {"extra guard",
       [](auto& guards) {
         auto extra = guards.back();
         extra.object_index = guards.size();
         guards.push_back(extra);
       }},
      {"no guards", [](auto& guards) { guards.clear(); }},
  };
  for (const auto& [name, mutate] : mutations) {
    SCOPED_TRACE(name);
    auto batch = CurrentBatch(moves);
    mutate(batch.expected_objects);
    ExpectRejectedAtomically(batch);
  }
}

TEST_F(DungeonAgentBatchTest, DuplicateMoveIndicesRejectAtomically) {
  ExpectRejectedAtomically(
      CurrentBatch({Move(0, 8, 6), Move(1, 26, 22), Move(0, 10, 6)}));
}

TEST_F(DungeonAgentBatchTest, LateInvalidMoveRejectsWholeBatchAtomically) {
  viewer_->object_interaction().SetSelectedObjects({2});
  {
    SCOPED_TRACE("x_tile past the last room column");
    ExpectRejectedAtomically(CurrentBatch(
        {Move(0, 8, 6), Move(1, 26, 22), Move(2, kRoomTiles, 40)}));
  }
  {
    SCOPED_TRACE("negative y_tile");
    ExpectRejectedAtomically(
        CurrentBatch({Move(0, 8, 6), Move(1, 26, 22), Move(2, 30, -1)}));
  }
  {
    SCOPED_TRACE("object index past the room object list");
    ExpectRejectedAtomically(
        CurrentBatch({Move(0, 8, 6), Move(1, 26, 22), Move(3, 10, 10)}));
  }
  EXPECT_FALSE(room_->HasUnsavedChanges());
}

TEST_F(DungeonAgentBatchTest, FootprintOutsideRoomRejectsAtomically) {
  const auto& objects = room_->GetTileObjects();
  const auto& dimensions = zelda3::DimensionService::Get();
  {
    SCOPED_TRACE("origin-valid x whose footprint exceeds 512px");
    constexpr int kTargetX = 60;
    zelda3::RoomObject wide = objects[0];
    ASSERT_LE(RightEdgePixels(wide), kRoomPixels);
    wide.set_x(kTargetX);
    ASSERT_GT(RightEdgePixels(wide), kRoomPixels)
        << "Fixture object must be wide enough to overflow from x=" << kTargetX;
    const auto dims = dimensions.GetDimensions(wide);
    ASSERT_GT((kTargetX + dims.offset_x_tiles + dims.width_tiles) * kTilePixels,
              kRoomPixels);

    ExpectRejectedAtomically(
        CurrentBatch({Move(1, 26, 22), Move(0, kTargetX, objects[0].y())}));
  }
  {
    SCOPED_TRACE("origin-valid y whose footprint exceeds 512px");
    constexpr int kTargetY = kRoomTiles - 1;
    zelda3::RoomObject tall = objects[2];
    ASSERT_LE(BottomEdgePixels(tall), kRoomPixels);
    tall.set_y(kTargetY);
    ASSERT_GT(BottomEdgePixels(tall), kRoomPixels)
        << "Fixture object must be tall enough to overflow from y=" << kTargetY;
    const auto dims = dimensions.GetDimensions(tall);
    ASSERT_GT(
        (kTargetY + dims.offset_y_tiles + dims.height_tiles) * kTilePixels,
        kRoomPixels);

    ExpectRejectedAtomically(
        CurrentBatch({Move(0, 8, 6), Move(2, objects[2].x(), kTargetY)}));
  }
  EXPECT_FALSE(room_->HasUnsavedChanges());
}

TEST_F(DungeonAgentBatchTest, PendingObjectCollisionOrWaterEditRejectsBatch) {
  ASSERT_NO_FATAL_FAILURE(MakePendingEdit());
  const std::vector<zelda3::RoomObjectMove> moves = {Move(0, 6, 6)};

  {
    SCOPED_TRACE("pending object undo snapshot");
    Peer::BeginObjectEdit(*editor_, kRoomId);
    ASSERT_TRUE(Peer::HasPendingObjectEdit(*editor_));
    ExpectRejectedAtomically(CurrentBatch(moves));
    EXPECT_TRUE(Peer::HasPendingObjectEdit(*editor_));
    Peer::FinalizeObjectEdit(*editor_, kRoomId);
    ASSERT_FALSE(Peer::HasPendingObjectEdit(*editor_));
  }
  {
    SCOPED_TRACE("pending collision undo snapshot");
    Peer::BeginCollisionEdit(*editor_, kRoomId);
    ASSERT_EQ(Peer::PendingCollisionRoom(*editor_), kRoomId);
    ExpectRejectedAtomically(CurrentBatch(moves));
    EXPECT_EQ(Peer::PendingCollisionRoom(*editor_), kRoomId);
    Peer::FinalizeCollisionEdit(*editor_, kRoomId);
    ASSERT_LT(Peer::PendingCollisionRoom(*editor_), 0);
  }
  {
    SCOPED_TRACE("pending water-fill undo snapshot");
    Peer::BeginWaterFillEdit(*editor_, kRoomId);
    ASSERT_EQ(Peer::PendingWaterFillRoom(*editor_), kRoomId);
    ExpectRejectedAtomically(CurrentBatch(moves));
    EXPECT_EQ(Peer::PendingWaterFillRoom(*editor_), kRoomId);
    Peer::FinalizeWaterFillEdit(*editor_, kRoomId);
    ASSERT_LT(Peer::PendingWaterFillRoom(*editor_), 0);
  }

  const size_t undo_before = editor_->undo_manager().UndoStackSize();
  const auto result = editor_->ApplyRoomObjectBatch(CurrentBatch(moves));
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->changed_object_count, 1u);
  EXPECT_EQ(editor_->undo_manager().UndoStackSize(), undo_before + 1);
  ExpectRomUnchanged();
}

TEST_F(DungeonAgentBatchTest, InvalidOrUnloadedRoomRejectsWithoutLoading) {
  ASSERT_EQ(editor_->rooms().GetIfMaterialized(kUnmaterializedRoomId), nullptr);
  zelda3::Room& unloaded = editor_->rooms()[kMaterializedUnloadedRoomId];
  ASSERT_FALSE(unloaded.IsLoaded());

  for (int room_id : {-1, zelda3::kNumberOfRooms, kUnmaterializedRoomId,
                      kMaterializedUnloadedRoomId}) {
    SCOPED_TRACE(room_id);
    auto batch = CurrentBatch({Move(0, 8, 6)});
    batch.room_id = room_id;
    ExpectRejectedAtomically(batch);
  }

  EXPECT_EQ(editor_->rooms().GetIfMaterialized(kUnmaterializedRoomId), nullptr)
      << "Rejected batches must not materialize or load rooms from ROM";
  EXPECT_FALSE(unloaded.IsLoaded());
  EXPECT_TRUE(unloaded.GetTileObjects().empty());
}

TEST(DungeonAgentBatchRomStateTest, RejectsWhenRomIsNotLoaded) {
  Rom rom;
  DungeonEditorV2 editor(&rom);
  const zelda3::RoomObjectBatch batch{
      .room_id = kRoomId,
      .expected_objects = {},
      .moves = {Move(0, 1, 1)},
  };

  EXPECT_FALSE(editor.ApplyRoomObjectBatch(batch).ok());
  EXPECT_EQ(editor.rooms().GetIfMaterialized(kRoomId), nullptr);
  EXPECT_FALSE(editor.undo_manager().CanUndo());
}

}  // namespace
}  // namespace yaze::editor
