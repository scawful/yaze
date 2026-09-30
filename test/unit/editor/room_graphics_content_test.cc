#include "app/editor/dungeon/workspace/room_graphics_content.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "framework/mock_renderer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace yaze::editor {

struct RoomGraphicsContentTestAccess {
  static void Refresh(RoomGraphicsContent& content, const zelda3::Room& room) {
    content.RefreshSheetPreviews(room);
  }

  static gfx::Bitmap& Sheet(RoomGraphicsContent& content, size_t index) {
    return content.sheet_previews_[index];
  }
};

namespace {

size_t ActiveSurfaceCount() {
  const auto& arena = gfx::Arena::Get();
  return arena.GetSurfaceCount() - arena.GetPooledSurfaceCount();
}

class RoomGraphicsContentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    game_data_.graphics_buffer.assign(223 * 4096, 1);
    room_ = std::make_unique<zelda3::Room>(0, &rom_, &game_data_);
    room_->LoadRoomGraphics();
    room_->CopyRoomGraphicsToBuffer();
  }

  void TearDown() override {
    auto& arena = gfx::Arena::Get();
    arena.ClearTextureQueue();
    ::testing::NiceMock<test::MockRenderer> renderer;
    arena.DrainRetiredBitmaps(&renderer);
  }

  void ReloadPixels(uint8_t pixel) {
    std::fill(game_data_.graphics_buffer.begin(),
              game_data_.graphics_buffer.end(), pixel);
    room_->CopyRoomGraphicsToBuffer();
  }

  Rom rom_;
  zelda3::GameData game_data_;
  std::unique_ptr<zelda3::Room> room_;
};

TEST_F(RoomGraphicsContentTest,
       ReloadRefreshesPixelsWithoutChangingRoomHeader) {
  RoomGraphicsContent content;
  RoomGraphicsContentTestAccess::Refresh(content, *room_);
  auto& sheet = RoomGraphicsContentTestAccess::Sheet(content, 0);
  EXPECT_EQ(sheet.at(0), 9);  // UW block zero uses the right palette half.
  const auto bitmap_generation = sheet.generation();
  const auto blocks = room_->blocks();

  RoomGraphicsContentTestAccess::Refresh(content, *room_);
  EXPECT_EQ(sheet.generation(), bitmap_generation);
  ReloadPixels(2);
  ASSERT_EQ(room_->blocks(), blocks);
  RoomGraphicsContentTestAccess::Refresh(content, *room_);
  EXPECT_EQ(sheet.at(0), 10);
  EXPECT_NE(sheet.generation(), bitmap_generation);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(RoomGraphicsContentTest, DrawRefreshesEntrancePixelsWithoutRoomCanvas) {
  ImGuiContext* previous = ImGui::GetCurrentContext();
  ImGuiContext* context = ImGui::CreateContext();
  const absl::Cleanup restore_context = [&] {
    ImGui::DestroyContext(context);
    ImGui::SetCurrentContext(previous);
  };
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1280, 720);
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  game_data_.main_blockset_ids[1][0] = 1;
  std::fill_n(game_data_.graphics_buffer.begin() + 4096, 4096, 2);
  DungeonRoomStore rooms(&rom_, &game_data_);
  int selected_room = 0;
  auto& room = rooms[0];
  room.SetTileObjects({});
  RoomGraphicsContent content(&selected_room, &rooms);
  const auto draw = [&] {
    ImGui::NewFrame();
    ImGui::Begin("Room graphics test");
    content.Draw(nullptr);
    ImGui::End();
    ImGui::Render();
  };
  draw();
  ASSERT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 0).at(0), 9);
  room.SetRenderEntranceBlockset(1);
  draw();
  EXPECT_EQ(room.blocks()[0], 1);
  EXPECT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 0).at(0), 10);
  room.SetRenderEntranceBlockset(0xFF);
  draw();
  EXPECT_EQ(room.blocks()[0], 0);
  EXPECT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 0).at(0), 9);
  // The group editor writes this shared table directly, without a room edit.
  game_data_.room_blockset_ids[0][0] = 1;
  draw();
  EXPECT_EQ(room.blocks()[3], 1);
  EXPECT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 3).at(0), 10);
  game_data_.room_blockset_ids[0][0] = 0;
  draw();
  EXPECT_EQ(room.blocks()[3], 0);
  EXPECT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 3).at(0), 9);
  // Identical sheet IDs still expand differently for main groups >= 0x20.
  const auto same_sheets = room.blocks();
  game_data_.room_default_entrances.resize(1);
  game_data_.room_default_entrances[0] = {
      .entrance_id = 0,
      .main_blockset = 0x20,
      .source = zelda3::RoomEntranceSource::kDirect};
  draw();
  EXPECT_EQ(room.blocks(), same_sheets);
  EXPECT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 1).at(0), 1);
  game_data_.room_default_entrances[0].main_blockset = 0;
  draw();
  EXPECT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 1).at(0), 9);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(RoomGraphicsContentTest, SameAddressRoomReplacementRefreshesPixels) {
  RoomGraphicsContent content;
  RoomGraphicsContentTestAccess::Refresh(content, *room_);
  EXPECT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 0).at(0), 9);
  const auto* original_address = room_.get();
  const auto blocks = room_->blocks();

  std::fill(game_data_.graphics_buffer.begin(),
            game_data_.graphics_buffer.end(), 2);
  zelda3::Room replacement(0, &rom_, &game_data_);
  replacement.LoadRoomGraphics();
  replacement.CopyRoomGraphicsToBuffer();
  *room_ = std::move(replacement);
  ASSERT_EQ(room_.get(), original_address);
  ASSERT_EQ(room_->blocks(), blocks);

  RoomGraphicsContentTestAccess::Refresh(content, *room_);
  EXPECT_EQ(RoomGraphicsContentTestAccess::Sheet(content, 0).at(0), 10);
  EXPECT_FALSE(rom_.dirty());
}

TEST_F(RoomGraphicsContentTest,
       ReloadKeepsBitmapOwnersAndUpdatesExistingTextures) {
  RoomGraphicsContent content;
  RoomGraphicsContentTestAccess::Refresh(content, *room_);
  std::array<int, 16> texture_storage{};
  std::array<gfx::Bitmap*, 16> owners{};
  ::testing::NiceMock<test::MockRenderer> renderer;
  EXPECT_CALL(renderer, CreateTexture).Times(0);
  EXPECT_CALL(renderer, DestroyTexture).Times(0);
  for (size_t i = 0; i < owners.size(); ++i) {
    owners[i] = &RoomGraphicsContentTestAccess::Sheet(content, i);
    owners[i]->set_texture(&texture_storage[i]);
    EXPECT_CALL(renderer,
                UpdateTexture(&texture_storage[i], ::testing::Ref(*owners[i])))
        .Times(1);
  }

  // Initial CREATE commands are still pending. A refresh must supersede them
  // without freeing a texture already referenced by this frame's draw list.
  ReloadPixels(2);
  RoomGraphicsContentTestAccess::Refresh(content, *room_);
  auto& arena = gfx::Arena::Get();
  while (arena.texture_command_queue_size() != 0) {
    arena.ProcessSingleTexture(&renderer);
  }
  for (size_t i = 0; i < owners.size(); ++i) {
    EXPECT_EQ(&RoomGraphicsContentTestAccess::Sheet(content, i), owners[i]);
    EXPECT_EQ(owners[i]->texture(), &texture_storage[i]);
  }
}

TEST_F(RoomGraphicsContentTest,
       DestructionCancelsQueuedOwnersAndDefersTextureRetirement) {
  auto& arena = gfx::Arena::Get();
  const auto initial_surfaces = ActiveSurfaceCount();
  const auto initial_retired = arena.retired_texture_handle_count();
  int texture_storage = 0;
  {
    RoomGraphicsContent content;
    RoomGraphicsContentTestAccess::Refresh(content, *room_);
    RoomGraphicsContentTestAccess::Sheet(content, 0)
        .set_texture(&texture_storage);
    ASSERT_GT(arena.texture_command_queue_size(), 0u);
    EXPECT_EQ(ActiveSurfaceCount(), initial_surfaces + 16);
  }

  EXPECT_EQ(arena.texture_command_queue_size(), 0u);
  EXPECT_EQ(ActiveSurfaceCount(), initial_surfaces);
  EXPECT_EQ(arena.retired_texture_handle_count(), initial_retired + 1);
  ::testing::NiceMock<test::MockRenderer> renderer;
  EXPECT_CALL(renderer, DestroyTexture(&texture_storage)).Times(1);
  arena.DrainRetiredBitmaps(&renderer);
}

}  // namespace
}  // namespace yaze::editor
