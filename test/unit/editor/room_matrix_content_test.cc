#include "app/editor/dungeon/workspace/room_matrix_content.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_room_composite.h"
#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_id_registry.h"
#include "gtest/gtest.h"
#include "imgui/imgui_internal.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

class RoomMatrixContentTestPeer {
 public:
  static zelda3::Room* Preview(RoomMatrixContent& matrix, int room_id) {
    auto* room = matrix.HoverPreviewRoom(room_id);
    if (room)
      PrepareCanonicalRoomComposite(*room, matrix.tooltip_composite_output_);
    return room;
  }

  static bool HasCachedRoom(const RoomMatrixContent& matrix) {
    return matrix.hover_preview_room_ != nullptr;
  }

  static bool HasCachedDocument(const RoomMatrixContent& matrix) {
    return matrix.hover_preview_document_.has_value();
  }

  static bool HasComposite(const RoomMatrixContent& matrix) {
    return matrix.tooltip_composite_output_.bitmap().is_active();
  }
};

namespace {

class RoomMatrixFixedPreviewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1000, 900);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigInputTrickleEventQueue = false;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    matrix_ = std::make_unique<RoomMatrixContent>(
        &current_, &active_, [this](int room_id) { clicked_ = room_id; });
  }

  void TearDown() override {
    matrix_.reset();
    gui::WidgetIdRegistry::Instance().Clear();
    gfx::Arena::Get().ClearTextureQueue();
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_);
  }

  void Frame(ImVec2 size = ImVec2(320, 480)) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("FixedMatrixHost", nullptr, ImGuiWindowFlags_NoSavedSettings);
    matrix_->DrawInWorkbench();
    ImGui::End();
    ImGui::Render();
  }

  std::optional<gui::WidgetIdRegistry::WidgetInfo> Cell(int room_id) {
    const std::string label = absl::StrFormat("Room 0x%03X", room_id);
    for (const auto& [path, info] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
      if (info.label == label)
        return info;
    }
    return std::nullopt;
  }

  ImGuiWindow* Child(const char* id) {
    const std::string name = std::string("/") + id + "_";
    for (auto* window : context_->Windows) {
      if (window->Active &&
          std::string(window->Name).find(name) != std::string::npos)
        return window;
    }
    return nullptr;
  }

  int current_ = 0x20;
  int clicked_ = -1;
  ImVector<int> active_;
  ImGuiContext* previous_ = nullptr;
  ImGuiContext* context_ = nullptr;
  std::unique_ptr<RoomMatrixContent> matrix_;
};

TEST_F(RoomMatrixFixedPreviewTest,
       HoverRetainsStablePreviewAndClickAloneNavigates) {
  Frame();
  Frame();
  EXPECT_EQ(matrix_->preview_room_id(), current_);
  const auto cell = Cell(0x50);
  ASSERT_TRUE(cell.has_value());
  auto* preview = Child("##RoomMatrixFixedPreview");
  ASSERT_NE(preview, nullptr);
  const ImVec2 preview_position = preview->Pos;
  const ImVec2 preview_size = preview->Size;
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent((cell->bounds.min_x + cell->bounds.max_x) * 0.5f,
                      (cell->bounds.min_y + cell->bounds.max_y) * 0.5f);
  Frame();
  Frame();
  EXPECT_EQ(matrix_->preview_room_id(), 0x50);
  EXPECT_EQ(current_, 0x20);
  EXPECT_EQ(clicked_, -1);
  preview = Child("##RoomMatrixFixedPreview");
  ASSERT_NE(preview, nullptr);
  EXPECT_FLOAT_EQ(preview->Pos.x, preview_position.x);
  EXPECT_FLOAT_EQ(preview->Pos.y, preview_position.y);
  EXPECT_FLOAT_EQ(preview->Size.x, preview_size.x);
  EXPECT_FLOAT_EQ(preview->Size.y, preview_size.y);
  for (auto* window : context_->Windows)
    EXPECT_FALSE(window->Active && (window->Flags & ImGuiWindowFlags_Tooltip));
  io.AddMouseButtonEvent(0, true);
  Frame();
  io.AddMouseButtonEvent(0, false);
  Frame();
  EXPECT_EQ(clicked_, 0x50);
  io.AddMousePosEvent(-1000, -1000);
  Frame();
  EXPECT_EQ(matrix_->preview_room_id(), 0x50);
  matrix_->InvalidatePreviewCache();
  EXPECT_EQ(matrix_->preview_room_id(), -1);
  Frame();
  EXPECT_EQ(matrix_->preview_room_id(), current_);
}

TEST_F(RoomMatrixFixedPreviewTest,
       ShortRegionScrollsUsableCellsAndReachesLastRoom) {
  const ImVec2 size(280, 240);
  Frame(size);
  Frame(size);
  auto* grid = Child("##RoomMatrixGrid");
  ASSERT_NE(grid, nullptr);
  EXPECT_GT(grid->ScrollMax.y, 0.0f);
  int count = 0;
  for (const auto& [path, cell] :
       gui::WidgetIdRegistry::Instance().GetAllWidgets()) {
    if (cell.label.find("Room 0x") == 0) {
      ++count;
      EXPECT_GE(cell.bounds.max_x - cell.bounds.min_x, 8.0f);
      EXPECT_GE(cell.bounds.max_y - cell.bounds.min_y, 8.0f);
    }
  }
  EXPECT_EQ(count, 296);
  auto* host = ImGui::FindWindowByName("FixedMatrixHost");
  ASSERT_NE(host, nullptr);
  EXPECT_EQ(host->ScrollMax.y, 0.0f);
  ImGui::SetScrollY(grid, grid->ScrollMax.y);
  Frame(size);
  Frame(size);
  const auto last = Cell(0x127);
  ASSERT_TRUE(last.has_value());
  EXPECT_TRUE(last->visible);
  EXPECT_GE(last->bounds.min_y, grid->InnerClipRect.Min.y);
  EXPECT_LE(last->bounds.max_y, grid->InnerClipRect.Max.y);
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent((last->bounds.min_x + last->bounds.max_x) * 0.5f,
                      (last->bounds.min_y + last->bounds.max_y) * 0.5f);
  Frame(size);
  io.AddMouseButtonEvent(0, true);
  Frame(size);
  io.AddMouseButtonEvent(0, false);
  Frame(size);
  EXPECT_EQ(clicked_, 0x127);
  EXPECT_EQ(host->ScrollMax.y, 0.0f);
}

TEST(RoomMatrixPreviewIsolationTest,
     PrivateRenderingUsesResolvedEntranceAndPreservesLoadedAndPendingRooms) {
  gfx::Arena::Get().ClearTextureQueue();
  gfx::Arena::Get().Initialize(nullptr);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  const auto pointer = [&rom](int address, int target) {
    const uint32_t snes = PcToSnes(target);
    for (int i = 0; i < 3; ++i)
      rom.mutable_data()[address + i] = (snes >> (8 * i)) & 0xFF;
  };
  pointer(zelda3::kRoomObjectPointer, 0x0F8000);
  for (int id = 0; id < zelda3::kNumberOfRooms; ++id)
    pointer(0x0F8000 + id * 3, 0x100000);
  ASSERT_TRUE(rom.WriteVector(0x100000, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
                                         0xFF, 0xFF, 0xFF})
                  .ok());
  ASSERT_TRUE(
      rom.WriteWord(SnesToPc(zelda3::kRoomLayoutPointers.front()), 0xFFFF)
          .ok());
  zelda3::GameData data;
  data.graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 0);
  data.main_blockset_ids[1][0] = 9;
  data.main_blockset_ids[2][0] = 10;
  data.palette_groups.dungeon_main.AddPalette(gfx::SnesPalette(
      std::vector<gfx::SnesColor>(90, gfx::SnesColor(64, 96, 128))));
  data.palette_groups.hud.AddPalette(gfx::SnesPalette(
      std::vector<gfx::SnesColor>(32, gfx::SnesColor(0, 0, 0))));
  DungeonRoomStore rooms(&rom, &data);
  auto& loaded = rooms[0];
  loaded.SetLoaded(true);
  loaded.SetTileObjects({});
  loaded.SetRenderEntranceBlockset(1);
  loaded.PrepareForRender();
  const auto authored = CaptureDungeonRoomDocument(loaded);
  const auto revision = loaded.composite_source_revision();
  const auto dirty = loaded.CaptureSaveDirtySnapshot();
  auto& pending = rooms[1];
  pending.custom_collision().SetOverride(42, 0);
  pending.MarkCustomCollisionDirty();
  const auto pending_map = pending.custom_collision();
  const auto bytes = rom.vector();
  int current = 0;
  ImVector<int> active;
  RoomMatrixContent matrix(&current, &active, [](int) {}, nullptr, &rooms);
  matrix.SetPreviewEntranceBlocksetResolver([](int) { return uint8_t{2}; });
  auto* preview = RoomMatrixContentTestPeer::Preview(matrix, 0);
  ASSERT_NE(preview, nullptr);
  EXPECT_NE(preview, &loaded);
  EXPECT_EQ(preview->render_entrance_blockset(), 2);
  EXPECT_EQ(preview->blocks()[0], 10);
  EXPECT_EQ(loaded.render_entrance_blockset(), 1);
  EXPECT_EQ(loaded.blocks()[0], 9);
  EXPECT_TRUE(
      SameDungeonRoomDocument(CaptureDungeonRoomDocument(loaded), authored));
  EXPECT_EQ(loaded.composite_source_revision(), revision);
  EXPECT_EQ(loaded.CaptureSaveDirtySnapshot().header, dirty.header);
  EXPECT_EQ(loaded.CaptureSaveDirtySnapshot().object_stream,
            dirty.object_stream);
  loaded.SetTileObjects({zelda3::RoomObject(0x01, 5, 5, 3, 0)});
  const auto edited = CaptureDungeonRoomDocument(loaded);
  const auto edited_revision = loaded.composite_source_revision();
  preview = RoomMatrixContentTestPeer::Preview(matrix, 0);
  ASSERT_NE(preview, nullptr);
  ASSERT_EQ(preview->GetTileObjects().size(), 1);
  EXPECT_EQ(preview->GetTileObjects()[0].x_, 5);
  EXPECT_TRUE(
      SameDungeonRoomDocument(CaptureDungeonRoomDocument(loaded), edited));
  EXPECT_EQ(loaded.composite_source_revision(), edited_revision);
  EXPECT_TRUE(loaded.CaptureSaveDirtySnapshot().object_stream);
  // Rebinding GameData does not change the room ID or authored document, so a
  // cached private room must explicitly pick up the new source and pixels.
  auto replacement = std::make_unique<zelda3::GameData>();
  replacement->graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 3);
  replacement->main_blockset_ids[2][0] = 11;
  replacement->palette_groups.dungeon_main.AddPalette(gfx::SnesPalette(
      std::vector<gfx::SnesColor>(90, gfx::SnesColor(64, 96, 128))));
  replacement->palette_groups.hud.AddPalette(gfx::SnesPalette(
      std::vector<gfx::SnesColor>(32, gfx::SnesColor(0, 0, 0))));
  rooms.SetGameData(replacement.get());
  preview = RoomMatrixContentTestPeer::Preview(matrix, 0);
  ASSERT_NE(preview, nullptr);
  EXPECT_EQ(preview->game_data(), replacement.get());
  EXPECT_EQ(preview->blocks()[0], 11);
  EXPECT_EQ(preview->get_gfx_buffer()[0],
            3 + preview->BackgroundPaletteOffset(0));
  EXPECT_EQ(loaded.get_gfx_buffer()[0], 0);

  // Preview-only sheet edits are runtime state, deliberately absent from the
  // portable room document. Copy and later clear them without rendering source.
  loaded.SetGraphicsSheetOverrides({{11, std::vector<uint8_t>(4096, 7)}});
  const auto override_revision = loaded.composite_source_revision();
  preview = RoomMatrixContentTestPeer::Preview(matrix, 0);
  ASSERT_NE(preview, nullptr);
  EXPECT_EQ(preview->graphics_sheet_overrides(),
            loaded.graphics_sheet_overrides());
  EXPECT_EQ(preview->get_gfx_buffer()[0],
            7 + preview->BackgroundPaletteOffset(0));
  EXPECT_EQ(loaded.get_gfx_buffer()[0], 0);
  EXPECT_EQ(loaded.composite_source_revision(), override_revision);
  loaded.SetGraphicsSheetOverrides({});
  preview = RoomMatrixContentTestPeer::Preview(matrix, 0);
  ASSERT_NE(preview, nullptr);
  EXPECT_TRUE(preview->graphics_sheet_overrides().empty());
  EXPECT_EQ(preview->get_gfx_buffer()[0],
            3 + preview->BackgroundPaletteOffset(0));
  rooms.SetGameData(&data);
  preview = RoomMatrixContentTestPeer::Preview(matrix, 1);
  ASSERT_NE(preview, nullptr);
  EXPECT_EQ(preview->custom_collision(), pending_map);
  EXPECT_EQ(pending.custom_collision(), pending_map);
  EXPECT_TRUE(pending.custom_collision_dirty());
  EXPECT_FALSE(pending.IsLoaded());
  EXPECT_EQ(rooms.LoadedCount(), 1);
  EXPECT_EQ(rooms.GetIfMaterialized(2), nullptr);
  ASSERT_NE(RoomMatrixContentTestPeer::Preview(matrix, 2), nullptr);
  EXPECT_EQ(rooms.GetIfMaterialized(2), nullptr);
  EXPECT_EQ(current, 0);
  EXPECT_EQ(active.Size, 0);
  EXPECT_EQ(rom.vector(), bytes);
  gfx::Arena::Get().ClearTextureQueue();
}

TEST(RoomMatrixPreviewIsolationTest,
     ResetSamePointerDocumentAndChangedRomRebuildPrivatePixels) {
  gfx::Arena::Get().ClearTextureQueue();
  gfx::Arena::Get().Initialize(nullptr);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  const auto pointer = [&rom](int address, int target) {
    const uint32_t snes = PcToSnes(target);
    for (int i = 0; i < 3; ++i)
      rom.mutable_data()[address + i] = (snes >> (8 * i)) & 0xFF;
  };
  pointer(zelda3::kRoomObjectPointer, 0x0F8000);
  for (int id = 0; id < zelda3::kNumberOfRooms; ++id)
    pointer(0x0F8000 + id * 3, 0x100000);
  ASSERT_TRUE(rom.WriteVector(0x100000, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
                                         0xFF, 0xFF, 0xFF})
                  .ok());
  ASSERT_TRUE(
      rom.WriteWord(SnesToPc(zelda3::kRoomLayoutPointers.front()), 0xFFFF)
          .ok());
  zelda3::GameData data;
  data.graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 1);
  std::fill_n(data.graphics_buffer.begin() + 11 * 4096, 4096, 3);
  std::fill_n(data.graphics_buffer.begin() + 12 * 4096, 4096, 5);
  data.room_blockset_ids[0][0] = 11;
  data.room_blockset_ids[1][0] = 12;
  data.palette_groups.dungeon_main.AddPalette(gfx::SnesPalette(
      std::vector<gfx::SnesColor>(90, gfx::SnesColor(64, 96, 128))));
  data.palette_groups.hud.AddPalette(gfx::SnesPalette(
      std::vector<gfx::SnesColor>(32, gfx::SnesColor(0, 0, 0))));
  DungeonRoomStore rooms(&rom, &data);
  auto& loaded = rooms[0];
  loaded.SetLoaded(true);
  loaded.SetTileObjects({});
  const auto document = CaptureDungeonRoomDocument(loaded);
  int current = 0;
  ImVector<int> active;
  RoomMatrixContent matrix(&current, &active, [](int) {}, nullptr, &rooms);
  matrix.SetPreviewEntranceBlocksetResolver([](int) { return uint8_t{0}; });
  auto* preview = RoomMatrixContentTestPeer::Preview(matrix, 0);
  ASSERT_NE(preview, nullptr);
  EXPECT_EQ(preview->get_gfx_buffer()[0],
            1 + preview->BackgroundPaletteOffset(0));
  ASSERT_TRUE(RoomMatrixContentTestPeer::HasCachedDocument(matrix));
  ASSERT_TRUE(RoomMatrixContentTestPeer::HasComposite(matrix));

  // Model a completed in-place reload: same ROM/store/GameData/Room addresses
  // and authored fields, with newly decoded graphics bytes.
  auto* original_room = &loaded;
  rooms[0] = zelda3::Room(0, &rom, &data);
  rooms[0].SetLoaded(true);
  rooms[0].SetTileObjects({});
  ASSERT_EQ(&rooms[0], original_room);
  ASSERT_TRUE(
      SameDungeonRoomDocument(CaptureDungeonRoomDocument(rooms[0]), document));
  std::fill_n(data.graphics_buffer.begin(), 4096, 6);
  matrix.InvalidatePreviewCache();
  EXPECT_FALSE(RoomMatrixContentTestPeer::HasCachedRoom(matrix));
  EXPECT_FALSE(RoomMatrixContentTestPeer::HasCachedDocument(matrix));
  EXPECT_FALSE(RoomMatrixContentTestPeer::HasComposite(matrix));
  EXPECT_EQ(matrix.preview_room_id(), -1);
  preview = RoomMatrixContentTestPeer::Preview(matrix, 0);
  ASSERT_NE(preview, nullptr);
  EXPECT_EQ(preview->get_gfx_buffer()[0],
            6 + preview->BackgroundPaletteOffset(0));

  // An unopened room has no shared document to change. A replacement ROM
  // still must invalidate its cached header and resulting room graphics.
  ASSERT_EQ(rooms.GetIfMaterialized(2), nullptr);
  preview = RoomMatrixContentTestPeer::Preview(matrix, 2);
  ASSERT_NE(preview, nullptr);
  EXPECT_EQ(preview->blockset(), 0);
  EXPECT_EQ(preview->get_gfx_buffer()[3 * 4096],
            3 + preview->BackgroundPaletteOffset(3));
  Rom replacement;
  ASSERT_TRUE(replacement.LoadFromData(rom.vector()).ok());
  // With this fixture's zero header table/bank, room 2's header is at PC 0.
  ASSERT_TRUE(replacement.WriteByte(2, 1).ok());
  rooms.SetRom(&replacement);
  preview = RoomMatrixContentTestPeer::Preview(matrix, 2);
  ASSERT_NE(preview, nullptr);
  EXPECT_EQ(preview->rom(), &replacement);
  EXPECT_EQ(preview->blockset(), 1);
  EXPECT_EQ(preview->get_gfx_buffer()[3 * 4096],
            5 + preview->BackgroundPaletteOffset(3));
  EXPECT_EQ(rooms.GetIfMaterialized(2), nullptr);
  matrix.SetRooms(nullptr);
  EXPECT_FALSE(RoomMatrixContentTestPeer::HasCachedRoom(matrix));
  EXPECT_FALSE(RoomMatrixContentTestPeer::HasCachedDocument(matrix));
  EXPECT_FALSE(RoomMatrixContentTestPeer::HasComposite(matrix));
  rooms.SetRom(&rom);
  gfx::Arena::Get().ClearTextureQueue();
}

}  // namespace
}  // namespace yaze::editor
