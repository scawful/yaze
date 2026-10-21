#include "app/editor/dungeon/dungeon_agent_tools.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/palette_manager.h"
#include "cli/service/agent/tool_dispatcher.h"
#include "cli/service/ai/common.h"
#include "core/features.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/room_object_batch.h"
#include "zelda3/game_data.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze::editor {
namespace {

#ifdef YAZE_WITH_JSON

using Json = nlohmann::json;
using ToolDispatcher = cli::agent::ToolDispatcher;

constexpr char kInspectTool[] = "dungeon-edit-inspect";
constexpr char kApplyTool[] = "dungeon-edit-apply";
constexpr char kSessionId[] = "native-session-1";

constexpr int kRoomId = 0;
constexpr int kUnloadedRoomId = 1;

constexpr int kObjectPointerTablePc = 0x0F8000;
constexpr int kObjectStreamPc = 0x100000;
constexpr int kHeaderTablePc = 0x0F6000;
constexpr int kHeaderPc = 0x114000;

constexpr uint8_t kObjectTilePalette = 2;

struct FixtureObject {
  int16_t id;
  uint8_t x;
  uint8_t y;
  uint8_t size;
  uint8_t stored_list;
};

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

  bool operator==(const ObjectRecord&) const = default;
};

void PrintTo(const ObjectRecord& record, std::ostream* os) {
  *os << "{id=0x" << std::hex << record.id << std::dec << " x=" << record.x
      << " y=" << record.y << " size=" << record.size
      << " list=" << record.stored_list << " options=" << record.options << "}";
}

std::vector<ObjectRecord> Records(
    const std::vector<zelda3::RoomObject>& objects) {
  std::vector<ObjectRecord> records;
  records.reserve(objects.size());
  for (const auto& object : objects) {
    records.push_back(ObjectRecord{
        .id = object.id_,
        .x = object.x(),
        .y = object.y(),
        .size = object.size(),
        .stored_list = object.GetLayerValue(),
        .options = static_cast<int>(object.options()),
    });
  }
  return records;
}

std::vector<zelda3::RoomObject> WithMoves(
    std::vector<zelda3::RoomObject> objects,
    const std::vector<zelda3::RoomObjectMove>& moves) {
  for (const auto& move : moves) {
    objects[move.object_index].set_x(static_cast<uint8_t>(move.x_tile));
    objects[move.object_index].set_y(static_cast<uint8_t>(move.y_tile));
  }
  return objects;
}

cli::ToolCall MakeToolCall(const std::string& name,
                           std::map<std::string, std::string> args) {
  cli::ToolCall call;
  call.tool_name = name;
  call.args = std::move(args);
  return call;
}

cli::ToolCall InspectCall(const std::string& room) {
  return MakeToolCall(kInspectTool, {{"room", room}});
}

cli::ToolCall ApplyCall(const std::string& room, const std::string& batch) {
  return MakeToolCall(kApplyTool, {{"room", room}, {"batch", batch}});
}

Json GuardsJson(const std::vector<zelda3::RoomObject>& objects) {
  Json guards = Json::array();
  for (const auto& guard : zelda3::CaptureRoomObjectGuards(objects)) {
    guards.push_back({{"object_index", guard.object_index},
                      {"object_id", guard.object_id},
                      {"x_tile", guard.x_tile},
                      {"y_tile", guard.y_tile},
                      {"size", guard.size},
                      {"stored_list", guard.stored_list}});
  }
  return guards;
}

Json MovesJson(const std::vector<zelda3::RoomObjectMove>& moves) {
  Json result = Json::array();
  for (const auto& move : moves) {
    result.push_back({{"object_index", move.object_index},
                      {"x_tile", move.x_tile},
                      {"y_tile", move.y_tile}});
  }
  return result;
}

std::string BatchJson(const Json& session_id, const Json& expected_objects,
                      const Json& moves) {
  return Json{{"session_id", session_id},
              {"expected_objects", expected_objects},
              {"moves", moves}}
      .dump();
}

absl::StatusOr<Json> DispatchJson(ToolDispatcher& dispatcher,
                                  const cli::ToolCall& call) {
  const auto output = dispatcher.Dispatch(call);
  if (!output.ok()) {
    return output.status();
  }
  Json parsed = Json::parse(*output, nullptr, /*allow_exceptions=*/false);
  if (parsed.is_discarded() || !parsed.is_object()) {
    return absl::InternalError("Tool output is not a JSON object: " + *output);
  }
  return parsed;
}

void ExpectNativeResultEnvelope(const Json& result, int room_id,
                                const std::string& session_id) {
  EXPECT_EQ(result.at("session_id"), session_id);
  EXPECT_EQ(result.at("room_id"), room_id);
  EXPECT_EQ(result.at("rom_saved"), false);
  ASSERT_TRUE(result.at("objects").is_array());
}

template <typename T>
std::vector<uint8_t> RawBytes(const std::vector<T>& values) {
  if (values.empty()) {
    return {};
  }
  const auto* first = reinterpret_cast<const uint8_t*>(values.data());
  return std::vector<uint8_t>(first, first + values.size() * sizeof(T));
}

// Byte components of a normal room render: the four room buffers (tile words,
// pixels, coverage, priority and reveal mask) plus the composite pixels.
using RenderSnapshot =
    std::vector<std::pair<std::string, std::vector<uint8_t>>>;

void AppendBuffer(RenderSnapshot& snapshot, const std::string& name,
                  const gfx::BackgroundBuffer& buffer) {
  snapshot.emplace_back(name + ".tile_words", RawBytes(buffer.buffer()));
  snapshot.emplace_back(name + ".pixels", buffer.bitmap().vector());
  snapshot.emplace_back(name + ".coverage", buffer.coverage_data());
  snapshot.emplace_back(name + ".priority", buffer.priority_data());
  snapshot.emplace_back(name + ".reveal", buffer.bg1_reveal_mask_data());
}

RenderSnapshot CaptureRoomRender(zelda3::Room& room) {
  zelda3::RoomLayerManager layers;
  layers.ApplyLayerMerging(room.layer_merging());
  layers.ApplyRoomEffect(room.effect());
  const auto& composite = room.GetCompositeBitmap(layers);
  RenderSnapshot snapshot;
  AppendBuffer(snapshot, "layout_bg1", room.bg1_buffer());
  AppendBuffer(snapshot, "layout_bg2", room.bg2_buffer());
  AppendBuffer(snapshot, "object_bg1", room.object_bg1_buffer());
  AppendBuffer(snapshot, "object_bg2", room.object_bg2_buffer());
  snapshot.emplace_back("composite.pixels", composite.vector());
  return snapshot;
}

const std::vector<uint8_t>& Component(const RenderSnapshot& snapshot,
                                      const std::string& name) {
  static const std::vector<uint8_t> kMissing;
  const auto it =
      std::find_if(snapshot.begin(), snapshot.end(),
                   [&name](const auto& entry) { return entry.first == name; });
  return it == snapshot.end() ? kMissing : it->second;
}

size_t CountDifferentBytes(const std::vector<uint8_t>& a,
                           const std::vector<uint8_t>& b) {
  size_t different =
      a.size() > b.size() ? a.size() - b.size() : b.size() - a.size();
  for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    different += a[i] != b[i] ? 1 : 0;
  }
  return different;
}

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

// Editor singletons shared by every fixture in this file.
class EditorEnvironmentScope {
 public:
  EditorEnvironmentScope()
      : previous_workbench_(core::FeatureFlags::get().dungeon.kUseWorkbench) {
    gfx::Arena::Get().ClearTextureQueue();
    gfx::Arena::Get().Initialize(nullptr);
    gfx::PaletteManager::Get().ResetForTesting();
    core::FeatureFlags::get().dungeon.kUseWorkbench = false;
  }

  ~EditorEnvironmentScope() {
    core::FeatureFlags::get().dungeon.kUseWorkbench = previous_workbench_;
    gfx::Arena::Get().ClearTextureQueue();
    gfx::Arena::Get().Initialize(nullptr);
    gfx::PaletteManager::Get().ResetForTesting();
  }

 private:
  bool previous_workbench_;
};

struct RenderFingerprint {
  std::vector<uint8_t> object_layers;
  std::vector<uint8_t> composite;

  bool operator==(const RenderFingerprint&) const = default;
};

struct ModelState {
  bool composite_dirty = false;
  std::vector<ObjectRecord> objects;
  size_t undo_size = 0;
  size_t redo_size = 0;
  bool unsaved = false;
  bool object_stream_dirty = false;
  RenderFingerprint render;
};

using DispatchFn = std::function<absl::StatusOr<std::string>()>;

class DungeonAgentToolsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    environment_ = std::make_unique<EditorEnvironmentScope>();

    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    ASSERT_NO_FATAL_FAILURE(SetupRomFixture());
    ASSERT_NO_FATAL_FAILURE(SetupGameData());
    gfx::PaletteManager::Get().Initialize(&game_data_);
    rom_.ClearDirty();
    rom_before_ = rom_.vector();
    ASSERT_TRUE(
        other_rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());

    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    editor_->SetGameData(&game_data_);
    editor_->add_room(kRoomId);
    room_ = editor_->rooms().GetIfLoaded(kRoomId);
    ASSERT_NE(room_, nullptr);
    ASSERT_EQ(room_->GetTileObjects().size(), kRomObjects.size());
    for (auto& object : room_->GetTileObjects()) {
      InjectVisibleTiles(object);
    }
    ASSERT_NO_FATAL_FAILURE(RenderToCleanState());
    ASSERT_FALSE(room_->HasUnsavedChanges());
    ASSERT_FALSE(editor_->undo_manager().CanUndo());
    ASSERT_EQ(editor_->rooms().GetIfLoaded(kUnloadedRoomId), nullptr);

    context_ = DefaultContext();
    dispatcher_ = MakeDispatcher(&rom_, /*allow_mutating_tools=*/true);
  }

  void TearDown() override {
    dispatcher_.reset();
    editor_.reset();
    environment_.reset();
  }

  void SetupRomFixture() {
    WriteLongPointer(rom_, zelda3::kRoomObjectPointer,
                     PcToSnes(kObjectPointerTablePc));
    WriteLongPointer(rom_, kObjectPointerTablePc + kRoomId * 3,
                     PcToSnes(kObjectStreamPc));
    std::vector<uint8_t> stream = {0x00, 0x00};
    for (const auto& fixture : kRomObjects) {
      const auto bytes = zelda3::RoomObject(fixture.id, fixture.x, fixture.y,
                                            fixture.size, fixture.stored_list)
                             .EncodeObjectToBytes();
      stream.insert(stream.end(), {bytes.b1, bytes.b2, bytes.b3, 0xFF, 0xFF});
    }
    ASSERT_TRUE(rom_.WriteVector(kObjectStreamPc, stream).ok());

    WriteLongPointer(rom_, zelda3::kRoomHeaderPointer,
                     PcToSnes(kHeaderTablePc));
    rom_.mutable_data()[zelda3::kRoomHeaderPointerBank] =
        (PcToSnes(kHeaderPc) >> 16) & 0xFF;
    const uint16_t header_pointer = static_cast<uint16_t>(PcToSnes(kHeaderPc));
    rom_.mutable_data()[kHeaderTablePc + kRoomId * 2] = header_pointer & 0xFF;
    rom_.mutable_data()[kHeaderTablePc + kRoomId * 2 + 1] =
        (header_pointer >> 8) & 0xFF;

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

  DungeonEditorToolContext DefaultContext() {
    return DungeonEditorToolContext{
        .rom = &rom_, .session_id = kSessionId, .dungeon = editor_.get()};
  }

  // The callback captures the constructing (editor) thread.
  std::unique_ptr<ToolDispatcher> MakeDispatcher(Rom* rom_context,
                                                 bool allow_mutating_tools,
                                                 bool dungeon_tools = true) {
    auto dispatcher = std::make_unique<ToolDispatcher>();
    dispatcher->SetRomContext(rom_context);
    ToolDispatcher::ToolPreferences preferences;
    preferences.allow_mutating_tools = allow_mutating_tools;
    preferences.dungeon = dungeon_tools;
    dispatcher->SetToolPreferences(preferences);
    dispatcher->SetDungeonEditorToolCallback(
        MakeDungeonEditorToolCallback([this]() -> DungeonEditorToolContext {
          ++provider_calls_;
          return context_;
        }));
    return dispatcher;
  }

  void RenderToCleanState() {
    room_->RenderRoomGraphics();
    room_->GetCompositeBitmap(layer_manager_);
    ASSERT_FALSE(room_->IsCompositeDirty());
  }

  // Unsaved, non-undo edit so the live model no longer matches the ROM.
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
    ASSERT_NE(Records(rom_room.GetTileObjects()), Records(pending));
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

  ModelState Capture() {
    ModelState state;
    state.composite_dirty = room_->IsCompositeDirty();
    state.objects = Records(room_->GetTileObjects());
    state.undo_size = editor_->undo_manager().UndoStackSize();
    state.redo_size = editor_->undo_manager().RedoStackSize();
    state.unsaved = room_->HasUnsavedChanges();
    state.object_stream_dirty = room_->object_stream_dirty();
    state.render = Fingerprint();
    return state;
  }

  void ExpectRomUnchanged() const {
    EXPECT_TRUE(rom_.vector() == rom_before_)
        << "Agent tools must never write source ROM bytes";
    EXPECT_FALSE(rom_.dirty());
  }

  void ExpectUnchangedSince(const ModelState& before) {
    const ModelState after = Capture();
    EXPECT_FALSE(after.composite_dirty)
        << "Room must not be re-rendered or invalidated";
    EXPECT_EQ(after.objects, before.objects);
    EXPECT_EQ(after.undo_size, before.undo_size);
    EXPECT_EQ(after.redo_size, before.redo_size);
    EXPECT_EQ(after.unsaved, before.unsaved);
    EXPECT_EQ(after.object_stream_dirty, before.object_stream_dirty);
    EXPECT_TRUE(after.render == before.render)
        << "Room buffers or composite changed";
    EXPECT_EQ(editor_->rooms().GetIfLoaded(kRoomId), room_);
    EXPECT_EQ(editor_->rooms().GetIfLoaded(kUnloadedRoomId), nullptr);
    ExpectRomUnchanged();
  }

  void ExpectRejected(const DispatchFn& dispatch, absl::StatusCode code,
                      bool provider_invoked) {
    const ModelState before = Capture();
    ASSERT_FALSE(before.composite_dirty);
    const int provider_before = provider_calls_.load();
    const auto result = dispatch();
    EXPECT_FALSE(result.ok()) << "Unexpected success: " << *result;
    EXPECT_EQ(result.status().code(), code) << result.status();
    EXPECT_EQ(provider_calls_.load() - provider_before,
              provider_invoked ? 1 : 0)
        << (provider_invoked ? "Host must consult the live editor context"
                             : "Rejected before the editor context provider");
    ExpectUnchangedSince(before);
  }

  // Valid apply request built from a real inspect of the live room.
  std::string InspectedBatch(const std::vector<zelda3::RoomObjectMove>& moves) {
    const auto inspected = DispatchJson(*dispatcher_, InspectCall("0"));
    EXPECT_TRUE(inspected.ok()) << inspected.status();
    if (!inspected.ok()) {
      return {};
    }
    return BatchJson(inspected->at("session_id"), inspected->at("objects"),
                     MovesJson(moves));
  }

  DispatchFn OnDispatcher(ToolDispatcher& dispatcher, cli::ToolCall call) {
    return [&dispatcher, call = std::move(call)]() {
      return dispatcher.Dispatch(call);
    };
  }

  DispatchFn OffEditorThread(cli::ToolCall call) {
    return [this, call = std::move(call)]() {
      absl::StatusOr<std::string> result = absl::UnknownError("not dispatched");
      std::thread worker([&] { result = dispatcher_->Dispatch(call); });
      worker.join();
      return result;
    };
  }

  std::unique_ptr<EditorEnvironmentScope> environment_;
  Rom rom_;
  Rom other_rom_;
  zelda3::GameData game_data_{&rom_};
  zelda3::RoomLayerManager layer_manager_;
  std::unique_ptr<DungeonEditorV2> editor_;
  zelda3::Room* room_ = nullptr;
  std::vector<uint8_t> rom_before_;
  DungeonEditorToolContext context_;
  std::atomic<int> provider_calls_{0};
  std::unique_ptr<ToolDispatcher> dispatcher_;
};

TEST_F(DungeonAgentToolsTest, InspectGuardsDriveOneUndoableRenderedApply) {
  ASSERT_NO_FATAL_FAILURE(MakePendingEdit());
  const ModelState pending = Capture();
  const std::vector<zelda3::RoomObject> pending_objects =
      room_->GetTileObjects();

  const auto inspected = DispatchJson(*dispatcher_, InspectCall("0"));

  ASSERT_TRUE(inspected.ok()) << inspected.status();
  ASSERT_NO_FATAL_FAILURE(
      ExpectNativeResultEnvelope(*inspected, kRoomId, kSessionId));
  EXPECT_EQ(inspected->at("objects"), GuardsJson(pending_objects))
      << "Inspect must report the live (unsaved) model, not the ROM";
  EXPECT_FALSE(inspected->contains("changed_object_count"));
  EXPECT_EQ(provider_calls_.load(), 1);
  ExpectUnchangedSince(pending);

  const std::vector<zelda3::RoomObjectMove> moves = {
      {.object_index = 0, .x_tile = 8, .y_tile = 10},
      {.object_index = 1, .x_tile = 26, .y_tile = 24},
      {.object_index = 2, .x_tile = 34, .y_tile = 36}};
  const std::vector<zelda3::RoomObject> expected =
      WithMoves(pending_objects, moves);

  const auto applied_json = DispatchJson(
      *dispatcher_,
      ApplyCall("0", BatchJson(inspected->at("session_id"),
                               inspected->at("objects"), MovesJson(moves))));

  ASSERT_TRUE(applied_json.ok()) << applied_json.status();
  ASSERT_NO_FATAL_FAILURE(
      ExpectNativeResultEnvelope(*applied_json, kRoomId, kSessionId));
  EXPECT_EQ(applied_json->at("changed_object_count"), 3);
  EXPECT_EQ(applied_json->at("rendered"), true);
  EXPECT_EQ(applied_json->at("undo_recorded"), true);
  EXPECT_EQ(applied_json->at("objects"), GuardsJson(expected));
  EXPECT_EQ(provider_calls_.load(), 2);
  EXPECT_EQ(editor_->rooms().GetIfLoaded(kRoomId), room_)
      << "The loaded room must be edited in place, not reloaded";

  const ModelState applied = Capture();
  EXPECT_TRUE(applied.composite_dirty)
      << "Apply must re-render the room through the editor";
  EXPECT_EQ(applied.objects, Records(expected));
  EXPECT_EQ(applied.undo_size, 1u);
  EXPECT_EQ(applied.redo_size, 0u);
  EXPECT_FALSE(applied.render.object_layers == pending.render.object_layers);
  EXPECT_FALSE(applied.render.composite == pending.render.composite);
  ExpectRomUnchanged();

  ASSERT_TRUE(editor_->Undo().ok());
  const ModelState undone = Capture();
  EXPECT_EQ(undone.objects, pending.objects);
  EXPECT_EQ(undone.undo_size, 0u);
  EXPECT_EQ(undone.redo_size, 1u);
  EXPECT_TRUE(undone.render == pending.render);
  EXPECT_FALSE(editor_->Undo().ok())
      << "The tool apply must occupy exactly one undo entry";

  ASSERT_TRUE(editor_->Redo().ok());
  const ModelState redone = Capture();
  EXPECT_EQ(redone.objects, applied.objects);
  EXPECT_EQ(redone.undo_size, 1u);
  EXPECT_EQ(redone.redo_size, 0u);
  EXPECT_TRUE(redone.render == applied.render);
  EXPECT_EQ(editor_->rooms().GetIfLoaded(kRoomId), room_);
  ExpectRomUnchanged();
}

TEST_F(DungeonAgentToolsTest, NoOpApplySucceedsWithoutUndoRenderOrDirtyState) {
  const ModelState before = Capture();
  ASSERT_FALSE(before.unsaved);
  const auto inspected = DispatchJson(*dispatcher_, InspectCall("0"));
  ASSERT_TRUE(inspected.ok()) << inspected.status();

  Json moves = Json::array();
  for (const auto& guard : inspected->at("objects")) {
    moves.push_back({{"object_index", guard.at("object_index")},
                     {"x_tile", guard.at("x_tile")},
                     {"y_tile", guard.at("y_tile")}});
  }
  const auto applied = DispatchJson(
      *dispatcher_, ApplyCall("0", BatchJson(inspected->at("session_id"),
                                             inspected->at("objects"), moves)));

  ASSERT_TRUE(applied.ok()) << applied.status();
  ASSERT_NO_FATAL_FAILURE(
      ExpectNativeResultEnvelope(*applied, kRoomId, kSessionId));
  EXPECT_EQ(applied->at("changed_object_count"), 0);
  EXPECT_EQ(applied->at("rendered"), false);
  EXPECT_EQ(applied->at("undo_recorded"), false);
  EXPECT_EQ(applied->at("objects"), inspected->at("objects"));
  ExpectUnchangedSince(before);
  EXPECT_FALSE(room_->HasUnsavedChanges());
}

TEST_F(DungeonAgentToolsTest, DispatcherAndThreadRejectionsNeverReachProvider) {
  const std::string batch =
      InspectedBatch({{.object_index = 0, .x_tile = 8, .y_tile = 6}});
  ASSERT_FALSE(batch.empty());

  {
    SCOPED_TRACE("mutating apply is not authorized");
    auto unauthorized = MakeDispatcher(&rom_, /*allow_mutating_tools=*/false);
    ExpectRejected(OnDispatcher(*unauthorized, ApplyCall("0", batch)),
                   absl::StatusCode::kPermissionDenied,
                   /*provider_invoked=*/false);
  }
  {
    SCOPED_TRACE("dungeon tool category disabled");
    auto disabled = MakeDispatcher(&rom_, /*allow_mutating_tools=*/true,
                                   /*dungeon_tools=*/false);
    ExpectRejected(OnDispatcher(*disabled, InspectCall("0")),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/false);
    ExpectRejected(OnDispatcher(*disabled, ApplyCall("0", batch)),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/false);
  }
  {
    SCOPED_TRACE("no ROM context on the dispatcher");
    auto no_rom = MakeDispatcher(nullptr, /*allow_mutating_tools=*/true);
    ExpectRejected(OnDispatcher(*no_rom, ApplyCall("0", batch)),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/false);
  }
  {
    SCOPED_TRACE("missing --batch");
    ExpectRejected(
        OnDispatcher(*dispatcher_, MakeToolCall(kApplyTool, {{"room", "0"}})),
        absl::StatusCode::kInvalidArgument, /*provider_invoked=*/false);
  }
  {
    SCOPED_TRACE("unsupported --write argument");
    ExpectRejected(
        OnDispatcher(
            *dispatcher_,
            MakeToolCall(kApplyTool,
                         {{"room", "0"}, {"batch", batch}, {"write", "true"}})),
        absl::StatusCode::kInvalidArgument, /*provider_invoked=*/false);
  }
  {
    SCOPED_TRACE("inspect does not accept --batch");
    ExpectRejected(
        OnDispatcher(
            *dispatcher_,
            MakeToolCall(kInspectTool, {{"room", "0"}, {"batch", batch}})),
        absl::StatusCode::kInvalidArgument, /*provider_invoked=*/false);
  }
  {
    SCOPED_TRACE("apply from a non-editor thread");
    ExpectRejected(OffEditorThread(ApplyCall("0", batch)),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/false);
  }
  {
    SCOPED_TRACE("inspect from a non-editor thread");
    ExpectRejected(OffEditorThread(InspectCall("0")),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/false);
  }

  SCOPED_TRACE("read-only inspect needs no mutating authorization");
  auto unauthorized = MakeDispatcher(&rom_, /*allow_mutating_tools=*/false);
  const ModelState before = Capture();
  const int provider_before = provider_calls_.load();
  const auto inspected = DispatchJson(*unauthorized, InspectCall("0"));
  ASSERT_TRUE(inspected.ok()) << inspected.status();
  EXPECT_EQ(provider_calls_.load() - provider_before, 1);
  ExpectUnchangedSince(before);
}

TEST_F(DungeonAgentToolsTest, ChangedSourceOrSessionRejectsWithoutMutation) {
  ASSERT_NO_FATAL_FAILURE(MakePendingEdit());
  const std::string batch =
      InspectedBatch({{.object_index = 0, .x_tile = 10, .y_tile = 10}});
  ASSERT_FALSE(batch.empty());

  struct ContextCase {
    const char* name;
    std::function<void(DungeonEditorToolContext&)> mutate;
  };
  const std::vector<ContextCase> context_cases = {
      {"session changed since inspect",
       [](auto& context) { context.session_id = "native-session-2"; }},
      {"empty session", [](auto& context) { context.session_id.clear(); }},
      {"no dungeon editor", [](auto& context) { context.dungeon = nullptr; }},
      {"no ROM", [](auto& context) { context.rom = nullptr; }},
  };
  for (const auto& context_case : context_cases) {
    SCOPED_TRACE(context_case.name);
    context_case.mutate(context_);
    ExpectRejected(OnDispatcher(*dispatcher_, ApplyCall("0", batch)),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/true);
    context_ = DefaultContext();
  }

  {
    SCOPED_TRACE("dispatcher ROM is not the editor's ROM");
    auto foreign = MakeDispatcher(&other_rom_, /*allow_mutating_tools=*/true);
    ExpectRejected(OnDispatcher(*foreign, InspectCall("0")),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/true);
    ExpectRejected(OnDispatcher(*foreign, ApplyCall("0", batch)),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/true);
  }
  {
    SCOPED_TRACE("context ROM matches dispatcher but not the editor");
    auto foreign = MakeDispatcher(&other_rom_, /*allow_mutating_tools=*/true);
    context_.rom = &other_rom_;
    ExpectRejected(OnDispatcher(*foreign, ApplyCall("0", batch)),
                   absl::StatusCode::kFailedPrecondition,
                   /*provider_invoked=*/true);
    context_ = DefaultContext();
  }
  {
    SCOPED_TRACE("batch names another session");
    const Json document = Json::parse(batch);
    ExpectRejected(
        OnDispatcher(*dispatcher_,
                     ApplyCall("0", BatchJson("other-session",
                                              document.at("expected_objects"),
                                              document.at("moves")))),
        absl::StatusCode::kFailedPrecondition, /*provider_invoked=*/true);
  }
}

TEST_F(DungeonAgentToolsTest, MalformedRequestsRejectWithoutMutation) {
  ASSERT_NO_FATAL_FAILURE(MakePendingEdit());
  const std::string batch =
      InspectedBatch({{.object_index = 0, .x_tile = 10, .y_tile = 10}});
  ASSERT_FALSE(batch.empty());
  const Json document = Json::parse(batch);
  const Json& guards = document.at("expected_objects");
  const Json& moves = document.at("moves");

  const auto with_guards = [&](const std::function<void(Json&)>& mutate) {
    Json mutated = guards;
    mutate(mutated);
    return BatchJson(kSessionId, mutated, moves);
  };
  const auto with_moves = [&](const std::function<void(Json&)>& mutate) {
    Json mutated = moves;
    mutate(mutated);
    return BatchJson(kSessionId, guards, mutated);
  };
  std::string duplicate_guard_key = guards.dump();
  const std::string first_index = "\"object_index\":0";
  const size_t first_index_at = duplicate_guard_key.find(first_index);
  ASSERT_NE(first_index_at, std::string::npos);
  duplicate_guard_key.insert(first_index_at, first_index + ",");

  struct BatchCase {
    const char* name;
    std::string batch;
    absl::StatusCode code;
  };
  const std::vector<BatchCase> batch_cases = {
      {"not JSON", "{", absl::StatusCode::kInvalidArgument},
      {"top-level array", "[]", absl::StatusCode::kInvalidArgument},
      {"duplicate top-level key",
       std::string(R"({"session_id":")") + kSessionId + R"(","session_id":")" +
           kSessionId + R"(","expected_objects":)" + guards.dump() +
           R"(,"moves":)" + moves.dump() + "}",
       absl::StatusCode::kInvalidArgument},
      {"duplicate guard key",
       std::string(R"({"session_id":")") + kSessionId +
           R"(","expected_objects":)" + duplicate_guard_key + R"(,"moves":)" +
           moves.dump() + "}",
       absl::StatusCode::kInvalidArgument},
      {"extra top-level key",
       Json{{"session_id", kSessionId},
            {"expected_objects", guards},
            {"moves", moves},
            {"write", true}}
           .dump(),
       absl::StatusCode::kInvalidArgument},
      {"missing moves",
       Json{{"session_id", kSessionId}, {"expected_objects", guards}}.dump(),
       absl::StatusCode::kInvalidArgument},
      {"session_id is not a string",
       Json{{"session_id", 1}, {"expected_objects", guards}, {"moves", moves}}
           .dump(),
       absl::StatusCode::kInvalidArgument},
      {"guard coordinate is a boolean",
       with_guards([](Json& g) { g[0]["x_tile"] = true; }),
       absl::StatusCode::kInvalidArgument},
      {"guard coordinate is a float",
       with_guards([](Json& g) { g[0]["x_tile"] = 6.0; }),
       absl::StatusCode::kInvalidArgument},
      {"guard has an extra key",
       with_guards([](Json& g) { g[0]["layer"] = 0; }),
       absl::StatusCode::kInvalidArgument},
      {"guard is missing stored_list",
       with_guards([](Json& g) { g[0].erase("stored_list"); }),
       absl::StatusCode::kInvalidArgument},
      {"move coordinate is a float",
       with_moves([](Json& m) { m[0]["y_tile"] = 10.5; }),
       absl::StatusCode::kInvalidArgument},
      {"move coordinate is a boolean",
       with_moves([](Json& m) { m[0]["x_tile"] = false; }),
       absl::StatusCode::kInvalidArgument},
      {"move index is negative",
       with_moves([](Json& m) { m[0]["object_index"] = -1; }),
       absl::StatusCode::kInvalidArgument},
      {"move is missing y_tile",
       with_moves([](Json& m) { m[0].erase("y_tile"); }),
       absl::StatusCode::kInvalidArgument},
      {"batch over 1 MiB", std::string(1024 * 1024 + 1, ' '),
       absl::StatusCode::kInvalidArgument},
      {"stale guard", with_guards([](Json& g) { g[1]["x_tile"] = 0; }),
       absl::StatusCode::kFailedPrecondition},
      {"target outside the room",
       with_moves([](Json& m) { m[0]["x_tile"] = 64; }),
       absl::StatusCode::kInvalidArgument},
  };
  for (const auto& batch_case : batch_cases) {
    SCOPED_TRACE(batch_case.name);
    ExpectRejected(OnDispatcher(*dispatcher_, ApplyCall("0", batch_case.batch)),
                   batch_case.code, /*provider_invoked=*/true);
  }

  struct RoomCase {
    const char* room;
    absl::StatusCode code;
  };
  const std::vector<RoomCase> room_cases = {
      {"zero", absl::StatusCode::kInvalidArgument},
      {"-1", absl::StatusCode::kInvalidArgument},
      {"0x", absl::StatusCode::kInvalidArgument},
      {"0 ", absl::StatusCode::kInvalidArgument},
      {"296", absl::StatusCode::kInvalidArgument},
      {"1", absl::StatusCode::kFailedPrecondition},
  };
  for (const auto& room_case : room_cases) {
    SCOPED_TRACE(testing::Message() << "room '" << room_case.room << "'");
    ExpectRejected(OnDispatcher(*dispatcher_, InspectCall(room_case.room)),
                   room_case.code, /*provider_invoked=*/true);
    ExpectRejected(OnDispatcher(*dispatcher_, ApplyCall(room_case.room, batch)),
                   room_case.code, /*provider_invoked=*/true);
  }
}

// Oracle of Secrets room $25, proposal "southern-basin-raised-two-rows": the
// 22 basin frame, water band and E7 pattern objects, as decoded from the base
// ROM (index, id, size, stored list, x, y). Each moves two tile rows north.
struct BasinObject {
  size_t index;
  int id;
  int size;
  int stored_list;
  int x;
  int y;
};

constexpr int kOracleRoomId = 0x25;
constexpr size_t kOracleRoomObjectCount = 53;
constexpr int kBasinRowsNorth = 2;
constexpr std::array<BasinObject, 22> kSouthernBasinObjects = {{
    {2, 0x108, 0, 0, 5, 43},    {3, 0x063, 3, 0, 5, 47},
    {4, 0x109, 0, 0, 5, 55},    {5, 0x003, 15, 0, 9, 43},
    {6, 0x003, 6, 0, 41, 43},   {7, 0x10A, 0, 0, 55, 43},
    {8, 0x064, 3, 0, 55, 47},   {9, 0x10B, 0, 0, 55, 55},
    {10, 0x004, 15, 0, 9, 55},  {11, 0x004, 6, 0, 41, 55},
    {41, 0x0D9, 15, 1, 5, 43},  {42, 0x0D9, 15, 1, 21, 43},
    {43, 0x0D9, 15, 1, 37, 43}, {44, 0x0D9, 7, 1, 53, 43},
    {45, 0x0E7, 14, 1, 6, 44},  {46, 0x0E7, 12, 1, 6, 54},
    {47, 0x0E7, 14, 1, 22, 44}, {48, 0x0E7, 12, 1, 22, 54},
    {49, 0x0E7, 14, 1, 38, 44}, {50, 0x0E7, 12, 1, 38, 54},
    {51, 0x0E7, 2, 1, 54, 44},  {52, 0x0E7, 0, 1, 54, 54},
}};

TEST(DungeonAgentToolsNativeRomTest,
     OracleRoom25SouthernBasinMovesTwoRowsNorthThroughTool) {
  const char* rom_path = std::getenv("YAZE_NATIVE_BATCH_ROM");
  if (rom_path == nullptr || rom_path[0] == '\0') {
    GTEST_SKIP() << "Set YAZE_NATIVE_BATCH_ROM to an Oracle of Secrets base "
                    "ROM to run the native room $25 scenario";
  }

  EditorEnvironmentScope environment;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok()) << rom_path;
  const std::vector<uint8_t> rom_before = rom.vector();
  zelda3::GameData data;
  const auto data_status = zelda3::LoadGameData(rom, data);
  ASSERT_TRUE(data_status.ok()) << data_status;
  gfx::PaletteManager::Get().Initialize(&data);

  DungeonEditorV2 editor(&rom);
  editor.SetGameData(&data);
  editor.add_room(kOracleRoomId);
  zelda3::Room* room = editor.rooms().GetIfLoaded(kOracleRoomId);
  ASSERT_NE(room, nullptr);
  room->LoadSprites();
  room->LoadRoomGraphics();

  const std::vector<zelda3::RoomObject> original_objects =
      room->GetTileObjects();
  ASSERT_EQ(original_objects.size(), kOracleRoomObjectCount);
  std::vector<zelda3::RoomObjectMove> moves;
  for (const auto& basin : kSouthernBasinObjects) {
    SCOPED_TRACE(testing::Message() << "object " << basin.index);
    const auto& object = original_objects.at(basin.index);
    ASSERT_EQ(object.id_, basin.id);
    ASSERT_EQ(object.size(), basin.size);
    ASSERT_EQ(object.GetLayerValue(), basin.stored_list);
    ASSERT_EQ(object.x(), basin.x);
    ASSERT_EQ(object.y(), basin.y);
    moves.push_back({.object_index = basin.index,
                     .x_tile = basin.x,
                     .y_tile = basin.y - kBasinRowsNorth});
  }
  const std::vector<zelda3::RoomObject> candidate_objects =
      WithMoves(original_objects, moves);

  room->RenderRoomGraphics();
  const RenderSnapshot original = CaptureRoomRender(*room);
  ASSERT_EQ(Component(original, "composite.pixels").size(), 512u * 512u);
  ASSERT_FALSE(editor.undo_manager().CanUndo());

  int provider_calls = 0;
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&rom);
  ToolDispatcher::ToolPreferences preferences;
  preferences.allow_mutating_tools = true;
  dispatcher.SetToolPreferences(preferences);
  dispatcher.SetDungeonEditorToolCallback(
      MakeDungeonEditorToolCallback([&]() -> DungeonEditorToolContext {
        ++provider_calls;
        return DungeonEditorToolContext{
            .rom = &rom, .session_id = "oracle-room-25", .dungeon = &editor};
      }));

  const auto inspected = DispatchJson(dispatcher, InspectCall("0x25"));
  ASSERT_TRUE(inspected.ok()) << inspected.status();
  ASSERT_NO_FATAL_FAILURE(
      ExpectNativeResultEnvelope(*inspected, kOracleRoomId, "oracle-room-25"));
  ASSERT_EQ(inspected->at("objects"), GuardsJson(original_objects));

  const auto applied = DispatchJson(
      dispatcher,
      ApplyCall("0x25", BatchJson(inspected->at("session_id"),
                                  inspected->at("objects"), MovesJson(moves))));

  ASSERT_TRUE(applied.ok()) << applied.status();
  ASSERT_NO_FATAL_FAILURE(
      ExpectNativeResultEnvelope(*applied, kOracleRoomId, "oracle-room-25"));
  EXPECT_EQ(applied->at("changed_object_count"), kSouthernBasinObjects.size());
  EXPECT_EQ(applied->at("rendered"), true);
  EXPECT_EQ(applied->at("undo_recorded"), true);
  EXPECT_EQ(applied->at("objects"), GuardsJson(candidate_objects));
  EXPECT_EQ(provider_calls, 2);
  EXPECT_EQ(editor.rooms().GetIfLoaded(kOracleRoomId), room);
  EXPECT_EQ(Records(room->GetTileObjects()), Records(candidate_objects))
      << "Only the 22 basin positions may change; IDs, sizes, lists and "
         "order are preserved";
  EXPECT_EQ(editor.undo_manager().UndoStackSize(), 1u);

  const RenderSnapshot candidate = CaptureRoomRender(*room);
  EXPECT_GT(CountDifferentBytes(Component(original, "composite.pixels"),
                                Component(candidate, "composite.pixels")),
            0u)
      << "Moving the basin must change the actual composite";
  EXPECT_FALSE(candidate == original);

  ASSERT_TRUE(editor.Undo().ok());
  EXPECT_EQ(Records(room->GetTileObjects()), Records(original_objects));
  const RenderSnapshot undone = CaptureRoomRender(*room);
  for (size_t i = 0; i < original.size(); ++i) {
    EXPECT_TRUE(undone[i] == original[i])
        << original[i].first << " differs after Undo";
  }
  EXPECT_FALSE(editor.Undo().ok())
      << "The 22-object move must occupy exactly one undo entry";

  ASSERT_TRUE(editor.Redo().ok());
  EXPECT_EQ(Records(room->GetTileObjects()), Records(candidate_objects));
  const RenderSnapshot redone = CaptureRoomRender(*room);
  for (size_t i = 0; i < candidate.size(); ++i) {
    EXPECT_TRUE(redone[i] == candidate[i])
        << candidate[i].first << " differs after Redo";
  }

  EXPECT_TRUE(rom.vector() == rom_before)
      << "The source ROM bytes must be unchanged";
  EXPECT_FALSE(rom.dirty());
}

#else  // !YAZE_WITH_JSON

TEST(DungeonAgentToolsTest, RequiresJsonSupport) {
  GTEST_SKIP() << "Native dungeon agent tools require YAZE_WITH_JSON";
}

#endif  // YAZE_WITH_JSON

}  // namespace
}  // namespace yaze::editor
