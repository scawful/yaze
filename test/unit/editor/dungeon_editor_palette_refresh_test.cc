#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_room_composite.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/resource/bitmap_texture_queue.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/widgets/palette_editor_widget.h"
#include "app/platform/sdl_compat.h"
#include "core/features.h"
#include "framework/mock_renderer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

size_t ActiveSurfaceCount(const gfx::Arena& arena) {
  return arena.GetSurfaceCount() - arena.GetPooledSurfaceCount();
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

class ScopedWorkbenchFlag {
 public:
  explicit ScopedWorkbenchFlag(bool enabled)
      : previous_(core::FeatureFlags::get().dungeon.kUseWorkbench) {
    core::FeatureFlags::get().dungeon.kUseWorkbench = enabled;
  }

  ~ScopedWorkbenchFlag() {
    core::FeatureFlags::get().dungeon.kUseWorkbench = previous_;
  }

 private:
  bool previous_;
};

class ScopedImGuiTestContext {
 public:
  ScopedImGuiTestContext() : previous_(ImGui::GetCurrentContext()) {
    context_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1920, 1080);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  ~ScopedImGuiTestContext() {
    if (frame_active_) {
      ImGui::EndFrame();
    }
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_);
  }

  void NextFrame() {
    if (frame_active_) {
      ImGui::EndFrame();
    }
    ImGui::NewFrame();
    frame_active_ = true;
  }

 private:
  ImGuiContext* previous_ = nullptr;
  ImGuiContext* context_ = nullptr;
  bool frame_active_ = false;
};

}  // namespace

class DungeonEditorPaletteRefreshTestPeer {
 public:
  static gfx::Bitmap* PrepareComposite(DungeonCanvasViewer& viewer,
                                       int room_id) {
    return viewer.PrepareRoomCompositeBitmap(room_id);
  }

  static std::string BuildDrawIssueReport(DungeonCanvasViewer& viewer,
                                          const zelda3::Room& room,
                                          int room_id) {
    return viewer.BuildDrawIssueReport(room, room_id);
  }

  static gfx::Bitmap* PrepareConnectedComposite(DungeonCanvasViewer& viewer,
                                                int room_id) {
    return viewer.PrepareConnectedRoomCompositeBitmap(room_id);
  }

  static void PruneConnectedComposites(DungeonCanvasViewer& viewer) {
    viewer.PruneConnectedRoomCompositeOutputs();
  }

  static size_t ConnectedCompositeCount(const DungeonCanvasViewer& viewer) {
    return viewer.connected_composite_outputs_.size();
  }

  static bool HasConnectedComposite(const DungeonCanvasViewer& viewer,
                                    int room_id) {
    return viewer.connected_composite_outputs_.contains(room_id);
  }

  static void ResetCompositeOutputs(DungeonCanvasViewer& viewer) {
    viewer.ResetRoomCompositeOutputs();
  }

  static gfx::Bitmap* PrimaryCompositeAddress(DungeonCanvasViewer& viewer) {
    return &viewer.primary_composite_output_.bitmap();
  }

  static void RegisterPaletteListener(DungeonEditorV2* editor) {
    editor->RegisterPaletteListener();
  }

  static void SetCurrentPaletteId(DungeonEditorV2* editor, int palette_id) {
    editor->current_palette_id_ = palette_id;
  }

  static const gfx::SnesPalette& CurrentPalette(const DungeonEditorV2& editor) {
    return editor.current_palette_;
  }

  static DungeonCanvasViewer* GetViewerForRoom(DungeonEditorV2* editor,
                                               int room_id) {
    return editor->GetViewerForRoom(room_id);
  }

  static void SetCurrentRoomId(DungeonEditorV2* editor, int room_id) {
    editor->current_room_id_ = room_id;
  }

  static void SetActiveRooms(DungeonEditorV2* editor,
                             std::initializer_list<int> room_ids) {
    editor->active_rooms_.clear();
    for (int room_id : room_ids) {
      editor->active_rooms_.push_back(room_id);
    }
  }

  static void HandlePaletteChanged(DungeonEditorV2* editor,
                                   gui::DungeonPaletteChange change) {
    editor->HandleDungeonPaletteChanged(change);
  }

  static DungeonRoomStore* Rooms(DungeonEditorV2* editor) {
    return &editor->rooms_;
  }
};

class DungeonEditorPaletteRefreshTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    gfx::Arena::Get().Initialize(nullptr);
    gfx::PaletteManager::Get().ResetForTesting();

    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    const int layout_address = SnesToPc(zelda3::kRoomLayoutPointers.front());
    ASSERT_GE(layout_address, 0);
    ASSERT_LT(layout_address + 1, static_cast<int>(rom_.size()));
    rom_.mutable_data()[layout_address] = 0xFF;
    rom_.mutable_data()[layout_address + 1] = 0xFF;

    game_data_.graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 0);
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

    SeedPaletteGroup(&game_data_.palette_groups.hud, /*palette_count=*/1,
                     /*color_count=*/32, /*seed=*/0x0100);
    SeedPaletteGroup(&game_data_.palette_groups.dungeon_main,
                     /*palette_count=*/4, /*color_count=*/90,
                     /*seed=*/0x0200);

    // Palette-set IDs 5 and 7 resolve to concrete dungeon palette 3, while
    // set 6 resolves to palette 2.
    game_data_.paletteset_ids[5][0] = 2;
    game_data_.paletteset_ids[6][0] = 4;
    game_data_.paletteset_ids[7][0] = 6;
    ASSERT_TRUE(rom_.WriteWord(zelda3::kDungeonPalettePointerTable + 2,
                               3 * zelda3::kDungeonPaletteBytes)
                    .ok());
    ASSERT_TRUE(rom_.WriteWord(zelda3::kDungeonPalettePointerTable + 4,
                               2 * zelda3::kDungeonPaletteBytes)
                    .ok());
    ASSERT_TRUE(rom_.WriteWord(zelda3::kDungeonPalettePointerTable + 6,
                               3 * zelda3::kDungeonPaletteBytes)
                    .ok());

    gfx::PaletteManager::Get().Initialize(&game_data_);
    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    editor_->SetGameData(&game_data_);
  }

  void TearDown() override {
    gfx::Arena::Get().ClearTextureQueue();
    gfx::Arena::Get().Initialize(nullptr);
    gfx::PaletteManager::Get().ResetForTesting();
  }

  void RenderToCleanState(zelda3::Room& room) {
    room.RenderRoomGraphics();
    room.GetCompositeBitmap(layer_manager_);
    ASSERT_FALSE(room.IsCompositeDirty());

    // A second render must hit Room::RenderRoomGraphics's clean-cache return.
    room.RenderRoomGraphics();
    ASSERT_FALSE(room.IsCompositeDirty());
  }

  gfx::Bitmap* PrepareComposite(DungeonCanvasViewer& viewer, int room_id) {
    return DungeonEditorPaletteRefreshTestPeer::PrepareComposite(viewer,
                                                                 room_id);
  }

  void SeedCompositeLayers(zelda3::Room& room, uint8_t bg1_pixel,
                           uint8_t bg2_pixel) {
    room.PrepareForRender();
    room.bg1_buffer().bitmap().Fill(255);
    room.bg2_buffer().bitmap().Fill(255);
    room.object_bg1_buffer().bitmap().Fill(255);
    room.object_bg2_buffer().bitmap().Fill(255);
    for (auto* buffer :
         {&room.bg1_buffer(), &room.bg2_buffer(), &room.object_bg1_buffer(),
          &room.object_bg2_buffer()}) {
      buffer->ClearPriorityBuffer();
      buffer->ClearCoverageBuffer();
      buffer->ClearBG1RevealMask();
    }
    room.bg1_buffer().bitmap().mutable_data()[0] = bg1_pixel;
    room.bg2_buffer().bitmap().mutable_data()[0] = bg2_pixel;
    room.MarkCompositeDirty();
  }

  void ConfigureCompositeLayers(DungeonCanvasViewer& viewer, int room_id,
                                bool show_bg1) {
    auto& manager = viewer.GetRoomLayerManager(room_id);
    manager.SetPriorityCompositing(false);
    for (const auto layer :
         {zelda3::LayerType::BG1_Layout, zelda3::LayerType::BG1_Objects,
          zelda3::LayerType::BG2_Layout, zelda3::LayerType::BG2_Objects}) {
      manager.SetLayerVisible(layer, true);
      manager.SetLayerBlendMode(layer, zelda3::LayerBlendMode::Normal);
    }
    manager.SetLayerVisible(zelda3::LayerType::BG1_Layout, show_bg1);
    manager.SetLayerVisible(zelda3::LayerType::BG1_Objects, show_bg1);
  }

  SDL_Color ReadSurfacePaletteColor(zelda3::Room& room, int color_index) {
    auto* surface = room.bg1_buffer().bitmap().surface();
    if (surface == nullptr) {
      ADD_FAILURE() << "Room render did not create a BG1 surface";
      return {};
    }
    SDL_Palette* palette = platform::GetSurfacePalette(surface);
    if (palette == nullptr || color_index < 0 ||
        color_index >= palette->ncolors) {
      ADD_FAILURE() << "Room render did not install the requested SDL color";
      return {};
    }
    return palette->colors[color_index];
  }

  void ExpectSurfaceColor(const SDL_Color& actual,
                          const gfx::SnesColor& expected) {
    const ImVec4 rgb = expected.rgb();
    EXPECT_EQ(actual.r, static_cast<Uint8>(rgb.x));
    EXPECT_EQ(actual.g, static_cast<Uint8>(rgb.y));
    EXPECT_EQ(actual.b, static_cast<Uint8>(rgb.z));
    EXPECT_EQ(actual.a, 255);
  }

  Rom rom_;
  zelda3::GameData game_data_{&rom_};
  std::unique_ptr<DungeonEditorV2> editor_;
  zelda3::RoomLayerManager layer_manager_;
};

TEST_F(DungeonEditorPaletteRefreshTest,
       SharedHudEditRefreshesRoomUsingDifferentDungeonPalette) {
  auto& selected_palette_room = editor_->rooms_[0];
  selected_palette_room.SetPalette(5);  // Resolves to dungeon palette 3.
  auto& different_palette_room = editor_->rooms_[1];
  different_palette_room.SetPalette(6);  // Resolves to dungeon palette 2.

  RenderToCleanState(selected_palette_room);
  RenderToCleanState(different_palette_room);

  constexpr int kHudDisplayIndex = 17;
  const SDL_Color before =
      ReadSurfacePaletteColor(different_palette_room, kHudDisplayIndex);
  const gfx::SnesColor edited_color(0x001F);

  editor_->palette_editor_.Initialize(&game_data_);
  editor_->palette_editor_.SetDungeonRenderPaletteMode(true);
  editor_->palette_editor_.SetCurrentPaletteId(3);
  editor_->palette_editor_.SetOnDungeonPaletteChanged(
      [this](gui::DungeonPaletteChange change) {
        editor_->InvalidateDungeonPaletteUsers(change);
      });

  ASSERT_TRUE(editor_->palette_editor_
                  .ApplyDungeonRenderColorEdit(kHudDisplayIndex, edited_color)
                  .ok());
  EXPECT_TRUE(selected_palette_room.IsCompositeDirty());
  EXPECT_TRUE(different_palette_room.IsCompositeDirty());

  different_palette_room.RenderRoomGraphics();
  const SDL_Color after =
      ReadSurfacePaletteColor(different_palette_room, kHudDisplayIndex);
  EXPECT_NE(before.r, after.r);
  ExpectSurfaceColor(after, edited_color);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       DungeonMainEditRefreshesResolvedAliasesOnly) {
  auto& matching_room = editor_->rooms_[0];
  matching_room.SetPalette(5);
  auto& unrelated_room = editor_->rooms_[1];
  unrelated_room.SetPalette(6);
  auto& aliased_matching_room = editor_->rooms_[2];
  aliased_matching_room.SetPalette(7);

  RenderToCleanState(matching_room);
  RenderToCleanState(unrelated_room);
  RenderToCleanState(aliased_matching_room);

  constexpr int kDungeonDisplayIndex = 33;
  const SDL_Color unrelated_before =
      ReadSurfacePaletteColor(unrelated_room, kDungeonDisplayIndex);
  const gfx::SnesColor edited_color(0x7C00);

  editor_->palette_editor_.Initialize(&game_data_);
  editor_->palette_editor_.SetDungeonRenderPaletteMode(true);
  editor_->palette_editor_.SetCurrentPaletteId(3);
  editor_->palette_editor_.SetOnDungeonPaletteChanged(
      [this](gui::DungeonPaletteChange change) {
        editor_->InvalidateDungeonPaletteUsers(change);
      });

  ASSERT_TRUE(
      editor_->palette_editor_
          .ApplyDungeonRenderColorEdit(kDungeonDisplayIndex, edited_color)
          .ok());
  EXPECT_TRUE(matching_room.IsCompositeDirty());
  EXPECT_FALSE(unrelated_room.IsCompositeDirty());
  EXPECT_TRUE(aliased_matching_room.IsCompositeDirty());

  matching_room.RenderRoomGraphics();
  aliased_matching_room.RenderRoomGraphics();
  unrelated_room.RenderRoomGraphics();
  EXPECT_FALSE(unrelated_room.IsCompositeDirty());

  ExpectSurfaceColor(
      ReadSurfacePaletteColor(matching_room, kDungeonDisplayIndex),
      edited_color);
  ExpectSurfaceColor(
      ReadSurfacePaletteColor(aliased_matching_room, kDungeonDisplayIndex),
      edited_color);
  const SDL_Color unrelated_after =
      ReadSurfacePaletteColor(unrelated_room, kDungeonDisplayIndex);
  EXPECT_EQ(unrelated_before.r, unrelated_after.r);
  EXPECT_EQ(unrelated_before.g, unrelated_after.g);
  EXPECT_EQ(unrelated_before.b, unrelated_after.b);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       ApplyPreviewChangesRefreshesAllMaterializedDungeonRooms) {
  auto& current_room = editor_->rooms()[0];
  current_room.SetPalette(5);  // Resolves to dungeon palette 3.
  auto& cached_room = editor_->rooms()[1];
  cached_room.SetPalette(6);  // Resolves to dungeon palette 2.

  RenderToCleanState(current_room);
  RenderToCleanState(cached_room);

  constexpr int kDungeonDisplayIndex = 33;
  constexpr int kDungeonColorIndex = 0;
  const gfx::SnesColor edited_color(0x7C00);
  const SDL_Color before =
      ReadSurfacePaletteColor(current_room, kDungeonDisplayIndex);

  DungeonEditorPaletteRefreshTestPeer::SetCurrentPaletteId(editor_.get(), 3);
  DungeonEditorPaletteRefreshTestPeer::RegisterPaletteListener(editor_.get());

  auto& palette_manager = gfx::PaletteManager::Get();
  ASSERT_TRUE(palette_manager
                  .SetColor("dungeon_main", 3, kDungeonColorIndex, edited_color)
                  .ok());
  ASSERT_FALSE(current_room.IsCompositeDirty());
  ASSERT_FALSE(cached_room.IsCompositeDirty());

  ASSERT_TRUE(palette_manager.ApplyPreviewChanges().ok());

  EXPECT_TRUE(current_room.IsCompositeDirty());
  EXPECT_TRUE(cached_room.IsCompositeDirty());
  const SDL_Color after =
      ReadSurfacePaletteColor(current_room, kDungeonDisplayIndex);
  EXPECT_NE(before.r, after.r);
  ExpectSurfaceColor(after, edited_color);
  EXPECT_EQ(
      DungeonEditorPaletteRefreshTestPeer::CurrentPalette(*editor_)[0].snes(),
      edited_color.snes());
}

TEST_F(DungeonEditorPaletteRefreshTest,
       ApplyPreviewChangesRefreshesSharedHudPaletteUsers) {
  auto& current_room = editor_->rooms()[0];
  current_room.SetPalette(5);
  auto& cached_room = editor_->rooms()[1];
  cached_room.SetPalette(6);

  RenderToCleanState(current_room);
  RenderToCleanState(cached_room);

  constexpr int kHudDisplayIndex = 17;
  const gfx::SnesColor edited_color(0x03E0);
  const SDL_Color before =
      ReadSurfacePaletteColor(current_room, kHudDisplayIndex);

  DungeonEditorPaletteRefreshTestPeer::SetCurrentPaletteId(editor_.get(), 3);
  DungeonEditorPaletteRefreshTestPeer::RegisterPaletteListener(editor_.get());

  auto& palette_manager = gfx::PaletteManager::Get();
  ASSERT_TRUE(
      palette_manager.SetColor("hud", 0, kHudDisplayIndex, edited_color).ok());
  ASSERT_FALSE(current_room.IsCompositeDirty());
  ASSERT_FALSE(cached_room.IsCompositeDirty());

  ASSERT_TRUE(palette_manager.ApplyPreviewChanges().ok());

  EXPECT_TRUE(current_room.IsCompositeDirty());
  EXPECT_TRUE(cached_room.IsCompositeDirty());
  const SDL_Color after =
      ReadSurfacePaletteColor(current_room, kHudDisplayIndex);
  EXPECT_NE(before.g, after.g);
  ExpectSurfaceColor(after, edited_color);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       SharedHudNotificationRefreshesInactiveCachedPanelGhostAndReusesTexture) {
  ScopedWorkbenchFlag standalone_workflow(/*enabled=*/false);
  constexpr int kGhostRoomId = 0;
  constexpr int kCurrentRoomId = 1;
  constexpr int kHudDisplayIndex = 17;
  auto& ghost_room = editor_->rooms()[kGhostRoomId];
  ghost_room.SetLoaded(true);
  ghost_room.SetPalette(5);  // Resolves to dungeon palette 3.
  ghost_room.SetTileObjects({});
  auto& current_room = editor_->rooms()[kCurrentRoomId];
  current_room.SetLoaded(true);
  current_room.SetPalette(7);  // Also resolves to dungeon palette 3.
  current_room.SetTileObjects({});

  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  RenderToCleanState(ghost_room);
  RenderToCleanState(current_room);
  while (gfx::Arena::Get().texture_command_queue_size() > 0) {
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
  }
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  DungeonEditorPaletteRefreshTestPeer::SetCurrentPaletteId(editor_.get(), 3);
  DungeonEditorPaletteRefreshTestPeer::SetCurrentRoomId(editor_.get(),
                                                        kCurrentRoomId);
  DungeonEditorPaletteRefreshTestPeer::SetActiveRooms(editor_.get(),
                                                      {kCurrentRoomId});
  DungeonEditorPaletteRefreshTestPeer::RegisterPaletteListener(editor_.get());
  DungeonCanvasViewer* viewer =
      DungeonEditorPaletteRefreshTestPeer::GetViewerForRoom(editor_.get(),
                                                            kGhostRoomId);
  ASSERT_NE(viewer, nullptr);
  viewer->object_interaction().SetCurrentRoom(
      DungeonEditorPaletteRefreshTestPeer::Rooms(editor_.get()), kGhostRoomId);
  DungeonCanvasViewer* current_viewer =
      DungeonEditorPaletteRefreshTestPeer::GetViewerForRoom(editor_.get(),
                                                            kCurrentRoomId);
  ASSERT_NE(current_viewer, nullptr);
  current_viewer->object_interaction().SetCurrentRoom(
      DungeonEditorPaletteRefreshTestPeer::Rooms(editor_.get()),
      kCurrentRoomId);
  auto palette_group = gfx::CreatePaletteGroupFromLargePalette(
      game_data_.palette_groups.dungeon_main.palette_ref(3));
  ASSERT_TRUE(palette_group.ok());
  viewer->SetCurrentPaletteGroup(*palette_group);

  zelda3::RoomObject preview(/*id=*/0x33, /*x=*/0, /*y=*/0, /*size=*/0x02,
                             /*layer=*/0);
  preview.mutable_tiles().assign(
      64, gfx::TileInfo(/*id=*/0, /*palette=*/0, /*v=*/false, /*h=*/false,
                        /*o=*/false));
  preview.tiles_loaded_ = true;
  gfx::Arena::Get().ClearTextureQueue();
  viewer->SetPreviewObject(preview);

  auto& tile_handler =
      viewer->object_interaction().entity_coordinator().tile_handler();
  const auto* ghost_buffer = tile_handler.ghost_preview_buffer_for_testing();
  ASSERT_NE(ghost_buffer, nullptr);
  const auto& ghost_bitmap = ghost_buffer->bitmap();
  SDL_Palette* ghost_palette =
      platform::GetSurfacePalette(ghost_bitmap.surface());
  ASSERT_NE(ghost_palette, nullptr);
  const SDL_Color before = ghost_palette->colors[kHudDisplayIndex];

  EXPECT_CALL(renderer,
              UpdateTexture(::testing::_, ::testing::Ref(ghost_bitmap)))
      .Times(1);
  gfx::Arena::Get().ProcessTextureQueue(&renderer);
  ASSERT_NE(ghost_bitmap.texture(), nullptr);
  const auto ghost_texture = ghost_bitmap.texture();
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  const gfx::SnesColor edited_color(0x03E0);
  auto& palette_manager = gfx::PaletteManager::Get();
  ASSERT_TRUE(
      palette_manager.SetColor("hud", 0, kHudDisplayIndex, edited_color).ok());
  ASSERT_TRUE(palette_manager.ApplyPreviewChanges().ok());

  SDL_Palette* refreshed_palette =
      platform::GetSurfacePalette(ghost_bitmap.surface());
  ASSERT_NE(refreshed_palette, nullptr);
  const SDL_Color after = refreshed_palette->colors[kHudDisplayIndex];
  EXPECT_NE(before.g, after.g);
  ExpectSurfaceColor(after, edited_color);

  EXPECT_CALL(renderer, CreateTexture).Times(0);
  EXPECT_CALL(renderer,
              UpdateTexture(ghost_texture, ::testing::Ref(ghost_bitmap)))
      .Times(1);
  for (int attempts = 0;
       attempts < 64 && gfx::Arena::Get().texture_command_queue_size() > 0;
       ++attempts) {
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
  }
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 0);
  EXPECT_EQ(tile_handler.ghost_preview_buffer_for_testing(), ghost_buffer);
  EXPECT_EQ(ghost_bitmap.texture(), ghost_texture);
  EXPECT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));
}

TEST_F(DungeonEditorPaletteRefreshTest,
       CachedPanelPaletteRefreshSurvivesWorkbenchModeAndPreservesRoomPalette) {
  ScopedWorkbenchFlag workflow_mode(/*enabled=*/false);
  constexpr int kCachedRoomId = 0;
  constexpr int kCurrentRoomId = 1;
  auto& cached_room = editor_->rooms()[kCachedRoomId];
  cached_room.SetLoaded(true);
  cached_room.SetPalette(6);  // Resolves to dungeon palette 2.
  cached_room.SetTileObjects({});
  auto& current_room = editor_->rooms()[kCurrentRoomId];
  current_room.SetLoaded(true);
  current_room.SetPalette(5);  // Resolves to dungeon palette 3.
  current_room.SetTileObjects({});

  DungeonEditorPaletteRefreshTestPeer::SetCurrentPaletteId(editor_.get(), 3);
  DungeonEditorPaletteRefreshTestPeer::SetCurrentRoomId(editor_.get(),
                                                        kCurrentRoomId);
  DungeonEditorPaletteRefreshTestPeer::SetActiveRooms(
      editor_.get(), {kCachedRoomId, kCurrentRoomId});
  DungeonCanvasViewer* cached_viewer =
      DungeonEditorPaletteRefreshTestPeer::GetViewerForRoom(editor_.get(),
                                                            kCachedRoomId);
  ASSERT_NE(cached_viewer, nullptr);
  cached_viewer->object_interaction().SetCurrentRoom(
      DungeonEditorPaletteRefreshTestPeer::Rooms(editor_.get()), kCachedRoomId);
  auto stale_palette_group = gfx::CreatePaletteGroupFromLargePalette(
      game_data_.palette_groups.dungeon_main.palette_ref(3));
  ASSERT_TRUE(stale_palette_group.ok());
  cached_viewer->SetCurrentPaletteId(3);
  cached_viewer->SetCurrentPaletteGroup(*stale_palette_group);

  core::FeatureFlags::get().dungeon.kUseWorkbench = true;
  DungeonEditorPaletteRefreshTestPeer::RegisterPaletteListener(editor_.get());
  auto& palette_manager = gfx::PaletteManager::Get();
  ASSERT_TRUE(
      palette_manager.SetColor("hud", 0, 17, gfx::SnesColor(0x03E0)).ok());
  ASSERT_TRUE(palette_manager.ApplyPreviewChanges().ok());

  const auto expected_palette_group = zelda3::BuildDungeonRenderPaletteGroup(
      game_data_.palette_groups.dungeon_main.palette_ref(2),
      &game_data_.palette_groups.hud.palette_ref(0));
  EXPECT_EQ(cached_viewer->current_palette_id_, 2);
  ASSERT_EQ(cached_viewer->current_palette_group_.size(),
            expected_palette_group.size());
  for (int i = 0; i < static_cast<int>(expected_palette_group.size()); ++i) {
    EXPECT_EQ(cached_viewer->current_palette_group_.palette_ref(i),
              expected_palette_group.palette_ref(i));
  }
}

TEST_F(DungeonEditorPaletteRefreshTest,
       DungeonMainNotificationSkipsUnrelatedCachedViewerWork) {
  ScopedWorkbenchFlag standalone_workflow(/*enabled=*/false);
  constexpr int kCachedRoomId = 0;
  constexpr int kCurrentRoomId = 1;
  auto& cached_room = editor_->rooms()[kCachedRoomId];
  cached_room.SetLoaded(true);
  cached_room.SetPalette(6);  // Resolves to dungeon palette 2.
  cached_room.SetTileObjects({});
  auto& current_room = editor_->rooms()[kCurrentRoomId];
  current_room.SetLoaded(true);
  current_room.SetPalette(5);  // Resolves to dungeon palette 3.
  current_room.SetTileObjects({});

  DungeonEditorPaletteRefreshTestPeer::SetCurrentPaletteId(editor_.get(), 3);
  DungeonEditorPaletteRefreshTestPeer::SetCurrentRoomId(editor_.get(),
                                                        kCurrentRoomId);
  DungeonEditorPaletteRefreshTestPeer::SetActiveRooms(editor_.get(),
                                                      {kCurrentRoomId});
  DungeonCanvasViewer* cached_viewer =
      DungeonEditorPaletteRefreshTestPeer::GetViewerForRoom(editor_.get(),
                                                            kCachedRoomId);
  ASSERT_NE(cached_viewer, nullptr);
  auto cached_palette_group = gfx::CreatePaletteGroupFromLargePalette(
      game_data_.palette_groups.dungeon_main.palette_ref(2));
  ASSERT_TRUE(cached_palette_group.ok());
  cached_viewer->SetCurrentPaletteGroup(*cached_palette_group);

  // Use the ID as a call sentinel while keeping the viewer's palette group
  // valid. An unrelated concrete-palette edit must skip this cached viewer.
  constexpr uint64_t kUntouchedSentinel = 0xFFFF;
  cached_viewer->current_palette_id_ = kUntouchedSentinel;
  DungeonEditorPaletteRefreshTestPeer::HandlePaletteChanged(
      editor_.get(), {3, gui::DungeonRenderPaletteSource::kDungeonMain});

  EXPECT_EQ(cached_viewer->current_palette_id_, kUntouchedSentinel);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       PaletteNotificationFromInactiveSessionIsIgnored) {
  auto& room = editor_->rooms()[0];
  room.SetPalette(5);
  RenderToCleanState(room);

  DungeonEditorPaletteRefreshTestPeer::RegisterPaletteListener(editor_.get());

  Rom other_rom;
  ASSERT_TRUE(other_rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData other_game_data(&other_rom);
  SeedPaletteGroup(&other_game_data.palette_groups.dungeon_main,
                   /*palette_count=*/1, /*color_count=*/90,
                   /*seed=*/0x0300);

  auto& palette_manager = gfx::PaletteManager::Get();
  palette_manager.Initialize(&other_game_data);
  ASSERT_TRUE(
      palette_manager.SetColor("dungeon_main", 0, 0, gfx::SnesColor(0x001F))
          .ok());
  ASSERT_TRUE(palette_manager.ApplyPreviewChanges().ok());

  EXPECT_FALSE(room.IsCompositeDirty());
}

TEST_F(DungeonEditorPaletteRefreshTest,
       CachedRoomRefreshesThroughViewerCompositePreparation) {
  constexpr int kCurrentRoomId = 0;
  constexpr int kCachedRoomId = 1;
  constexpr int kHudDisplayIndex = 17;
  editor_->current_room_id_ = kCurrentRoomId;

  auto& cached_room = editor_->rooms_[kCachedRoomId];
  cached_room.SetPalette(6);  // Resolves away from selected palette 3.
  cached_room.SetTileObjects({});

  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  {
    DungeonCanvasViewer viewer(&rom_);
    viewer.SetGameData(&game_data_);
    viewer.SetRenderer(&renderer);
    viewer.SetRooms(&editor_->rooms_);

    EXPECT_CALL(renderer, CreateTexture).Times(::testing::AtLeast(1));
    EXPECT_CALL(renderer, UpdateTexture).Times(::testing::AtLeast(1));
    gfx::Bitmap* first_composite =
        viewer.PrepareRoomCompositeBitmap(kCachedRoomId);
    ASSERT_NE(first_composite, nullptr);
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
    ASSERT_NE(first_composite->texture(), nullptr);
    ASSERT_FALSE(cached_room.IsCompositeDirty());
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

    const SDL_Color before =
        ReadSurfacePaletteColor(cached_room, kHudDisplayIndex);
    const gfx::SnesColor edited_color(0x03E0);

    editor_->palette_editor_.Initialize(&game_data_);
    editor_->palette_editor_.SetDungeonRenderPaletteMode(true);
    editor_->palette_editor_.SetCurrentPaletteId(3);
    editor_->palette_editor_.SetOnDungeonPaletteChanged(
        [this](gui::DungeonPaletteChange change) {
          editor_->InvalidateDungeonPaletteUsers(change);
        });

    ASSERT_TRUE(editor_->palette_editor_
                    .ApplyDungeonRenderColorEdit(kHudDisplayIndex, edited_color)
                    .ok());
    ASSERT_TRUE(cached_room.IsCompositeDirty());

    // Exercise the connected/compare composite preparation path. The test
    // must not call Room::RenderRoomGraphics directly after the edit.
    EXPECT_CALL(renderer, UpdateTexture).Times(::testing::AtLeast(1));
    gfx::Bitmap* refreshed_composite =
        viewer.PrepareRoomCompositeBitmap(kCachedRoomId);
    ASSERT_EQ(refreshed_composite, first_composite);
    gfx::Arena::Get().ProcessTextureQueue(&renderer);

    const SDL_Color after =
        ReadSurfacePaletteColor(cached_room, kHudDisplayIndex);
    EXPECT_NE(before.g, after.g);
    ExpectSurfaceColor(after, edited_color);
    EXPECT_FALSE(cached_room.IsCompositeDirty());
    EXPECT_NE(refreshed_composite->texture(), nullptr);
  }

  EXPECT_CALL(renderer, DestroyTexture).Times(1);
  EXPECT_EQ(gfx::Arena::Get().DrainRetiredBitmaps(&renderer), 1u);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       CachedCompositePreparationPreservesTargetEntranceContext) {
  constexpr int kCachedRoomId = 1;
  constexpr uint8_t kTargetEntranceBlockset = 1;
  constexpr uint8_t kViewerEntranceBlockset = 2;
  constexpr uint8_t kTargetSheet = 9;
  constexpr uint8_t kViewerSheet = 10;
  game_data_.main_blockset_ids[kTargetEntranceBlockset][0] = kTargetSheet;
  game_data_.main_blockset_ids[kViewerEntranceBlockset][0] = kViewerSheet;

  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& cached_room = rooms[kCachedRoomId];
  cached_room.SetTileObjects({});
  cached_room.SetRenderEntranceBlockset(kTargetEntranceBlockset);
  cached_room.PrepareForRender();
  ASSERT_EQ(cached_room.render_entrance_blockset(), kTargetEntranceBlockset);
  ASSERT_EQ(cached_room.blocks()[0], kTargetSheet);
  cached_room.MarkGraphicsDirty();

  DungeonCanvasViewer viewer(&rom_);
  viewer.SetGameData(&game_data_);
  viewer.SetRooms(&rooms);
  viewer.SetEntranceRenderContext(/*entrance_id=*/0x12,
                                  kViewerEntranceBlockset);

  ASSERT_NE(viewer.PrepareRoomCompositeBitmap(kCachedRoomId), nullptr);
  EXPECT_EQ(cached_room.render_entrance_blockset(), kTargetEntranceBlockset);
  EXPECT_EQ(cached_room.blocks()[0], kTargetSheet);
  EXPECT_NE(cached_room.blocks()[0], kViewerSheet);
  gfx::Arena::Get().ClearTextureQueue();
}

TEST_F(DungeonEditorPaletteRefreshTest,
       CompareViewerScopesEntranceContextToRequestedRoom) {
  constexpr int kCompareRoomId = 1;
  constexpr int kUnrelatedRoomId = 2;
  constexpr uint8_t kEntranceBlockset = 3;
  editor_->current_room_id_ = 0;
  editor_->current_entrance_id_ = 0;
  auto spawn_or = zelda3::DungeonSpawnPoint::Load(rom_, 0);
  ASSERT_TRUE(spawn_or.ok()) << spawn_or.status();
  editor_->spawn_points_[0] = *spawn_or;
  editor_->spawn_points_[0].room_id = kCompareRoomId;
  editor_->spawn_points_[0].main_gfx = kEntranceBlockset;

  DungeonCanvasViewer* compare_viewer =
      editor_->GetWorkbenchCompareViewer(kCompareRoomId);
  ASSERT_NE(compare_viewer, nullptr);
  EXPECT_EQ(compare_viewer->current_entrance_id(), 0);
  EXPECT_EQ(compare_viewer->current_entrance_blockset(), kEntranceBlockset);

  compare_viewer = editor_->GetWorkbenchCompareViewer(kUnrelatedRoomId);
  ASSERT_NE(compare_viewer, nullptr);
  EXPECT_EQ(compare_viewer->current_entrance_id(), -1);
  EXPECT_EQ(compare_viewer->current_entrance_blockset(), 0xFF);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       LegacyPaletteIdCallbackSupportsGenericLambdaAndNullReset) {
  gui::PaletteEditorWidget widget;
  widget.Initialize(&game_data_);
  widget.SetDungeonRenderPaletteMode(true);
  widget.SetCurrentPaletteId(3);

  int notified_palette_id = -1;
  int callback_count = 0;
  widget.SetOnPaletteChanged([&](auto palette_id) {
    notified_palette_id = palette_id;
    ++callback_count;
  });

  ASSERT_TRUE(widget
                  .ApplyDungeonRenderColorEdit(
                      /*display_index=*/33, gfx::SnesColor(0x4210))
                  .ok());
  EXPECT_EQ(notified_palette_id, 3);
  EXPECT_EQ(callback_count, 1);

  widget.SetOnPaletteChanged(nullptr);
  ASSERT_TRUE(widget
                  .ApplyDungeonRenderColorEdit(
                      /*display_index=*/34, gfx::SnesColor(0x5294))
                  .ok());
  EXPECT_EQ(callback_count, 1);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       LayerBlendOverridesSurviveFirstAndRepeatedCanvasPreparation) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  DungeonCanvasViewer viewer(&rom_);
  viewer.SetGameData(&game_data_);
  viewer.SetRooms(&rooms);

  // Workbench controls can run before the canvas's first draw.
  auto& manager = viewer.GetRoomLayerManager(0);
  manager.SetLayerBlendMode(zelda3::LayerType::BG2_Objects,
                            zelda3::LayerBlendMode::Off);
  manager.SetLayerBlendMode(zelda3::LayerType::BG1_Layout,
                            zelda3::LayerBlendMode::Dark);
  const auto signature = manager.CompositeStateSignature();
  for (int frame = 0; frame < 3; ++frame) {
    ASSERT_NE(PrepareComposite(viewer, 0), nullptr);
    EXPECT_EQ(manager.GetLayerBlendMode(zelda3::LayerType::BG2_Objects),
              zelda3::LayerBlendMode::Off);
    EXPECT_EQ(manager.GetLayerBlendMode(zelda3::LayerType::BG1_Layout),
              zelda3::LayerBlendMode::Dark);
    EXPECT_EQ(manager.CompositeStateSignature(), signature);
  }
}

TEST_F(DungeonEditorPaletteRefreshTest,
       LayerBlendDefaultsFollowHeaderChangesWithoutRestoringHiddenLayers) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  DungeonCanvasViewer viewer(&rom_);
  viewer.SetGameData(&game_data_);
  viewer.SetRooms(&rooms);
  ASSERT_NE(PrepareComposite(viewer, 0), nullptr);
  auto& manager = viewer.GetRoomLayerManager(0);
  manager.SetLayerVisible(zelda3::LayerType::BG2_Objects, false);
  manager.SetLayerBlendMode(zelda3::LayerType::BG1_Objects,
                            zelda3::LayerBlendMode::Off);

  room.SetEffect(zelda3::EffectKey::Moving_Water);
  ASSERT_NE(PrepareComposite(viewer, 0), nullptr);
  EXPECT_EQ(manager.GetLayerBlendMode(zelda3::LayerType::BG2_Layout),
            zelda3::LayerBlendMode::Translucent);
  EXPECT_EQ(manager.GetLayerBlendMode(zelda3::LayerType::BG1_Objects),
            zelda3::LayerBlendMode::Normal);
  EXPECT_FALSE(manager.IsLayerVisible(zelda3::LayerType::BG2_Objects));

  room.SetEffect(zelda3::EffectKey::Effect_Nothing);
  ASSERT_NE(PrepareComposite(viewer, 0), nullptr);
  EXPECT_EQ(manager.GetLayerBlendMode(zelda3::LayerType::BG2_Layout),
            zelda3::LayerBlendMode::Normal);
  room.SetLayerMerging(zelda3::LayerMerge08);
  ASSERT_NE(PrepareComposite(viewer, 0), nullptr);
  EXPECT_EQ(manager.GetLayerBlendMode(zelda3::LayerType::BG1_Layout),
            zelda3::LayerBlendMode::Dark);
  EXPECT_FALSE(manager.IsLayerVisible(zelda3::LayerType::BG2_Objects));
}

TEST_F(DungeonEditorPaletteRefreshTest,
       LayerBlendOverridesStayRoomLocalAndResetOnRomRefresh) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  for (int room_id : {0, 1}) {
    rooms[room_id].SetTileObjects({});
    rooms[room_id].SetLayerMerging(zelda3::LayerMerge01);
  }
  DungeonCanvasViewer viewer(&rom_);
  viewer.SetGameData(&game_data_);
  viewer.SetRooms(&rooms);
  ASSERT_NE(PrepareComposite(viewer, 0), nullptr);
  viewer.SetLayerBlendMode(0, zelda3::LayerType::BG2_Objects,
                           zelda3::LayerBlendMode::Off);
  ASSERT_NE(PrepareComposite(viewer, 1), nullptr);
  EXPECT_EQ(viewer.GetLayerBlendMode(1, zelda3::LayerType::BG2_Objects),
            zelda3::LayerBlendMode::Normal);
  ASSERT_NE(PrepareComposite(viewer, 0), nullptr);
  EXPECT_EQ(viewer.GetLayerBlendMode(0, zelda3::LayerType::BG2_Objects),
            zelda3::LayerBlendMode::Off);

  viewer.RefreshRomBackedState(&rom_, &game_data_, &rooms, 0);
  ASSERT_NE(PrepareComposite(viewer, 0), nullptr);
  EXPECT_EQ(viewer.GetLayerBlendMode(0, zelda3::LayerType::BG2_Objects),
            zelda3::LayerBlendMode::Normal);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       CanvasAndCanonicalPreviewKeepIndependentTextureIdentity) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(room, /*bg1_pixel=*/11, /*bg2_pixel=*/22);

  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  int canvas_texture_storage = 0;
  int preview_texture_storage = 0;
  EXPECT_CALL(renderer, CreateTexture)
      .WillOnce(::testing::Return(
          static_cast<gfx::TextureHandle>(&canvas_texture_storage)))
      .WillOnce(::testing::Return(
          static_cast<gfx::TextureHandle>(&preview_texture_storage)));
  EXPECT_CALL(renderer, UpdateTexture).Times(2);

  RoomCompositeOutput canonical_output;
  {
    DungeonCanvasViewer canvas_viewer(&rom_);
    canvas_viewer.SetGameData(&game_data_);
    canvas_viewer.SetRenderer(&renderer);
    canvas_viewer.SetRooms(&rooms);
    ConfigureCompositeLayers(canvas_viewer, 0, /*show_bg1=*/false);

    gfx::Bitmap* canvas_composite = PrepareComposite(canvas_viewer, 0);
    ASSERT_NE(canvas_composite, nullptr);
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
    ASSERT_EQ(canvas_composite->data()[0], 22);
    ASSERT_EQ(canvas_composite->texture(),
              static_cast<gfx::TextureHandle>(&canvas_texture_storage));
    const std::vector<uint8_t> retained_canvas_pixels =
        canvas_composite->vector();

    gfx::Bitmap& preview_composite =
        PrepareCanonicalRoomComposite(room, canonical_output);
    gfx::EnsureCompositeBitmapTextureQueued(preview_composite);
    gfx::Arena::Get().ProcessTextureQueue(&renderer);

    EXPECT_NE(canvas_composite, &preview_composite);
    EXPECT_NE(canvas_composite->texture(), preview_composite.texture());
    EXPECT_EQ(preview_composite.data()[0], 11);
    EXPECT_EQ(canvas_composite->vector(), retained_canvas_pixels);
    EXPECT_EQ(canvas_composite->texture(),
              static_cast<gfx::TextureHandle>(&canvas_texture_storage));
  }

  canonical_output.Retire();
  EXPECT_CALL(renderer, DestroyTexture).Times(2);
  EXPECT_EQ(gfx::Arena::Get().DrainRetiredBitmaps(&renderer), 2u);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       EveryViewerCompositeRefreshesAfterRoomContentChanges) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(room, /*bg1_pixel=*/11, /*bg2_pixel=*/22);

  DungeonCanvasViewer canvas_viewer(&rom_);
  canvas_viewer.SetGameData(&game_data_);
  canvas_viewer.SetRooms(&rooms);
  ConfigureCompositeLayers(canvas_viewer, 0, /*show_bg1=*/false);

  DungeonCanvasViewer preview_viewer(&rom_);
  preview_viewer.SetGameData(&game_data_);
  preview_viewer.SetRooms(&rooms);
  ConfigureCompositeLayers(preview_viewer, 0, /*show_bg1=*/true);

  gfx::Bitmap* canvas_composite = PrepareComposite(canvas_viewer, 0);
  gfx::Bitmap* preview_composite = PrepareComposite(preview_viewer, 0);
  ASSERT_NE(canvas_composite, nullptr);
  ASSERT_NE(preview_composite, nullptr);
  ASSERT_EQ(canvas_composite->data()[0], 22);
  ASSERT_EQ(preview_composite->data()[0], 11);

  SeedCompositeLayers(room, /*bg1_pixel=*/33, /*bg2_pixel=*/44);
  ASSERT_EQ(PrepareComposite(canvas_viewer, 0)->data()[0], 44);
  EXPECT_EQ(preview_composite->data()[0], 11);
  EXPECT_EQ(PrepareComposite(preview_viewer, 0)->data()[0], 33);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       AuxiliaryPreviewDoesNotReplaceCanvasDebuggerBitmap) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& canvas_room = rooms[0];
  canvas_room.SetPalette(5);  // Resolves to dungeon palette 3.
  canvas_room.SetTileObjects({});
  canvas_room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(canvas_room, /*bg1_pixel=*/33, /*bg2_pixel=*/34);

  DungeonCanvasViewer viewer(&rom_);
  viewer.SetGameData(&game_data_);
  viewer.SetRooms(&rooms);
  ConfigureCompositeLayers(viewer, 0, /*show_bg1=*/true);
  gfx::Bitmap* canvas_composite = PrepareComposite(viewer, 0);
  ASSERT_NE(canvas_composite, nullptr);
  ASSERT_EQ(canvas_composite->data()[0], 33);
  const auto before = zelda3::PaletteDebugger::Get().SamplePixelAt(0, 0);
  ASSERT_EQ(before.palette_index, 33);
  ASSERT_TRUE(before.matches);

  auto& preview_room = rooms[1];
  preview_room.SetPalette(6);  // Resolves to dungeon palette 2.
  preview_room.SetTileObjects({});
  preview_room.MarkGraphicsDirty();
  RoomCompositeOutput preview_output;
  ASSERT_TRUE(
      PrepareCanonicalRoomComposite(preview_room, preview_output).is_active());

  const auto after = zelda3::PaletteDebugger::Get().SamplePixelAt(0, 0);
  EXPECT_EQ(after.palette_index, 33);
  EXPECT_EQ(after.expected_r, before.expected_r);
  EXPECT_EQ(after.expected_g, before.expected_g);
  EXPECT_EQ(after.expected_b, before.expected_b);
  EXPECT_EQ(after.actual_r, before.actual_r);
  EXPECT_EQ(after.actual_g, before.actual_g);
  EXPECT_EQ(after.actual_b, before.actual_b);
  EXPECT_TRUE(after.matches);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       DrawIssueReportRebindsDebuggerAfterCompareViewer) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& primary_room = rooms[0];
  primary_room.SetPalette(5);  // Resolves to dungeon palette 3.
  primary_room.SetTileObjects({});
  primary_room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(primary_room, /*bg1_pixel=*/33, /*bg2_pixel=*/34);

  auto& compare_room = rooms[1];
  compare_room.SetPalette(6);  // Resolves to dungeon palette 2.
  compare_room.SetTileObjects({});
  compare_room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(compare_room, /*bg1_pixel=*/44, /*bg2_pixel=*/45);

  DungeonCanvasViewer primary_viewer(&rom_);
  primary_viewer.SetGameData(&game_data_);
  primary_viewer.SetRooms(&rooms);
  ConfigureCompositeLayers(primary_viewer, 0, /*show_bg1=*/true);

  DungeonCanvasViewer compare_viewer(&rom_);
  compare_viewer.SetGameData(&game_data_);
  compare_viewer.SetRooms(&rooms);
  ConfigureCompositeLayers(compare_viewer, 1, /*show_bg1=*/true);

  gfx::Bitmap* primary_composite = PrepareComposite(primary_viewer, 0);
  ASSERT_NE(primary_composite, nullptr);
  const auto primary_sample =
      zelda3::PaletteDebugger::Get().SamplePixelAt(0, 0);
  ASSERT_EQ(primary_sample.palette_index, 33);
  ASSERT_TRUE(primary_sample.matches);

  ASSERT_NE(PrepareComposite(compare_viewer, 1), nullptr);
  const auto compare_sample =
      zelda3::PaletteDebugger::Get().SamplePixelAt(0, 0);
  ASSERT_EQ(compare_sample.palette_index, 44);
  ASSERT_TRUE(compare_sample.matches);

  // Capture Issue executes before the normal room draw pass. The report must
  // rebind the cached primary presentation after the compare viewer published
  // last during the previous frame.
  const std::string report =
      DungeonEditorPaletteRefreshTestPeer::BuildDrawIssueReport(
          primary_viewer, primary_room, 0);
  EXPECT_NE(report.find("Dungeon Draw Issue Report"), std::string::npos);
  const auto rebound_sample =
      zelda3::PaletteDebugger::Get().SamplePixelAt(0, 0);
  EXPECT_EQ(rebound_sample.palette_index, 33);
  EXPECT_EQ(rebound_sample.expected_r, primary_sample.expected_r);
  EXPECT_EQ(rebound_sample.expected_g, primary_sample.expected_g);
  EXPECT_EQ(rebound_sample.expected_b, primary_sample.expected_b);
  EXPECT_EQ(rebound_sample.actual_r, primary_sample.actual_r);
  EXPECT_EQ(rebound_sample.actual_g, primary_sample.actual_g);
  EXPECT_EQ(rebound_sample.actual_b, primary_sample.actual_b);
  EXPECT_TRUE(rebound_sample.matches);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       RoomCompositeRefreshesAfterViewerConsumesDirtyState) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(room, /*bg1_pixel=*/11, /*bg2_pixel=*/22);
  zelda3::RoomLayerManager canonical_layers;
  canonical_layers.ApplyLayerMerging(room.layer_merging());
  canonical_layers.ApplyRoomEffect(room.effect());
  ASSERT_EQ(room.GetCompositeBitmap(canonical_layers).data()[0], 11);

  DungeonCanvasViewer viewer(&rom_);
  viewer.SetGameData(&game_data_);
  viewer.SetRooms(&rooms);
  ConfigureCompositeLayers(viewer, 0, /*show_bg1=*/false);

  SeedCompositeLayers(room, /*bg1_pixel=*/33, /*bg2_pixel=*/44);
  ASSERT_EQ(PrepareComposite(viewer, 0)->data()[0], 44);
  ASSERT_FALSE(room.IsCompositeDirty());

  // Room's compatibility cache has the same layer signature it used before
  // the edit. Its own source stamp, not the shared dirty bit, must refresh it.
  EXPECT_EQ(room.GetCompositeBitmap(canonical_layers).data()[0], 33);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       ViewerCompositeRefreshesWhenLayerPresentationChanges) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(room, /*bg1_pixel=*/11, /*bg2_pixel=*/22);

  DungeonCanvasViewer viewer(&rom_);
  viewer.SetGameData(&game_data_);
  viewer.SetRooms(&rooms);
  ConfigureCompositeLayers(viewer, 0, /*show_bg1=*/true);

  gfx::Bitmap* composite = PrepareComposite(viewer, 0);
  ASSERT_NE(composite, nullptr);
  ASSERT_EQ(composite->data()[0], 11);
  const uint64_t source_revision = room.composite_source_revision();

  ConfigureCompositeLayers(viewer, 0, /*show_bg1=*/false);
  EXPECT_EQ(PrepareComposite(viewer, 0), composite);
  EXPECT_EQ(composite->data()[0], 22);
  EXPECT_EQ(room.composite_source_revision(), source_revision);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       ViewerCompositeRefreshesAfterSameAddressRoomReplacement) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(room, /*bg1_pixel=*/11, /*bg2_pixel=*/22);

  DungeonCanvasViewer viewer(&rom_);
  viewer.SetGameData(&game_data_);
  viewer.SetRooms(&rooms);
  ConfigureCompositeLayers(viewer, 0, /*show_bg1=*/true);

  zelda3::Room* room_address = &room;
  gfx::Bitmap* composite = PrepareComposite(viewer, 0);
  ASSERT_NE(composite, nullptr);
  ASSERT_EQ(composite->data()[0], 11);
  const uint64_t source_revision = room.composite_source_revision();

  zelda3::Room replacement = zelda3::LoadRoomFromRom(&rom_, 0);
  replacement.SetGameData(&game_data_);
  replacement.SetTileObjects({});
  replacement.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(replacement, /*bg1_pixel=*/33, /*bg2_pixel=*/44);
  rooms[0] = std::move(replacement);

  EXPECT_EQ(&rooms[0], room_address);
  EXPECT_NE(rooms[0].composite_source_revision(), source_revision);
  EXPECT_EQ(PrepareComposite(viewer, 0), composite);
  EXPECT_EQ(composite->data()[0], 33);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       ViewerPrimaryCompositeAddressSurvivesRefreshReset) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(room, /*bg1_pixel=*/11, /*bg2_pixel=*/22);

  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  int old_texture_storage = 0;
  int refreshed_texture_storage = 0;
  const auto old_texture =
      static_cast<gfx::TextureHandle>(&old_texture_storage);
  const auto refreshed_texture =
      static_cast<gfx::TextureHandle>(&refreshed_texture_storage);
  EXPECT_CALL(renderer, CreateTexture)
      .WillOnce(::testing::Return(old_texture))
      .WillOnce(::testing::Return(refreshed_texture));
  EXPECT_CALL(renderer, UpdateTexture).Times(2);

  {
    DungeonCanvasViewer viewer(&rom_);
    viewer.SetGameData(&game_data_);
    viewer.SetRenderer(&renderer);
    viewer.SetRooms(&rooms);
    ConfigureCompositeLayers(viewer, 0, /*show_bg1=*/true);

    gfx::Bitmap* retained_canvas_pointer = PrepareComposite(viewer, 0);
    ASSERT_NE(retained_canvas_pointer, nullptr);
    ASSERT_NE(retained_canvas_pointer->surface(), nullptr);
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
    ASSERT_EQ(retained_canvas_pointer->texture(), old_texture);

    retained_canvas_pointer->set_modified(true);
    gfx::EnsureCompositeBitmapTextureQueued(*retained_canvas_pointer);
    ASSERT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);

    DungeonEditorPaletteRefreshTestPeer::ResetCompositeOutputs(viewer);
    EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 0u);
    EXPECT_EQ(
        DungeonEditorPaletteRefreshTestPeer::PrimaryCompositeAddress(viewer),
        retained_canvas_pointer);
    EXPECT_EQ(retained_canvas_pointer->surface(), nullptr);
    EXPECT_EQ(retained_canvas_pointer->texture(), nullptr);
    EXPECT_EQ(gfx::Arena::Get().retired_texture_handle_count(), 1u);

    // Recreate the surface and live texture on the same Bitmap shell before
    // the renderer is allowed to destroy the previous frame's texture.
    gfx::Bitmap* refreshed = PrepareComposite(viewer, 0);
    EXPECT_EQ(refreshed, retained_canvas_pointer);
    ASSERT_NE(refreshed->surface(), nullptr);
    EXPECT_EQ(refreshed->data()[0], 11);
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
    EXPECT_EQ(refreshed->texture(), refreshed_texture);
    EXPECT_EQ(gfx::Arena::Get().retired_texture_handle_count(), 1u);

    EXPECT_CALL(renderer, DestroyTexture(old_texture)).Times(1);
    EXPECT_EQ(gfx::Arena::Get().DrainRetiredBitmaps(&renderer), 1u);
    EXPECT_EQ(refreshed->texture(), refreshed_texture);
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));
  }

  EXPECT_CALL(renderer, DestroyTexture(refreshed_texture)).Times(1);
  EXPECT_EQ(gfx::Arena::Get().DrainRetiredBitmaps(&renderer), 1u);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       ConnectedRoomCompositePruningKeepsOneFrameGrace) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  for (int room_id : {0, 1}) {
    rooms[room_id].SetTileObjects({});
    rooms[room_id].SetLayerMerging(zelda3::LayerMerge01);
  }
  SeedCompositeLayers(rooms[0], /*bg1_pixel=*/11, /*bg2_pixel=*/22);
  SeedCompositeLayers(rooms[1], /*bg1_pixel=*/33, /*bg2_pixel=*/44);

  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  int room_zero_texture_storage = 0;
  int room_one_texture_storage = 0;
  EXPECT_CALL(renderer, CreateTexture)
      .WillOnce(::testing::Return(
          static_cast<gfx::TextureHandle>(&room_zero_texture_storage)))
      .WillOnce(::testing::Return(
          static_cast<gfx::TextureHandle>(&room_one_texture_storage)));
  EXPECT_CALL(renderer, UpdateTexture).Times(2);

  ScopedImGuiTestContext imgui;
  imgui.NextFrame();
  {
    DungeonCanvasViewer viewer(&rom_);
    viewer.SetGameData(&game_data_);
    viewer.SetRenderer(&renderer);
    viewer.SetRooms(&rooms);
    ConfigureCompositeLayers(viewer, 0, /*show_bg1=*/true);
    ConfigureCompositeLayers(viewer, 1, /*show_bg1=*/true);

    gfx::Bitmap* room_zero =
        DungeonEditorPaletteRefreshTestPeer::PrepareConnectedComposite(viewer,
                                                                       0);
    gfx::Bitmap* room_one =
        DungeonEditorPaletteRefreshTestPeer::PrepareConnectedComposite(viewer,
                                                                       1);
    ASSERT_NE(room_zero, nullptr);
    ASSERT_NE(room_one, nullptr);
    EXPECT_NE(room_zero, room_one);
    EXPECT_EQ(room_zero->data()[0], 11);
    EXPECT_EQ(room_one->data()[0], 33);
    EXPECT_EQ(DungeonEditorPaletteRefreshTestPeer::PrepareConnectedComposite(
                  viewer, 0),
              room_zero);
    EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 2u)
        << "Repeated preparation must not duplicate a pending texture create";
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
    EXPECT_NE(room_zero->texture(), room_one->texture());
    EXPECT_EQ(
        DungeonEditorPaletteRefreshTestPeer::ConnectedCompositeCount(viewer),
        2u);

    imgui.NextFrame();
    DungeonEditorPaletteRefreshTestPeer::PruneConnectedComposites(viewer);
    EXPECT_EQ(DungeonEditorPaletteRefreshTestPeer::PrepareConnectedComposite(
                  viewer, 1),
              room_one);
    EXPECT_EQ(
        DungeonEditorPaletteRefreshTestPeer::ConnectedCompositeCount(viewer),
        2u);

    imgui.NextFrame();
    DungeonEditorPaletteRefreshTestPeer::PruneConnectedComposites(viewer);
    EXPECT_FALSE(
        DungeonEditorPaletteRefreshTestPeer::HasConnectedComposite(viewer, 0));
    EXPECT_TRUE(
        DungeonEditorPaletteRefreshTestPeer::HasConnectedComposite(viewer, 1));
    EXPECT_EQ(gfx::Arena::Get().retired_texture_handle_count(), 1u);
  }

  EXPECT_CALL(renderer, DestroyTexture).Times(2);
  EXPECT_EQ(gfx::Arena::Get().DrainRetiredBitmaps(&renderer), 2u);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       CompositeOutputRetirementCancelsQueuedTextureWork) {
  gfx::Arena& arena = gfx::Arena::Get();
  const size_t active_surfaces_before = ActiveSurfaceCount(arena);
  int texture_storage = 0;
  const auto texture = static_cast<gfx::TextureHandle>(&texture_storage);

  {
    RoomCompositeOutput output;
    output.bitmap().Create(8, 8, 8, std::vector<uint8_t>(64, 7));
    output.bitmap().set_texture(texture);
    arena.QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                              &output.bitmap());
    ASSERT_EQ(arena.texture_command_queue_size(), 1u);
    ASSERT_EQ(ActiveSurfaceCount(arena), active_surfaces_before + 1);
  }

  EXPECT_EQ(arena.texture_command_queue_size(), 0u);
  EXPECT_EQ(ActiveSurfaceCount(arena), active_surfaces_before);
  EXPECT_EQ(arena.retired_texture_handle_count(), 1u);

  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  EXPECT_CALL(renderer, DestroyTexture(texture)).Times(1);
  EXPECT_EQ(arena.DrainRetiredBitmaps(&renderer), 1u);
  EXPECT_EQ(arena.retired_texture_handle_count(), 0u);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       CanonicalCompositeAppliesRoomEffectAfterLayerMerge) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  room.SetEffect(zelda3::EffectKey::Moving_Water);
  SeedCompositeLayers(room, /*bg1_pixel=*/33, /*bg2_pixel=*/34);

  std::vector<SDL_Color> palette(256, {0, 0, 0, 255});
  palette[33] = {66, 0, 0, 255};
  palette[34] = {33, 0, 0, 255};
  palette[36] = {49, 0, 0, 255};
  for (auto* buffer : {&room.bg1_buffer(), &room.bg2_buffer(),
                       &room.object_bg1_buffer(), &room.object_bg2_buffer()}) {
    buffer->bitmap().SetPalette(palette);
  }
  room.MarkCompositeDirty();

  RoomCompositeOutput canonical_output;
  // BGACT 1 puts the lower tilemap on the sub screen with CGADSUB $20
  // (backdrop only), and the moving-water effect only scrolls
  // (Underworld_HandleLayerEffect), so the game does not blend an opaque
  // upper pixel: it shows 33, not the half-add 36 yaze used to approximate.
  EXPECT_EQ(PrepareCanonicalRoomComposite(room, canonical_output).data()[0],
            33);
}

TEST_F(DungeonEditorPaletteRefreshTest,
       CompositeTextureQueueCreatesOnceAndUpdatesInPlace) {
  DungeonRoomStore rooms(&rom_, &game_data_);
  auto& room = rooms[0];
  room.SetTileObjects({});
  room.SetLayerMerging(zelda3::LayerMerge01);
  SeedCompositeLayers(room, /*bg1_pixel=*/11, /*bg2_pixel=*/22);

  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  int texture_storage = 0;
  const auto texture = static_cast<gfx::TextureHandle>(&texture_storage);
  EXPECT_CALL(renderer, CreateTexture).WillOnce(::testing::Return(texture));
  EXPECT_CALL(renderer, UpdateTexture).Times(2);

  {
    RoomCompositeOutput canonical_output;
    gfx::Bitmap& composite =
        PrepareCanonicalRoomComposite(room, canonical_output);
    gfx::EnsureCompositeBitmapTextureQueued(composite);
    ASSERT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
    ASSERT_EQ(composite.texture(), texture);
    EXPECT_EQ(composite.metadata().purpose,
              gfx::Bitmap::BitmapPurpose::kCompositeOutput);

    EXPECT_EQ(&PrepareCanonicalRoomComposite(room, canonical_output),
              &composite);
    gfx::EnsureCompositeBitmapTextureQueued(composite);
    EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 0u);

    SeedCompositeLayers(room, /*bg1_pixel=*/33, /*bg2_pixel=*/44);
    EXPECT_EQ(&PrepareCanonicalRoomComposite(room, canonical_output),
              &composite);
    gfx::EnsureCompositeBitmapTextureQueued(composite);
    ASSERT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
    EXPECT_EQ(composite.texture(), texture);
    EXPECT_EQ(composite.data()[0], 33);
  }

  EXPECT_CALL(renderer, DestroyTexture(texture)).Times(1);
  EXPECT_EQ(gfx::Arena::Get().DrainRetiredBitmaps(&renderer), 1u);
}

TEST(DungeonPaletteResponsiveLayoutTest, UsesLogicalRowWidthsWhenTheyFit) {
  EXPECT_EQ(gui::ResolveDungeonRenderPaletteColumns(
                /*available_width=*/254.0f, /*min_swatch_size=*/14.0f,
                /*item_spacing=*/2.0f),
            16);
  EXPECT_EQ(gui::ResolveDungeonRenderPaletteColumns(
                /*available_width=*/126.0f, /*min_swatch_size=*/14.0f,
                /*item_spacing=*/2.0f),
            8);
}

TEST(DungeonPaletteResponsiveLayoutTest,
     WrapsWithoutChangingLogicalPaletteIndices) {
  EXPECT_EQ(gui::ResolveDungeonRenderPaletteColumns(
                /*available_width=*/253.0f, /*min_swatch_size=*/14.0f,
                /*item_spacing=*/2.0f),
            8);
  EXPECT_EQ(gui::ResolveDungeonRenderPaletteColumns(
                /*available_width=*/125.0f, /*min_swatch_size=*/14.0f,
                /*item_spacing=*/2.0f),
            4);
  EXPECT_EQ(gui::ResolveDungeonRenderPaletteColumns(
                /*available_width=*/1.0f, /*min_swatch_size=*/14.0f,
                /*item_spacing=*/2.0f),
            1);
}

}  // namespace yaze::editor
