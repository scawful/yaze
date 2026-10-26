#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/application.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/ui/window/overlay_manager_panel.h"
#include "app/editor/editor_manager.h"
#include "app/editor/events/core_events.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/graphics_palette_preview.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/graphics/sheet_png_transfer.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/palette/palette_editor.h"
#include "app/editor/session_types.h"
#include "app/editor/system/session/rom_lifecycle_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/palette_manager.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/core/color.h"
#include "app/platform/sdl_compat.h"
#include "core/features.h"
#include "core/native_save_receipt.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "framework/mock_renderer.h"
#include "framework/rom_save_fault.h"
#include "gmock/gmock.h"
#include "imgui/imgui_internal.h"
#include "rom/snes.h"
#include "testing.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "util/indexed_png.h"
#include "util/json.h"
#include "util/platform_paths.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/gfx_sheet_png.h"
#include "zelda3/gfx_sheet_storage.h"
#include "zelda3/graphics_sheet_store.h"

#include "imgui/imgui.h"

namespace yaze::editor {

class GraphicsEditorSaveStoplossTestPeer {
 public:
  // Tests place a session's sheet in the shared Arena by hand, so claim the
  // Arena for that session the way a real ROM load does.
  static void MarkSheetModified(GraphicsEditor* editor, uint16_t sheet_id) {
    gfx::Arena::Get().set_gfx_sheets_owner(editor->game_data());
    editor->state_.MarkSheetModified(sheet_id);
  }
  static GraphicsEditorState& State(GraphicsEditor* editor) {
    return editor->state_;
  }
  // Lazy-mode tests put sheets in the Arena by hand; give the session's
  // sheet store the same pixels, as a full load does. A store that is
  // already loaded is left alone.
  static void FillStoreFromArena(GraphicsEditor* editor) {
    zelda3::GameData* data = editor->game_data();
    ASSERT_NE(data, nullptr);
    constexpr size_t kBytes = zelda3::GraphicsSheetStore::kSheetBytes;
    if (data->graphics_buffer.size() < 223 * kBytes) {
      data->graphics_buffer.assign(223 * kBytes, 0);
      const auto& sheets = gfx::Arena::Get().gfx_sheets();
      for (size_t i = 0; i < 223; ++i) {
        if (sheets[i].is_active() && sheets[i].vector().size() == kBytes) {
          std::copy(sheets[i].vector().begin(), sheets[i].vector().end(),
                    data->graphics_buffer.begin() + i * kBytes);
        }
      }
      data->sheet_store.MarkAllSheetsChanged();
    }
    gfx::Arena::Get().set_gfx_sheets_owner(data);
    AttachSheetStore(editor->state_, data);
  }
  static absl::Status PaintPixel(GraphicsEditor* editor, uint8_t value) {
    auto& state = editor->state_;
    const auto pixels = state.game_data->sheet_store.Sheet(0);
    std::vector<uint8_t> before(pixels.begin(), pixels.end());
    auto after = before;
    after[0] = value;
    auto status = CommitSheetPixels(state, 0, after);
    if (!status.ok())
      return status;
    editor->undo_manager_.Push(std::make_unique<GraphicsPixelEditAction>(
        &state, MakeSheetPixelDiff(0, before, after), "Session pixel"));
    return absl::OkStatus();
  }
  // Runs the real read-back check against pixels with one index changed, so
  // every sheet the save writes fails verification.
  static void ForceVerificationMismatch(GraphicsEditor* editor) {
    editor->verify_written_sheet_ =
        [](const Rom& rom, uint16_t sheet_id,
           const std::vector<uint8_t>& expected,
           const zelda3::GfxSheetPointerTables& tables) {
          std::vector<uint8_t> wrong = expected;
          wrong[0] = static_cast<uint8_t>((wrong[0] + 1) % 8);
          return VerifyWrittenSheetPixels(rom, sheet_id, wrong, tables);
        };
  }
  static void RestoreVerification(GraphicsEditor* editor) {
    editor->verify_written_sheet_ = VerifyWrittenSheetPixels;
  }
};

class ScreenEditorSaveStoplossTestPeer {
 public:
  static void EditLoadedBossRoom(ScreenEditor* editor, uint16_t room) {
    ASSERT_FALSE(editor->dungeon_maps_.empty());
    editor->SaveDungeonMapUndoState("Edit loaded dungeon boss room");
    editor->dungeon_maps_[0].boss_room = room;
    editor->CommitDungeonMapUndo();
  }
  static uint16_t LoadedBossRoom(const ScreenEditor* editor) {
    return editor->dungeon_maps_.at(0).boss_room;
  }
  static uint8_t LoadedFirstMapRoom(const ScreenEditor* editor) {
    return editor->dungeon_maps_.at(0).floor_rooms.at(0).at(0);
  }
  static void EditLoadedFirstMapRoom(ScreenEditor* editor, uint8_t room) {
    ASSERT_TRUE(editor->IsRomBackedStateValid());
    ASSERT_EQ(editor->dungeon_maps_.size(), zelda3::kNumDungeons);
    ASSERT_EQ(editor->dungeon_maps_[0].floor_rooms.size(), 1u);
    editor->selected_dungeon = 0;
    editor->floor_number = 0;
    editor->SaveDungeonMapUndoState("Edit loaded dungeon map room");
    editor->dungeon_maps_[0].floor_rooms[0][0] = room;
    editor->dungeon_maps_[0].floor_gfx[0][0] = 0;
    editor->CommitDungeonMapUndo();
  }
  static void StartUnfinishedDungeonMapEdit(ScreenEditor* editor) {
    editor->selected_dungeon = 0;
    editor->SaveDungeonMapUndoState("Unfinished dungeon map edit");
  }
  static void CancelUnfinishedDungeonMapEdit(ScreenEditor* editor) {
    editor->has_pending_dungeon_undo_ = false;
    editor->pending_dungeon_desc_.clear();
  }

  struct BitmapOwners {
    zelda3::Inventory* inventory = nullptr;
    gfx::Bitmap* inventory_bitmap = nullptr;
    gfx::Bitmap* inventory_tilesheet = nullptr;
    gfx::Bitmap* title_bg1 = nullptr;
    gfx::Bitmap* title_bg2 = nullptr;
    gfx::Bitmap* title_oam = nullptr;
    gfx::Bitmap* title_tiles8 = nullptr;
    gfx::Bitmap* title_composite = nullptr;
    gfx::Bitmap* overworld_tiles8 = nullptr;
    gfx::Bitmap* overworld_map = nullptr;
    gfx::Bitmap* sheet0 = nullptr;
    gfx::Bitmap* tile16_atlas = nullptr;
    gfx::Bitmap* tile8_atlas = nullptr;
    gfx::Bitmap* tile16_cached = nullptr;
    gfx::Bitmap* tile8_cached = nullptr;
  };

  static std::array<gui::Canvas*, 9> AllCanvases(ScreenEditor* editor) {
    return {&editor->current_tile_canvas_,   &editor->screen_canvas_,
            &editor->tilesheet_canvas_,      &editor->tilemap_canvas_,
            &editor->title_bg1_canvas_,      &editor->title_bg2_canvas_,
            &editor->title_blockset_canvas_, &editor->ow_map_canvas_,
            &editor->ow_tileset_canvas_};
  }

  static void PrimeCanvasSelection(gui::Canvas* canvas, float seed) {
    const ImVec2 point(seed, seed + 1.0f);
    canvas->mutable_points()->push_back(point);
    canvas->mutable_selected_points()->push_back(point);
    canvas->mutable_selected_tiles()->push_back(point);
    canvas->GetSelection().selected_points.push_back(point);
    canvas->GetSelection().selected_tiles.push_back(point);
    canvas->GetSelection().selected_tile_pos = point;
    canvas->GetSelection().select_rect_active = true;
    canvas->GetInteractionHandler().SetSelectedTilePosition(point);
    canvas->SetSelectedTilePos(point);
  }

  static bool HasCanvasSelectionResidue(const gui::Canvas& canvas) {
    const auto& selection = canvas.GetSelection();
    const auto& interaction = canvas.GetInteractionHandler();
    return !canvas.points().empty() || !canvas.selected_points().empty() ||
           !canvas.selected_tiles().empty() ||
           canvas.selected_tile_pos().x != -1.0f ||
           !selection.selected_points.empty() ||
           !selection.selected_tiles.empty() ||
           selection.selected_tile_pos.x != -1.0f ||
           selection.select_rect_active ||
           interaction.GetSelectedTilePosition().x != -1.0f ||
           !interaction.GetSelectedPoints().empty() ||
           !interaction.GetSelectedTiles().empty();
  }

  static void MarkDungeonMapModified(ScreenEditor* editor) {
    editor->MarkDungeonMapModified();
  }

  static void MarkDungeonMapTile16Modified(ScreenEditor* editor) {
    editor->MarkDungeonMapTile16Modified();
  }

  static void MarkTitleScreenModified(ScreenEditor* editor) {
    editor->MarkTitleScreenModified();
  }

  static void MarkOverworldMapModified(ScreenEditor* editor) {
    editor->MarkOverworldMapModified();
  }

  static void MarkOverworldMapPaletteModified(ScreenEditor* editor) {
    editor->MarkOverworldMapPaletteModified();
  }

  static void ClearPendingChanges(ScreenEditor* editor) {
    editor->pending_dungeon_map_changes_ = false;
    editor->pending_dungeon_map_tile16_changes_ = false;
    editor->pending_title_screen_changes_ = false;
    editor->pending_overworld_map_changes_ = false;
    editor->pending_overworld_map_palette_changes_ = false;
  }

  static void CommitDungeonMapEdit(ScreenEditor* editor) {
    std::array<uint8_t, zelda3::kNumRooms> rooms{};
    rooms.fill(0x0F);
    std::array<uint8_t, zelda3::kNumRooms> graphics{};
    graphics.fill(0xFF);
    editor->dungeon_maps_.emplace_back(
        0xFFFF, 1, 0,
        std::vector<std::array<uint8_t, zelda3::kNumRooms>>{rooms},
        std::vector<std::array<uint8_t, zelda3::kNumRooms>>{graphics});
    editor->dungeon_map_labels_[0].emplace_back();
    editor->selected_dungeon = 0;
    editor->SaveDungeonMapUndoState("Edit dungeon map");
    editor->dungeon_maps_[0].boss_room = 0x0123;
    editor->CommitDungeonMapUndo();
  }

  static void CommitDungeonMapTile16Edit(ScreenEditor* editor) {
    editor->selected_tile16_ = 4;
    editor->current_tile16_info = {};
    editor->SaveTile16CompUndoState("Edit dungeon-map Tile16");
    editor->current_tile16_info[0] = gfx::TileInfo(1, 0, false, false, false);
    editor->CommitTile16CompUndo();
  }

  static void PrimeRomBackedState(ScreenEditor* editor) {
    const std::vector<uint8_t> pixel{1};
    editor->dungeon_maps_.emplace_back(
        0xFFFF, 0, 0, std::vector<std::array<uint8_t, zelda3::kNumRooms>>{},
        std::vector<std::array<uint8_t, zelda3::kNumRooms>>{});
    MarkDungeonMapModified(editor);
    MarkDungeonMapTile16Modified(editor);
    MarkTitleScreenModified(editor);
    MarkOverworldMapModified(editor);
    MarkOverworldMapPaletteModified(editor);
    editor->has_pending_dungeon_undo_ = true;
    editor->has_pending_tile16_undo_ = true;
    editor->pending_dungeon_desc_ = "pending dungeon edit";
    editor->pending_tile16_desc_ = "pending Tile16 edit";
    editor->dungeon_map_labels_[0].emplace_back();
    editor->binary_gfx_loaded_ = true;
    editor->inventory_loaded_ = true;
    editor->title_screen_loaded_ = true;
    editor->ow_map_loaded_ = true;
    editor->selected_room = 7;
    editor->selected_tile16_ = 8;
    editor->selected_tile8_ = 9;
    editor->selected_dungeon = 10;
    editor->floor_number = 11;
    editor->sheets_[0] = std::make_unique<gfx::Bitmap>(1, 1, 8, pixel);
    gfx::Bitmap tile16_cached(1, 1, 8, pixel);
    gfx::Bitmap tile8_cached(1, 1, 8, pixel);
    editor->tile16_blockset_.atlas.Create(1, 1, 8, pixel);
    editor->tile8_tilemap_.atlas.Create(1, 1, 8, pixel);
    editor->tile16_blockset_.tile_info.push_back({});
    editor->tile8_tilemap_.tile_info.push_back({});
    editor->tile16_blockset_.tile_cache.CacheTile(1, tile16_cached);
    editor->tile8_tilemap_.tile_cache.CacheTile(2, tile8_cached);
    editor->inventory_->item_icons().push_back(
        zelda3::ItemIcon{1, 2, 3, 4, "stale item"});
    editor->inventory_->palette().AddColor(gfx::SnesColor(1));
    editor->inventory_->bitmap().Create(1, 1, 8, std::vector<uint8_t>{1});
    editor->inventory_->tilesheet().Create(1, 1, 8, std::vector<uint8_t>{1});
    editor->title_screen_.bg1_bitmap().Create(1, 1, 8, pixel);
    editor->title_screen_.bg2_bitmap().Create(1, 1, 8, pixel);
    editor->title_screen_.oam_bitmap().Create(1, 1, 8, pixel);
    editor->title_screen_.tiles8_bitmap().Create(1, 1, 8, pixel);
    editor->title_screen_.composite_bitmap().Create(1, 1, 8, pixel);
    editor->ow_map_screen_.tiles8_bitmap().Create(1, 1, 8, pixel);
    editor->ow_map_screen_.map_bitmap().Create(1, 1, 8, pixel);
    editor->current_mode_ = ScreenEditor::EditingMode::EDIT;
    editor->selected_title_tile16_ = 12;
    editor->title_h_flip_ = true;
    editor->title_v_flip_ = true;
    editor->title_palette_ = 3;
    editor->show_title_bg1_ = false;
    editor->show_title_bg2_ = false;
    editor->selected_ow_tile_ = 13;
    editor->ow_show_dark_world_ = true;
    editor->rom_backed_state_valid_ = true;
    float canvas_seed = 1.0f;
    for (auto* canvas : AllCanvases(editor)) {
      PrimeCanvasSelection(canvas, canvas_seed++);
    }

    ScreenSnapshot before;
    ScreenSnapshot after;
    editor->undo_manager_.Push(std::make_unique<ScreenEditAction>(
        before, after, [](const ScreenSnapshot&) {}, "older screen edit"));
    editor->undo_manager_.Push(std::make_unique<ScreenEditAction>(
        before, after, [](const ScreenSnapshot&) {}, "newer screen edit"));
    editor->undo_manager_.Undo().IgnoreError();
  }

  static void ResetRomBackedStateForLoad(ScreenEditor* editor) {
    editor->ResetRomBackedStateForLoad();
  }

  static bool HasReloadResidue(ScreenEditor& editor) {
    const bool has_labels =
        std::ranges::any_of(editor.dungeon_map_labels_,
                            [](const auto& labels) { return !labels.empty(); });
    const bool has_canvas_selection = std::ranges::any_of(
        AllCanvases(&editor),
        [](const auto* canvas) { return HasCanvasSelectionResidue(*canvas); });
    return !editor.dungeon_maps_.empty() || has_labels ||
           editor.undo_manager_.CanUndo() || editor.undo_manager_.CanRedo() ||
           editor.has_pending_dungeon_undo_ ||
           editor.has_pending_tile16_undo_ ||
           !editor.pending_dungeon_desc_.empty() ||
           !editor.pending_tile16_desc_.empty() || editor.binary_gfx_loaded_ ||
           editor.inventory_loaded_ || editor.title_screen_loaded_ ||
           editor.ow_map_loaded_ || editor.selected_room != 0 ||
           editor.selected_tile16_ != 0 || editor.selected_tile8_ != 0 ||
           editor.selected_dungeon != 0 || editor.floor_number != 0 ||
           !editor.tile16_blockset_.tile_info.empty() ||
           !editor.tile8_tilemap_.tile_info.empty() ||
           !editor.inventory_->item_icons().empty() ||
           editor.inventory_->palette().size() != 0 ||
           editor.title_screen_.palette().size() != 0 ||
           editor.ow_map_screen_.lw_palette().size() != 0 ||
           editor.ow_map_screen_.dw_palette().size() != 0 ||
           editor.current_mode_ != ScreenEditor::EditingMode::DRAW ||
           editor.selected_title_tile16_ != 0 || editor.title_h_flip_ ||
           editor.title_v_flip_ || editor.title_palette_ != 0 ||
           !editor.show_title_bg1_ || !editor.show_title_bg2_ ||
           editor.selected_ow_tile_ != 0 || editor.ow_show_dark_world_ ||
           has_canvas_selection || editor.HasPendingScreenChanges() ||
           editor.rom_backed_state_valid_;
  }

  static BitmapOwners CaptureBitmapOwners(ScreenEditor* editor) {
    return {
        .inventory = editor->inventory_.get(),
        .inventory_bitmap = &editor->inventory_->bitmap(),
        .inventory_tilesheet = &editor->inventory_->tilesheet(),
        .title_bg1 = &editor->title_screen_.bg1_bitmap(),
        .title_bg2 = &editor->title_screen_.bg2_bitmap(),
        .title_oam = &editor->title_screen_.oam_bitmap(),
        .title_tiles8 = &editor->title_screen_.tiles8_bitmap(),
        .title_composite = &editor->title_screen_.composite_bitmap(),
        .overworld_tiles8 = &editor->ow_map_screen_.tiles8_bitmap(),
        .overworld_map = &editor->ow_map_screen_.map_bitmap(),
        .sheet0 = editor->sheets_.at(0).get(),
        .tile16_atlas = &editor->tile16_blockset_.atlas,
        .tile8_atlas = &editor->tile8_tilemap_.atlas,
        .tile16_cached = editor->tile16_blockset_.tile_cache.GetTile(1),
        .tile8_cached = editor->tile8_tilemap_.tile_cache.GetTile(2),
    };
  }

  static void QueueBitmapOwners(const BitmapOwners& owners) {
    for (auto* bitmap :
         {owners.inventory_bitmap, owners.inventory_tilesheet, owners.title_bg1,
          owners.title_bg2, owners.title_oam, owners.title_tiles8,
          owners.title_composite, owners.overworld_tiles8, owners.overworld_map,
          owners.sheet0, owners.tile16_atlas, owners.tile8_atlas,
          owners.tile16_cached, owners.tile8_cached}) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, bitmap);
    }
  }

  static void ExpectSameBitmapOwners(const BitmapOwners& expected,
                                     const BitmapOwners& actual) {
    EXPECT_EQ(actual.inventory, expected.inventory);
    EXPECT_EQ(actual.inventory_bitmap, expected.inventory_bitmap);
    EXPECT_EQ(actual.inventory_tilesheet, expected.inventory_tilesheet);
    EXPECT_EQ(actual.title_bg1, expected.title_bg1);
    EXPECT_EQ(actual.title_bg2, expected.title_bg2);
    EXPECT_EQ(actual.title_oam, expected.title_oam);
    EXPECT_EQ(actual.title_tiles8, expected.title_tiles8);
    EXPECT_EQ(actual.title_composite, expected.title_composite);
    EXPECT_EQ(actual.overworld_tiles8, expected.overworld_tiles8);
    EXPECT_EQ(actual.overworld_map, expected.overworld_map);
    EXPECT_EQ(actual.sheet0, expected.sheet0);
    EXPECT_EQ(actual.tile16_atlas, expected.tile16_atlas);
    EXPECT_EQ(actual.tile8_atlas, expected.tile8_atlas);
    EXPECT_EQ(actual.tile16_cached, expected.tile16_cached);
    EXPECT_EQ(actual.tile8_cached, expected.tile8_cached);
  }

  static size_t DungeonMapCount(const ScreenEditor& editor) {
    return editor.dungeon_maps_.size();
  }

  static int DungeonFloorCount(const ScreenEditor& editor, size_t dungeon) {
    const auto& map = editor.dungeon_maps_.at(dungeon);
    return map.nbr_of_floor + map.nbr_of_basement;
  }

  static void DrawDungeonMapsEditor(ScreenEditor* editor) {
    editor->DrawDungeonMapsEditor();
  }

  static void DrawEmptyDungeonMapMethods(ScreenEditor* editor) {
    editor->rom_backed_state_valid_ = true;
    editor->DrawDungeonMapScreen(0);
    editor->DrawDungeonMapsTabs();
    editor->DrawDungeonMapsRoomGfx();
    editor->DrawDungeonMapsEditor();
  }

  static absl::Status CreateTitleScreenModel(ScreenEditor* editor, Rom* rom,
                                             zelda3::GameData* game_data) {
    auto status = editor->title_screen_.Create(rom, game_data);
    editor->title_screen_loaded_ = status.ok();
    return status;
  }

  static absl::Status CreateOverworldMapModel(ScreenEditor* editor, Rom* rom) {
    auto status = editor->ow_map_screen_.Create(rom);
    editor->ow_map_loaded_ = status.ok();
    return status;
  }

  static size_t TitleScreenPaletteSize(ScreenEditor& editor) {
    return editor.title_screen_.palette().size();
  }

  static size_t LightWorldPaletteSize(ScreenEditor& editor) {
    return editor.ow_map_screen_.lw_palette().size();
  }

  static size_t DarkWorldPaletteSize(ScreenEditor& editor) {
    return editor.ow_map_screen_.dw_palette().size();
  }

  static absl::Status SaveTitleScreenToRom(ScreenEditor* editor) {
    return editor->SaveTitleScreenToRom();
  }

  static absl::Status SaveOverworldMapToRom(ScreenEditor* editor) {
    return editor->SaveOverworldMapToRom();
  }

  static bool HasPendingOverworldMapTileChanges(ScreenEditor& editor) {
    return editor.pending_overworld_map_changes_;
  }

  static bool HasPendingOverworldMapPaletteChanges(ScreenEditor& editor) {
    return editor.pending_overworld_map_palette_changes_;
  }
};

class DungeonEditorV2ReloadTestPeer {
 public:
  static DungeonCanvasViewer* GetViewerForRoom(DungeonEditorV2* editor,
                                               int room_id) {
    return editor->GetViewerForRoom(room_id);
  }

  static bool HasRoomViewer(const DungeonEditorV2& editor, int room_id) {
    return editor.room_viewers_.Contains(room_id);
  }

  static zelda3::RoomEntrance* GetRegularEntrance(DungeonEditorV2* editor,
                                                  int entrance_id) {
    return &editor->entrances_[zelda3::kNumDungeonSpawnPoints + entrance_id];
  }

  static zelda3::DungeonSpawnPoint* GetSpawnPoint(DungeonEditorV2* editor,
                                                  int spawn_id) {
    return &editor->spawn_points_[spawn_id];
  }

  static std::shared_ptr<zelda3::DungeonObjectEditor> GetObjectEditor(
      DungeonEditorV2* editor) {
    return editor->dungeon_editor_system_->GetObjectEditor();
  }

  static OverlayManagerPanel* GetOverlayManagerPanel(DungeonEditorV2* editor) {
    return editor->overlay_manager_panel_;
  }

  static bool* GetOverlayGridBinding(DungeonEditorV2* editor) {
    return editor->overlay_manager_panel_
               ? editor->overlay_manager_panel_->state_.show_grid
               : nullptr;
  }

  static void SyncPanelsToRoom(DungeonEditorV2* editor, int room_id) {
    editor->SyncPanelsToRoom(room_id);
  }
};

namespace {

struct FeatureFlagsGuard {
  core::FeatureFlags::Flags prev = core::FeatureFlags::get();
  ~FeatureFlagsGuard() { core::FeatureFlags::get() = prev; }
};

struct PaletteManagerGuard {
  PaletteManagerGuard() { gfx::PaletteManager::Get().ResetForTesting(); }
  ~PaletteManagerGuard() { gfx::PaletteManager::Get().ResetForTesting(); }
};

struct ScopedFileCleanup {
  std::filesystem::path path;
  ~ScopedFileCleanup() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
};

struct ScopedDirectoryCleanup {
  std::filesystem::path path;
  ~ScopedDirectoryCleanup() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

struct ScopedGraphicsSheetRestore {
  explicit ScopedGraphicsSheetRestore(gfx::Bitmap* sheet)
      : sheet(sheet), original(std::move(*sheet)) {}
  ~ScopedGraphicsSheetRestore() { *sheet = std::move(original); }

  gfx::Bitmap* sheet;
  gfx::Bitmap original;
};

struct ScopedTextureQueueClear {
  ScopedTextureQueueClear() { gfx::Arena::Get().ClearTextureQueue(); }
  ~ScopedTextureQueueClear() { gfx::Arena::Get().ClearTextureQueue(); }
};

struct ScopedImGuiContext {
  ImGuiContext* ctx = nullptr;
  ScopedImGuiContext() {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() {
    if (ctx) {
      ImGui::DestroyContext(ctx);
      ctx = nullptr;
    }
  }
};

std::filesystem::path MakeTempFilePath(const std::string& basename) {
  const auto nonce = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         (basename + "_" + std::to_string(nonce));
}

using ::yaze::test::InitializeWithIsolatedSettings;

uint8_t ReadByteAt(const std::filesystem::path& path, std::streamoff offset) {
  std::ifstream file(path, std::ios::binary);
  EXPECT_TRUE(file.is_open());
  file.seekg(offset, std::ios::beg);
  char b = 0;
  file.read(&b, 1);
  EXPECT_TRUE(file.good());
  return static_cast<uint8_t>(b);
}

void WriteByteAt(const std::filesystem::path& path, std::streamoff offset,
                 uint8_t value) {
  std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
  ASSERT_TRUE(file.is_open());
  file.seekp(offset, std::ios::beg);
  file.put(static_cast<char>(value));
  ASSERT_TRUE(file.good());
}

void WriteWordAt(const std::filesystem::path& path, std::streamoff offset,
                 uint16_t value) {
  WriteByteAt(path, offset, static_cast<uint8_t>(value & 0xFF));
  WriteByteAt(path, offset + 1, static_cast<uint8_t>(value >> 8));
}

void WriteTestRom(const std::filesystem::path& path,
                  const std::string& title = "YAZE TEST ROM") {
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

void WriteScreenModelTestRom(const std::filesystem::path& path,
                             const std::string& title = "SCREEN MODEL TEST") {
  std::vector<uint8_t> rom_data(1024 * 1024, 0x00);
  for (size_t i = 0; i < title.size() && i < 20; ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

void WriteAdmissibleScreenMapTestRom(const std::filesystem::path& path,
                                     const std::string& title) {
  WriteScreenModelTestRom(path, title);
  // The expanded map layout reads Tile16 at PC 0x109010, beyond 1 MiB.
  std::filesystem::resize_file(path, 2 * 1024 * 1024);
  WriteByteAt(path, zelda3::kDungeonMapExpCheck, 0x20);
  // One real floor with one room; all fourteen maps have a valid no-boss
  // sentinel. The remaining thirteen maps intentionally have zero floors.
  for (int dungeon = 0; dungeon < zelda3::kNumDungeons; ++dungeon) {
    WriteWordAt(path, zelda3::kDungeonMapBossRooms + dungeon * 2, 0xFFFF);
  }
  WriteByteAt(path, zelda3::kDungeonMapFloors, 0x10);
  const uint16_t room_ptr =
      static_cast<uint16_t>(PcToSnes(zelda3::kDungeonMapDataStart));
  const uint16_t gfx_ptr = static_cast<uint16_t>(
      PcToSnes(zelda3::kDungeonMapDataStart + zelda3::kNumRooms));
  WriteWordAt(path, zelda3::kDungeonMapRoomsPtr, room_ptr);
  WriteWordAt(path, zelda3::kDungeonMapGfxPtr, gfx_ptr);
  WriteByteAt(path, zelda3::kDungeonMapDataStart, 0x01);
  for (int room = 1; room < zelda3::kNumRooms; ++room) {
    WriteByteAt(path, zelda3::kDungeonMapDataStart + room, 0x0F);
  }
  WriteByteAt(path, zelda3::kDungeonMapDataStart + zelda3::kNumRooms, 0x00);
}

void WriteTooSmallForAssetsTestRom(const std::filesystem::path& path) {
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  const std::string title = "INVALID ASSET ROM";
  for (size_t i = 0; i < title.size() && i < 20; ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

void PopulateTitleScreenTestGameData(zelda3::GameData* game_data) {
  ASSERT_NE(game_data, nullptr);
  game_data->graphics_buffer.resize(zelda3::kNumGfxSheets * 0x1000, 0x00);

  gfx::SnesPalette palette;
  for (uint16_t color = 0; color < 8; ++color) {
    palette.AddColor(gfx::SnesColor(color));
  }
  auto add_palettes = [&palette](gfx::PaletteGroup* group, size_t count) {
    ASSERT_NE(group, nullptr);
    for (size_t i = 0; i < count; ++i) {
      group->AddPalette(palette);
    }
  };
  add_palettes(&game_data->palette_groups.overworld_main, 6);
  add_palettes(&game_data->palette_groups.overworld_animated, 1);
  add_palettes(&game_data->palette_groups.overworld_aux, 4);
  add_palettes(&game_data->palette_groups.hud, 1);
  add_palettes(&game_data->palette_groups.sprites_aux1, 2);
}

constexpr int kDungeonHeaderTablePc = 0x0F6000;
constexpr int kDungeonRoom0HeaderPc = 0x114000;

void WriteDungeonHeaderTestRom(const std::filesystem::path& path,
                               uint8_t room_palette, uint16_t entrance_room_id,
                               uint16_t spawn_room_id) {
  std::vector<uint8_t> rom_data(2 * 1024 * 1024, 0x00);
  const std::string title = "DUNGEON RESTORE";
  for (size_t i = 0; i < title.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }

  const uint32_t header_table_snes = PcToSnes(kDungeonHeaderTablePc);
  rom_data[zelda3::kRoomHeaderPointer] = header_table_snes & 0xFF;
  rom_data[zelda3::kRoomHeaderPointer + 1] = (header_table_snes >> 8) & 0xFF;
  rom_data[zelda3::kRoomHeaderPointer + 2] = (header_table_snes >> 16) & 0xFF;

  const uint32_t room_header_snes = PcToSnes(kDungeonRoom0HeaderPc);
  rom_data[zelda3::kRoomHeaderPointerBank] = (room_header_snes >> 16) & 0xFF;
  rom_data[kDungeonHeaderTablePc] = room_header_snes & 0xFF;
  rom_data[kDungeonHeaderTablePc + 1] = (room_header_snes >> 8) & 0xFF;
  rom_data[kDungeonRoom0HeaderPc + 1] = room_palette;
  rom_data[zelda3::kEntranceRoom] = entrance_room_id & 0xFF;
  rom_data[zelda3::kEntranceRoom + 1] = (entrance_room_id >> 8) & 0xFF;
  rom_data[zelda3::kDungeonSpawnRoom] = spawn_room_id & 0xFF;
  rom_data[zelda3::kDungeonSpawnRoom + 1] = (spawn_room_id >> 8) & 0xFF;

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

void WriteTestProject(const std::filesystem::path& path,
                      const std::filesystem::path& rom_path) {
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out << "[project]\n"
         "name=Backing File Identity Test\n\n"
         "[files]\n"
      << "rom_filename=" << rom_path.string() << "\n"
      << "code_folder=\n"
         "rom_backup_folder=backups\n"
         "output_folder=output\n"
         "labels_filename=\n"
         "symbols_filename=\n";
  ASSERT_TRUE(out.good());
}

void DisableRomWritesForTest() {
  auto& flags = core::FeatureFlags::get();
  flags.kSaveDungeonMaps = false;
  flags.kSaveGraphicsSheet = false;
  flags.kSaveMessages = false;

  auto& d = flags.dungeon;
  d.kSaveObjects = false;
  d.kSaveSprites = false;
  d.kSaveRoomHeaders = false;
  d.kSaveTorches = false;
  d.kSavePits = false;
  d.kSaveBlocks = false;
  d.kSaveCollision = false;
  d.kSaveWaterFillZones = false;
  d.kSaveChests = false;
  d.kSavePotItems = false;
  d.kSaveEntrances = false;
  d.kSavePalettes = false;

  auto& o = flags.overworld;
  o.kSaveOverworldMaps = false;
  o.kSaveOverworldEntrances = false;
  o.kSaveOverworldExits = false;
  o.kSaveOverworldItems = false;
  o.kSaveOverworldProperties = false;
  o.kSaveOverworldSprites = false;
}

#if defined(YAZE_WITH_JSON)
class ScopedPublicationLockFailure {
 public:
  explicit ScopedPublicationLockFailure(const std::filesystem::path& directory)
      : lock_(directory / core::kSourceArtifactPublicationLockBasename),
        backup_(lock_.string() + ".failure-test-backup") {}

  absl::Status Install() {
    std::error_code error;
    if (std::filesystem::exists(lock_, error)) {
      std::filesystem::rename(lock_, backup_, error);
      if (error)
        return absl::InternalError(error.message());
      backed_up_ = true;
    } else if (error) {
      return absl::InternalError(error.message());
    }
    installed_ = std::filesystem::create_directory(lock_, error);
    return installed_ && !error
               ? absl::OkStatus()
               : absl::InternalError("Cannot inject publication lock failure");
  }

  ~ScopedPublicationLockFailure() {
    std::error_code error;
    if (installed_) {
      EXPECT_TRUE(std::filesystem::remove(lock_, error));
      EXPECT_FALSE(error) << error.message();
    }
    if (backed_up_) {
      std::filesystem::rename(backup_, lock_, error);
      EXPECT_FALSE(error) << error.message();
    }
  }

 private:
  std::filesystem::path lock_, backup_;
  bool backed_up_ = false;
  bool installed_ = false;
};

class NativeSaveReceiptIntegrationTest : public ::testing::Test {
 protected:
  static std::string ReceiptPathText(const std::filesystem::path& path) {
    const auto bytes = std::filesystem::canonical(path).generic_u8string();
    return std::string(bytes.begin(), bytes.end());
  }

  void SetUp() override {
    std::filesystem::create_directory(cleanup_.path);
    source_ = cleanup_.path / "source.sfc";
    WriteTestRom(source_, "NATIVE RECEIPT TEST");
    manager_ = std::make_unique<EditorManager>();
    InitializeWithIsolatedSettings(*manager_, &renderer_);
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager_->user_settings().prefs().backup_before_save = false;
    ASSERT_OK(manager_->OpenRomOrProject(source_.string()));
    DisableRomWritesForTest();
  }

  void BindProject() {
    auto* project = manager_->GetCurrentProject();
    ASSERT_NE(project, nullptr);
    project->name = "Native save receipt";
    project->metadata.project_id = "native_save_receipt_fixture";
    project->filepath = (cleanup_.path / "project.yaze").string();
    project->rom_filename = source_.string();
    project->workspace_settings.backup_on_save = false;
    project->rom_metadata.write_policy = project::RomWritePolicy::kAllow;
    project->feature_flags = core::FeatureFlags::get();
    ASSERT_OK(project->Save());
    receipt_ = project->filepath + ".native-save.json";
  }

  void CommitBaseline() {
    ASSERT_NO_FATAL_FAILURE(BindProject());
    ASSERT_OK(manager_->SaveRom());
    const auto context = core::GetNativeSaveContext(
        source_.string(), manager_->GetCurrentProject()->filepath);
    ASSERT_TRUE(context.ok()) << context.status();
    context_ = context->path;
    ASSERT_TRUE(ReadReceipt(receipt_).at("bindings_complete"));
    ASSERT_EQ(ReadReceipt(context_).at("state"), "committed");
  }

  static std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.is_open()) << path;
    return std::string(std::istreambuf_iterator<char>(input), {});
  }

  static Json ReadReceipt(const std::filesystem::path& path) {
    return Json::parse(ReadText(path));
  }

  static void ExpectArtifact(const Json& receipt, const std::string& role,
                             const std::filesystem::path& path) {
    ASSERT_TRUE(receipt.at("artifacts").is_array());
    const auto& artifacts = receipt.at("artifacts");
    const auto found = std::find_if(
        artifacts.begin(), artifacts.end(),
        [&role](const Json& artifact) { return artifact.at("role") == role; });
    ASSERT_NE(found, artifacts.end()) << role;
    EXPECT_EQ(found->at("path"), ReceiptPathText(path));
    EXPECT_EQ(found->at("bytes"), std::filesystem::file_size(path));
    EXPECT_EQ(found->at("sha256"),
              core::ComputeSourceArtifactSha256(ReadText(path)));
  }

  static void ExpectProducer(const Json& receipt) {
    const auto executable = util::PlatformPaths::GetExecutablePath();
    ASSERT_TRUE(executable.ok()) << executable.status();
    const auto& artifacts = receipt.at("artifacts");
    const auto found = std::find_if(
        artifacts.begin(), artifacts.end(),
        [](const Json& artifact) { return artifact.at("role") == "producer"; });
    ASSERT_NE(found, artifacts.end());
    EXPECT_EQ(found->at("path"), ReceiptPathText(*executable));
    EXPECT_EQ(found->at("bytes"), std::filesystem::file_size(*executable));
    EXPECT_EQ(found->at("sha256").get<std::string>().size(), 64u);
  }

  FeatureFlagsGuard flags_;
  ScopedDirectoryCleanup cleanup_{MakeTempFilePath("yaze_native_receipt")};
  ScopedImGuiContext imgui_;
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
  std::filesystem::path source_, receipt_, context_;
};

TEST_F(NativeSaveReceiptIntegrationTest,
       NativeCommitsAdvanceGenerationEvenWhenRomBytesAreIdentical) {
  ASSERT_NO_FATAL_FAILURE(BindProject());
  ASSERT_FALSE(std::filesystem::exists(receipt_));
  ASSERT_OK(manager_->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  ASSERT_OK(manager_->SaveCurrentSession());
  ASSERT_TRUE(std::filesystem::exists(receipt_));
  const auto first = ReadReceipt(receipt_);
  if (const auto* evidence = std::getenv("YAZE_NATIVE_SAVE_RECEIPT_EVIDENCE")) {
    const auto directory = std::filesystem::path(evidence);
    ASSERT_TRUE(std::filesystem::is_directory(directory));
    ASSERT_TRUE(std::filesystem::copy_file(
        receipt_, directory / "first.native-save.json",
        std::filesystem::copy_options::overwrite_existing));
    ASSERT_TRUE(std::filesystem::copy_file(
        manager_->GetCurrentProject()->filepath, directory / "first.yaze",
        std::filesystem::copy_options::overwrite_existing));
  }
  EXPECT_EQ(first.at("kind"), "yaze.native_save.v1");
  EXPECT_EQ(first.at("generation"), 1u);
  EXPECT_EQ(first.at("operation"), "save");
  EXPECT_TRUE(first.at("bindings_complete")) << first.dump(2);
  EXPECT_TRUE(first.at("binding_errors").empty()) << first.dump(2);
  ASSERT_FALSE(first.at("operation_id").get<std::string>().empty());
  ExpectArtifact(first, "rom", source_);
  ExpectArtifact(first, "project", manager_->GetCurrentProject()->filepath);
  ExpectProducer(first);
  const auto saved_bytes = ReadText(source_);

  // An explicit native ROM save commits again even without dirty bytes.
  ASSERT_OK(manager_->SaveRom());
  const auto second = ReadReceipt(receipt_);
  EXPECT_EQ(ReadText(source_), saved_bytes);
  EXPECT_EQ(second.at("generation"), 2u);
  EXPECT_EQ(second.at("operation"), "save");
  EXPECT_NE(second.at("operation_id"), first.at("operation_id"));
  EXPECT_EQ(second.at("artifacts"), first.at("artifacts"));
  EXPECT_EQ(second.at("receipt_path"), ReceiptPathText(receipt_));
}

TEST_F(NativeSaveReceiptIntegrationTest,
       RawRomSaveAsBindsDestinationAndLeavesSourceUnchanged) {
  const auto original = ReadText(source_);
  const auto context = core::GetNativeSaveContext(source_.string(), "");
  ASSERT_TRUE(context.ok()) << context.status();
  const auto destination = cleanup_.path / "saved-copy.sfc";
  const auto receipt = destination.string() + ".native-save.json";
  ASSERT_OK(manager_->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  ASSERT_TRUE(std::filesystem::exists(receipt));
  const auto saved = ReadReceipt(receipt);
  EXPECT_EQ(saved.at("generation"), 1u);
  EXPECT_EQ(saved.at("operation"), "save_as");
  EXPECT_EQ(saved.at("receipt_path"), ReceiptPathText(receipt));
  ASSERT_EQ(saved.at("artifacts").size(), 2u);
  ExpectArtifact(saved, "rom", destination);
  ExpectProducer(saved);
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), destination.string());
  const auto active_context = ReadReceipt(context->path);
  EXPECT_EQ(active_context.at("editing_source_path"), ReceiptPathText(source_));
  EXPECT_EQ(active_context.at("rom_path"), ReceiptPathText(destination));
  EXPECT_EQ(active_context.at("state"), "committed");
  EXPECT_EQ(active_context.at("native_commit").at("receipt_path"),
            ReceiptPathText(receipt));
  EXPECT_EQ(ReadText(source_), original);
  EXPECT_FALSE(std::filesystem::exists(source_.string() + ".native-save.json"));
}

TEST_F(NativeSaveReceiptIntegrationTest,
       PendingCancelledAndFailedSavesPreserveCommittedReceipt) {
  ASSERT_NO_FATAL_FAILURE(BindProject());
  ASSERT_OK(manager_->SaveRom());
  const auto committed_receipt = ReadText(receipt_);
  const auto committed_rom = ReadText(source_);
  ASSERT_EQ(ReadReceipt(receipt_).at("generation"), 1u);
  ASSERT_TRUE(ReadReceipt(receipt_).at("bindings_complete"));
  const auto context = core::GetNativeSaveContext(
      source_.string(), manager_->GetCurrentProject()->filepath);
  ASSERT_TRUE(context.ok()) << context.status();
  const auto committed_context = ReadText(context->path);
  auto* project = manager_->GetCurrentProject();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";
  // The configured editing source is exempt; an unbound reference must warn.
  project->rom_filename.clear();
  ASSERT_TRUE(manager_->IsRomHashMismatch());
  ASSERT_OK(manager_->GetCurrentRom()->WriteByte(0x1234, 0xA5));

  EXPECT_TRUE(absl::IsCancelled(manager_->SaveRom()));
  EXPECT_TRUE(manager_->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadText(receipt_), committed_receipt);
  EXPECT_EQ(ReadText(context->path), committed_context);
  manager_->CancelRomWriteConfirm();
  EXPECT_FALSE(manager_->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadText(receipt_), committed_receipt);
  EXPECT_EQ(ReadText(source_), committed_rom);
  EXPECT_EQ(ReadText(context->path), committed_context);

  ASSERT_TRUE(absl::IsCancelled(manager_->SaveRom()));
  manager_->ConfirmRomWrite();
  {
    test::ScopedRomStagingFailure failure;
    EXPECT_FALSE(manager_->ResumePendingRomSave().ok());
  }
  EXPECT_EQ(ReadText(receipt_), committed_receipt);
  EXPECT_EQ(ReadText(source_), committed_rom);
  EXPECT_TRUE(manager_->GetCurrentRom()->dirty());
  const auto retired = ReadReceipt(context->path);
  EXPECT_EQ(retired.at("state"), "rebound");
  EXPECT_TRUE(retired.at("native_commit").is_null());
}

// A project Save rewrites build inputs (the descriptor, and any terrain or
// other drafts), so it is a native Save and publishes a receipt. It binds the
// clean ROM as it is on disk and never rewrites it.
TEST_F(NativeSaveReceiptIntegrationTest,
       ProjectOnlySaveIssuesNativeReceiptWithoutWritingRom) {
  ASSERT_NO_FATAL_FAILURE(BindProject());
  const auto original_rom = ReadText(source_);
  const auto rom_time = std::filesystem::last_write_time(source_);
  manager_->GetCurrentProject()->name = "Project-only saved name";
  manager_->MarkCurrentProjectDirty();
  ASSERT_FALSE(manager_->GetCurrentRom()->dirty());
  ASSERT_OK(manager_->SaveCurrentSession());
  EXPECT_FALSE(manager_->IsCurrentProjectDirty());
  EXPECT_EQ(ReadText(source_), original_rom);
  EXPECT_EQ(std::filesystem::last_write_time(source_), rom_time);
  EXPECT_FALSE(std::filesystem::exists(source_.string() + ".native-save.json"));
  ASSERT_TRUE(std::filesystem::exists(receipt_));
  const auto receipt = ReadReceipt(receipt_);
  EXPECT_EQ(receipt.at("generation"), 1u);
  EXPECT_EQ(receipt.at("operation"), "save");
  EXPECT_TRUE(receipt.at("bindings_complete")) << receipt.dump(2);
  ExpectArtifact(receipt, "rom", source_);
  ExpectArtifact(receipt, "project", manager_->GetCurrentProject()->filepath);
  ExpectProducer(receipt);
  const auto context = core::GetNativeSaveContext(
      source_.string(), manager_->GetCurrentProject()->filepath);
  ASSERT_TRUE(context.ok()) << context.status();
  EXPECT_EQ(ReadReceipt(context->path).at("state"), "committed");
  project::YazeProject reopened;
  ASSERT_OK(reopened.Open(manager_->GetCurrentProject()->filepath));
  EXPECT_EQ(reopened.name, "Project-only saved name");
  // Nothing new to persist: Save does not invent another generation.
  ASSERT_OK(manager_->SaveCurrentSession());
  EXPECT_EQ(ReadReceipt(receipt_).at("generation"), 1u);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       ProjectSaveRetriesFailedCompletionWithoutNewEdits) {
  ASSERT_NO_FATAL_FAILURE(BindProject());
  const auto original_rom = ReadText(source_);
  // A directory at the receipt path makes publication fail after the project
  // bytes are already persisted.
  ASSERT_TRUE(std::filesystem::create_directory(receipt_));
  manager_->GetCurrentProject()->name = "Saved before completion";
  manager_->MarkCurrentProjectDirty();
  const auto failed = manager_->SaveCurrentSession();
  ASSERT_FALSE(failed.ok());
  EXPECT_NE(failed.message().find("Project saved"), std::string::npos)
      << failed;
  EXPECT_NE(failed.message().find("Save again to retry"), std::string::npos)
      << failed;
  EXPECT_EQ(failed.message().find("ROM saved"), std::string::npos) << failed;
  EXPECT_FALSE(manager_->IsCurrentProjectDirty());
  project::YazeProject reopened;
  ASSERT_OK(reopened.Open(manager_->GetCurrentProject()->filepath));
  EXPECT_EQ(reopened.name, "Saved before completion");

  std::filesystem::remove(receipt_);
  const auto descriptor = ReadText(manager_->GetCurrentProject()->filepath);
  ASSERT_OK(manager_->SaveCurrentSession());
  EXPECT_EQ(ReadText(manager_->GetCurrentProject()->filepath), descriptor);
  EXPECT_EQ(ReadText(source_), original_rom);
  ASSERT_TRUE(std::filesystem::is_regular_file(receipt_));
  const auto receipt = ReadReceipt(receipt_);
  EXPECT_EQ(receipt.at("generation"), 1u);
  ExpectArtifact(receipt, "project", manager_->GetCurrentProject()->filepath);
  ExpectArtifact(receipt, "rom", source_);
}

// A project-only receipt binds the ROM only as this session loaded or last
// wrote it, copier header included; another writer's bytes are never blessed.
TEST_F(NativeSaveReceiptIntegrationTest,
       ProjectOnlySaveRefusesRomChangedOnDiskByAnotherWriter) {
  const auto body = ReadText(source_);
  const std::string header(512, '\x11');
  const auto headered = cleanup_.path / "headered.sfc";
  std::ofstream(headered, std::ios::binary) << header << body;
  ASSERT_OK(manager_->OpenRomOrProject(headered.string()));
  DisableRomWritesForTest();
  ASSERT_TRUE(manager_->GetCurrentRom()->has_copier_header());
  source_ = headered;
  ASSERT_NO_FATAL_FAILURE(BindProject());
  const auto loaded = ReadText(headered);
  auto external = loaded;
  external[7] = '\x22';
  std::ofstream(headered, std::ios::binary | std::ios::trunc) << external;

  manager_->GetCurrentProject()->name = "Saved over an external ROM edit";
  manager_->MarkCurrentProjectDirty();
  ASSERT_FALSE(manager_->GetCurrentRom()->dirty());
  const auto refused = manager_->SaveCurrentSession();
  ASSERT_FALSE(refused.ok());
  EXPECT_NE(refused.message().find("changed on disk"), std::string::npos)
      << refused;
  EXPECT_NE(refused.message().find("Revert to Saved"), std::string::npos)
      << refused;
  EXPECT_FALSE(std::filesystem::exists(receipt_));
  EXPECT_EQ(ReadText(headered), external);
  EXPECT_FALSE(manager_->SaveCurrentSession().ok());
  EXPECT_FALSE(std::filesystem::exists(receipt_));
  EXPECT_EQ(ReadText(headered), external);

  // Restoring the bytes yaze loaded lets the pending completion publish.
  std::ofstream(headered, std::ios::binary | std::ios::trunc) << loaded;
  ASSERT_OK(manager_->SaveCurrentSession());
  const auto receipt = ReadReceipt(receipt_);
  EXPECT_TRUE(receipt.at("bindings_complete")) << receipt.dump(2);
  ExpectArtifact(receipt, "rom", headered);
  ExpectArtifact(receipt, "project", manager_->GetCurrentProject()->filepath);
}

// Unsaved ROM edits stay a draft: a project-only Save binds the file as last
// loaded and neither writes nor discards the in-memory edits.
TEST_F(NativeSaveReceiptIntegrationTest,
       ProjectOnlySaveKeepsUnsavedRomDraftAndBindsLoadedFile) {
  ASSERT_NO_FATAL_FAILURE(BindProject());
  const auto original_rom = ReadText(source_);
  ASSERT_OK(manager_->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  manager_->GetCurrentProject()->name = "Project saved beside a ROM draft";
  manager_->MarkCurrentProjectDirty();
  ASSERT_OK(manager_->SaveProject());
  EXPECT_TRUE(manager_->GetCurrentRom()->dirty());
  EXPECT_EQ(ReadText(source_), original_rom);
  const auto receipt = ReadReceipt(receipt_);
  EXPECT_TRUE(receipt.at("bindings_complete")) << receipt.dump(2);
  ExpectArtifact(receipt, "rom", source_);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       ProjectSaveAsRetriesFailedReceiptWithoutNewEdits) {
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  const auto old_receipt = ReadText(receipt_);
  const auto destination = cleanup_.path / "relocated.yaze";
  const auto new_receipt =
      std::filesystem::path(destination.string() + ".native-save.json");
  ASSERT_TRUE(std::filesystem::create_directory(new_receipt));
  const auto failed = manager_->SaveProjectAs(destination.string());
  ASSERT_FALSE(failed.ok());
  EXPECT_NE(failed.message().find("Project saved"), std::string::npos)
      << failed;
  EXPECT_EQ(manager_->GetCurrentProject()->filepath, destination.string());
  EXPECT_FALSE(manager_->IsCurrentProjectDirty());
  const auto retired = ReadReceipt(context_);
  EXPECT_EQ(retired.at("state"), "rebound");
  EXPECT_TRUE(retired.at("native_commit").is_null());
  EXPECT_EQ(ReadText(receipt_), old_receipt);

  std::filesystem::remove(new_receipt);
  const auto descriptor = ReadText(destination);
  ASSERT_OK(manager_->SaveCurrentSession());
  EXPECT_EQ(ReadText(destination), descriptor);
  const auto receipt = ReadReceipt(new_receipt);
  EXPECT_EQ(receipt.at("operation"), "save_as");
  EXPECT_EQ(receipt.at("generation"), 1u);
  ExpectArtifact(receipt, "project", destination);
  ExpectArtifact(receipt, "rom", source_);
  EXPECT_EQ(ReadReceipt(context_).at("state"), "committed");
}

// Autosave shares Save's boundary: with ROM work pending, the ROM write
// publishes the single receipt; a failed project-only completion is retried
// by the next autosave without new edits.
TEST_F(NativeSaveReceiptIntegrationTest,
       AutosaveDefersProjectReceiptToRomSaveAndRetriesCleanDraft) {
  ASSERT_NO_FATAL_FAILURE(BindProject());
  manager_->GetCurrentProject()->name = "Autosaved with the ROM";
  manager_->MarkCurrentProjectDirty();
  ASSERT_OK(manager_->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  ASSERT_OK(manager_->AutosaveActiveSession());
  EXPECT_FALSE(manager_->GetCurrentRom()->dirty());
  EXPECT_EQ(static_cast<uint8_t>(ReadText(source_)[0x1234]), 0xA5);
  const auto first = ReadReceipt(receipt_);
  EXPECT_EQ(first.at("generation"), 1u);
  EXPECT_EQ(first.at("operation"), "save");
  ExpectArtifact(first, "rom", source_);
  ExpectArtifact(first, "project", manager_->GetCurrentProject()->filepath);

  std::filesystem::remove(receipt_);
  ASSERT_TRUE(std::filesystem::create_directory(receipt_));
  manager_->GetCurrentProject()->name = "Autosaved before completion";
  manager_->MarkCurrentProjectDirty();
  EXPECT_FALSE(manager_->AutosaveActiveSession().ok());
  EXPECT_FALSE(manager_->IsCurrentProjectDirty());
  std::filesystem::remove(receipt_);
  ASSERT_OK(manager_->AutosaveActiveSession());
  const auto retried = ReadReceipt(receipt_);
  ExpectArtifact(retried, "project", manager_->GetCurrentProject()->filepath);
  ExpectArtifact(retried, "rom", source_);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       NativeReceiptExcludesLivePlanningButKeepsAliasedBuildRoles) {
  ASSERT_NO_FATAL_FAILURE(BindProject());
  const auto plan = cleanup_.path / "live.plan.json";
  const auto labels = cleanup_.path / "labels.json";
  std::ofstream(plan) << "{\"schema\":\"yaze.plan/1\"}";
  std::ofstream(labels) << "{}";
  auto* project = manager_->GetCurrentProject();
  project->planning.file = plan.string();
  project->labels_filename = labels.string();
  manager_->MarkCurrentProjectDirty();
  ASSERT_OK(manager_->SaveCurrentSession());
  const auto first = ReadReceipt(receipt_);
  for (const auto& artifact : first.at("artifacts")) {
    EXPECT_NE(artifact.at("role"), "dependency.planning") << first.dump(2);
  }
  ExpectArtifact(first, "dependency.labels_filename", labels);

  // A file that is also a build input stays bound under that role.
  project->planning.file = labels.string();
  manager_->MarkCurrentProjectDirty();
  ASSERT_OK(manager_->SaveCurrentSession());
  const auto second = ReadReceipt(receipt_);
  EXPECT_EQ(second.at("generation"), 2u);
  ExpectArtifact(second, "dependency.labels_filename", labels);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       HeaderedRomReceiptsBindCompleteNativeSaveAndSaveAsBytes) {
  auto body = ReadText(source_);
  ASSERT_EQ(body.size(), 512u * 1024u);
  std::string header(512, '\x5A');
  header.front() = 'S';
  header.back() = 'H';
  const auto headered_source = cleanup_.path / "headered.sfc";
  {
    std::ofstream output(headered_source, std::ios::binary);
    ASSERT_TRUE(output.is_open());
    output << header << body;
    ASSERT_TRUE(output.good());
  }
  ASSERT_OK(manager_->OpenRomOrProject(headered_source.string()));
  DisableRomWritesForTest();
  auto* rom = manager_->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_TRUE(rom->has_copier_header());
  EXPECT_EQ(
      std::string(rom->copier_header().begin(), rom->copier_header().end()),
      header);
  ASSERT_EQ(rom->size(), body.size());

  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));
  body[0x1234] = static_cast<char>(0xA5);
  const auto expected_source = header + body;
  ASSERT_OK(manager_->SaveCurrentSession());
  EXPECT_EQ(ReadText(headered_source), expected_source);
  const auto source_receipt =
      ReadReceipt(headered_source.string() + ".native-save.json");
  EXPECT_EQ(source_receipt.at("operation"), "save");
  ExpectArtifact(source_receipt, "rom", headered_source);
  EXPECT_NE(core::ComputeSourceArtifactSha256(expected_source),
            core::ComputeSourceArtifactSha256(body));

  ASSERT_OK(rom->WriteByte(0x1235, 0xB6));
  body[0x1235] = static_cast<char>(0xB6);
  const auto destination = cleanup_.path / "headered-copy.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  EXPECT_EQ(ReadText(destination), header + body);
  EXPECT_EQ(ReadText(headered_source), expected_source);
  const auto destination_receipt =
      ReadReceipt(destination.string() + ".native-save.json");
  EXPECT_EQ(destination_receipt.at("operation"), "save_as");
  ExpectArtifact(destination_receipt, "rom", destination);
  ASSERT_TRUE(rom->has_copier_header());
  EXPECT_EQ(
      std::string(rom->copier_header().begin(), rom->copier_header().end()),
      header);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       ProjectSaveAsCommitsNewOwnerWithoutChangingPriorReceiptOrSources) {
  ASSERT_NO_FATAL_FAILURE(BindProject());
  ASSERT_OK(manager_->SaveRom());
  const auto committed = ReadReceipt(receipt_);
  ASSERT_TRUE(committed.at("bindings_complete")) << committed.dump(2);
  ASSERT_TRUE(committed.at("binding_errors").empty()) << committed.dump(2);
  ASSERT_EQ(committed.at("generation"), 1u);
  const auto old_project = manager_->GetCurrentProject()->filepath;
  const auto context =
      core::GetNativeSaveContext(source_.string(), old_project);
  ASSERT_TRUE(context.ok()) << context.status();
  const auto context_path = context->path;
  const auto project_bytes = ReadText(old_project);
  const auto receipt_bytes = ReadText(receipt_);
  const auto rom_bytes = ReadText(source_);
  const auto active_context = ReadReceipt(context_path);
  ASSERT_EQ(active_context.at("state"), "committed");
  ASSERT_EQ(active_context.at("native_commit").at("receipt_path"),
            ReceiptPathText(receipt_));
  ASSERT_EQ(active_context.at("native_commit").at("generation"), 1u);

  const auto destination = cleanup_.path / "copy" / "Saved.yaze";
  ASSERT_TRUE(std::filesystem::create_directory(destination.parent_path()));
  ASSERT_OK(manager_->SaveProjectAs(destination.string()));
  // The old owner is retired and Save As completes the new owner's receipt.
  const auto new_receipt_path = destination.string() + ".native-save.json";
  const auto new_receipt = ReadReceipt(new_receipt_path);
  EXPECT_EQ(new_receipt.at("operation"), "save_as");
  EXPECT_EQ(new_receipt.at("generation"), 1u);
  EXPECT_TRUE(new_receipt.at("bindings_complete")) << new_receipt.dump(2);
  ExpectArtifact(new_receipt, "project", destination);
  ExpectArtifact(new_receipt, "rom", source_);
  ExpectProducer(new_receipt);
  const auto rebound = ReadReceipt(context_path);
  EXPECT_EQ(rebound.at("kind"), "yaze.native_save_context.v1");
  EXPECT_EQ(rebound.size(), 7u);
  EXPECT_EQ(rebound.at("editing_source_path"), ReceiptPathText(source_));
  EXPECT_EQ(rebound.at("producer"), active_context.at("producer"));
  EXPECT_EQ(rebound.at("state"), "committed");
  EXPECT_EQ(rebound.at("project_path"), ReceiptPathText(destination));
  EXPECT_EQ(rebound.at("rom_path"), ReceiptPathText(source_));
  EXPECT_EQ(rebound.at("native_commit").at("receipt_path"),
            ReceiptPathText(new_receipt_path));
  EXPECT_EQ(ReadText(old_project), project_bytes);
  EXPECT_EQ(ReadText(receipt_), receipt_bytes);
  EXPECT_EQ(ReadText(source_), rom_bytes);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       ContextFailureRefusesSameByteNativeCommit) {
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  const auto receipt_bytes = ReadText(receipt_);
  const auto context_bytes = ReadText(context_);
  const auto rom_bytes = ReadText(source_);
  const auto alias = cleanup_.path / "original-rom-link.sfc";
  std::filesystem::create_hard_link(source_, alias);
  {
    ScopedPublicationLockFailure failure(context_.parent_path());
    ASSERT_OK(failure.Install());
    const auto saved = manager_->SaveRom();
    EXPECT_FALSE(saved.ok()) << saved;
    EXPECT_TRUE(std::filesystem::equivalent(source_, alias));
    EXPECT_EQ(ReadText(source_), rom_bytes);
    EXPECT_EQ(ReadText(receipt_), receipt_bytes);
    EXPECT_EQ(ReadText(context_), context_bytes);
  }
  ASSERT_OK(manager_->SaveRom());
  EXPECT_FALSE(std::filesystem::equivalent(source_, alias));
  EXPECT_EQ(ReadReceipt(receipt_).at("generation"), 2u);
  EXPECT_EQ(ReadReceipt(context_).at("state"), "committed");
}

TEST_F(NativeSaveReceiptIntegrationTest,
       ReceiptFailureAfterSameByteCommitKeepsTransferRetired) {
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  const auto receipt_bytes = ReadText(receipt_);
  const auto committed = ReadReceipt(receipt_);
  const auto rom_bytes = ReadText(source_);
  const auto alias = cleanup_.path / "original-rom-link.sfc";
  std::filesystem::create_hard_link(source_, alias);
  {
    ScopedPublicationLockFailure failure(receipt_.parent_path());
    ASSERT_OK(failure.Install());
    const auto saved = manager_->SaveRom();
    EXPECT_FALSE(saved.ok()) << saved;
    EXPECT_FALSE(std::filesystem::equivalent(source_, alias));
    EXPECT_EQ(ReadText(source_), rom_bytes);
    EXPECT_EQ(ReadText(receipt_), receipt_bytes);
    const auto retired = ReadReceipt(context_);
    ASSERT_EQ(retired.at("state"), "rebound") << retired.dump(2);
    EXPECT_TRUE(retired.at("native_commit").is_null());
  }
  ASSERT_OK(manager_->SaveRom());
  const auto recovered = ReadReceipt(receipt_);
  EXPECT_EQ(recovered.at("generation"), 2u);
  EXPECT_NE(recovered.at("operation_id"), committed.at("operation_id"));
  const auto current = ReadReceipt(context_);
  EXPECT_EQ(current.at("state"), "committed");
  EXPECT_EQ(current.at("native_commit").at("generation"), 2u);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       ProjectSaveAsFailurePreservesCommittedOwner) {
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  const auto project = manager_->GetCurrentProject()->filepath;
  const auto project_bytes = ReadText(project);
  const auto receipt_bytes = ReadText(receipt_);
  const auto context_bytes = ReadText(context_);
  const auto rom_bytes = ReadText(source_);
  const auto destination = cleanup_.path / "copy.yaze";
  {
    ScopedPublicationLockFailure failure(context_.parent_path());
    ASSERT_OK(failure.Install());
    EXPECT_FALSE(manager_->SaveProjectAs(destination.string()).ok());
    EXPECT_EQ(manager_->GetCurrentProject()->filepath, project);
    EXPECT_FALSE(std::filesystem::exists(destination));
    EXPECT_EQ(ReadText(project), project_bytes);
    EXPECT_EQ(ReadText(receipt_), receipt_bytes);
    EXPECT_EQ(ReadText(context_), context_bytes);
    EXPECT_EQ(ReadText(source_), rom_bytes);
  }
  ASSERT_OK(manager_->SaveProjectAs(destination.string()));
  EXPECT_EQ(manager_->GetCurrentProject()->filepath, destination.string());
  const auto rebound = ReadReceipt(context_);
  EXPECT_EQ(rebound.at("state"), "committed");
  EXPECT_EQ(rebound.at("project_path"), ReceiptPathText(destination));
  EXPECT_EQ(ReadText(project), project_bytes);
  EXPECT_EQ(ReadText(receipt_), receipt_bytes);
  EXPECT_EQ(ReadText(source_), rom_bytes);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       SwitchFailurePreservesActiveSessionAndPendingSave) {
  const auto second = cleanup_.path / "second.sfc";
  WriteTestRom(second, "SECOND RECEIPT ROM");
  ASSERT_OK(manager_->OpenRomOrProject(second.string()));
  manager_->SwitchToSession(0);
  ASSERT_EQ(manager_->GetCurrentSessionIndex(), 0u);
  DisableRomWritesForTest();
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  const auto session_id = manager_->GetCurrentSessionId();
  auto* const rom = manager_->GetCurrentRom();
  auto* const second_rom =
      &static_cast<RomSession*>(manager_->session_coordinator()->GetSession(1))
           ->rom;
  const auto receipt_bytes = ReadText(receipt_);
  const auto context_bytes = ReadText(context_);
  const auto rom_bytes = ReadText(source_);
  {
    ScopedPublicationLockFailure failure(context_.parent_path());
    ASSERT_OK(failure.Install());
    manager_->SwitchToSession(1);
    ASSERT_EQ(manager_->GetCurrentSessionId(), session_id);
    manager_->session_coordinator()->SetActiveSessionIndex(1);
    ASSERT_EQ(manager_->GetCurrentSessionId(), session_id);
    EXPECT_EQ(manager_->GetCurrentRom(), rom);
    EXPECT_FALSE(manager_->SetCurrentRom(second_rom).ok());
    ASSERT_EQ(manager_->GetCurrentSessionId(), session_id);
    EXPECT_EQ(manager_->GetCurrentRom(), rom);

    auto* project = manager_->GetCurrentProject();
    project->rom_filename.clear();
    project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
    project->rom_metadata.expected_hash = "deadbeef";
    ASSERT_TRUE(manager_->IsRomHashMismatch());
    ASSERT_OK(rom->WriteByte(0x1234, 0xA5));
    ASSERT_TRUE(absl::IsCancelled(manager_->SaveRom()));
    manager_->session_coordinator()->SwitchToSession(1);
    ASSERT_EQ(manager_->GetCurrentSessionId(), session_id);
    EXPECT_TRUE(manager_->IsRomWriteConfirmPending());
    EXPECT_EQ(ReadText(receipt_), receipt_bytes);
    EXPECT_EQ(ReadText(context_), context_bytes);
    EXPECT_EQ(ReadText(source_), rom_bytes);
    manager_->CancelRomWriteConfirm();
    EXPECT_FALSE(manager_->IsRomWriteConfirmPending());
  }
  manager_->session_coordinator()->SetActiveSessionIndex(1);
  EXPECT_NE(manager_->GetCurrentSessionId(), session_id);
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), second.string());
  const auto retired = ReadReceipt(context_);
  EXPECT_EQ(retired.at("state"), "rebound");
  EXPECT_TRUE(retired.at("native_commit").is_null());
  EXPECT_EQ(ReadText(receipt_), receipt_bytes);
  EXPECT_EQ(ReadText(source_), rom_bytes);
}

TEST_F(NativeSaveReceiptIntegrationTest, CloseFailurePreservesWorkspace) {
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  const auto session_id = manager_->GetCurrentSessionId();
  auto* const rom = manager_->GetCurrentRom();
  const auto receipt_bytes = ReadText(receipt_);
  const auto context_bytes = ReadText(context_);
  const auto rom_bytes = ReadText(source_);
  {
    ScopedPublicationLockFailure failure(context_.parent_path());
    ASSERT_OK(failure.Install());
    manager_->CloseRom();
    ASSERT_EQ(manager_->GetActiveSessionCount(), 1u);
    EXPECT_EQ(manager_->GetCurrentSessionId(), session_id);
    EXPECT_EQ(manager_->GetCurrentRom(), rom);
    manager_->session_coordinator()->ClearAllSessions();
    ASSERT_EQ(manager_->GetActiveSessionCount(), 1u);
    EXPECT_FALSE(manager_->session_coordinator()
                     ->DiscardProvisionalSession(session_id)
                     .ok());
    EXPECT_EQ(manager_->GetCurrentRom(), rom);
    EXPECT_EQ(ReadText(receipt_), receipt_bytes);
    EXPECT_EQ(ReadText(context_), context_bytes);
    EXPECT_EQ(ReadText(source_), rom_bytes);
  }
  manager_->CloseRom();
  EXPECT_EQ(manager_->GetActiveSessionCount(), 0u);
  EXPECT_EQ(manager_->GetCurrentRom(), nullptr);
  const auto retired = ReadReceipt(context_);
  EXPECT_EQ(retired.at("state"), "rebound");
  EXPECT_TRUE(retired.at("native_commit").is_null());
  EXPECT_EQ(ReadText(receipt_), receipt_bytes);
  EXPECT_EQ(ReadText(source_), rom_bytes);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       SessionCreationFailureDoesNotInsertOrActivate) {
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  const auto session_id = manager_->GetCurrentSessionId();
  auto* const rom = manager_->GetCurrentRom();
  const auto receipt_bytes = ReadText(receipt_);
  const auto context_bytes = ReadText(context_);
  const auto second = cleanup_.path / "second.sfc";
  WriteTestRom(second, "SECOND RECEIPT ROM");
  Rom candidate;
  ASSERT_OK(candidate.LoadFromFile(second.string()));
  {
    ScopedPublicationLockFailure failure(context_.parent_path());
    ASSERT_OK(failure.Install());
    const auto created = manager_->session_coordinator()->CreateSessionFromRom(
        std::move(candidate), second.string());
    EXPECT_FALSE(created.ok());
    ASSERT_EQ(manager_->GetActiveSessionCount(), 1u);
    EXPECT_EQ(manager_->GetCurrentSessionId(), session_id);
    EXPECT_EQ(manager_->GetCurrentRom(), rom);
    EXPECT_TRUE(candidate.is_loaded());
    EXPECT_EQ(ReadText(receipt_), receipt_bytes);
    EXPECT_EQ(ReadText(context_), context_bytes);
  }
  const auto created = manager_->session_coordinator()->CreateSessionFromRom(
      std::move(candidate), second.string());
  ASSERT_TRUE(created.ok()) << created.status();
  EXPECT_EQ(manager_->GetActiveSessionCount(), 2u);
  EXPECT_NE(manager_->GetCurrentSessionId(), session_id);
  EXPECT_EQ(manager_->GetCurrentRom()->filename(), second.string());
  const auto retired = ReadReceipt(context_);
  EXPECT_EQ(retired.at("state"), "rebound");
  EXPECT_TRUE(retired.at("native_commit").is_null());
  EXPECT_EQ(ReadText(receipt_), receipt_bytes);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       SaveAndContinueRefusalDoesNotSaveTheWrongProject) {
  const auto second = cleanup_.path / "second.sfc";
  const auto target_project = cleanup_.path / "target.yaze";
  WriteTestRom(second, "SECOND RECEIPT ROM");
  ASSERT_OK(manager_->OpenRomOrProject(second.string()));
  DisableRomWritesForTest();
  auto* project = manager_->GetCurrentProject();
  project->name = "Saved target";
  project->metadata.project_id = "native_receipt_target";
  project->filepath = target_project.string();
  project->rom_filename = second.string();
  project->feature_flags = core::FeatureFlags::get();
  ASSERT_OK(project->Save());
  const auto target_bytes = ReadText(target_project);
  project->name = "Unsaved target";
  manager_->MarkCurrentProjectDirty();
  manager_->session_coordinator()->SetActiveSessionIndex(0);
  ASSERT_EQ(manager_->GetCurrentSessionIndex(), 0u);
  DisableRomWritesForTest();
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  const auto active_id = manager_->GetCurrentSessionId();
  const auto active_project = manager_->GetCurrentProject()->filepath;
  const auto active_bytes = ReadText(active_project);
  const auto receipt_bytes = ReadText(receipt_);
  const auto context_bytes = ReadText(context_);
  manager_->GetCurrentProject()->name = "Unsaved active project";
  manager_->MarkCurrentProjectDirty();
  manager_->RemoveSession(1);
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  {
    ScopedPublicationLockFailure failure(context_.parent_path());
    ASSERT_OK(failure.Install());
    manager_->ConfirmPendingUnsavedSessionActionSaveAndContinue();
    EXPECT_EQ(manager_->GetCurrentSessionId(), active_id);
    EXPECT_EQ(manager_->GetActiveSessionCount(), 2u);
    EXPECT_TRUE(manager_->IsCurrentProjectDirty());
    EXPECT_EQ(ReadText(active_project), active_bytes);
    EXPECT_EQ(ReadText(target_project), target_bytes);
    EXPECT_EQ(ReadText(receipt_), receipt_bytes);
    EXPECT_EQ(ReadText(context_), context_bytes);
  }
  manager_->RemoveSession(1);
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->ConfirmPendingUnsavedSessionActionSaveAndContinue();
  EXPECT_EQ(manager_->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager_->GetCurrentSessionId(), active_id);
  EXPECT_TRUE(manager_->IsCurrentProjectDirty());
  EXPECT_EQ(ReadText(active_project), active_bytes);
  project::YazeProject saved_target;
  ASSERT_OK(saved_target.Open(target_project.string()));
  EXPECT_EQ(saved_target.name, "Unsaved target");
  EXPECT_EQ(ReadText(receipt_), receipt_bytes);
}

TEST_F(NativeSaveReceiptIntegrationTest,
       RevertActivationRefusalDoesNotRevertTheWrongRom) {
  const auto second = cleanup_.path / "second.sfc";
  WriteTestRom(second, "SECOND RECEIPT ROM");
  ASSERT_OK(manager_->OpenRomOrProject(second.string()));
  manager_->SwitchToSession(0);
  ASSERT_EQ(manager_->GetCurrentSessionIndex(), 0u);
  DisableRomWritesForTest();
  ASSERT_NO_FATAL_FAILURE(CommitBaseline());
  auto* const target_rom = manager_->GetCurrentRom();
  ASSERT_OK(target_rom->WriteByte(0x1234, 0xA5));
  ASSERT_OK(manager_->RevertRomToSaved());
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->session_coordinator()->SetActiveSessionIndex(1);
  ASSERT_EQ(manager_->GetCurrentSessionIndex(), 1u);
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  DisableRomWritesForTest();
  const auto other_project = cleanup_.path / "other.yaze";
  auto* project = manager_->GetCurrentProject();
  project->name = "Other active project";
  project->metadata.project_id = "native_receipt_other";
  project->filepath = other_project.string();
  project->rom_filename = second.string();
  project->feature_flags = core::FeatureFlags::get();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kAllow;
  ASSERT_OK(project->Save());
  ASSERT_OK(manager_->SaveRom());
  const auto other_receipt = other_project.string() + ".native-save.json";
  const auto receipt_bytes = ReadText(other_receipt);
  const auto other_context =
      core::GetNativeSaveContext(second.string(), other_project.string());
  ASSERT_TRUE(other_context.ok()) << other_context.status();
  const auto context_bytes = ReadText(other_context->path);
  const auto active_id = manager_->GetCurrentSessionId();
  auto* const active_rom = manager_->GetCurrentRom();
  ASSERT_OK(active_rom->WriteByte(0x1234, 0xB6));
  const auto active_bytes = active_rom->vector();
  const auto target_bytes = target_rom->vector();
  const auto disk_bytes = ReadText(second);
  {
    ScopedPublicationLockFailure failure(other_context->path.parent_path());
    ASSERT_OK(failure.Install());
    manager_->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
    EXPECT_EQ(manager_->GetCurrentSessionId(), active_id);
    EXPECT_EQ(manager_->GetCurrentRom(), active_rom);
    EXPECT_EQ(active_rom->vector(), active_bytes);
    EXPECT_TRUE(active_rom->dirty());
    EXPECT_EQ(target_rom->vector(), target_bytes);
    EXPECT_EQ(ReadText(second), disk_bytes);
    EXPECT_EQ(ReadText(other_receipt), receipt_bytes);
    EXPECT_EQ(ReadText(other_context->path), context_bytes);
  }
  manager_->session_coordinator()->SetActiveSessionIndex(0);
  ASSERT_EQ(manager_->GetCurrentRom(), target_rom);
  ASSERT_OK(manager_->RevertRomToSaved());
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_EQ(target_rom->ReadByte(0x1234).value(), 0u);
  EXPECT_FALSE(target_rom->dirty());
  EXPECT_EQ(active_rom->vector(), active_bytes);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_EQ(ReadText(second), disk_bytes);
  EXPECT_EQ(ReadText(other_receipt), receipt_bytes);
}
#endif  // YAZE_WITH_JSON

class RomLifecycleRebindPolicyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = MakeTempFilePath("yaze_rebind_policy");
    std::filesystem::create_directories(root_ / "Roms");
    std::filesystem::create_directories(root_ / "Other");
    development_ = root_ / "Roms" / "dev.sfc";
    alternate_ = root_ / "Other" / "dev.sfc";
    output_ = root_ / "Roms" / "devx.sfc";
    for (const auto& path : {development_, alternate_, output_}) {
      WriteTestRom(path);
    }
    ASSERT_OK(rom_.LoadFromFile(development_.string()));
    project_.name = "Rebind Policy";
    project_.filepath = (root_ / "project.yaze").string();
    project_.rom_filename = development_.string();
    project_.build_target = output_.string();
    project_.rom_metadata.expected_hash = "unchanged expectation";
    lifecycle_.SetProjectContext(&project_);
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }

  void LoadManifest() {
    ASSERT_OK(project_.hack_manifest.LoadFromString(R"json({
      "manifest_version": 2,
      "hack_name": "Rebind Policy",
      "build_pipeline": {
        "dev_rom": "Roms/dev.sfc",
        "patched_rom": "Roms/devx.sfc"
      }
    })json"));
  }

  std::filesystem::path root_, development_, alternate_, output_;
  project::YazeProject project_;
  Rom rom_;
  RomLifecycleManager lifecycle_;
};

TEST_F(RomLifecycleRebindPolicyTest, ReadOnlyBlocksEditableSourceAndSaveAs) {
  project_.rom_metadata.write_policy = project::RomWritePolicy::kReadOnly;
  for (bool with_manifest : {false, true}) {
    if (with_manifest)
      LoadManifest();
    // A configured editable source is normally exempt from hash checks.
    for (const std::string expected :
         {std::string(), std::string("deadbeef")}) {
      project_.rom_metadata.expected_hash = expected;
      lifecycle_.ConfirmRomWrite();
      EXPECT_EQ(lifecycle_.CheckRomWritePolicy(&rom_).code(),
                absl::StatusCode::kPermissionDenied);
      EXPECT_EQ(
          lifecycle_.CheckRomWritePolicy(&rom_, alternate_.string()).code(),
          absl::StatusCode::kPermissionDenied);
      EXPECT_EQ(lifecycle_.CheckRomWritePolicy(&rom_, output_.string()).code(),
                absl::StatusCode::kPermissionDenied);
    }
  }
}

TEST_F(RomLifecycleRebindPolicyTest, NoFixedManifestRetainsGenericRebind) {
  rom_.set_filename(alternate_.string());
  for (const auto policy :
       {project::RomWritePolicy::kBlock, project::RomWritePolicy::kWarn,
        project::RomWritePolicy::kAllow}) {
    project_.rom_metadata.write_policy = policy;
    EXPECT_TRUE(lifecycle_.CheckRomRebindPolicy(&rom_).ok());
  }
  ASSERT_OK(project_.hack_manifest.LoadFromString(
      R"json({"manifest_version":2,"hack_name":"No fixed development ROM"})json"));
  project_.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  EXPECT_TRUE(lifecycle_.CheckRomRebindPolicy(&rom_).ok());
}

TEST_F(RomLifecycleRebindPolicyTest, GuardedManifestRequiresExactBackingFile) {
  LoadManifest();
  for (const auto policy :
       {project::RomWritePolicy::kBlock, project::RomWritePolicy::kWarn}) {
    project_.rom_metadata.write_policy = policy;
    rom_.set_filename(development_.string());
    EXPECT_TRUE(lifecycle_.CheckRomRebindPolicy(&rom_).ok());
    // Same basename and byte-identical content are not the same backing file.
    rom_.set_filename(alternate_.string());
    const auto status = lifecycle_.CheckRomRebindPolicy(&rom_);
    EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
    EXPECT_NE(status.message().find("Project source binding"),
              absl::string_view::npos);
    EXPECT_NE(status.message().find(development_.string()),
              absl::string_view::npos);
    EXPECT_NE(status.message().find(alternate_.string()),
              absl::string_view::npos);
    EXPECT_FALSE(lifecycle_.IsRomWriteConfirmPending());
    EXPECT_EQ(project_.rom_filename, development_.string());
    EXPECT_EQ(project_.rom_metadata.expected_hash, "unchanged expectation");
  }
}

TEST_F(RomLifecycleRebindPolicyTest, GuardedManifestAcceptsEquivalentAliases) {
  LoadManifest();
  project_.rom_metadata.write_policy = project::RomWritePolicy::kBlock;
  rom_.set_filename((root_ / "Roms" / "." / "dev.sfc").string());
  EXPECT_TRUE(lifecycle_.CheckRomRebindPolicy(&rom_).ok());

  const auto hard_alias = root_ / "hard-alias.sfc";
  std::filesystem::create_hard_link(development_, hard_alias);
  rom_.set_filename(hard_alias.string());
  EXPECT_TRUE(lifecycle_.CheckRomRebindPolicy(&rom_).ok());
#ifndef _WIN32
  const auto symlink_alias = root_ / "symlink-alias.sfc";
  std::filesystem::create_symlink(development_, symlink_alias);
  rom_.set_filename(symlink_alias.string());
  EXPECT_TRUE(lifecycle_.CheckRomRebindPolicy(&rom_).ok());
#endif
}

#ifndef _WIN32
TEST_F(RomLifecycleRebindPolicyTest,
       ManifestSymlinkKeepsCanonicalBindingWritable) {
  const auto manifest_alias = root_ / "Roms" / "manifest-dev-alias.sfc";
  std::filesystem::create_symlink(development_, manifest_alias);
  ASSERT_OK(project_.hack_manifest.LoadFromString(R"json({
    "manifest_version": 2,
    "hack_name": "Rebind Policy",
    "build_pipeline": {
      "dev_rom": "Roms/manifest-dev-alias.sfc",
      "patched_rom": "Roms/devx.sfc"
    }
  })json"));
  // Swap binds the accepted alias to the canonical development-ROM path.
  rom_.set_filename(std::filesystem::canonical(manifest_alias).string());
  project_.rom_filename = rom_.filename();
  RomSession session(std::move(rom_));
  ASSERT_TRUE(session.rom_baseline.has_value());
  lifecycle_.ActivateRomIdentity(&session);
  ASSERT_FALSE(lifecycle_.current_rom_hash().empty());
  const std::string expected_hash =
      lifecycle_.current_rom_hash() == "00000000" ? "ffffffff" : "00000000";
  project_.rom_metadata.expected_hash = expected_hash;
  ASSERT_EQ(session.rom_baseline->hashes.Match(expected_hash),
            util::RomHashMatch::kMismatch);

  for (const auto policy :
       {project::RomWritePolicy::kBlock, project::RomWritePolicy::kWarn}) {
    project_.rom_metadata.write_policy = policy;
    EXPECT_OK(lifecycle_.CheckRomRebindPolicy(&session.rom));
    EXPECT_OK(lifecycle_.CheckRomWritePolicy(&session.rom));
    EXPECT_FALSE(lifecycle_.IsRomHashMismatch());
    EXPECT_FALSE(lifecycle_.IsRomWriteConfirmPending());
  }
  EXPECT_EQ(project_.rom_metadata.expected_hash, expected_hash);
  EXPECT_EQ(project_.hack_manifest.build_pipeline().dev_rom,
            "Roms/manifest-dev-alias.sfc");
}
#endif

TEST_F(RomLifecycleRebindPolicyTest,
       OutputProtectionAndAllowPolicyAreUnchanged) {
  LoadManifest();
  rom_.set_filename(output_.string());
  for (const auto policy :
       {project::RomWritePolicy::kBlock, project::RomWritePolicy::kWarn}) {
    project_.rom_metadata.write_policy = policy;
    const auto status = lifecycle_.CheckRomRebindPolicy(&rom_);
    EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
    EXPECT_NE(status.message().find("build output"), absl::string_view::npos);
  }
  project_.rom_metadata.write_policy = project::RomWritePolicy::kAllow;
  EXPECT_TRUE(lifecycle_.CheckRomRebindPolicy(&rom_).ok());
  rom_.set_filename(alternate_.string());
  EXPECT_TRUE(lifecycle_.CheckRomRebindPolicy(&rom_).ok());
  EXPECT_EQ(project_.rom_metadata.expected_hash, "unchanged expectation");
}

TEST(EditorManagerBackingFileIdentityTest,
     RawRomOpenActivatesLexicalAliasOfActiveBackingFile) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_path = MakeTempFilePath("yaze_lexical_identity.sfc");
  const auto alias_dir = MakeTempFilePath("yaze_lexical_alias_dir");
  ScopedFileCleanup rom_cleanup{rom_path};
  ScopedDirectoryCleanup alias_cleanup{alias_dir};
  WriteTestRom(rom_path, "LEXICAL IDENTITY");
  ASSERT_TRUE(std::filesystem::create_directory(alias_dir));

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_OK(active_rom->WriteByte(0x100, 0xA5));
  const auto alias_path = alias_dir / ".." / rom_path.filename();
  const auto status = manager->OpenRomOrProject(alias_path.string());

  EXPECT_TRUE(status.ok()) << status;
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  ASSERT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->vector()[0x100], 0xA5);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_EQ(manager->GetCurrentRom()->filename(), rom_path.string());
}

TEST(EditorManagerBackingFileIdentityTest,
     RawRomOpenActivatesEquivalentHardLinkOfActiveBackingFile) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_path = MakeTempFilePath("yaze_canonical_identity.sfc");
  const auto hard_link_path = MakeTempFilePath("yaze_canonical_alias.sfc");
  ScopedFileCleanup rom_cleanup{rom_path};
  ScopedFileCleanup hard_link_cleanup{hard_link_path};
  WriteTestRom(rom_path, "CANONICAL IDENTITY");

  std::error_code link_ec;
  std::filesystem::create_hard_link(rom_path, hard_link_path, link_ec);
  if (link_ec) {
    GTEST_SKIP() << "Hard links unavailable: " << link_ec.message();
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  Rom* const active_rom = manager->GetCurrentRom();
  const auto status = manager->OpenRomOrProject(hard_link_path.string());

  EXPECT_TRUE(status.ok()) << status;
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
}

TEST(EditorManagerBackingFileIdentityTest,
     ProjectOpenRejectsRomOwnedByActiveSessionWithoutReplacingContext) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_path = MakeTempFilePath("yaze_project_identity.sfc");
  auto project_path = MakeTempFilePath("yaze_project_identity");
  project_path += ".yaze";
  ScopedFileCleanup rom_cleanup{rom_path};
  ScopedFileCleanup project_cleanup{project_path};
  WriteTestRom(rom_path, "PROJECT IDENTITY");
  WriteTestProject(project_path, rom_path);

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  Rom* const active_rom = manager->GetCurrentRom();
  const std::string active_project_path =
      manager->GetCurrentProject()->filepath;

  const auto status = manager->OpenRomOrProject(project_path.string());

  EXPECT_EQ(status.code(), absl::StatusCode::kAlreadyExists) << status;
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(manager->GetCurrentProject()->filepath, active_project_path);
}

TEST(EditorManagerBackingFileIdentityTest,
     SaveRomAsRejectsOtherSessionButAllowsCurrentBackingFileAlias) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_a = MakeTempFilePath("yaze_save_as_owner_a.sfc");
  const auto rom_b = MakeTempFilePath("yaze_save_as_owner_b.sfc");
  const auto alias_dir = MakeTempFilePath("yaze_save_as_alias_dir");
  ScopedFileCleanup cleanup_a{rom_a};
  ScopedFileCleanup cleanup_b{rom_b};
  ScopedDirectoryCleanup alias_cleanup{alias_dir};
  WriteTestRom(rom_a, "SAVE AS OWNER A");
  WriteTestRom(rom_b, "SAVE AS OWNER B");
  ASSERT_TRUE(std::filesystem::create_directory(alias_dir));
  const auto rom_a_alias = alias_dir / ".." / rom_a.filename();

  ASSERT_OK(manager->OpenRomOrProject(rom_a.string()));
  ASSERT_OK(manager->OpenRomOrProject(rom_b.string()));
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  Rom* const rom_b_buffer = manager->GetCurrentRom();
  ASSERT_NE(rom_b_buffer, nullptr);

  const auto collision = manager->SaveRomAs(rom_a_alias.string());
  EXPECT_EQ(collision.code(), absl::StatusCode::kAlreadyExists) << collision;
  EXPECT_EQ(rom_b_buffer->filename(), rom_b.string());
  auto* session_b =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(1));
  ASSERT_NE(session_b, nullptr);
  EXPECT_EQ(session_b->filepath, rom_b.string());

  manager->SwitchToSession(0);
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  DisableRomWritesForTest();
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "Save As Current Owner";
  project->filepath =
      (rom_a.parent_path() / "save_as_current_owner.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kAllow;
  project->rom_metadata.expected_hash.clear();

  ASSERT_OK(manager->SaveRomAs(rom_a_alias.string()));
  EXPECT_EQ(manager->GetCurrentRom()->filename(), rom_a_alias.string());
}

TEST(EditorManagerRomWritePolicyTest,
     SaveRomWarnsOnHashMismatchAndConfirmsOnce) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_rom_write_policy_warn.sfc");
  ScopedFileCleanup cleanup{rom_path};

  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  const std::string title = "YAZE TEST ROM";
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(rom_data.data()),
              static_cast<std::streamsize>(rom_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();  // OpenRomOrProject resets feature flags.

  // Mark a project as opened so the save-time write policy check is active.
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "RomWritePolicyWarn";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;

  // Ensure we have a baseline ROM hash, then force a mismatch.
  ASSERT_FALSE(manager->GetCurrentRomHash().empty());
  project->rom_metadata.expected_hash = "deadbeef";
  ASSERT_TRUE(manager->IsRomHashMismatch());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_TRUE(rom->is_loaded());

  constexpr uint32_t kPcOffset = 0x1234;
  const uint8_t original = rom_data[kPcOffset];
  const uint8_t mutated = static_cast<uint8_t>(original ^ 0xFF);
  ASSERT_OK(rom->WriteByte(static_cast<int>(kPcOffset), mutated));

  // First save attempt should be blocked awaiting confirmation.
  auto status = manager->SaveRom();
  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled) << status;
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            original);

  // Confirm once and resume the bound request: should save successfully.
  manager->ConfirmRomWrite();
  EXPECT_FALSE(manager->IsRomWriteConfirmPending());

  auto status2 = manager->ResumePendingRomSave();
  EXPECT_TRUE(status2.ok()) << status2.message();
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            mutated);
}

TEST(EditorManagerBackupRestoreTest,
     AutomaticBackupRestoreStagesAndCommitsAReversibleRoundTrip) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_round_trip");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "BACKUP RESTORE");
  {
    std::ofstream labels(rom_path.string() + ".labels",
                         std::ios::out | std::ios::trunc);
    ASSERT_TRUE(labels.is_open());
    labels << "[rooms]\n0=On-disk room label\n";
    ASSERT_TRUE(labels.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);
  active_rom->resource_label()->EditLabel("rooms", "0", "In-memory room label");
  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(active_rom->dirty());
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);

  const auto initial_backups = manager->GetRomBackups();
  ASSERT_EQ(initial_backups.size(), 1u);
  EXPECT_EQ(ReadByteAt(initial_backups.front().path, kPcOffset), kOriginal);

  ASSERT_OK(manager->RestoreRomBackup(initial_backups.front().path));
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->filename(),
            std::filesystem::absolute(rom_path).string());
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_EQ(active_rom->resource_label()->GetLabel("rooms", "0"),
            "In-memory room label");
  EXPECT_EQ(active_rom->resource_label()->filename_,
            std::filesystem::absolute(rom_path).string() + ".labels");
  // Restore is staged: the backing file remains untouched until Save ROM.
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  EXPECT_TRUE(session->backup_restore_pending);

  // Autosave is enabled by default, but a staged restore must require an
  // explicit decision even when automatic backups are disabled.
  manager->user_settings().prefs().backup_before_save = false;
  const auto autosave_status = manager->AutosaveActiveSession();
  EXPECT_EQ(autosave_status.code(), absl::StatusCode::kCancelled)
      << autosave_status;
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
  manager->user_settings().prefs().backup_before_save = true;

  // Backup names include millisecond precision; ensure this second save gets
  // its own entry even on very fast filesystems.
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(active_rom->dirty());
  EXPECT_FALSE(session->backup_restore_pending);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kOriginal);

  const auto final_backups = manager->GetRomBackups();
  ASSERT_EQ(final_backups.size(), 2u);
  bool preserved_original = false;
  bool preserved_edited = false;
  for (const auto& backup : final_backups) {
    const uint8_t value = ReadByteAt(backup.path, kPcOffset);
    preserved_original |= value == kOriginal;
    preserved_edited |= value == kEdited;
  }
  EXPECT_TRUE(preserved_original);
  EXPECT_TRUE(preserved_edited);
}

TEST(EditorManagerBackupRestoreTest,
     RestoreRefreshesMaterializedDungeonStateInPlace) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_dungeon_state");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  constexpr uint8_t kBackupPalette = 0x03;
  constexpr uint8_t kEditedPalette = 0x2A;
  constexpr uint16_t kBackupEntranceRoom = 0x0012;
  constexpr uint16_t kEditedEntranceRoom = 0x0023;
  constexpr uint16_t kBackupSpawnRoom = 0x0034;
  constexpr uint16_t kEditedSpawnRoom = 0x0045;
  WriteDungeonHeaderTestRom(rom_path, kBackupPalette, kBackupEntranceRoom,
                            kBackupSpawnRoom);

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().dungeon.kUseWorkbench = false;
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kDungeon));

  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);
  auto* dungeon = static_cast<DungeonEditorV2*>(
      manager->GetCurrentEditorSet()->GetEditor(EditorType::kDungeon));
  ASSERT_NE(dungeon, nullptr);

  dungeon->rooms()[0] = zelda3::LoadRoomHeaderFromRom(active_rom, 0);
  dungeon->rooms()[0].ClearSaveDirtyState();
  ASSERT_EQ(dungeon->rooms()[0].palette(), kBackupPalette);
  auto* const room_before = dungeon->rooms().GetIfMaterialized(0);
  auto* const viewer_before =
      DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0);
  ASSERT_NE(room_before, nullptr);
  ASSERT_NE(viewer_before, nullptr);
  DungeonEditorV2ReloadTestPeer::SyncPanelsToRoom(dungeon, 0);
  auto* const overlay_panel_before =
      DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon);
  ASSERT_NE(overlay_panel_before, nullptr);
  bool* const overlay_toggle_before = viewer_before->mutable_show_grid();
  ASSERT_NE(overlay_toggle_before, nullptr);
  ASSERT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayGridBinding(dungeon),
            overlay_toggle_before);
  ASSERT_TRUE(DungeonEditorV2ReloadTestPeer::HasRoomViewer(*dungeon, 0));
  ASSERT_OK(dungeon->RefreshRomBackedState());

  auto* const entrance_before =
      DungeonEditorV2ReloadTestPeer::GetRegularEntrance(dungeon, 0);
  auto* const spawn_before =
      DungeonEditorV2ReloadTestPeer::GetSpawnPoint(dungeon, 0);
  auto object_editor_before =
      DungeonEditorV2ReloadTestPeer::GetObjectEditor(dungeon);
  ASSERT_NE(object_editor_before, nullptr);
  ASSERT_EQ(dungeon->rooms().GetIfMaterialized(0), room_before);
  ASSERT_EQ(DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0),
            viewer_before);
  ASSERT_EQ(viewer_before->mutable_show_grid(), overlay_toggle_before);
  ASSERT_EQ(object_editor_before->GetMutableRoom(), room_before);
  ASSERT_EQ(entrance_before->room_, kBackupEntranceRoom);
  ASSERT_EQ(spawn_before->room_id, kBackupSpawnRoom);

  ASSERT_OK(active_rom->WriteByte(kDungeonRoom0HeaderPc + 1, kEditedPalette));
  ASSERT_OK(active_rom->WriteShort(zelda3::kEntranceRoom, kEditedEntranceRoom));
  ASSERT_OK(
      active_rom->WriteShort(zelda3::kDungeonSpawnRoom, kEditedSpawnRoom));
  ASSERT_OK(manager->SaveRom());
  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_EQ(ReadByteAt(backups.front().path, kDungeonRoom0HeaderPc + 1),
            kBackupPalette);

  ASSERT_OK(dungeon->RefreshRomBackedState());
  ASSERT_EQ(room_before->palette(), kEditedPalette);
  ASSERT_EQ(entrance_before->room_, kEditedEntranceRoom);
  ASSERT_EQ(spawn_before->room_id, kEditedSpawnRoom);
  ASSERT_EQ(dungeon->rooms().GetIfMaterialized(0), room_before);
  ASSERT_EQ(DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0),
            viewer_before);
  ASSERT_TRUE(DungeonEditorV2ReloadTestPeer::HasRoomViewer(*dungeon, 0));

  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kDungeonRoom0HeaderPc + 1).ok());
  EXPECT_EQ(*active_rom->ReadByte(kDungeonRoom0HeaderPc + 1), kBackupPalette);
  EXPECT_EQ(dungeon->rooms().GetIfMaterialized(0), room_before);
  EXPECT_EQ(room_before->palette(), kBackupPalette);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0),
            viewer_before);
  EXPECT_EQ(viewer_before->mutable_show_grid(), overlay_toggle_before);
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  ASSERT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon),
            overlay_panel_before);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayGridBinding(dungeon),
            overlay_toggle_before);
  EXPECT_EQ(viewer_before->rom(), active_rom);
  EXPECT_EQ(viewer_before->rooms(), &dungeon->rooms());
  auto object_editor_after =
      DungeonEditorV2ReloadTestPeer::GetObjectEditor(dungeon);
  EXPECT_EQ(object_editor_after.get(), object_editor_before.get());
  EXPECT_EQ(object_editor_after->GetMutableRoom(), room_before);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetRegularEntrance(dungeon, 0),
            entrance_before);
  EXPECT_EQ(entrance_before->room_, kBackupEntranceRoom);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetSpawnPoint(dungeon, 0),
            spawn_before);
  EXPECT_EQ(spawn_before->room_id, kBackupSpawnRoom);

  // Desktop/full asset loading reconstructs the dungeon panels and editor
  // system before applying the same in-place room/viewer refresh. Exercise
  // that path explicitly so the replacement panels cannot retain stale
  // bindings after restore.
  object_editor_before.reset();
  ASSERT_OK(active_rom->WriteByte(kDungeonRoom0HeaderPc + 1, kEditedPalette));
  ASSERT_OK(active_rom->WriteShort(zelda3::kEntranceRoom, kEditedEntranceRoom));
  ASSERT_OK(
      active_rom->WriteShort(zelda3::kDungeonSpawnRoom, kEditedSpawnRoom));
  ASSERT_OK(manager->SaveRom());
  AppConfig full_mode_config;
  full_mode_config.startup_editor = "dungeon";
  manager->SetStartupLoadHints(full_mode_config);
  manager->SetAssetLoadMode(AssetLoadMode::kFull);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));

  EXPECT_EQ(dungeon->rooms().GetIfMaterialized(0), room_before);
  EXPECT_EQ(room_before->palette(), kBackupPalette);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetViewerForRoom(dungeon, 0),
            viewer_before);
  auto* const full_mode_overlay_panel =
      DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon);
  ASSERT_NE(full_mode_overlay_panel, nullptr);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayGridBinding(dungeon),
            overlay_toggle_before);
  auto full_mode_object_editor =
      DungeonEditorV2ReloadTestPeer::GetObjectEditor(dungeon);
  ASSERT_NE(full_mode_object_editor, nullptr);
  EXPECT_EQ(full_mode_object_editor->GetMutableRoom(), room_before);
  EXPECT_EQ(entrance_before->room_, kBackupEntranceRoom);
  EXPECT_EQ(spawn_before->room_id, kBackupSpawnRoom);

  // A full reload can skip an already-initialized dungeon editor when another
  // startup surface is active. Its refreshed state must remain marked loaded,
  // otherwise the next activation reconstructs the preserved panels again.
  auto* const full_mode_object_editor_ptr = full_mode_object_editor.get();
  full_mode_object_editor.reset();
  ASSERT_OK(active_rom->WriteByte(kDungeonRoom0HeaderPc + 1, kEditedPalette));
  ASSERT_OK(active_rom->WriteShort(zelda3::kEntranceRoom, kEditedEntranceRoom));
  ASSERT_OK(
      active_rom->WriteShort(zelda3::kDungeonSpawnRoom, kEditedSpawnRoom));
  ASSERT_OK(manager->SaveRom());
  AppConfig inactive_dungeon_config;
  inactive_dungeon_config.startup_editor = "assembly";
  manager->SetStartupLoadHints(inactive_dungeon_config);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));

  auto* const restored_session =
      manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(restored_session, nullptr);
  const size_t dungeon_index = EditorTypeIndex(EditorType::kDungeon);
  EXPECT_TRUE(restored_session->editor_initialized[dungeon_index]);
  EXPECT_TRUE(restored_session->editor_assets_loaded[dungeon_index]);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon),
            full_mode_overlay_panel);
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kDungeon));
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayManagerPanel(dungeon),
            full_mode_overlay_panel);
  auto inactive_full_mode_object_editor =
      DungeonEditorV2ReloadTestPeer::GetObjectEditor(dungeon);
  EXPECT_EQ(inactive_full_mode_object_editor.get(),
            full_mode_object_editor_ptr);
  EXPECT_EQ(inactive_full_mode_object_editor->GetMutableRoom(), room_before);
  EXPECT_EQ(DungeonEditorV2ReloadTestPeer::GetOverlayGridBinding(dungeon),
            overlay_toggle_before);
  EXPECT_EQ(room_before->palette(), kBackupPalette);
  EXPECT_EQ(entrance_before->room_, kBackupEntranceRoom);
  EXPECT_EQ(spawn_before->room_id, kBackupSpawnRoom);
}

TEST(EditorManagerBackupRestoreTest,
     RestoreRejectsDirtySessionAndAllowsManagedSizeChange) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_guards");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  const auto backup_path = temp_dir / "oracle-copy_backup_manual.sfc";
  const auto corrupt_backup_path = temp_dir / "oracle-copy_backup_corrupt.sfc";
  const auto expanded_backup_path =
      temp_dir / "oracle-copy_backup_size-change.sfc";
  const auto unrelated_path = temp_dir / "unrelated.sfc";
  WriteTestRom(rom_path, "RESTORE GUARDS");
  std::filesystem::copy_file(rom_path, backup_path);
  std::filesystem::copy_file(rom_path, unrelated_path);
  {
    std::ofstream corrupt(corrupt_backup_path,
                          std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(corrupt.is_open());
    corrupt.put('\0');
    ASSERT_TRUE(corrupt.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);
  constexpr uint32_t kPcOffset = 0x1234;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, 0xA5));

  const auto dirty_status = manager->RestoreRomBackup(backup_path.string());
  EXPECT_EQ(dirty_status.code(), absl::StatusCode::kFailedPrecondition)
      << dirty_status;
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), 0xA5);
  active_rom->ClearDirty();

  const auto unrelated_status =
      manager->RestoreRomBackup(unrelated_path.string());
  EXPECT_EQ(unrelated_status.code(), absl::StatusCode::kInvalidArgument)
      << unrelated_status;
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->size(), 512u * 1024u);

  const std::string title_before_corrupt_restore = active_rom->title();
  const auto corrupt_status =
      manager->RestoreRomBackup(corrupt_backup_path.string());
  EXPECT_EQ(corrupt_status.code(), absl::StatusCode::kInvalidArgument)
      << corrupt_status;
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->size(), 512u * 1024u);
  EXPECT_EQ(active_rom->title(), title_before_corrupt_restore);
  EXPECT_FALSE(active_rom->dirty());

  std::vector<uint8_t> expanded_data(1024 * 1024, 0x00);
  const std::string expanded_title = "RESTORE EXPANDED";
  for (size_t i = 0; i < expanded_title.size(); ++i) {
    expanded_data[0x7FC0 + i] = static_cast<uint8_t>(expanded_title[i]);
  }
  {
    std::ofstream out(expanded_backup_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(expanded_data.data()),
              static_cast<std::streamsize>(expanded_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->RestoreRomBackup(expanded_backup_path.string()));
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->size(), 1024u * 1024u);
  EXPECT_NE(active_rom->title().find(expanded_title), std::string::npos);
  EXPECT_TRUE(active_rom->dirty());
}

TEST(EditorManagerBackupRestoreTest,
     RestoreRejectsProjectBuildOutputBackingPath) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_build_output");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto dev_rom_path = temp_dir / "oracle-dev.sfc";
  const auto build_rom_path = temp_dir / "oracle-build.sfc";
  WriteTestRom(dev_rom_path, "RESTORE DEV");
  WriteTestRom(build_rom_path, "RESTORE BUILD");

  ASSERT_OK(manager->OpenRomOrProject(build_rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "Backup Restore Build Output";
  project->filepath = (temp_dir / "project.yaze").string();
  project->rom_filename = dev_rom_path.filename().string();
  project->build_target = build_rom_path.filename().string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;

  const auto restore_status = manager->RestoreRomBackup(backups.front().path);

  EXPECT_EQ(restore_status.code(), absl::StatusCode::kFailedPrecondition)
      << restore_status;
  EXPECT_NE(std::string(restore_status.message()).find("build output"),
            std::string::npos);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kEdited);
  EXPECT_FALSE(active_rom->dirty());
  EXPECT_FALSE(manager->IsRomBackupRestorePending());
  EXPECT_EQ(ReadByteAt(build_rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerBackupRestoreTest,
     DiscardPendingRomBackupRestoreReloadsBackingWithoutWriting) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_discard");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "RESTORE DISCARD");
  {
    std::ofstream labels(rom_path.string() + ".labels",
                         std::ios::out | std::ios::trunc);
    ASSERT_TRUE(labels.is_open());
    labels << "[rooms]\n0=On-disk room label\n";
    ASSERT_TRUE(labels.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);
  active_rom->resource_label()->EditLabel("rooms", "0", "In-memory room label");

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());
  const std::string backing_hash = manager->GetCurrentRomHash();
  const size_t session_id = manager->GetCurrentSessionId();

  const auto no_restore_status = manager->DiscardPendingRomBackupRestore();
  EXPECT_EQ(no_restore_status.code(), absl::StatusCode::kFailedPrecondition)
      << no_restore_status;
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
  EXPECT_NE(manager->GetCurrentRomHash(), backing_hash);

  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);
  ASSERT_TRUE(active_rom->dirty());

  ASSERT_OK(manager->DiscardPendingRomBackupRestore());

  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(manager->GetCurrentSessionId(), session_id);
  EXPECT_EQ(active_rom->filename(),
            std::filesystem::absolute(rom_path).string());
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kEdited);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
  EXPECT_FALSE(active_rom->dirty());
  EXPECT_FALSE(session->backup_restore_pending);
  EXPECT_EQ(manager->GetCurrentRomHash(), backing_hash);
  EXPECT_EQ(active_rom->resource_label()->GetLabel("rooms", "0"),
            "In-memory room label");
}

TEST(EditorManagerBackupRestoreTest,
     DiscardPendingRomBackupRestoreRejectsPendingDungeonMetadata) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir =
      MakeTempFilePath("yaze_backup_restore_discard_pending_room");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "DISCARD PENDING ROOM");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));

  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);
  const std::string staged_hash = manager->GetCurrentRomHash();

  auto* const game_data = manager->GetCurrentGameData();
  ASSERT_NE(game_data, nullptr);
  game_data->pit_damage_table.MarkDirty();
  ASSERT_TRUE(game_data->pit_damage_table.dirty());
  auto* const dungeon =
      manager->GetCurrentEditorSet()->GetEditorAs<DungeonEditorV2>(
          EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  ASSERT_EQ(dungeon->PendingRoomCount(), 0);
  ASSERT_TRUE(dungeon->HasPendingDungeonChanges());

  const auto discard_status = manager->DiscardPendingRomBackupRestore();

  EXPECT_EQ(discard_status.code(), absl::StatusCode::kFailedPrecondition)
      << discard_status;
  EXPECT_NE(std::string(discard_status.message())
                .find("pending graphics, screen, dungeon, or palette edits"),
            std::string::npos);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_EQ(manager->GetCurrentRomHash(), staged_hash);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_TRUE(game_data->pit_damage_table.dirty());
  EXPECT_TRUE(dungeon->HasPendingDungeonChanges());
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerBackupRestoreTest,
     DiscardPendingRomBackupRestoreRejectsPendingPaletteWrite) {
  FeatureFlagsGuard guard;
  PaletteManagerGuard palette_guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir =
      MakeTempFilePath("yaze_backup_restore_discard_pending_palette");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "DISCARD PENDING PALETTE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));

  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);
  const std::string staged_hash = manager->GetCurrentRomHash();

  auto* const game_data = manager->GetCurrentGameData();
  ASSERT_NE(game_data, nullptr);
  auto* const group = game_data->palette_groups.get_group("dungeon_main");
  ASSERT_NE(group, nullptr);
  group->clear();
  gfx::SnesPalette palette;
  palette.AddColor(gfx::SnesColor(0x0100));
  group->AddPalette(palette);
  gfx::PaletteManager::Get().Initialize(game_data);
  ASSERT_OK(gfx::PaletteManager::Get().SetColor("dungeon_main", 0, 0,
                                                gfx::SnesColor(0x1111)));
  ASSERT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(game_data));

  const auto discard_status = manager->DiscardPendingRomBackupRestore();

  EXPECT_EQ(discard_status.code(), absl::StatusCode::kFailedPrecondition)
      << discard_status;
  EXPECT_NE(std::string(discard_status.message())
                .find("pending graphics, screen, dungeon, or palette edits"),
            std::string::npos);
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_EQ(manager->GetCurrentRomHash(), staged_hash);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges(game_data));
  EXPECT_EQ(gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 0).snes(),
            0x1111);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerBackupRestoreTest,
     DiscardPendingRomBackupRestoreFailureKeepsStagedState) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_discard_failure");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  const auto moved_rom_path = temp_dir / "oracle-copy-moved.sfc";
  WriteTestRom(rom_path, "DISCARD FAILURE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));
  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);
  const std::string staged_hash = manager->GetCurrentRomHash();
  const std::string staged_title = active_rom->title();
  const size_t staged_size = active_rom->size();

  std::filesystem::rename(rom_path, moved_rom_path);
  const auto discard_status = manager->DiscardPendingRomBackupRestore();

  EXPECT_FALSE(discard_status.ok()) << discard_status;
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  EXPECT_EQ(active_rom->filename(),
            std::filesystem::absolute(rom_path).string());
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_EQ(active_rom->title(), staged_title);
  EXPECT_EQ(active_rom->size(), staged_size);
  EXPECT_EQ(manager->GetCurrentRomHash(), staged_hash);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_EQ(ReadByteAt(moved_rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerBackupRestoreTest,
     SingleSessionCloseDoesNotOfferImpossibleRestoreDiscard) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = true;

  const auto temp_dir = MakeTempFilePath("yaze_backup_restore_single_close");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "oracle-copy.sfc";
  WriteTestRom(rom_path, "RESTORE CLOSE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  Rom* const active_rom = manager->GetCurrentRom();
  ASSERT_NE(active_rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kOriginal = 0x00;
  constexpr uint8_t kEdited = 0xA5;
  ASSERT_OK(active_rom->WriteByte(kPcOffset, kEdited));
  ASSERT_OK(manager->SaveRom());

  const auto backups = manager->GetRomBackups();
  ASSERT_EQ(backups.size(), 1u);
  ASSERT_OK(manager->RestoreRomBackup(backups.front().path));
  auto* const session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->backup_restore_pending);

  manager->CloseCurrentSession();

  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetCurrentRom(), active_rom);
  ASSERT_TRUE(active_rom->ReadByte(kPcOffset).ok());
  EXPECT_EQ(*active_rom->ReadByte(kPcOffset), kOriginal);
  EXPECT_TRUE(active_rom->dirty());
  EXPECT_TRUE(session->backup_restore_pending);
  EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), kEdited);
}

TEST(EditorManagerRomWritePolicyTest,
     ReadOnlyBlocksSaveAndSaveAsForConfiguredEditableSource) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  const auto path = MakeTempFilePath("yaze_readonly_project.sfc");
  const auto out = MakeTempFilePath("yaze_readonly_refused.sfc");
  ScopedFileCleanup cleanup{path};
  ScopedFileCleanup out_cleanup{out};
  WriteTestRom(path);
  const auto size_before = std::filesystem::file_size(path);
  const auto byte_before = ReadByteAt(path, 0x1234);
  ASSERT_OK(manager->OpenRomOrProject(path.string()));
  DisableRomWritesForTest();
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "ReadOnlyReference";
  project->filepath = (path.parent_path() / "readonly.yaze").string();
  project->rom_filename = path.string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kReadOnly;
  project->rom_metadata.expected_hash.clear();
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, byte_before ^ 0xFF));
  EXPECT_EQ(manager->SaveRom().code(), absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(manager->SaveRomAs(out.string()).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_FALSE(std::filesystem::exists(out));
  EXPECT_EQ(ReadByteAt(path, 0x1234), byte_before);
  EXPECT_EQ(std::filesystem::file_size(path), size_before);
  EXPECT_FALSE(manager->IsRomWriteConfirmPending());
}

TEST(EditorManagerRomWritePolicyTest,
     SaveRomBlocksOnHashMismatchWhenPolicyBlock) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_rom_write_policy_block.sfc");
  ScopedFileCleanup cleanup{rom_path};

  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  const std::string title = "YAZE TEST ROM";
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    ASSERT_TRUE(out.is_open());
    out.write(reinterpret_cast<const char*>(rom_data.data()),
              static_cast<std::streamsize>(rom_data.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "RomWritePolicyBlock";
  project->filepath = (rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kBlock;

  ASSERT_FALSE(manager->GetCurrentRomHash().empty());
  project->rom_metadata.expected_hash = "deadbeef";
  ASSERT_TRUE(manager->IsRomHashMismatch());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);

  constexpr uint32_t kPcOffset = 0x1234;
  const uint8_t original = rom_data[kPcOffset];
  const uint8_t mutated = static_cast<uint8_t>(original ^ 0xFF);
  ASSERT_OK(rom->WriteByte(static_cast<int>(kPcOffset), mutated));

  auto status = manager->SaveRom();
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_FALSE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(rom_path, static_cast<std::streamoff>(kPcOffset)),
            original);
}

TEST(EditorManagerRomWritePolicyTest,
     EditableProjectTargetBypassesHashMismatchChecks) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path dev_rom_path =
      MakeTempFilePath("yaze_project_dev_target.sfc");
  const std::filesystem::path build_rom_path =
      MakeTempFilePath("yaze_project_build_target.sfc");
  ScopedFileCleanup dev_cleanup{dev_rom_path};
  ScopedFileCleanup build_cleanup{build_rom_path};
  WriteTestRom(dev_rom_path, "YAZE DEV TARGET");
  WriteTestRom(build_rom_path, "YAZE BUILD OUT");

  ASSERT_OK(manager->OpenRomOrProject(dev_rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "EditableProjectTarget";
  project->filepath = (dev_rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_filename = dev_rom_path.filename().string();
  project->build_target = build_rom_path.filename().string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  ASSERT_TRUE(
      project->hack_manifest
          .LoadFromString(absl::StrFormat(R"json(
{
  "manifest_version": 2,
  "hack_name": "Editable Project Target",
  "build_pipeline": {
    "dev_rom": "%s",
    "patched_rom": "%s"
  }
}
)json",
                                          dev_rom_path.filename().string(),
                                          build_rom_path.filename().string()))
          .ok());

  ASSERT_FALSE(manager->GetCurrentRomHash().empty());
  project->rom_metadata.expected_hash = "deadbeef";
  EXPECT_FALSE(manager->IsRomHashMismatch());
}

TEST(EditorManagerRomWritePolicyTest,
     SaveRomBlocksWhenLoadedRomMatchesProjectBuildOutput) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path dev_rom_path =
      MakeTempFilePath("yaze_build_output_editable.sfc");
  const std::filesystem::path build_rom_path =
      MakeTempFilePath("yaze_build_output_loaded.sfc");
  ScopedFileCleanup dev_cleanup{dev_rom_path};
  ScopedFileCleanup build_cleanup{build_rom_path};
  WriteTestRom(dev_rom_path, "YAZE DEV TARGET");
  WriteTestRom(build_rom_path, "YAZE BUILD OUT");

  ASSERT_OK(manager->OpenRomOrProject(build_rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "BuildOutputLoaded";
  project->filepath = (build_rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_filename = dev_rom_path.filename().string();
  project->build_target = build_rom_path.filename().string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  ASSERT_TRUE(
      project->hack_manifest
          .LoadFromString(absl::StrFormat(R"json(
{
  "manifest_version": 2,
  "hack_name": "Build Output Loaded",
  "build_pipeline": {
    "dev_rom": "%s",
    "patched_rom": "%s"
  }
}
)json",
                                          dev_rom_path.filename().string(),
                                          build_rom_path.filename().string()))
          .ok());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_TRUE(rom->is_loaded());

  constexpr uint32_t kPcOffset = 0x1234;
  const uint8_t original =
      ReadByteAt(build_rom_path, static_cast<std::streamoff>(kPcOffset));
  const uint8_t mutated = static_cast<uint8_t>(original ^ 0xFF);
  ASSERT_OK(rom->WriteByte(static_cast<int>(kPcOffset), mutated));

  auto status = manager->SaveRom();
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_FALSE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(build_rom_path, static_cast<std::streamoff>(kPcOffset)),
            original);
}

TEST(EditorManagerRomWritePolicyTest,
     SaveRomAsPreservesTargetAcrossWriteConfirmation) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_path = MakeTempFilePath("yaze_save_as_confirm_source.sfc");
  const auto target_path = MakeTempFilePath("yaze_save_as_confirm_target.sfc");
  ScopedFileCleanup source_cleanup{source_path};
  ScopedFileCleanup target_cleanup{target_path};
  WriteTestRom(source_path);

  ASSERT_OK(manager->OpenRomOrProject(source_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "SaveAsConfirmation";
  project->filepath = (source_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kMutated = 0xA5;
  ASSERT_OK(rom->WriteByte(kPcOffset, kMutated));

  auto status = manager->SaveRomAs(target_path.string());
  EXPECT_EQ(status.code(), absl::StatusCode::kCancelled) << status;
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(source_path, kPcOffset), 0x00);
  EXPECT_FALSE(std::filesystem::exists(target_path));

  manager->ConfirmRomWrite();
  auto resumed = manager->ResumePendingRomSave();
  ASSERT_OK(resumed);

  EXPECT_EQ(ReadByteAt(source_path, kPcOffset), 0x00);
  EXPECT_EQ(ReadByteAt(target_path, kPcOffset), kMutated);
  EXPECT_EQ(rom->filename(), target_path.string());
}

TEST(EditorManagerRomWritePolicyTest, SaveRomAsCannotTargetProjectBuildOutput) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto dev_rom_path = MakeTempFilePath("yaze_save_as_policy_dev.sfc");
  const auto build_rom_path = MakeTempFilePath("yaze_save_as_policy_build.sfc");
  ScopedFileCleanup dev_cleanup{dev_rom_path};
  ScopedFileCleanup build_cleanup{build_rom_path};
  WriteTestRom(dev_rom_path, "YAZE DEV TARGET");
  WriteTestRom(build_rom_path, "YAZE BUILD OUT");

  ASSERT_OK(manager->OpenRomOrProject(dev_rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "SaveAsBuildOutputPolicy";
  project->filepath = (dev_rom_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_filename = dev_rom_path.filename().string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash.clear();
  ASSERT_OK(project->hack_manifest.LoadFromString(absl::StrFormat(
      R"json({
        "manifest_version": 2,
        "hack_name": "Save As Build Output Policy",
        "build_pipeline": {
          "dev_rom": "%s",
          "patched_rom": "%s"
        }
      })json",
      dev_rom_path.filename().string(), build_rom_path.filename().string())));

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  constexpr uint32_t kPcOffset = 0x1234;
  ASSERT_OK(rom->WriteByte(kPcOffset, 0xA5));

  auto status = manager->SaveRomAs(build_rom_path.string());
  EXPECT_EQ(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_EQ(ReadByteAt(dev_rom_path, kPcOffset), 0x00);
  EXPECT_EQ(ReadByteAt(build_rom_path, kPcOffset), 0x00);
  EXPECT_EQ(rom->filename(), dev_rom_path.string());
}

TEST(EditorManagerRomWritePolicyTest, SaveRomAsRefreshesLifecycleHashAndPath) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_path = MakeTempFilePath("yaze_save_as_hash_source.sfc");
  const auto target_path = MakeTempFilePath("yaze_save_as_hash_target.sfc");
  ScopedFileCleanup source_cleanup{source_path};
  ScopedFileCleanup target_cleanup{target_path};
  WriteTestRom(source_path);

  ASSERT_OK(manager->OpenRomOrProject(source_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "SaveAsLifecycleRefresh";
  project->filepath = (source_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_filename = source_path.string();
  project->rom_metadata.write_policy = project::RomWritePolicy::kAllow;
  project->rom_metadata.expected_hash = manager->GetCurrentRomHash();
  const std::string expected_hash = project->rom_metadata.expected_hash;
  EXPECT_FALSE(manager->IsRomHashMismatch());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));
  ASSERT_OK(manager->SaveRomAs(target_path.string()));

  EXPECT_EQ(rom->filename(), target_path.string());
  // Our Save As retains the canonical editing-source exemption, while the
  // baseline hash follows committed bytes and the project expectation stays put.
  EXPECT_FALSE(manager->IsRomHashMismatch());
  EXPECT_NE(manager->GetCurrentRomHash(), expected_hash);
  EXPECT_EQ(project->rom_metadata.expected_hash, expected_hash);
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(target_path.string()));
  EXPECT_EQ(manager->GetCurrentRomHash(),
            util::ComputeRomHash(reopened.data(), reopened.size()));
}

TEST(EditorManagerRomWritePolicyTest,
     FailedResumeConsumesHashConfirmationBypass) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_path = MakeTempFilePath("yaze_resume_failure_source.sfc");
  const auto target_path = MakeTempFilePath("yaze_resume_failure_target.sfc");
  ScopedFileCleanup source_cleanup{source_path};
  ScopedFileCleanup target_cleanup{target_path};
  WriteTestRom(source_path);

  ASSERT_OK(manager->OpenRomOrProject(source_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "ResumeFailureConsumesBypass";
  project->filepath = (source_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";
  project->hack_manifest.Clear();
  project->hack_manifest_file =
      (source_path.parent_path() / "missing-save-manifest.json").string();

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));

  EXPECT_TRUE(absl::IsCancelled(manager->SaveRomAs(target_path.string())));
  manager->ConfirmRomWrite();
  const auto failed_resume = manager->ResumePendingRomSave();
  EXPECT_EQ(failed_resume.code(), absl::StatusCode::kFailedPrecondition)
      << failed_resume;

  // Removing the downstream failure must not let the next, unrelated save
  // inherit the prior confirmation.
  project->hack_manifest_file.clear();
  const auto retry = manager->SaveRom();
  EXPECT_TRUE(absl::IsCancelled(retry)) << retry;
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());
  EXPECT_EQ(ReadByteAt(source_path, 0x1234), 0x00);
  EXPECT_FALSE(std::filesystem::exists(target_path));
}

TEST(EditorManagerRomWritePolicyTest,
     EnabledPotScopeWithoutEditsDoesNotAddUnloadedRoomPrompt) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_path = MakeTempFilePath("yaze_prompt_cancel_source.sfc");
  const auto target_path = MakeTempFilePath("yaze_prompt_cancel_target.sfc");
  ScopedFileCleanup source_cleanup{source_path};
  ScopedFileCleanup target_cleanup{target_path};
  WriteTestRom(source_path);

  ASSERT_OK(manager->OpenRomOrProject(source_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().dungeon.kSavePotItems = true;
  ASSERT_NE(manager->GetCurrentEditorSet()->GetEditor(EditorType::kDungeon),
            nullptr);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "LaterPromptCancelConsumesBypass";
  project->filepath = (source_path.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));

  EXPECT_TRUE(absl::IsCancelled(manager->SaveRomAs(target_path.string())));
  manager->ConfirmRomWrite();
  ASSERT_OK(manager->ResumePendingRomSave());
  EXPECT_FALSE(manager->HasPendingPotItemSaveConfirmation());
  EXPECT_EQ(ReadByteAt(source_path, 0x1234), 0x00);
  EXPECT_EQ(ReadByteAt(target_path, 0x1234), 0xA5);
}

TEST(EditorManagerRomWritePolicyTest,
     PendingSaveAsCannotResumeAfterSessionSwitch) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto source_a = MakeTempFilePath("yaze_session_a.sfc");
  const auto source_b = MakeTempFilePath("yaze_session_b.sfc");
  const auto target_a = MakeTempFilePath("yaze_session_a_target.sfc");
  ScopedFileCleanup cleanup_a{source_a};
  ScopedFileCleanup cleanup_b{source_b};
  ScopedFileCleanup cleanup_target{target_a};
  WriteTestRom(source_a, "YAZE SESSION A");
  WriteTestRom(source_b, "YAZE SESSION B");

  ASSERT_OK(manager->OpenRomOrProject(source_a.string()));
  const size_t session_a = manager->GetCurrentSessionIndex();
  ASSERT_OK(manager->OpenRomOrProject(source_b.string()));
  const size_t session_b = manager->GetCurrentSessionIndex();
  const std::string session_b_hash = manager->GetCurrentRomHash();
  ASSERT_NE(session_a, session_b);

  manager->SwitchToSession(session_a);
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "PendingSaveSessionBinding";
  project->filepath = (source_a.parent_path() / "project.yaze").string();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.write_policy = project::RomWritePolicy::kWarn;
  project->rom_metadata.expected_hash = "deadbeef";

  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  EXPECT_TRUE(absl::IsCancelled(manager->SaveRomAs(target_a.string())));
  EXPECT_TRUE(manager->IsRomWriteConfirmPending());

  manager->SwitchToSession(session_b);
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetCurrentSessionIndex(), session_b);
  EXPECT_EQ(manager->GetCurrentRomHash(), session_b_hash);

  // Simulate a stale confirmation callback after the session switch. It must
  // not serialize session B or write it to session A's Save As target.
  manager->ConfirmRomWrite();
  const auto stale_resume = manager->ResumePendingRomSave();
  EXPECT_EQ(stale_resume.code(), absl::StatusCode::kFailedPrecondition)
      << stale_resume;
  EXPECT_FALSE(std::filesystem::exists(target_a));
  EXPECT_EQ(ReadByteAt(source_a, 0x1234), 0x00);
  EXPECT_EQ(ReadByteAt(source_b, 0x1234), 0x00);
}

TEST(GraphicsSaveStoplossTest,
     CleanSaveDoesNotMaterializeTheLazyGraphicsEditor) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_path = MakeTempFilePath("yaze_clean_graphics_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteTestRom(rom_path, "CLEAN GRAPHICS SAVE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().kSaveGraphicsSheet = true;

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kGraphics), nullptr);
  EXPECT_FALSE(editor_set->HasPendingGraphicsChanges());
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kGraphics), nullptr);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  ASSERT_OK(manager->SaveRom());
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kGraphics), nullptr);
}

TEST(EditorManagerRomWritePolicyTest,
     CleanSavePreservesContextBoundUnloadedOverworldEditor) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_path = MakeTempFilePath("yaze_clean_lazy_editor_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteTestRom(rom_path, "CLEAN LAZY EDITOR SAVE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* dungeon_editor = editor_set->GetEditor(EditorType::kDungeon);
  ASSERT_NE(dungeon_editor, nullptr);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kDungeon),
            dungeon_editor);
  // Project context binding may create the editor without loading its assets.
  auto* const overworld_editor = static_cast<OverworldEditor*>(
      editor_set->GetExistingEditor(EditorType::kOverworld));
  ASSERT_NE(overworld_editor, nullptr);
  EXPECT_FALSE(overworld_editor->overworld().is_loaded());
  const auto original_bytes = manager->GetCurrentRom()->vector();
  const auto original_hash = util::ComputeFileSha1Hex(rom_path.string());

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  ASSERT_OK(manager->SaveRom());
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kDungeon),
            dungeon_editor);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kOverworld),
            overworld_editor);
  EXPECT_FALSE(overworld_editor->overworld().is_loaded());
  EXPECT_EQ(manager->GetCurrentRom()->vector(), original_bytes);
  EXPECT_EQ(util::ComputeFileSha1Hex(rom_path.string()), original_hash);
}

TEST(EditorManagerRomWritePolicyTest,
     UnrelatedRomSaveAsSkipsContextBoundUnloadedOverworld) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;
  const auto source = MakeTempFilePath("yaze_unrelated_lazy_save_source.sfc");
  const auto destination =
      MakeTempFilePath("yaze_unrelated_lazy_save_copy.sfc");
  ScopedFileCleanup source_cleanup{source};
  ScopedFileCleanup destination_cleanup{destination};
  WriteTestRom(source, "UNRELATED LAZY SAVE");
  const auto source_hash = util::ComputeFileSha1Hex(source.string());
  ASSERT_OK(manager->OpenRomOrProject(source.string()));
  DisableRomWritesForTest();
  // Keep this unrelated save scope enabled. A context-only editor has no
  // loaded item model to publish, and must not try to read or write it.
  core::FeatureFlags::get().overworld.kSaveOverworldItems = true;
  auto* const editors = manager->GetCurrentEditorSet();
  ASSERT_NE(editors, nullptr);
  auto* const overworld = static_cast<OverworldEditor*>(
      editors->GetExistingEditor(EditorType::kOverworld));
  ASSERT_NE(overworld, nullptr);
  ASSERT_FALSE(overworld->overworld().is_loaded());
  auto* const rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x120, 0xA5));
  const auto edited = rom->vector();

  ASSERT_OK(manager->SaveRomAs(destination.string()));

  EXPECT_EQ(editors->GetExistingEditor(EditorType::kOverworld), overworld);
  EXPECT_FALSE(overworld->overworld().is_loaded());
  EXPECT_TRUE(core::FeatureFlags::get().overworld.kSaveOverworldItems);
  EXPECT_EQ(rom->vector(), edited);
  EXPECT_FALSE(rom->dirty());
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(destination.string()));
  EXPECT_EQ(reopened.vector(), edited);
  EXPECT_EQ(util::ComputeFileSha1Hex(source.string()), source_hash);
}

TEST(GraphicsSaveStoplossTest,
     PendingSheetsBlockOrRollBackBeforeAnythingReachesDisk) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto rom_path = MakeTempFilePath("yaze_pending_graphics_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteTestRom(rom_path, "PENDING GRAPHICS");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* graphics =
      editor_set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  ASSERT_NE(graphics, nullptr);
  GraphicsEditorSaveStoplossTestPeer::MarkSheetModified(graphics, 0x20);
  ASSERT_TRUE(editor_set->HasPendingGraphicsChanges());

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  EXPECT_FALSE(rom->dirty());
  EXPECT_TRUE(manager->session_coordinator()->IsSessionModified(
      manager->GetCurrentSessionIndex()));

  manager->Quit();
  EXPECT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->CancelPendingUnsavedSessionAction();

  const bool screen_existed =
      editor_set->GetExistingEditor(EditorType::kScreen) != nullptr;
  const bool dungeon_existed =
      editor_set->GetExistingEditor(EditorType::kDungeon) != nullptr;
  const bool overworld_existed =
      editor_set->GetExistingEditor(EditorType::kOverworld) != nullptr;

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kPendingRomByte = 0xA5;
  ASSERT_OK(rom->WriteByte(kPcOffset, kPendingRomByte));

  // Disabled: blocked before any serializer runs. Enabled: the Graphics
  // editor joins the coordinated save, sheet 0x20 is not loaded, so its save
  // fails and the whole transaction rolls back. The Arena is process-global;
  // clear the sheet so an earlier test cannot leave it loaded.
  auto& arena_sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(0x20);
  ScopedGraphicsSheetRestore restore_sheet(&arena_sheet);
  arena_sheet = gfx::Bitmap();
  ASSERT_FALSE(arena_sheet.is_active());
  for (const bool graphics_scope_enabled : {false, true}) {
    core::FeatureFlags::get().kSaveGraphicsSheet = graphics_scope_enabled;
    const auto status = manager->SaveRom();

    EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
    EXPECT_NE(std::string(status.message())
                  .find(graphics_scope_enabled
                            ? "Graphics save refused for sheet(s) 0x20"
                            : "graphics sheet edits are pending"),
              std::string::npos)
        << status;
    EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), 0x00);
    ASSERT_TRUE(rom->ReadByte(kPcOffset).ok());
    EXPECT_EQ(*rom->ReadByte(kPcOffset), kPendingRomByte);
    EXPECT_TRUE(editor_set->HasPendingGraphicsChanges());
  }

  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kScreen) != nullptr,
            screen_existed);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kDungeon) != nullptr,
            dungeon_existed);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kOverworld) != nullptr,
            overworld_existed);
}

TEST(GraphicsSaveStoplossTest, EnabledSaveWritesTheEditedSheetToDisk) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  constexpr uint16_t kSheetId = 0x20;
  auto fixture = test::BuildGfxSheetTestRom();
  const std::string title = "GFX WRITEBACK";
  std::copy(title.begin(), title.end(), fixture.bytes.begin() + 0x7FC0);
  fixture.bytes[0x7FD9] = 0x01;  // US pointer tables
  const auto rom_path = MakeTempFilePath("yaze_graphics_writeback.sfc");
  ScopedFileCleanup cleanup{rom_path};
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(fixture.bytes.data()),
              static_cast<std::streamsize>(fixture.bytes.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().kSaveGraphicsSheet = true;
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* graphics =
      editor_set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  ASSERT_NE(graphics, nullptr);

  // A smaller edit of the stored sheet, so it fits in place without a
  // manifest-registered free region.
  auto edited = fixture.sheets.at(kSheetId);
  std::fill(edited.begin(), edited.begin() + 0x100, 0x00);
  auto indexed = gfx::SnesTo8bppSheet(edited, /*bpp=*/3);
  auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(kSheetId);
  ScopedGraphicsSheetRestore restore_sheet(&sheet);
  sheet.Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
               gfx::kTilesheetDepth, indexed);
  ASSERT_TRUE(sheet.is_active());
  GraphicsEditorSaveStoplossTestPeer::MarkSheetModified(graphics, kSheetId);

  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(editor_set->HasPendingGraphicsChanges());

  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(rom_path.string()));
  auto readback = zelda3::ReadGfxSheetData(reopened, kSheetId);
  ASSERT_TRUE(readback.ok()) << readback.status();
  EXPECT_EQ(*readback, edited);
  for (uint16_t other = 0; other < zelda3::kGfxSheetCount; ++other) {
    if (other == kSheetId) {
      continue;
    }
    auto data = zelda3::ReadGfxSheetData(reopened, other);
    ASSERT_TRUE(data.ok()) << other;
    EXPECT_EQ(*data, fixture.sheets.at(other)) << "sheet " << other;
  }
}

// Found in review: the Arena is shared by every open ROM, and the pixel
// editor can hold indices up to 15 while a 3bpp sheet stores 0-7. Both must
// refuse the save and leave the ROM on disk unchanged.
TEST(GraphicsSaveStoplossTest, SaveRefusesForeignSheetsAndColorsAboveSeven) {
  for (const bool foreign_owner : {true, false}) {
    SCOPED_TRACE(foreign_owner ? "sheets owned by another ROM"
                               : "color index 9");
    FeatureFlagsGuard guard;
    ScopedImGuiContext imgui;

    auto renderer = std::make_unique<gfx::NullRenderer>();
    auto manager = std::make_unique<EditorManager>();
    InitializeWithIsolatedSettings(*manager, renderer.get());
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager->user_settings().prefs().backup_before_save = false;

    constexpr uint16_t kSheetId = 0x20;
    auto fixture = test::BuildGfxSheetTestRom();
    const std::string title = "GFX REFUSAL";
    std::copy(title.begin(), title.end(), fixture.bytes.begin() + 0x7FC0);
    fixture.bytes[0x7FD9] = 0x01;  // US pointer tables
    const auto rom_path = MakeTempFilePath("yaze_graphics_refusal.sfc");
    ScopedFileCleanup cleanup{rom_path};
    {
      std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
      out.write(reinterpret_cast<const char*>(fixture.bytes.data()),
                static_cast<std::streamsize>(fixture.bytes.size()));
      ASSERT_TRUE(out.good());
    }

    ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
    DisableRomWritesForTest();
    core::FeatureFlags::get().kSaveGraphicsSheet = true;
    auto* project = manager->GetCurrentProject();
    ASSERT_NE(project, nullptr);
    project->workspace_settings.backup_on_save = false;
    project->rom_metadata.expected_hash.clear();

    auto* editor_set = manager->GetCurrentEditorSet();
    ASSERT_NE(editor_set, nullptr);
    auto* graphics =
        editor_set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
    ASSERT_NE(graphics, nullptr);

    auto edited = fixture.sheets.at(kSheetId);
    std::fill(edited.begin(), edited.begin() + 0x100, 0x00);
    auto indexed = gfx::SnesTo8bppSheet(edited, /*bpp=*/3);
    if (!foreign_owner) {
      indexed[0] = 9;
    }
    auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(kSheetId);
    ScopedGraphicsSheetRestore restore_sheet(&sheet);
    sheet.Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
                 gfx::kTilesheetDepth, indexed);
    GraphicsEditorSaveStoplossTestPeer::MarkSheetModified(graphics, kSheetId);

    const void* owner = gfx::Arena::Get().gfx_sheets_owner();
    ASSERT_NE(owner, nullptr);
    int other_rom_game_data = 0;
    if (foreign_owner) {
      gfx::Arena::Get().set_gfx_sheets_owner(&other_rom_game_data);
    }
    const absl::Status status = manager->SaveRom();
    gfx::Arena::Get().set_gfx_sheets_owner(owner);

    EXPECT_FALSE(status.ok());
    EXPECT_THAT(std::string(status.message()),
                ::testing::HasSubstr(foreign_owner ? "another open ROM"
                                                   : "above index 7"));
    std::ifstream in(rom_path, std::ios::binary);
    const std::vector<uint8_t> disk((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    EXPECT_EQ(disk, fixture.bytes);
  }
}

// The Sheet Browser's PNG path end to end: export the Arena sheet, edit one
// pixel, preview and apply the import, then save through the graphics
// write-back and read the sheet back from disk.
void ExpectPngPixelEditSurvivesSave(EditorManager* manager, uint16_t sheet_id,
                                    const std::filesystem::path& rom_path) {
  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* graphics =
      editor_set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  ASSERT_NE(graphics, nullptr);
  const auto& sheet = gfx::Arena::Get().gfx_sheets()[sheet_id];
  ASSERT_TRUE(sheet.is_active());
  const std::vector<uint8_t> before = sheet.vector();

  const auto palette = zelda3::GrayscaleSheetPalette();
  auto png = ExportSheetPixelsPng(sheet_id, before, palette);
  ASSERT_TRUE(png.ok()) << png.status();
  auto image = util::DecodePng(*png);
  ASSERT_TRUE(image.ok()) << image.status();
  constexpr int kX = 37;
  constexpr int kY = 11;
  const uint8_t new_index = (before[kY * 128 + kX] + 1) % 8;
  image->indices[kY * image->width + kX] = new_index;
  auto edited_png = util::EncodeIndexedPng(image->width, image->height,
                                           image->indices, image->palette);
  ASSERT_TRUE(edited_png.ok());

  auto preview = PreviewSheetPngImport(sheet_id, before, *edited_png, palette);
  ASSERT_TRUE(preview.ok()) << preview.status();
  ASSERT_EQ(preview->changed_tiles.size(), 1u);
  GraphicsEditorSaveStoplossTestPeer::FillStoreFromArena(graphics);
  ASSERT_TRUE(ApplySheetPngImport(
                  *preview, GraphicsEditorSaveStoplossTestPeer::State(graphics),
                  nullptr)
                  .ok());
  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(editor_set->HasPendingGraphicsChanges());

  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(rom_path.string()));
  auto readback = zelda3::ReadGfxSheetData(reopened, sheet_id);
  ASSERT_TRUE(readback.ok()) << readback.status();
  auto expected = before;
  expected[kY * 128 + kX] = new_index;
  EXPECT_EQ(gfx::SnesTo8bppSheet(*readback, /*bpp=*/3), expected);
}

TEST(GraphicsSaveStoplossTest, PngImportSavesThroughTheGraphicsWriteBack) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  constexpr uint16_t kSheetId = 0x20;
  auto fixture = test::BuildGfxSheetTestRom();
  const std::string title = "GFX PNG IMPORT";
  std::copy(title.begin(), title.end(), fixture.bytes.begin() + 0x7FC0);
  fixture.bytes[0x7FD9] = 0x01;  // US pointer tables
  const auto rom_path = MakeTempFilePath("yaze_graphics_png_import.sfc");
  ScopedFileCleanup cleanup{rom_path};
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(fixture.bytes.data()),
              static_cast<std::streamsize>(fixture.bytes.size()));
    ASSERT_TRUE(out.good());
  }
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().kSaveGraphicsSheet = true;
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  // Lazy mode does not load the sheets; put this ROM's sheet in the Arena
  // and claim it, as a full load does.
  auto* editor_set = manager->GetCurrentEditorSet();
  auto* graphics =
      editor_set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  ASSERT_NE(graphics, nullptr);
  auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(kSheetId);
  ScopedGraphicsSheetRestore restore_sheet(&sheet);
  sheet.Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
               gfx::kTilesheetDepth,
               gfx::SnesTo8bppSheet(fixture.sheets.at(kSheetId), /*bpp=*/3));
  gfx::Arena::Get().set_gfx_sheets_owner(graphics->game_data());

  ExpectPngPixelEditSurvivesSave(manager.get(), kSheetId, rom_path);
}

// Same flow on a scratch copy of an Oracle of Secrets project (never the
// real one): YAZE_TEST_ORACLE_PROJECT_COPY names its .yaze file, whose ROM
// and custom_collision.json are copies. Skipped when unset.
TEST(GraphicsSaveStoplossTest, PngImportOnOracleProjectCopy) {
  const char* project_env = std::getenv("YAZE_TEST_ORACLE_PROJECT_COPY");
  if (project_env == nullptr || *project_env == '\0') {
    GTEST_SKIP() << "YAZE_TEST_ORACLE_PROJECT_COPY is not set";
  }
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kFull);
  manager->user_settings().prefs().backup_before_save = false;
  ASSERT_OK(manager->OpenRomOrProject(project_env));
  core::FeatureFlags::get().kSaveGraphicsSheet = true;
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  const std::filesystem::path rom_path =
      project->GetAbsolutePath(project->rom_filename);
  ASSERT_TRUE(std::filesystem::exists(rom_path)) << rom_path;

  // Sheet 0x92 (Stalfos): a compressed 3bpp sprite sheet.
  ExpectPngPixelEditSurvivesSave(manager.get(), 0x92, rom_path);
}

// Fills block 0 (16x16) of an indexed sheet with seeded noise in colors 0-7,
// so the sheet no longer compresses into its old slot.
std::vector<uint8_t> WithNoisyBlock0(std::vector<uint8_t> pixels,
                                     uint32_t seed) {
  std::mt19937 rng(seed);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      pixels[y * 128 + x] = static_cast<uint8_t>(rng() % 8);
    }
  }
  return pixels;
}

// Plan, save and reopen a sheet that has to move: the preflight names the
// new address, the save relocates it there, and the reopened ROM decodes the
// edited pixels from that address.
void ExpectRelocatedSheetSurvivesSave(EditorManager* manager, uint16_t sheet_id,
                                      const std::filesystem::path& rom_path,
                                      uint32_t region_begin,
                                      uint32_t region_end) {
  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* graphics =
      editor_set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  ASSERT_NE(graphics, nullptr);
  auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet_id);
  ASSERT_TRUE(sheet.is_active());
  const auto edited = WithNoisyBlock0(sheet.vector(), /*seed=*/0x5A5A);
  sheet.set_data(edited);
  GraphicsEditorSaveStoplossTestPeer::MarkSheetModified(graphics, sheet_id);

  // Close the read handle before manager->SaveRom() replaces rom_path below:
  // on Windows an open handle without FILE_SHARE_DELETE blocks the atomic
  // move of the saved ROM into place.
  std::vector<uint8_t> disk_before;
  {
    std::ifstream before_in(rom_path, std::ios::binary);
    disk_before.assign((std::istreambuf_iterator<char>(before_in)),
                       std::istreambuf_iterator<char>());
  }
  auto plan = graphics->PlanGraphicsSave();
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->size(), 1u);
  const GraphicsSavePlanEntry entry = plan->front();
  ASSERT_TRUE(entry.refusal.empty()) << entry.refusal;
  EXPECT_EQ(entry.placement, zelda3::GfxSheetPlacement::kRelocated);
  EXPECT_GE(entry.new_pc, region_begin);
  EXPECT_LT(entry.new_pc, region_end);
  EXPECT_GT(entry.new_stored_size, 0u);
  {
    std::ifstream in(rom_path, std::ios::binary);
    const std::vector<uint8_t> disk((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    EXPECT_EQ(disk, disk_before) << "the plan must not write";
  }

  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(editor_set->HasPendingGraphicsChanges());
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(rom_path.string()));
  auto pc = zelda3::ReadGfxSheetPc(reopened, sheet_id);
  ASSERT_TRUE(pc.ok()) << pc.status();
  EXPECT_EQ(*pc, entry.new_pc);
  auto readback = zelda3::ReadGfxSheetData(reopened, sheet_id);
  ASSERT_TRUE(readback.ok()) << readback.status();
  EXPECT_EQ(gfx::SnesTo8bppSheet(*readback, /*bpp=*/3), edited);
}

TEST(GraphicsSaveStoplossTest, RelocatedSheetIsPlannedSavedAndReopened) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  constexpr uint16_t kSheetId = 0x20;
  auto fixture = test::BuildGfxSheetTestRom();
  const std::string title = "GFX RELOCATE";
  std::copy(title.begin(), title.end(), fixture.bytes.begin() + 0x7FC0);
  fixture.bytes[0x7FD9] = 0x01;  // US pointer tables
  const auto rom_path = MakeTempFilePath("yaze_graphics_relocate.sfc");
  ScopedFileCleanup cleanup{rom_path};
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(fixture.bytes.data()),
              static_cast<std::streamsize>(fixture.bytes.size()));
    ASSERT_TRUE(out.good());
  }
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().kSaveGraphicsSheet = true;
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  auto* graphics = manager->GetCurrentEditorSet()->GetEditorAs<GraphicsEditor>(
      EditorType::kGraphics);
  ASSERT_NE(graphics, nullptr);
  // Register the fixture's free space, PC 0xE0000-0xF0000, in the manager's
  // project and in the session copy the save policy reads (SaveRom syncs
  // them, so a project file load sets both).
  constexpr char kManifest[] =
      R"json({"manifest_version": 3, "graphics_sheet_regions":
          {"allocation_regions": [{"start": "0x1C8000", "end": "0x1E8000"}]}})json";
  ASSERT_OK(project->hack_manifest.LoadFromString(kManifest));
  ASSERT_NE(graphics->project(), nullptr);
  ASSERT_OK(graphics->project()->hack_manifest.LoadFromString(kManifest));
  auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(kSheetId);
  ScopedGraphicsSheetRestore restore_sheet(&sheet);
  sheet.Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
               gfx::kTilesheetDepth,
               gfx::SnesTo8bppSheet(fixture.sheets.at(kSheetId), /*bpp=*/3));

  ExpectRelocatedSheetSurvivesSave(manager.get(), kSheetId, rom_path, 0xE0000,
                                   0xF0000);
}

// A sheet that does not read back as edited stops the save: the ROM bytes
// in memory (including the relocation target) and the file on disk stay as
// they were, and the edit stays pending.
TEST(GraphicsSaveStoplossTest,
     VerificationMismatchRestoresTheRomAndWritesNothing) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  constexpr uint16_t kSheetId = 0x20;
  auto fixture = test::BuildGfxSheetTestRom();
  const std::string title = "GFX VERIFY FAIL";
  std::copy(title.begin(), title.end(), fixture.bytes.begin() + 0x7FC0);
  fixture.bytes[0x7FD9] = 0x01;  // US pointer tables
  const auto rom_path = MakeTempFilePath("yaze_graphics_verify_fail.sfc");
  ScopedFileCleanup cleanup{rom_path};
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(fixture.bytes.data()),
              static_cast<std::streamsize>(fixture.bytes.size()));
    ASSERT_TRUE(out.good());
  }
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  core::FeatureFlags::get().kSaveGraphicsSheet = true;
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  auto* editor_set = manager->GetCurrentEditorSet();
  auto* graphics =
      editor_set->GetEditorAs<GraphicsEditor>(EditorType::kGraphics);
  ASSERT_NE(graphics, nullptr);
  constexpr char kManifest[] =
      R"json({"manifest_version": 3, "graphics_sheet_regions":
          {"allocation_regions": [{"start": "0x1C8000", "end": "0x1E8000"}]}})json";
  ASSERT_OK(project->hack_manifest.LoadFromString(kManifest));
  ASSERT_NE(graphics->project(), nullptr);
  ASSERT_OK(graphics->project()->hack_manifest.LoadFromString(kManifest));

  auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(kSheetId);
  ScopedGraphicsSheetRestore restore_sheet(&sheet);
  sheet.Create(gfx::kTilesheetWidth, gfx::kTilesheetHeight,
               gfx::kTilesheetDepth,
               gfx::SnesTo8bppSheet(fixture.sheets.at(kSheetId), /*bpp=*/3));
  const auto edited = WithNoisyBlock0(sheet.vector(), /*seed=*/0xBEEF);
  sheet.set_data(edited);
  GraphicsEditorSaveStoplossTestPeer::MarkSheetModified(graphics, kSheetId);

  Rom* const rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  const std::vector<uint8_t> memory_before = rom->vector();
  const bool dirty_before = rom->dirty();

  GraphicsEditorSaveStoplossTestPeer::ForceVerificationMismatch(graphics);
  // The editor restores the bytes itself, not only EditorManager's rollback.
  const absl::Status editor_failed = graphics->Save();
  ASSERT_FALSE(editor_failed.ok());
  EXPECT_NE(std::string(editor_failed.message()).find("nothing was written"),
            std::string::npos)
      << editor_failed;
  EXPECT_EQ(rom->vector(), memory_before);
  EXPECT_EQ(rom->dirty(), dirty_before);

  const absl::Status failed = manager->SaveRom();
  ASSERT_FALSE(failed.ok());
  EXPECT_NE(std::string(failed.message()).find("nothing was written"),
            std::string::npos)
      << failed;
  EXPECT_EQ(rom->vector(), memory_before);
  EXPECT_EQ(rom->dirty(), dirty_before);
  {
    std::ifstream in(rom_path, std::ios::binary);
    const std::vector<uint8_t> disk((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    EXPECT_EQ(disk, fixture.bytes);
  }
  EXPECT_TRUE(editor_set->HasPendingGraphicsChanges());
  EXPECT_EQ(sheet.vector(), edited);

  // With the real check back, the same edit saves.
  GraphicsEditorSaveStoplossTestPeer::RestoreVerification(graphics);
  ASSERT_OK(manager->SaveRom());
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(rom_path.string()));
  auto readback = zelda3::ReadGfxSheetData(reopened, kSheetId);
  ASSERT_TRUE(readback.ok()) << readback.status();
  EXPECT_EQ(gfx::SnesTo8bppSheet(*readback, /*bpp=*/3), edited);
}

// The same on a scratch copy of the Oracle project (skipped unless
// YAZE_TEST_ORACLE_PROJECT_COPY names its .yaze file): sheet 0x92 grows and
// moves into the manifest's bank $23 region (PC 0x118000-0x120000).
TEST(GraphicsSaveStoplossTest, RelocatedSheetOnOracleProjectCopy) {
  const char* project_env = std::getenv("YAZE_TEST_ORACLE_PROJECT_COPY");
  if (project_env == nullptr || *project_env == '\0') {
    GTEST_SKIP() << "YAZE_TEST_ORACLE_PROJECT_COPY is not set";
  }
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kFull);
  manager->user_settings().prefs().backup_before_save = false;
  ASSERT_OK(manager->OpenRomOrProject(project_env));
  core::FeatureFlags::get().kSaveGraphicsSheet = true;
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  const std::filesystem::path rom_path =
      project->GetAbsolutePath(project->rom_filename);

  ExpectRelocatedSheetSurvivesSave(manager.get(), 0x92, rom_path, 0x118000,
                                   0x120000);
}

TEST(GfxGroupSaveTest, SpritesetEditSavesAndReopensOrBlocksWhenDisabled) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  auto fixture = test::BuildGfxSheetTestRom();
  const std::string title = "GFX GROUP SAVE";
  std::copy(title.begin(), title.end(), fixture.bytes.begin() + 0x7FC0);
  fixture.bytes[0x7FD9] = 0x01;  // US tables
  const auto rom_path = MakeTempFilePath("yaze_gfx_group_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  {
    std::ofstream out(rom_path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(fixture.bytes.data()),
              static_cast<std::streamsize>(fixture.bytes.size()));
    ASSERT_TRUE(out.good());
  }

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();
  ASSERT_OK(manager->EnsureGameDataLoaded());
  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_FALSE(session->HasPendingGfxGroupChanges());

  // Spriteset 0x0C slot 2 -> sprite value 0x55 (sheet 0xC8).
  constexpr int kSet = 0x0C;
  constexpr uint32_t kSlotPc = 0x5B57 + kSet * 4;
  session->game_data.spriteset_ids[kSet][2] = 0x55;
  EXPECT_TRUE(session->HasPendingGfxGroupChanges());
  EXPECT_TRUE(manager->session_coordinator()->IsSessionModified(
      manager->GetCurrentSessionIndex()));

  core::FeatureFlags::get().kSaveGfxGroups = false;
  auto blocked = manager->SaveRom();
  EXPECT_EQ(blocked.code(), absl::StatusCode::kFailedPrecondition) << blocked;
  EXPECT_NE(std::string(blocked.message()).find("Save Gfx Groups is disabled"),
            std::string::npos);
  EXPECT_EQ(ReadByteAt(rom_path, kSlotPc + 2), 0x00);
  EXPECT_TRUE(session->HasPendingGfxGroupChanges());

  core::FeatureFlags::get().kSaveGfxGroups = true;
  ASSERT_OK(manager->SaveRom());
  EXPECT_FALSE(session->HasPendingGfxGroupChanges());

  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(rom_path.string()));
  for (int slot = 0; slot < 4; ++slot) {
    EXPECT_EQ(reopened.vector()[kSlotPc + slot],
              session->game_data.spriteset_ids[kSet][slot])
        << "slot " << slot;
  }
  EXPECT_EQ(reopened.vector()[kSlotPc + 2], 0x55);
}

TEST(GraphicsSaveStoplossTest, PixelUndoAndRedoRemarkTheSheetDirty) {
  constexpr uint16_t kSheetId = 0x20;
  constexpr size_t kBytes = zelda3::GraphicsSheetStore::kSheetBytes;
  std::vector<uint8_t> before_data(kBytes, 0x01);
  std::vector<uint8_t> after_data = before_data;
  after_data[3] = 0x05;
  after_data[200] = 0x06;

  auto data = std::make_unique<zelda3::GameData>();
  data->graphics_buffer.assign(223 * kBytes, 0);
  std::copy(after_data.begin(), after_data.end(),
            data->graphics_buffer.begin() + kSheetId * kBytes);
  data->sheet_store.MarkAllSheetsChanged();
  auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(kSheetId);
  ScopedGraphicsSheetRestore restore_sheet(&sheet);
  sheet.Create(128, 32, 8, after_data);
  const void* owner = gfx::Arena::Get().gfx_sheets_owner();
  gfx::Arena::Get().set_gfx_sheets_owner(data.get());
  GraphicsEditorState state;
  AttachSheetStore(state, data.get());

  GraphicsPixelEditAction action(
      &state, MakeSheetPixelDiff(kSheetId, before_data, after_data),
      "Edit pixels");

  ASSERT_FALSE(state.HasUnsavedChanges());
  ASSERT_OK(action.Undo());
  EXPECT_EQ(sheet.vector(), before_data);
  EXPECT_TRUE(state.HasUnsavedChanges());
  EXPECT_TRUE(state.modified_sheets.contains(kSheetId));

  state.ClearModifiedSheets();
  ASSERT_OK(action.Redo());
  EXPECT_EQ(sheet.vector(), after_data);
  EXPECT_TRUE(state.HasUnsavedChanges());
  EXPECT_TRUE(state.modified_sheets.contains(kSheetId));
  gfx::Arena::Get().set_gfx_sheets_owner(owner);
}

TEST(ScreenSaveStoplossTest,
     PendingQueryDoesNotMaterializeTheLazyScreenEditor) {
  EditorSet editor_set;

  EXPECT_EQ(editor_set.GetExistingEditor(EditorType::kScreen), nullptr);
  EXPECT_FALSE(editor_set.HasPendingScreenChanges());
  editor_set.InvalidateScreenRomBackedState();
  EXPECT_EQ(editor_set.GetExistingEditor(EditorType::kScreen), nullptr);
}

TEST(ScreenSaveStoplossTest,
     CleanFailedScreenLoadDoesNotBlockUnrelatedRomSave) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_path =
      MakeTempFilePath("yaze_clean_failed_screen_load_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteScreenModelTestRom(rom_path, "FAILED SCREEN LOAD SAVE");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  // Load GameData through a different editor, then make ScreenEditor::Load()
  // fail its palette precondition after it has reset its ROM-backed state.
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  auto* game_data = manager->GetCurrentGameData();
  ASSERT_NE(game_data, nullptr);
  game_data->palette_groups.dungeon_main.clear();

  const auto load_status =
      manager->EnsureEditorAssetsLoaded(EditorType::kScreen);
  EXPECT_EQ(load_status.code(), absl::StatusCode::kFailedPrecondition)
      << load_status;
  EXPECT_NE(std::string(load_status.message()).find("dungeon palette 3"),
            std::string::npos)
      << load_status;

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* screen = static_cast<ScreenEditor*>(
      editor_set->GetExistingEditor(EditorType::kScreen));
  ASSERT_NE(screen, nullptr);
  EXPECT_FALSE(screen->IsRomBackedStateValid());
  EXPECT_FALSE(screen->HasPendingScreenChanges());

  constexpr uint32_t kUnrelatedOffset = 0x1234;
  constexpr uint8_t kUnrelatedValue = 0x5A;
  ASSERT_OK(
      manager->GetCurrentRom()->WriteByte(kUnrelatedOffset, kUnrelatedValue));
  ASSERT_OK(manager->SaveRom());
  EXPECT_EQ(ReadByteAt(rom_path, kUnrelatedOffset), kUnrelatedValue);
}

TEST(ScreenSaveStoplossTest,
     RomReplacementRefreshesLoadedScreenInPlaceBeforeEnabledGlobalSave) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto temp_dir = MakeTempFilePath("yaze_screen_replace_save");
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  ScopedDirectoryCleanup cleanup{temp_dir};
  const auto rom_path = temp_dir / "game.sfc";
  const auto project_path = temp_dir / "game.yaze";
  WriteScreenModelTestRom(rom_path, "BEFORE SCREEN RELOAD");
  WriteTestProject(project_path, rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));

  DisableRomWritesForTest();
  core::FeatureFlags::get().kSaveDungeonMaps = true;
  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->feature_flags = core::FeatureFlags::get();
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();
  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  session->feature_flags = core::FeatureFlags::get();

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kScreen));
  auto* screen = editor_set->GetEditorAs<ScreenEditor>(EditorType::kScreen);
  ASSERT_NE(screen, nullptr);
  ASSERT_TRUE(screen->IsRomBackedStateValid());
  ASSERT_EQ(ScreenEditorSaveStoplossTestPeer::DungeonMapCount(*screen), 14u);
  ASSERT_FALSE(screen->HasPendingScreenChanges());
  auto* const sheet0_before =
      ScreenEditorSaveStoplossTestPeer::CaptureBitmapOwners(screen).sheet0;

  constexpr uint8_t kReplacementFloorByte = 0x10;
  const auto screen_index = static_cast<size_t>(EditorType::kScreen);
  for (const auto mode : {AssetLoadMode::kLazy, AssetLoadMode::kFull}) {
    SCOPED_TRACE(mode == AssetLoadMode::kLazy ? "lazy replacement"
                                              : "full replacement");
    manager->SetAssetLoadMode(mode);
    WriteScreenModelTestRom(rom_path, "AFTER SCREEN RELOAD");
    // This successful-save fixture uses the supported vanilla Tile16 layout.
    WriteByteAt(rom_path, zelda3::kDungeonMapExpCheck, 0xB9);
    WriteByteAt(rom_path, zelda3::kDungeonMapFloors, kReplacementFloorByte);
    for (int dungeon = 0; dungeon < zelda3::kNumDungeons; ++dungeon) {
      WriteWordAt(rom_path, zelda3::kDungeonMapBossRooms + (dungeon * 2),
                  0xFFFF);
    }
    ASSERT_OK(manager->ReloadProjectRom());

    EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kScreen), screen);
    EXPECT_TRUE(screen->IsRomBackedStateValid());
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::DungeonMapCount(*screen), 14u);
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::DungeonFloorCount(*screen, 0),
              1);
    EXPECT_EQ(
        ScreenEditorSaveStoplossTestPeer::CaptureBitmapOwners(screen).sheet0,
        sheet0_before);
    EXPECT_TRUE(session->editor_initialized[screen_index]);
    EXPECT_TRUE(session->editor_assets_loaded[screen_index]);
  }

  // Exercise the visible panel after replacement, including the populated
  // first floor and guarded empty states for the other synthetic dungeons.
  ImGui::NewFrame();
  ImGui::Begin("ScreenReloadDrawSafety");
  ScreenEditorSaveStoplossTestPeer::DrawDungeonMapsEditor(screen);
  ImGui::End();
  ImGui::Render();

  EXPECT_TRUE(core::FeatureFlags::get().kSaveDungeonMaps);
  ASSERT_NE(manager->GetCurrentRom(), nullptr);
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0x5A));
  ASSERT_OK(manager->SaveRom());

  EXPECT_EQ(ReadByteAt(rom_path, zelda3::kDungeonMapFloors),
            kReplacementFloorByte);
  EXPECT_EQ(ReadByteAt(rom_path, 0x1234), 0x5A);
}

TEST(ScreenSaveStoplossTest,
     FailedRomReplacementPreservesLoadedScreenAfterAssetFailure) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto temp_dir = MakeTempFilePath("yaze_screen_replace_rollback");
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  ScopedDirectoryCleanup cleanup{temp_dir};
  const auto rom_path = temp_dir / "game.sfc";
  const auto project_path = temp_dir / "game.yaze";
  WriteScreenModelTestRom(rom_path, "SCREEN ROLLBACK OLD");
  WriteTestProject(project_path, rom_path);
  ASSERT_OK(manager->OpenRomOrProject(project_path.string()));
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kScreen));

  auto* session = manager->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(session, nullptr);
  ASSERT_TRUE(session->game_data_loaded);
  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* screen = editor_set->GetEditorAs<ScreenEditor>(EditorType::kScreen);
  ASSERT_NE(screen, nullptr);
  ASSERT_TRUE(screen->IsRomBackedStateValid());
  auto* const sheet0_before =
      ScreenEditorSaveStoplossTestPeer::CaptureBitmapOwners(screen).sheet0;
  Rom* const rom_before = manager->GetCurrentRom();

  // This ROM passes the 512 KiB file gate but cannot load palettes. A failed
  // replacement must leave the loaded screen usable, including bitmap owners.
  WriteTooSmallForAssetsTestRom(rom_path);
  const auto reload_status = manager->ReloadProjectRom();
  EXPECT_EQ(reload_status.code(), absl::StatusCode::kOutOfRange)
      << reload_status;
  EXPECT_NE(std::string(reload_status.message()).find("palette"),
            std::string::npos)
      << reload_status;

  EXPECT_EQ(manager->GetCurrentRom(), rom_before);
  ASSERT_NE(manager->GetCurrentRom(), nullptr);
  EXPECT_NE(manager->GetCurrentRom()->title().find("SCREEN ROLLBACK OLD"),
            std::string::npos);
  EXPECT_EQ(editor_set->GetExistingEditor(EditorType::kScreen), screen);
  EXPECT_TRUE(screen->IsRomBackedStateValid());
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::DungeonMapCount(*screen), 14u);
  EXPECT_EQ(
      ScreenEditorSaveStoplossTestPeer::CaptureBitmapOwners(screen).sheet0,
      sheet0_before);
  const size_t screen_index = EditorTypeIndex(EditorType::kScreen);
  EXPECT_TRUE(session->editor_initialized[screen_index]);
  EXPECT_TRUE(session->editor_assets_loaded[screen_index]);
  core::FeatureFlags::get().kSaveDungeonMaps = false;
  EXPECT_TRUE(screen->Save().ok());

  ImGui::NewFrame();
  ImGui::Begin("ScreenRollbackDrawSafety");
  ScreenEditorSaveStoplossTestPeer::DrawDungeonMapsEditor(screen);
  ImGui::End();
  ImGui::Render();
}

TEST(ScreenSaveStoplossTest,
     EveryPendingDomainBlocksLifecycleForBothScopeStates) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  manager->user_settings().prefs().backup_before_save = false;

  const auto rom_path = MakeTempFilePath("yaze_pending_screen_save.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteTestRom(rom_path, "PENDING SCREEN");

  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  DisableRomWritesForTest();

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->workspace_settings.backup_on_save = false;
  project->rom_metadata.expected_hash.clear();

  auto* editor_set = manager->GetCurrentEditorSet();
  ASSERT_NE(editor_set, nullptr);
  auto* screen = editor_set->GetEditorAs<ScreenEditor>(EditorType::kScreen);
  ASSERT_NE(screen, nullptr);

  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  EXPECT_FALSE(rom->dirty());

  constexpr uint32_t kPcOffset = 0x1234;
  constexpr uint8_t kPendingRomByte = 0xA5;

  enum class PendingDomain {
    kDungeonMap,
    kDungeonMapTile16,
    kTitle,
    kOverworld,
    kOverworldPalette
  };
  for (const PendingDomain domain :
       {PendingDomain::kDungeonMap, PendingDomain::kDungeonMapTile16,
        PendingDomain::kTitle, PendingDomain::kOverworld,
        PendingDomain::kOverworldPalette}) {
    SCOPED_TRACE(static_cast<int>(domain));
    ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(screen);
    switch (domain) {
      case PendingDomain::kDungeonMap:
        ScreenEditorSaveStoplossTestPeer::MarkDungeonMapModified(screen);
        break;
      case PendingDomain::kDungeonMapTile16:
        ScreenEditorSaveStoplossTestPeer::MarkDungeonMapTile16Modified(screen);
        break;
      case PendingDomain::kTitle:
        ScreenEditorSaveStoplossTestPeer::MarkTitleScreenModified(screen);
        break;
      case PendingDomain::kOverworld:
        ScreenEditorSaveStoplossTestPeer::MarkOverworldMapModified(screen);
        break;
      case PendingDomain::kOverworldPalette:
        ScreenEditorSaveStoplossTestPeer::MarkOverworldMapPaletteModified(
            screen);
        break;
    }

    ASSERT_TRUE(editor_set->HasPendingScreenChanges());
    EXPECT_EQ(screen->HasPendingDungeonMapChanges(),
              domain == PendingDomain::kDungeonMap);
    EXPECT_EQ(screen->HasPendingDungeonMapTile16Changes(),
              domain == PendingDomain::kDungeonMapTile16);
    EXPECT_EQ(screen->HasPendingTitleScreenChanges(),
              domain == PendingDomain::kTitle);
    EXPECT_EQ(screen->HasPendingOverworldMapChanges(),
              domain == PendingDomain::kOverworld ||
                  domain == PendingDomain::kOverworldPalette);
    EXPECT_TRUE(manager->session_coordinator()->IsSessionModified(
        manager->GetCurrentSessionIndex()));

    manager->Quit();
    ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
    EXPECT_NE(manager->GetPendingUnsavedSessionActionPrompt().find(
                  "unapplied Screen Editor edits"),
              std::string::npos);
    manager->CancelPendingUnsavedSessionAction();

    const auto autosave_status = manager->AutosaveActiveSession();
    EXPECT_EQ(autosave_status.code(), absl::StatusCode::kFailedPrecondition)
        << autosave_status;
    EXPECT_NE(std::string(autosave_status.message())
                  .find(domain == PendingDomain::kDungeonMap
                            ? "ROM-backed state is invalid"
                            : "unsupported domains"),
              std::string::npos);

    auto* session = manager->session_coordinator()->GetActiveRomSession();
    ASSERT_NE(session, nullptr);
    session->backup_restore_pending = true;
    const auto discard_status = manager->DiscardPendingRomBackupRestore();
    EXPECT_EQ(discard_status.code(), absl::StatusCode::kFailedPrecondition)
        << discard_status;
    EXPECT_NE(std::string(discard_status.message())
                  .find("pending graphics, screen, dungeon, or palette edits"),
              std::string::npos);
    EXPECT_TRUE(session->backup_restore_pending);
    session->backup_restore_pending = false;

    ASSERT_OK(rom->WriteByte(kPcOffset, kPendingRomByte));
    for (const bool screen_scope_enabled : {false, true}) {
      core::FeatureFlags::get().kSaveDungeonMaps = screen_scope_enabled;
      const auto save_status = manager->SaveRom();

      EXPECT_EQ(save_status.code(), absl::StatusCode::kFailedPrecondition)
          << save_status;
      const std::string save_message(save_status.message());
      if (domain == PendingDomain::kDungeonMap) {
        // This peer marks a map pending without loading valid ROM-backed
        // Screen assets. Enabling map writes must not admit that model.
        EXPECT_NE(save_message.find("ROM-backed state"), std::string::npos);
      } else {
        EXPECT_NE(save_message.find("unsupported domains"), std::string::npos);
      }
      EXPECT_EQ(ReadByteAt(rom_path, kPcOffset), 0x00);
      ASSERT_TRUE(rom->ReadByte(kPcOffset).ok());
      EXPECT_EQ(*rom->ReadByte(kPcOffset), kPendingRomByte);
      EXPECT_TRUE(editor_set->HasPendingScreenChanges());
    }
  }
}

TEST(ScreenSaveStoplossTest, DirectEditorSaveFailsClosedForEveryDomain) {
  ScreenEditor editor;

  const std::array mark_pending = {
      &ScreenEditorSaveStoplossTestPeer::MarkDungeonMapModified,
      &ScreenEditorSaveStoplossTestPeer::MarkDungeonMapTile16Modified,
      &ScreenEditorSaveStoplossTestPeer::MarkTitleScreenModified,
      &ScreenEditorSaveStoplossTestPeer::MarkOverworldMapModified,
      &ScreenEditorSaveStoplossTestPeer::MarkOverworldMapPaletteModified,
  };
  for (const auto mark : mark_pending) {
    ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&editor);
    mark(&editor);
    const auto status = editor.Save();
    EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  }
}

class ScreenMapSaveAdmissionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = MakeTempFilePath("yaze_screen_map_save_admission");
    ASSERT_TRUE(std::filesystem::create_directories(root_));
    source_ = root_ / "source.sfc";
    const auto project_path = root_ / "source.yaze";
    WriteAdmissibleScreenMapTestRom(source_, "MAP SAVE ADMISSION");
    WriteTestProject(project_path, source_);
    manager_ = std::make_unique<EditorManager>();
    InitializeWithIsolatedSettings(*manager_, &renderer_);
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager_->user_settings().prefs().backup_before_save = false;
    ASSERT_OK(manager_->OpenRomOrProject(project_path.string()));
    DisableRomWritesForTest();
    core::FeatureFlags::get().kSaveDungeonMaps = true;
    auto* project = manager_->GetCurrentProject();
    ASSERT_NE(project, nullptr);
    project->workspace_settings.backup_on_save = false;
    project->rom_metadata.expected_hash.clear();
    project->feature_flags = core::FeatureFlags::get();
    auto* session = manager_->session_coordinator()->GetActiveRomSession();
    ASSERT_NE(session, nullptr);
    session->feature_flags = core::FeatureFlags::get();
    ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kScreen));
    screen_ = manager_->GetCurrentEditorSet()->GetScreenEditor();
    ASSERT_NE(screen_, nullptr);
    ASSERT_TRUE(screen_->IsRomBackedStateValid());
    ASSERT_EQ(ScreenEditorSaveStoplossTestPeer::DungeonMapCount(*screen_), 14u);
    ASSERT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedFirstMapRoom(screen_),
              0x01);
  }

  void TearDown() override {
    manager_.reset();
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }

  void EditFirstRoom() {
    ScreenEditorSaveStoplossTestPeer::EditLoadedFirstMapRoom(screen_, 0x02);
    ASSERT_TRUE(screen_->HasPendingDungeonMapChanges());
    ASSERT_TRUE(screen_->CanUndo());
  }

  FeatureFlagsGuard flags_;
  ScopedImGuiContext imgui_;
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
  std::filesystem::path root_;
  std::filesystem::path source_;
  ScreenEditor* screen_ = nullptr;
};

TEST_F(ScreenMapSaveAdmissionTest,
       MapOnlySaveAsReopensAndUndoAfterCommitCanBeSavedAgain) {
  EditFirstRoom();
  const auto source_hash = util::ComputeFileSha1Hex(source_.string());
  const auto destination = root_ / "saved.sfc";
  ASSERT_OK(manager_->SaveRomAs(destination.string()));
  EXPECT_EQ(util::ComputeFileSha1Hex(source_.string()), source_hash);
  EXPECT_EQ(ReadByteAt(destination, zelda3::kDungeonMapDataStart), 0x02);
  EXPECT_FALSE(screen_->HasPendingDungeonMapChanges());
  ASSERT_TRUE(screen_->CanUndo());
  ASSERT_OK(screen_->Undo());
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedFirstMapRoom(screen_),
            0x01);
  EXPECT_TRUE(screen_->HasPendingDungeonMapChanges());
  ASSERT_OK(screen_->Redo());
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedFirstMapRoom(screen_),
            0x02);
  EXPECT_TRUE(screen_->HasPendingDungeonMapChanges());
  ASSERT_OK(manager_->SaveRom());
  EXPECT_FALSE(screen_->HasPendingDungeonMapChanges());

  manager_.reset();
  auto reopened = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*reopened, &renderer_);
  reopened->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_OK(reopened->OpenRomOrProject(destination.string()));
  ASSERT_OK(reopened->EnsureEditorAssetsLoaded(EditorType::kScreen));
  auto* fresh_screen = reopened->GetCurrentEditorSet()->GetScreenEditor();
  ASSERT_NE(fresh_screen, nullptr);
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedFirstMapRoom(fresh_screen),
            0x02);
}

TEST_F(ScreenMapSaveAdmissionTest,
       DirectSaveKeepsMapPendingUntilCoordinatedFileCommit) {
  EditFirstRoom();
  ASSERT_OK(screen_->Save());
  EXPECT_TRUE(screen_->HasPendingDungeonMapChanges());
  EXPECT_TRUE(screen_->CanUndo());
  EXPECT_EQ(ReadByteAt(source_, zelda3::kDungeonMapDataStart), 0x01);
  ASSERT_OK(manager_->SaveRom());
  EXPECT_EQ(ReadByteAt(source_, zelda3::kDungeonMapDataStart), 0x02);
  EXPECT_FALSE(screen_->HasPendingDungeonMapChanges());
  EXPECT_TRUE(screen_->CanUndo());
}

TEST_F(ScreenMapSaveAdmissionTest,
       FailedFileCommitPreservesMapModelHistoryAndPendingForRetry) {
  EditFirstRoom();
  const auto memory_before = manager_->GetCurrentRom()->vector();
  {
    test::ScopedRomStagingFailure fault;
    const auto failed = manager_->SaveRom();
    EXPECT_FALSE(failed.ok());
    EXPECT_NE(failed.message().find("staging failure injected for testing"),
              absl::string_view::npos)
        << failed;
  }
  EXPECT_EQ(ReadByteAt(source_, zelda3::kDungeonMapDataStart), 0x01);
  EXPECT_EQ(manager_->GetCurrentRom()->vector(), memory_before);
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedFirstMapRoom(screen_),
            0x02);
  EXPECT_TRUE(screen_->HasPendingDungeonMapChanges());
  EXPECT_TRUE(screen_->CanUndo());
  ASSERT_OK(manager_->SaveRom());
  EXPECT_EQ(ReadByteAt(source_, zelda3::kDungeonMapDataStart), 0x02);
  EXPECT_FALSE(screen_->HasPendingDungeonMapChanges());
}

TEST_F(ScreenMapSaveAdmissionTest,
       DisabledMixedAndUnfinishedMapEditsAreRefusedBeforeWriting) {
  EditFirstRoom();
  core::FeatureFlags::get().kSaveDungeonMaps = false;
  const auto disabled = manager_->SaveRom();
  EXPECT_EQ(disabled.code(), absl::StatusCode::kFailedPrecondition) << disabled;
  EXPECT_NE(disabled.message().find("disabled"), absl::string_view::npos);
  core::FeatureFlags::get().kSaveDungeonMaps = true;

  ScreenEditorSaveStoplossTestPeer::MarkDungeonMapTile16Modified(screen_);
  const auto mixed = manager_->SaveRom();
  EXPECT_EQ(mixed.code(), absl::StatusCode::kFailedPrecondition) << mixed;
  EXPECT_NE(mixed.message().find("unsupported domains"),
            absl::string_view::npos);
  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(screen_);
  ScreenEditorSaveStoplossTestPeer::EditLoadedFirstMapRoom(screen_, 0x03);
  ScreenEditorSaveStoplossTestPeer::StartUnfinishedDungeonMapEdit(screen_);
  const auto unfinished = manager_->SaveRom();
  EXPECT_EQ(unfinished.code(), absl::StatusCode::kFailedPrecondition)
      << unfinished;
  EXPECT_NE(unfinished.message().find("Finish the current"),
            absl::string_view::npos);
  ScreenEditorSaveStoplossTestPeer::CancelUnfinishedDungeonMapEdit(screen_);
  EXPECT_EQ(ReadByteAt(source_, zelda3::kDungeonMapDataStart), 0x01);
  EXPECT_TRUE(screen_->CanUndo());
}

TEST_F(ScreenMapSaveAdmissionTest,
       ChangedRomRefusesAndManagerRestoresGameDataOwner) {
  EditFirstRoom();
  auto* original_rom = manager_->GetCurrentRom();
  auto* original_data = manager_->GetCurrentGameData();
  const auto memory_before = original_rom->vector();
  const auto source_hash = util::ComputeFileSha1Hex(source_.string());
  Rom other_rom;
  ASSERT_OK(other_rom.LoadFromFile(source_.string()));
  screen_->set_rom(&other_rom);
  const auto wrong_rom = manager_->SaveRom();
  EXPECT_EQ(wrong_rom.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(wrong_rom.message().find("current ROM session"),
            absl::string_view::npos);
  screen_->set_rom(original_rom);
  zelda3::GameData other_data;
  screen_->SetGameData(&other_data);
  const auto wrong_data = screen_->Save();
  EXPECT_EQ(wrong_data.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(wrong_data.message().find("current ROM session"),
            absl::string_view::npos);
  EXPECT_EQ(original_rom->vector(), memory_before);
  EXPECT_EQ(util::ComputeFileSha1Hex(source_.string()), source_hash);
  EXPECT_TRUE(screen_->HasPendingDungeonMapChanges());
  EXPECT_TRUE(screen_->CanUndo());
  // Project-save preparation rebinds the active session's dependencies before
  // Screen Editor preflight, repairing the stale GameData pointer above.
  ASSERT_OK(manager_->SaveRom());
  EXPECT_EQ(screen_->game_data(), original_data);
  EXPECT_FALSE(screen_->HasPendingScreenChanges());
}

TEST_F(ScreenMapSaveAdmissionTest,
       ManifestConflictBlocksMapSaveWithAndWithoutBackingFile) {
  EditFirstRoom();
  auto* project = manager_->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  ASSERT_OK(project->hack_manifest.LoadFromString(R"json({
    "manifest_version": 3,
    "protected_regions": {"regions": [
      {"start": "0x0AF039", "end": "0x0AF03A", "module": "Map hook"}
    ]}
  })json"));
  const auto blocked = manager_->SaveRom();
  EXPECT_EQ(blocked.code(), absl::StatusCode::kCancelled) << blocked;
  EXPECT_FALSE(manager_->pending_write_conflicts().empty());
  EXPECT_EQ(ReadByteAt(source_, zelda3::kDungeonMapDataStart), 0x01);
  EXPECT_TRUE(screen_->HasPendingDungeonMapChanges());
  EXPECT_TRUE(screen_->CanUndo());

  // With the original ROM file absent, diffing cannot identify changed
  // bytes. The preflight map plan must still protect the same ASM-owned cell.
  manager_->ClearPendingWriteConflicts();
  ASSERT_TRUE(std::filesystem::remove(source_));
  const auto destination = root_ / "blocked-without-source.sfc";
  const auto fallback_blocked = manager_->SaveRomAs(destination.string());
  EXPECT_EQ(fallback_blocked.code(), absl::StatusCode::kCancelled)
      << fallback_blocked;
  EXPECT_FALSE(manager_->pending_write_conflicts().empty());
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_TRUE(screen_->HasPendingDungeonMapChanges());
  EXPECT_TRUE(screen_->CanUndo());
}

TEST(ScreenSaveStoplossTest,
     InvalidStateSaveAndEmptyDungeonDrawMethodsFailClosed) {
  ScopedImGuiContext imgui;
  ScreenEditor editor;

  const auto save_status = editor.Save();
  EXPECT_EQ(save_status.code(), absl::StatusCode::kFailedPrecondition)
      << save_status;
  EXPECT_NE(std::string(save_status.message()).find("ROM-backed state"),
            std::string::npos);

  ImGui::NewFrame();
  ImGui::Begin("EmptyDungeonMapDrawSafety");
  ScreenEditorSaveStoplossTestPeer::DrawEmptyDungeonMapMethods(&editor);
  ImGui::End();
  ImGui::Render();
}

TEST(ScreenSaveStoplossTest,
     DirectTitleAndPauseMapSavesFailWithoutClearingPendingDomains) {
  ScreenEditor editor;

  ScreenEditorSaveStoplossTestPeer::MarkTitleScreenModified(&editor);
  const auto title_status =
      ScreenEditorSaveStoplossTestPeer::SaveTitleScreenToRom(&editor);
  EXPECT_EQ(title_status.code(), absl::StatusCode::kFailedPrecondition)
      << title_status;
  EXPECT_TRUE(editor.HasPendingTitleScreenChanges());

  ScreenEditorSaveStoplossTestPeer::MarkOverworldMapModified(&editor);
  ScreenEditorSaveStoplossTestPeer::MarkOverworldMapPaletteModified(&editor);
  const auto map_status =
      ScreenEditorSaveStoplossTestPeer::SaveOverworldMapToRom(&editor);
  EXPECT_EQ(map_status.code(), absl::StatusCode::kFailedPrecondition)
      << map_status;
  EXPECT_TRUE(
      ScreenEditorSaveStoplossTestPeer::HasPendingOverworldMapTileChanges(
          editor));
  EXPECT_TRUE(
      ScreenEditorSaveStoplossTestPeer::HasPendingOverworldMapPaletteChanges(
          editor));
  EXPECT_TRUE(editor.HasPendingOverworldMapChanges());
}

TEST(ScreenSaveStoplossTest, MutationUndoAndRedoRemarkEachDomainPending) {
  ScreenEditor dungeon_map_editor;
  ScreenEditorSaveStoplossTestPeer::CommitDungeonMapEdit(&dungeon_map_editor);
  ASSERT_TRUE(dungeon_map_editor.HasPendingDungeonMapChanges());

  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&dungeon_map_editor);
  ASSERT_OK(dungeon_map_editor.Undo());
  EXPECT_TRUE(dungeon_map_editor.HasPendingDungeonMapChanges());

  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&dungeon_map_editor);
  ASSERT_OK(dungeon_map_editor.Redo());
  EXPECT_TRUE(dungeon_map_editor.HasPendingDungeonMapChanges());

  ScreenEditor tile16_editor;
  ScreenEditorSaveStoplossTestPeer::CommitDungeonMapTile16Edit(&tile16_editor);
  ASSERT_TRUE(tile16_editor.HasPendingDungeonMapTile16Changes());

  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&tile16_editor);
  ASSERT_OK(tile16_editor.Undo());
  EXPECT_TRUE(tile16_editor.HasPendingDungeonMapTile16Changes());

  ScreenEditorSaveStoplossTestPeer::ClearPendingChanges(&tile16_editor);
  ASSERT_OK(tile16_editor.Redo());
  EXPECT_TRUE(tile16_editor.HasPendingDungeonMapTile16Changes());
}

TEST(ScreenSaveStoplossTest, ReloadResetClearsRomBackedStateAndHistory) {
  ScopedTextureQueueClear texture_queue;
  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  ScreenEditor editor;
  ScreenEditorSaveStoplossTestPeer::PrimeRomBackedState(&editor);
  ASSERT_TRUE(ScreenEditorSaveStoplossTestPeer::HasReloadResidue(editor));
  const auto owners_before =
      ScreenEditorSaveStoplossTestPeer::CaptureBitmapOwners(&editor);
  ScreenEditorSaveStoplossTestPeer::QueueBitmapOwners(owners_before);

  ScreenEditorSaveStoplossTestPeer::ResetRomBackedStateForLoad(&editor);

  EXPECT_FALSE(ScreenEditorSaveStoplossTestPeer::HasReloadResidue(editor));
  const auto owners_after =
      ScreenEditorSaveStoplossTestPeer::CaptureBitmapOwners(&editor);
  ScreenEditorSaveStoplossTestPeer::ExpectSameBitmapOwners(owners_before,
                                                           owners_after);

  // Process the actual queue after reset. Under ASan this catches any future
  // reset that frees a Bitmap owner still referenced by Arena.
  while (gfx::Arena::Get().HasPendingTextures()) {
    gfx::Arena::Get().ProcessTextureQueue(&renderer);
  }
}

TEST(ScreenSaveStoplossTest,
     RepeatedScreenModelCreateDoesNotAccumulatePaletteState) {
  ScopedImGuiContext imgui;
  ScopedTextureQueueClear texture_queue;

  const auto rom_path = MakeTempFilePath("yaze_screen_model_reload.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteScreenModelTestRom(rom_path);

  Rom rom;
  ASSERT_OK(rom.LoadFromFile(rom_path.string()));
  zelda3::GameData game_data(&rom);
  PopulateTitleScreenTestGameData(&game_data);
  ScreenEditor editor(&rom);

  for (int load = 0; load < 2; ++load) {
    if (load != 0) {
      ScreenEditorSaveStoplossTestPeer::ResetRomBackedStateForLoad(&editor);
    }
    ASSERT_OK(ScreenEditorSaveStoplossTestPeer::CreateTitleScreenModel(
        &editor, &rom, &game_data));
    ASSERT_OK(ScreenEditorSaveStoplossTestPeer::CreateOverworldMapModel(&editor,
                                                                        &rom));
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::TitleScreenPaletteSize(editor),
              64u);
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LightWorldPaletteSize(editor),
              128u);
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::DarkWorldPaletteSize(editor),
              128u);
  }
}

// --- File > Close ROM / Revert to Saved -----------------------------------

TEST(EditorManagerFileLifecycleTest, CloseRomClosesTheOnlyCleanSession) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto temp_dir = MakeTempFilePath("yaze_close_rom_clean");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "close.sfc";
  WriteTestRom(rom_path, "CLOSE ROM");
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  ASSERT_EQ(manager->GetActiveSessionCount(), 1u);
  ASSERT_TRUE(manager->CanCloseRom());

  manager->CloseRom();

  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetActiveSessionCount(), 0u);
  EXPECT_EQ(manager->GetCurrentRom(), nullptr);
  EXPECT_FALSE(manager->CanCloseRom());
  EXPECT_FALSE(manager->CanRevertRom());
}

TEST(EditorManagerFileLifecycleTest, CloseRomAsksBeforeDiscardingEdits) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto temp_dir = MakeTempFilePath("yaze_close_rom_dirty");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "close_dirty.sfc";
  WriteTestRom(rom_path, "CLOSE DIRTY");
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));
  ASSERT_TRUE(rom->dirty());

  manager->CloseRom();

  // The confirmation is pending; nothing is closed yet.
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  EXPECT_EQ(manager->GetPendingUnsavedSessionActionContinueLabel(),
            "Close Without Saving");
  EXPECT_EQ(manager->GetPendingUnsavedSessionActionSaveLabel(),
            "Save Work & Close ROM");

  manager->CancelPendingUnsavedSessionAction();
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);

  manager->CloseRom();
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_EQ(manager->GetActiveSessionCount(), 0u);
  EXPECT_EQ(manager->GetCurrentRom(), nullptr);
  // Discarding never touches the file on disk.
  EXPECT_EQ(ReadByteAt(rom_path, 0x1234), 0x00);
}

TEST(EditorManagerFileLifecycleTest,
     RevertRomToSavedDiscardsEditsAfterConfirm) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto temp_dir = MakeTempFilePath("yaze_revert_rom_dirty");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "revert.sfc";
  WriteTestRom(rom_path, "REVERT DIRTY");
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));
  ASSERT_TRUE(manager->CanRevertRom());
  Rom* rom = manager->GetCurrentRom();
  ASSERT_NE(rom, nullptr);
  ASSERT_OK(rom->WriteByte(0x1234, 0xA5));

  ASSERT_OK(manager->RevertRomToSaved());
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  // Saving then reloading would be a no-op, so no save button is offered.
  EXPECT_EQ(manager->GetPendingUnsavedSessionActionSaveLabel(), "");
  EXPECT_EQ(manager->GetPendingUnsavedSessionActionContinueLabel(),
            "Discard Edits & Revert");
  // Still the edited bytes while the prompt is up.
  ASSERT_TRUE(manager->GetCurrentRom()->ReadByte(0x1234).ok());
  EXPECT_EQ(*manager->GetCurrentRom()->ReadByte(0x1234), 0xA5);

  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();

  Rom* reverted = manager->GetCurrentRom();
  ASSERT_NE(reverted, nullptr);
  EXPECT_EQ(manager->GetActiveSessionCount(), 1u);
  ASSERT_TRUE(reverted->ReadByte(0x1234).ok());
  EXPECT_EQ(*reverted->ReadByte(0x1234), 0x00);
  EXPECT_FALSE(reverted->dirty());
  EXPECT_EQ(ReadByteAt(rom_path, 0x1234), 0x00);
}

TEST(EditorManagerFileLifecycleTest, RevertCleanRomPicksUpDiskChanges) {
  FeatureFlagsGuard guard;
  ScopedImGuiContext imgui;

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const auto temp_dir = MakeTempFilePath("yaze_revert_rom_clean");
  ScopedDirectoryCleanup cleanup{temp_dir};
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  const auto rom_path = temp_dir / "revert_clean.sfc";
  WriteTestRom(rom_path, "REVERT CLEAN");
  ASSERT_OK(manager->OpenRomOrProject(rom_path.string()));

  // Another tool (assembler, emulator) rewrote the file on disk.
  WriteByteAt(rom_path, 0x2000, 0x5A);

  // No unsaved work: revert runs immediately without a prompt.
  ASSERT_OK(manager->RevertRomToSaved());
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  Rom* reverted = manager->GetCurrentRom();
  ASSERT_NE(reverted, nullptr);
  ASSERT_TRUE(reverted->ReadByte(0x2000).ok());
  EXPECT_EQ(*reverted->ReadByte(0x2000), 0x5A);
  EXPECT_FALSE(reverted->dirty());
}

// These exercise the product lifecycle with strict hash gating. Serializer
// opt-outs isolate raw-byte edits; the screen-model test retains its existing
// save refusal. Every file and project context belongs to this fixture.
class SessionRomIdentityTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root = MakeTempFilePath("yaze_session_identity");
    ASSERT_TRUE(std::filesystem::create_directories(root));
    a = root / "A.sfc";
    b = root / "B.sfc";
    WriteScreenModelTestRom(a, "IDENTITY A");
    WriteScreenModelTestRom(b, "IDENTITY B");
    manager = std::make_unique<EditorManager>();
    InitializeWithIsolatedSettings(*manager, &renderer);
    manager->SetAssetLoadMode(AssetLoadMode::kLazy);
    manager->user_settings().prefs().backup_before_save = false;
    ASSERT_OK(manager->OpenRomOrProject(a.string()));
    StrictProject();
    ASSERT_OK(manager->OpenRomOrProject(b.string()));
    StrictProject();
    Switch(0);
  }

  void TearDown() override {
    manager.reset();
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  }

  void StrictProject() {
    DisableRomWritesForTest();
    core::FeatureFlags::get().kSaveAllPalettes = false;
    core::FeatureFlags::get().kSaveGfxGroups = false;
    auto* project = manager->GetCurrentProject();
    project->name = "Strict identity";
    project->filepath =
        (root /
         ("strict-" + std::to_string(manager->GetCurrentSessionId()) + ".yaze"))
            .string();
    // Explicitly avoid the editable-target exemption.
    project->rom_filename = (root / "different-edit-target.sfc").string();
    project->workspace_settings.backup_on_save = false;
    project->feature_flags = core::FeatureFlags::get();
    project->rom_metadata.write_policy = project::RomWritePolicy::kBlock;
    project->rom_metadata.expected_hash = manager->GetCurrentRomHash();
    ASSERT_FALSE(manager->IsRomHashMismatch());
  }

  void Switch(size_t index) {
    manager->SwitchToSession(index);
    if (manager->HasPendingUnsavedSessionAction()) {
      EXPECT_EQ(manager->GetPendingUnsavedSessionActionContinueLabel(),
                "Switch Without Saving");
      manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
    }
    ASSERT_EQ(manager->GetCurrentSessionIndex(), index);
  }

  RomSession* Session() {
    return manager->session_coordinator()->GetActiveRomSession();
  }

  void ExpectBaseline(const RomBaselineIdentity& baseline) {
    ASSERT_TRUE(Session()->rom_baseline.has_value());
    const auto& actual = *Session()->rom_baseline;
    EXPECT_EQ(actual.normalized_path, baseline.normalized_path);
    EXPECT_EQ(actual.editing_source_path, baseline.editing_source_path);
    EXPECT_EQ(actual.hashes.crc32, baseline.hashes.crc32);
    EXPECT_EQ(actual.hashes.legacy_crc32, baseline.hashes.legacy_crc32);
    EXPECT_EQ(actual.hashes.sha1, baseline.hashes.sha1);
    EXPECT_EQ(manager->GetCurrentRomHash(), baseline.hashes.crc32);
  }

  void CheckFailedRevertPreservesScreenModelAndUndo() {

    ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kScreen));
    auto* screen = manager->GetCurrentEditorSet()->GetScreenEditor();
    ASSERT_NE(screen, nullptr);
    const auto baseline = *Session()->rom_baseline;
    const auto raw = manager->GetCurrentRom()->vector();
    const uint16_t before =
        ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen);
    ASSERT_NE(before, 0x0123);
    ScreenEditorSaveStoplossTestPeer::EditLoadedBossRoom(screen, 0x0123);
    ASSERT_TRUE(screen->HasPendingScreenChanges());
    ASSERT_TRUE(screen->CanUndo());
    const auto undo_description = screen->GetUndoDescription();
    EXPECT_EQ(manager->GetCurrentRom()->vector(), raw);

    WriteTooSmallForAssetsTestRom(a);
    const auto disk_hash = util::ComputeFileSha1Hex(a.string());
    ASSERT_OK(manager->RevertRomToSaved());
    ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
    manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
    EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
    const auto* toast = manager->toast_manager()->GetPendingToast(
        manager->toast_manager()->pending_count() - 1);
    ASSERT_NE(toast, nullptr);
    EXPECT_EQ(toast->type, ToastType::kError);
    EXPECT_NE(toast->message.find("Revert failed:"), std::string::npos);
    EXPECT_NE(toast->message.find("palette"), std::string::npos);
    ExpectBaseline(baseline);
    EXPECT_EQ(manager->GetCurrentRom()->vector(), raw);
    EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), disk_hash);
    EXPECT_EQ(manager->GetCurrentEditorSet()->GetScreenEditor(), screen);
    EXPECT_TRUE(screen->HasPendingScreenChanges());
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen), 0x0123);
    EXPECT_EQ(screen->GetUndoDescription(), undo_description);
    ASSERT_TRUE(screen->CanUndo());
    ASSERT_OK(screen->Undo());
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen), before);
    ASSERT_OK(screen->Redo());
    EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen), 0x0123);
  }

  FeatureFlagsGuard flags;
  ScopedImGuiContext imgui;
  gfx::NullRenderer renderer;
  std::unique_ptr<EditorManager> manager;
  std::filesystem::path root, a, b;
};

TEST_F(SessionRomIdentityTest, SwitchedRawEditSavesAndFreshReopens) {
  const auto baseline = *Session()->rom_baseline;
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  const auto bytes = manager->GetCurrentRom()->vector();
  Switch(1);
  Switch(0);
  ExpectBaseline(baseline);
  EXPECT_FALSE(manager->IsRomHashMismatch());
  ASSERT_OK(manager->SaveRom());
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(a.string()));
  EXPECT_EQ(reopened.vector(), bytes);
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());
  EXPECT_EQ(manager->GetCurrentRomHash(),
            util::ComputeRomHash(reopened.data(), reopened.size()));
  EXPECT_EQ(Session()->rom_baseline->normalized_path, a.string());
  EXPECT_EQ(manager->GetCurrentProject()->rom_metadata.expected_hash,
            baseline.hashes.crc32);
}

TEST_F(SessionRomIdentityTest, SwitchedRawEditSaveAsRebindsOnlyAfterCommit) {
  const auto baseline = *Session()->rom_baseline;
  const auto source_hash = util::ComputeFileSha1Hex(a.string());
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  const auto bytes = manager->GetCurrentRom()->vector();
  Switch(1);
  Switch(0);
  ExpectBaseline(baseline);
  const auto destination = root / "saved.sfc";
  ASSERT_OK(manager->SaveRomAs(destination.string()));
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(destination.string()));
  EXPECT_EQ(reopened.vector(), bytes);
  EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), source_hash);
  const auto committed = *Session()->rom_baseline;
  EXPECT_EQ(committed.normalized_path, destination.string());
  EXPECT_EQ(committed.hashes.crc32,
            util::ComputeRomHash(reopened.data(), reopened.size()));
  EXPECT_EQ(manager->GetCurrentProject()->rom_metadata.expected_hash,
            baseline.hashes.crc32);
  // The committed identity must survive another edit/activation unchanged.
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1235, 0x72));
  Switch(1);
  Switch(0);
  ExpectBaseline(committed);
}

TEST_F(SessionRomIdentityTest,
       EditableSaveAsRetainsSourceAcrossFailureSwitchAndRepeatedSaves) {
  auto* project = manager->GetCurrentProject();
  project->rom_filename = a.string();
  project->build_target = (root / "patched.sfc").string();
  const auto expected_hash = project->rom_metadata.expected_hash;
  const auto project_path = project->filepath;
  const auto source_hash = util::ComputeFileSha1Hex(a.string());
  const auto first = root / "first.sfc";
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  ASSERT_OK(manager->SaveRomAs(first.string()));
  const auto committed = *Session()->rom_baseline;
  EXPECT_EQ(committed.normalized_path, first.string());
  EXPECT_EQ(committed.editing_source_path, a.string());
  EXPECT_FALSE(manager->IsRomHashMismatch());
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1235, 0x72));
  const auto pending = manager->GetCurrentRom()->vector();
  const auto first_hash = util::ComputeFileSha1Hex(first.string());
  const auto second = root / "second.sfc";
  {
    test::ScopedRomStagingFailure fault;
    const auto failed = manager->SaveRomAs(second.string());
    EXPECT_FALSE(failed.ok());
    EXPECT_NE(failed.message().find("staging failure injected for testing"),
              absl::string_view::npos)
        << failed;
  }
  ExpectBaseline(committed);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), pending);
  EXPECT_TRUE(manager->GetCurrentRom()->dirty());
  EXPECT_FALSE(std::filesystem::exists(second));
  EXPECT_EQ(util::ComputeFileSha1Hex(first.string()), first_hash);
  Switch(1);
  EXPECT_EQ(Session()->rom_baseline->editing_source_path, b.string());
  Switch(0);
  ExpectBaseline(committed);
  EXPECT_FALSE(manager->IsRomHashMismatch());
  ASSERT_OK(manager->SaveRomAs(second.string()));
  EXPECT_EQ(Session()->rom_baseline->editing_source_path, a.string());
  EXPECT_EQ(Session()->rom_baseline->normalized_path, second.string());
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1236, 0x31));
  ASSERT_OK(manager->SaveRom());
  EXPECT_EQ(ReadByteAt(second, 0x1236), 0x31);
  EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), source_hash);
  EXPECT_EQ(project->rom_metadata.expected_hash, expected_hash);
  EXPECT_EQ(project->filepath, project_path);

  // The session's own Save As does not authorize writing a build output.
  const auto saved = *Session()->rom_baseline;
  const auto blocked = manager->SaveRomAs(project->build_target);
  EXPECT_EQ(blocked.code(), absl::StatusCode::kPermissionDenied) << blocked;
  EXPECT_NE(blocked.message().find("build output"), absl::string_view::npos);
  EXPECT_FALSE(std::filesystem::exists(project->build_target));
  ExpectBaseline(saved);
  // Changing the configured editable target also removes this exemption.
  project->rom_filename = (root / "different-edit-target.sfc").string();
  EXPECT_TRUE(manager->IsRomHashMismatch());
  EXPECT_EQ(manager->SaveRom().code(), absl::StatusCode::kPermissionDenied);
  ExpectBaseline(saved);
}

TEST_F(SessionRomIdentityTest,
       HashMatchedForeignSaveDoesNotAcquireEditableSource) {
  // StrictProject deliberately configures a different editable target. A hash
  // match permits the first save but must not turn it into a trusted lineage.
  const auto expected_hash =
      manager->GetCurrentProject()->rom_metadata.expected_hash;
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  const auto first = root / "foreign-copy.sfc";
  ASSERT_OK(manager->SaveRomAs(first.string()));
  EXPECT_TRUE(manager->IsRomHashMismatch());
  const auto saved = *Session()->rom_baseline;
  const auto disk_hash = util::ComputeFileSha1Hex(first.string());
  const auto second = root / "refused-copy.sfc";
  EXPECT_EQ(manager->SaveRomAs(second.string()).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_FALSE(std::filesystem::exists(second));
  EXPECT_EQ(util::ComputeFileSha1Hex(first.string()), disk_hash);
  ExpectBaseline(saved);
  EXPECT_EQ(manager->GetCurrentProject()->rom_metadata.expected_hash,
            expected_hash);
}

TEST_F(SessionRomIdentityTest, ReloadingSavedCopyResetsEditableSource) {
  ASSERT_OK(manager->EnsureGameDataLoaded());
  manager->GetCurrentProject()->rom_filename = a.string();
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  const auto destination = root / "saved-copy.sfc";
  ASSERT_OK(manager->SaveRomAs(destination.string()));
  EXPECT_EQ(Session()->rom_baseline->editing_source_path, a.string());
  const auto saved = *Session()->rom_baseline;
  // Failed external replacement preserves our own committed lineage.
  WriteTooSmallForAssetsTestRom(destination);
  EXPECT_FALSE(manager->RevertRomToSaved().ok());
  ExpectBaseline(saved);
  // Once different disk bytes are loaded successfully, their source is the
  // replacement file, not the original editable target.
  WriteScreenModelTestRom(destination, "EXTERNAL REPLACEMENT");
  ASSERT_OK(manager->RevertRomToSaved());
  EXPECT_EQ(Session()->rom_baseline->editing_source_path, destination.string());
  EXPECT_TRUE(manager->IsRomHashMismatch());
  EXPECT_EQ(manager->SaveRom().code(), absl::StatusCode::kPermissionDenied);
}

TEST_F(SessionRomIdentityTest, TwoDirtySessionsKeepTheirOwnPolicyAndIdentity) {
  auto* a_rom = manager->GetCurrentRom();
  const auto a_baseline = *Session()->rom_baseline;
  ASSERT_OK(a_rom->WriteByte(0x1234, 0xA5));
  Switch(1);
  auto* b_rom = manager->GetCurrentRom();
  const auto b_baseline = *Session()->rom_baseline;
  ASSERT_NE(a_baseline.hashes.crc32, b_baseline.hashes.crc32);
  ASSERT_OK(b_rom->WriteByte(0x1234, 0x72));
  for (int repeat = 0; repeat < 2; ++repeat) {
    Switch(0);
    ExpectBaseline(a_baseline);
    EXPECT_FALSE(manager->IsRomHashMismatch());
    EXPECT_TRUE(a_rom->dirty());
    EXPECT_EQ(a_rom->vector()[0x1234], 0xA5);
    Switch(1);
    ExpectBaseline(b_baseline);
    EXPECT_FALSE(manager->IsRomHashMismatch());
    EXPECT_TRUE(b_rom->dirty());
    EXPECT_EQ(b_rom->vector()[0x1234], 0x72);
  }
  // The pointer-selection entry point is activation too, including same ROM.
  ASSERT_OK(manager->SetCurrentRom(a_rom));
  ExpectBaseline(a_baseline);
  ASSERT_OK(manager->SetCurrentRom(a_rom));
  ExpectBaseline(a_baseline);
  // The frame loop visits both sessions through transient activation.
  ImGui::NewFrame();
  manager->session_coordinator()->UpdateSessions();
  ImGui::Render();
  EXPECT_EQ(manager->GetCurrentRom(), a_rom);
  ExpectBaseline(a_baseline);
  EXPECT_FALSE(manager->IsRomHashMismatch());
  EXPECT_EQ(ReadByteAt(a, 0x1234), 0);
  EXPECT_EQ(ReadByteAt(b, 0x1234), 0);
}

TEST_F(SessionRomIdentityTest, FailedSaveAsKeepsBaselineAndPendingRawBytes) {
  const auto baseline = *Session()->rom_baseline;
  const auto disk_hash = util::ComputeFileSha1Hex(a.string());
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  const auto bytes = manager->GetCurrentRom()->vector();
  Switch(1);
  Switch(0);
  const auto parent_file = root / "not-a-directory";
  std::ofstream(parent_file) << "PRESERVE";
  const auto destination_hash = util::ComputeFileSha1Hex(parent_file.string());
  const auto status = manager->SaveRomAs((parent_file / "fail.sfc").string());
  EXPECT_FALSE(status.ok());
  EXPECT_NE(status.code(), absl::StatusCode::kPermissionDenied) << status;
  EXPECT_NE(
      std::string(status.message()).find("Could not create temp ROM file"),
      std::string::npos)
      << status;
  ExpectBaseline(baseline);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), bytes);
  EXPECT_TRUE(manager->GetCurrentRom()->dirty());
  EXPECT_EQ(manager->GetCurrentRom()->filename(), a.string());
  EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), disk_hash);
  EXPECT_EQ(util::ComputeFileSha1Hex(parent_file.string()), destination_hash);
}

TEST_F(SessionRomIdentityTest, FailedRevertRestoresDirtyBytesAndPriorBaseline) {
  ASSERT_OK(manager->EnsureGameDataLoaded());
  const auto baseline = *Session()->rom_baseline;
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  const auto bytes = manager->GetCurrentRom()->vector();
  const auto id = manager->GetCurrentSessionId();
  // Pass file loading, then fail shared-asset validation on the palette table.
  WriteTooSmallForAssetsTestRom(a);
  const auto external_disk_hash = util::ComputeFileSha1Hex(a.string());
  ASSERT_OK(manager->RevertRomToSaved());
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_FALSE(manager->HasPendingUnsavedSessionAction());
  const auto* toast = manager->toast_manager()->GetPendingToast(
      manager->toast_manager()->pending_count() - 1);
  ASSERT_NE(toast, nullptr);
  EXPECT_EQ(toast->type, ToastType::kError);
  EXPECT_NE(toast->message.find("Revert failed:"), std::string::npos);
  EXPECT_NE(toast->message.find("palette"), std::string::npos);
  EXPECT_EQ(manager->GetCurrentSessionId(), id);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), bytes);
  EXPECT_TRUE(manager->GetCurrentRom()->dirty());
  EXPECT_EQ(manager->GetCurrentRom()->filename(), a.string());
  EXPECT_TRUE(Session()->game_data_loaded);
  ExpectBaseline(baseline);
  EXPECT_FALSE(manager->IsRomHashMismatch());
  EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), external_disk_hash);
  Switch(1);
  Switch(0);
  ExpectBaseline(baseline);
}

TEST_F(SessionRomIdentityTest, FailedRevertPreservesScreenModelAndUndo) {
  CheckFailedRevertPreservesScreenModelAndUndo();
}

TEST_F(SessionRomIdentityTest,
       FailedFullLoadRevertPreservesScreenModelAndUndo) {
  manager->SetAssetLoadMode(AssetLoadMode::kFull);
  CheckFailedRevertPreservesScreenModelAndUndo();
}

TEST_F(SessionRomIdentityTest,
       SuccessfulRevertAndSwapCaptureReplacementIdentity) {
  const auto baseline = *Session()->rom_baseline;
  WriteByteAt(a, 0x2000, 0x5A);
  ASSERT_OK(manager->RevertRomToSaved());
  EXPECT_NE(manager->GetCurrentRomHash(), baseline.hashes.crc32);
  EXPECT_EQ(manager->GetCurrentRomHash(),
            util::ComputeRomHash(manager->GetCurrentRom()->data(),
                                 manager->GetCurrentRom()->size()));
  const auto replacement = *Session()->rom_baseline;
  EXPECT_EQ(replacement.normalized_path, a.string());
  EXPECT_EQ(replacement.editing_source_path, a.string());
  Switch(1);
  Switch(0);
  ExpectBaseline(replacement);
  // Capturing does not silently rewrite the project's expected hash.
  EXPECT_EQ(manager->GetCurrentProject()->rom_metadata.expected_hash,
            baseline.hashes.crc32);

  const auto replacement_file = root / "replacement.sfc";
  WriteScreenModelTestRom(replacement_file, "REBOUND IDENTITY");
  const auto source_hash = util::ComputeFileSha1Hex(a.string());
  ASSERT_OK(manager->SwapProjectRom(replacement_file.string()));
  const auto rebound = *Session()->rom_baseline;
  EXPECT_EQ(rebound.normalized_path, replacement_file.string());
  EXPECT_EQ(rebound.editing_source_path, replacement_file.string());
  EXPECT_EQ(rebound.hashes.crc32,
            util::ComputeRomHash(manager->GetCurrentRom()->data(),
                                 manager->GetCurrentRom()->size()));
  EXPECT_NE(rebound.hashes.crc32, replacement.hashes.crc32);
  EXPECT_EQ(manager->GetCurrentProject()->rom_metadata.expected_hash,
            baseline.hashes.crc32);
  EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), source_hash);
  Switch(1);
  Switch(0);
  ExpectBaseline(rebound);
}

TEST_F(SessionRomIdentityTest,
       CloseToDirtySurvivorAndEmptyDuplicateClearIdentity) {
  const auto baseline = *Session()->rom_baseline;
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0x1234, 0xA5));
  Switch(1);
  manager->CloseCurrentSession();
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 0u);
  ExpectBaseline(baseline);
  EXPECT_FALSE(manager->IsRomHashMismatch());
  manager->DuplicateCurrentSession();
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  ASSERT_EQ(manager->GetCurrentSessionIndex(), 1u);
  EXPECT_FALSE(Session()->rom.is_loaded());
  EXPECT_FALSE(Session()->rom_baseline.has_value());
  EXPECT_TRUE(manager->GetCurrentRomHash().empty());
  manager->CloseCurrentSession();
  ExpectBaseline(baseline);
  manager->CloseRom();
  ASSERT_TRUE(manager->HasPendingUnsavedSessionAction());
  manager->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_EQ(manager->GetCurrentRom(), nullptr);
  EXPECT_TRUE(manager->GetCurrentRomHash().empty());
  manager->CreateNewSession();
  ASSERT_NE(Session(), nullptr);
  EXPECT_FALSE(Session()->rom_baseline.has_value());
  EXPECT_TRUE(manager->GetCurrentRomHash().empty());
}

TEST_F(SessionRomIdentityTest,
       ModelOnlyEditAndUndoSurviveSwitchAndSaveRefusal) {
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kScreen));
  auto* screen = manager->GetCurrentEditorSet()->GetScreenEditor();
  ASSERT_NE(screen, nullptr);
  const auto baseline = *Session()->rom_baseline;
  const auto raw = manager->GetCurrentRom()->vector();
  const uint16_t before =
      ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen);
  ASSERT_NE(before, 0x0123);
  ScreenEditorSaveStoplossTestPeer::EditLoadedBossRoom(screen, 0x0123);
  ASSERT_TRUE(screen->HasPendingScreenChanges());
  ASSERT_TRUE(screen->CanUndo());
  EXPECT_EQ(manager->GetCurrentRom()->vector(), raw);
  core::FeatureFlags::get().kSaveDungeonMaps = true;
  Switch(1);
  Switch(0);
  EXPECT_EQ(manager->GetCurrentEditorSet()->GetScreenEditor(), screen);
  ExpectBaseline(baseline);
  EXPECT_FALSE(manager->IsRomHashMismatch());
  EXPECT_EQ(manager->GetCurrentRom()->vector(), raw);
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen), 0x0123);
  ASSERT_OK(screen->Undo());
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen), before);
  ASSERT_OK(screen->Redo());
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen), 0x0123);
  const auto undo_description = screen->GetUndoDescription();
  // This identity fixture has an unsupported map-layout marker. The failed
  // product save must preserve its edited model and history after switching.
  const auto destination = root / "model-refused.sfc";
  const auto source_hash = util::ComputeFileSha1Hex(a.string());
  const auto failed = manager->SaveRomAs(destination.string());
  EXPECT_EQ(failed.code(), absl::StatusCode::kFailedPrecondition) << failed;
  EXPECT_NE(std::string(failed.message()).find("Unsupported dungeon map"),
            std::string::npos);
  ExpectBaseline(baseline);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), raw);
  EXPECT_TRUE(screen->HasPendingScreenChanges());
  EXPECT_TRUE(screen->CanUndo());
  EXPECT_EQ(screen->GetUndoDescription(), undo_description);
  EXPECT_EQ(ScreenEditorSaveStoplossTestPeer::LoadedBossRoom(screen), 0x0123);
  EXPECT_FALSE(std::filesystem::exists(destination));
  EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), source_hash);
  EXPECT_EQ(Session()->rom_baseline->normalized_path, a.string());
  EXPECT_EQ(manager->GetCurrentProject()->rom_metadata.expected_hash,
            baseline.hashes.crc32);
}

// Real coordinator activation with loaded ROMs, not manual Arena owner changes.
class GraphicsSessionActivationTest : public SessionRomIdentityTest {
 protected:
  void SetUp() override {
    SessionRomIdentityTest::SetUp();
    for (size_t i = 0; i < 2; ++i) {
      Switch(i);
      ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kGraphics));
      sessions[i] = Session();
      graphics[i] = manager->GetCurrentEditorSet()->GetEditorAs<GraphicsEditor>(
          EditorType::kGraphics);
      ASSERT_NE(graphics[i], nullptr);
      ASSERT_TRUE(Session()->game_data.sheet_store.HasSheet(0));
      // Distinct fixture colors make cross-session contamination observable.
      Session()->game_data.palette_groups.overworld_main.SetColor(
          0, 0, gfx::SnesColor(uint16_t(i == 0 ? 0x001F : 0x03E0)));
      *graphics[i]->active() = false;
    }
    Switch(0);
  }

  GraphicsEditorState& State(size_t i) {
    return GraphicsEditorSaveStoplossTestPeer::State(graphics[i]);
  }
  RomSession* sessions[2]{};
  GraphicsEditor* graphics[2]{};
};

TEST_F(GraphicsSessionActivationTest, SwitchPreservesPixelsPaletteAndUndo) {
  auto& arena = gfx::Arena::Get();
  ASSERT_EQ(arena.gfx_sheets_owner(), &sessions[0]->game_data);
  auto* sheet_a = arena.mutable_gfx_sheet(0);
  const auto original_a = sheet_a->vector();
  const auto rom_a = sessions[0]->rom.vector();
  const auto rom_b = sessions[1]->rom.vector();
  const auto baseline = *sessions[0]->rom_baseline;
  ASSERT_OK(GraphicsEditorSaveStoplossTestPeer::PaintPixel(graphics[0], 3));
  ASSERT_OK(SetGraphicsPreviewPalette(State(0), 0,
                                      GraphicsPaletteSelection{"ow_main"}));
  Switch(1);
  ASSERT_EQ(arena.gfx_sheets_owner(), &sessions[1]->game_data);
  auto* sheet_b = arena.mutable_gfx_sheet(0);
  EXPECT_NE(sheet_a, sheet_b);
  ASSERT_OK(GraphicsEditorSaveStoplossTestPeer::PaintPixel(graphics[1], 5));
  ASSERT_OK(SetGraphicsPreviewPalette(State(1), 0,
                                      GraphicsPaletteSelection{"ow_main"}));
  for (int repeat = 0; repeat < 3; ++repeat) {
    Switch(0);
    EXPECT_EQ(arena.mutable_gfx_sheet(0), sheet_a);
    EXPECT_EQ(sheet_a->vector()[0], 3);
    const auto* colors = platform::GetSurfacePalette(sheet_a->surface());
    ASSERT_NE(colors, nullptr);
    EXPECT_EQ(colors->colors[1].r, 255);
    EXPECT_EQ(colors->colors[1].g, 0);
    ASSERT_NE(State(0).PreviewPalette(0), nullptr);
    EXPECT_EQ(State(0).PreviewPalette(0)->group, "ow_main");
    ExpectBaseline(baseline);
    ASSERT_OK(graphics[0]->Undo());
    EXPECT_EQ(sheet_a->vector(), original_a);
    ASSERT_OK(graphics[0]->Redo());
    EXPECT_EQ(sheet_a->vector()[0], 3);
    Switch(1);
    EXPECT_EQ(arena.mutable_gfx_sheet(0), sheet_b);
    EXPECT_EQ(sheet_b->vector()[0], 5);
    colors = platform::GetSurfacePalette(sheet_b->surface());
    ASSERT_NE(colors, nullptr);
    EXPECT_EQ(colors->colors[1].g, 255);
    EXPECT_EQ(colors->colors[1].r, 0);
    EXPECT_TRUE(graphics[1]->CanUndo());
  }
  EXPECT_EQ(sessions[0]->game_data.sheet_store.Sheet(0)[0], 3);
  EXPECT_EQ(sessions[1]->game_data.sheet_store.Sheet(0)[0], 5);
  EXPECT_TRUE(graphics[0]->HasPendingGraphicsChanges());
  EXPECT_TRUE(graphics[1]->HasPendingGraphicsChanges());
  EXPECT_EQ(sessions[0]->rom.vector(), rom_a);
  EXPECT_EQ(sessions[1]->rom.vector(), rom_b);
}

TEST_F(GraphicsSessionActivationTest, FrameSwitchesAndCloseKeepCacheOwnership) {
  auto& arena = gfx::Arena::Get();
  auto* sheet_a = arena.mutable_gfx_sheet(0);
  Switch(1);
  auto* sheet_b = arena.mutable_gfx_sheet(0);
  ASSERT_NE(sheet_a, sheet_b);
  // Both queued commands must still address their own live bitmap after the
  // real frame loop visits B and restores A.
  arena.ClearTextureQueue();
  arena.NotifySheetModified(0);
  Switch(0);
  arena.NotifySheetModified(0);
  auto* events = ContentRegistry::Context::event_bus();
  ASSERT_NE(events, nullptr);
  std::vector<size_t> visits;
  const auto observer = events->Subscribe<SessionSwitchedEvent>(
      [&](const SessionSwitchedEvent& event) {
        if (!event.transient)
          return;
        visits.push_back(event.new_index);
        EXPECT_EQ(arena.gfx_sheets_owner(), &event.session->game_data);
        EXPECT_EQ(arena.mutable_gfx_sheet(0),
                  event.new_index == 0 ? sheet_a : sheet_b);
      });
  ImGui::NewFrame();
  manager->session_coordinator()->UpdateSessions();
  ImGui::Render();
  events->Unsubscribe(observer);
  EXPECT_EQ(visits, (std::vector<size_t>{1, 0}));
  EXPECT_EQ(manager->GetCurrentSessionIndex(), 0);
  EXPECT_EQ(arena.gfx_sheets_owner(), &sessions[0]->game_data);
  EXPECT_EQ(arena.mutable_gfx_sheet(0), sheet_a);
  EXPECT_TRUE(arena.HasPendingTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, sheet_a));
  EXPECT_TRUE(arena.HasPendingTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, sheet_b));
  manager->session_coordinator()->CloseSessionAllowingEmpty(1);
  EXPECT_EQ(arena.gfx_sheets_owner(), &sessions[0]->game_data);
  EXPECT_EQ(arena.texture_command_queue_size(), 1);
  manager->session_coordinator()->CloseSessionAllowingEmpty(0);
  EXPECT_EQ(arena.gfx_sheets_owner(), nullptr);
  EXPECT_EQ(arena.texture_command_queue_size(), 0);
}

TEST_F(GraphicsSessionActivationTest, QueuedUploadsKeepBothSessionsAlive) {
  class RecordingRenderer : public gfx::NullRenderer {
   public:
    gfx::TextureHandle CreateTexture(int, int) override {
      return reinterpret_cast<gfx::TextureHandle>(++next);
    }
    void UpdateTexture(gfx::TextureHandle texture,
                       const gfx::Bitmap& bitmap) override {
      uploads[texture] = bitmap.vector()[0];
    }
    void DestroyTexture(gfx::TextureHandle texture) override {
      destroyed.push_back(texture);
    }
    uintptr_t next = 0;
    std::map<gfx::TextureHandle, uint8_t> uploads;
    std::vector<gfx::TextureHandle> destroyed;
  } recorder;
  auto& arena = gfx::Arena::Get();
  // Limit this renderer trace to our two explicitly requested sheets.
  arena.ClearTextureQueue();
  ASSERT_OK(GraphicsEditorSaveStoplossTestPeer::PaintPixel(graphics[0], 3));
  auto* sheet_a = arena.mutable_gfx_sheet(0);
  Switch(1);
  ASSERT_OK(GraphicsEditorSaveStoplossTestPeer::PaintPixel(graphics[1], 5));
  auto* sheet_b = arena.mutable_gfx_sheet(0);
  Switch(0);
  arena.ProcessTextureQueue(&recorder);
  const auto texture_a = sheet_a->texture();
  const auto texture_b = sheet_b->texture();
  EXPECT_NE(texture_a, nullptr);
  EXPECT_NE(texture_b, nullptr);
  EXPECT_NE(texture_a, texture_b);
  EXPECT_EQ(recorder.uploads[texture_a], 3);
  EXPECT_EQ(recorder.uploads[texture_b], 5);
  EXPECT_TRUE(recorder.destroyed.empty());
  Switch(1);
  EXPECT_EQ(arena.mutable_gfx_sheet(0)->texture(), texture_b);
  Switch(0);
  EXPECT_EQ(arena.mutable_gfx_sheet(0)->texture(), texture_a);
  manager->session_coordinator()->CloseSessionAllowingEmpty(1);
  // Texture retirement is deferred until the frame has finished drawing.
  EXPECT_TRUE(recorder.destroyed.empty());
  arena.DrainRetiredBitmaps(&recorder);
  EXPECT_EQ(recorder.destroyed, (std::vector<gfx::TextureHandle>{texture_b}));
  manager->session_coordinator()->CloseSessionAllowingEmpty(0);
  arena.DrainRetiredBitmaps(&recorder);
  EXPECT_EQ(recorder.destroyed,
            (std::vector<gfx::TextureHandle>{texture_b, texture_a}));
  EXPECT_EQ(arena.gfx_sheets_owner(), nullptr);
}

// Exercise the registered workspace and real ImGui inputs on disposable ROMs.
class PaletteWorkspaceTest : public SessionRomIdentityTest {
 protected:
  void SetUp() override {
    SessionRomIdentityTest::SetUp();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1440, 1000);
    io.DeltaTime = 1.0f / 60;
    io.ConfigInputTrickleEventQueue = false;
    ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
    palette = manager->GetCurrentEditorSet()->GetEditorAs<PaletteEditor>(
        EditorType::kPalette);
    ASSERT_NE(palette, nullptr);
    workspace =
        manager->window_manager().GetWindowContent(0, "palette.control_panel");
    ASSERT_NE(workspace, nullptr);
    // Ordinary navigation should not open comparison windows.
    for (const char* id :
         {"palette.ow_main", "palette.ow_animated", "palette.dungeon_main"})
      manager->window_manager().CloseWindow(0, id);
    Frame();
    Frame();
  }
  void TearDown() override {
    gui::WidgetIdRegistry::Instance().Clear();
    SessionRomIdentityTest::TearDown();
  }
  void Frame(float width = 1000) {
    gui::WidgetIdRegistry::Instance().Clear();
    ImGui::NewFrame();
    if (visit_sessions_)
      manager->session_coordinator()->UpdateSessions();
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, 820), ImGuiCond_Always);
    ImGui::Begin("PaletteWorkspaceTestHost", nullptr,
                 ImGuiWindowFlags_NoSavedSettings);
    workspace->Draw(nullptr);
    ImGui::End();
    ImGui::Render();
  }
  std::optional<gui::WidgetIdRegistry::WidgetInfo> Widget(const char* name) {
    for (const auto& [path, item] :
         gui::WidgetIdRegistry::Instance().GetAllWidgets())
      if (item.label == gui::WidgetIdRegistry::NormalizeLabel(name))
        return item;
    return std::nullopt;
  }
  void Click(const char* name, float width = 1000) {
    const auto item = Widget(name);
    ASSERT_TRUE(item) << name;
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent((item->bounds.min_x + item->bounds.max_x) / 2,
                        (item->bounds.min_y + item->bounds.max_y) / 2);
    Frame(width);
    io.AddMouseButtonEvent(0, true);
    Frame(width);
    io.AddMouseButtonEvent(0, false);
    Frame(width);
    Frame(width);
  }
  void EnterColor(const char* value) {
    Click("SNES color");
    auto& io = ImGui::GetIO();
    // AddKeyEvent takes physical modifiers; ImGui swaps Command internally.
    const ImGuiKey command =
        io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;
    io.AddKeyEvent(command, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    Frame();
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(command, false);
    Frame();
    io.AddInputCharactersUTF8(value);
    Frame();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    Frame();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    Frame();
    Frame();
  }
  // Hit the rendered hue wheel, then move it across multiple actual frames.
  void MoveOnWheel(float angle) {
    const auto item = Widget("Palette color picker");
    ASSERT_TRUE(item);
    const float width = ImGui::GetFontSize() * 19;
    const float bar = ImGui::GetFrameHeight();
    const float size = width - bar - ImGui::GetStyle().ItemInnerSpacing.x;
    const ImVec2 center(item->bounds.min_x + (size + bar) * 0.5f,
                        item->bounds.min_y + size * 0.5f);
    ImGui::GetIO().AddMousePosEvent(center.x + size * 0.46f * std::cos(angle),
                                    center.y + size * 0.46f * std::sin(angle));
    Frame();
  }
  void StartWheelDrag() {
    MoveOnWheel(0.5f);
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    Frame();
  }
  void ReleaseWheel() {
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    Frame();
    Frame();
  }
  std::optional<ImU32> SwatchColor(const char* name) {
    auto item = Widget(name);
    if (!item)
      return std::nullopt;
    auto* window = ImGui::FindWindowByName(item->window_name.c_str());
    if (!window)
      return std::nullopt;
    const ImVec2 point(item->bounds.min_x + 7, item->bounds.min_y + 7);
    const auto& draw = *window->DrawList;
    std::optional<ImU32> result;
    for (const auto& cmd : draw.CmdBuffer) {
      for (unsigned i = 0; i + 2 < cmd.ElemCount; i += 3) {
        const auto& a =
            draw.VtxBuffer[cmd.VtxOffset + draw.IdxBuffer[cmd.IdxOffset + i]];
        const auto& b = draw.VtxBuffer[cmd.VtxOffset +
                                       draw.IdxBuffer[cmd.IdxOffset + i + 1]];
        const auto& c = draw.VtxBuffer[cmd.VtxOffset +
                                       draw.IdxBuffer[cmd.IdxOffset + i + 2]];
        if (a.col == b.col && b.col == c.col &&
            ImTriangleContainsPoint(a.pos, b.pos, c.pos, point))
          result = a.col;
      }
    }
    return result;
  }
  bool visit_sessions_ = false;
  PaletteEditor* palette = nullptr;
  WindowContent* workspace = nullptr;
};

TEST_F(PaletteWorkspaceTest, NavigationFitsAndDoesNotMultiplyWindows) {
  const auto before = manager->GetCurrentRom()->vector();
  Click("Dungeon Main");
  EXPECT_FALSE(
      manager->window_manager().IsWindowOpen(0, "palette.dungeon_main"));
  auto* panel = dynamic_cast<DungeonMainPalettePanel*>(
      manager->window_manager().GetWindowContent(0, "palette.dungeon_main"));
  ASSERT_NE(panel, nullptr);
  for (float width : {720.f, 1000.f, 1280.f}) {
    Frame(width);
    Frame(width);
    Click("Palette color 14", width);
    EXPECT_EQ(panel->GetSelectedColorIndex(), 14);
    auto cell = Widget("Palette color 14");
    ASSERT_TRUE(cell);
    EXPECT_LT(cell->bounds.max_x, width + 10);
    for (auto* win : ImGui::GetCurrentContext()->Windows)
      if (win->Active && std::string(win->Name).find(
                             "PaletteWorkspaceTestHost") != std::string::npos)
        EXPECT_LE(win->ScrollMax.x, 0.f) << win->Name;
  }
  palette->JumpToPalette("dungeon_main", 2);
  Frame();
  Frame();
  EXPECT_EQ(panel->GetSelectedPaletteIndex(), 2);
  EXPECT_EQ(panel->GetSelectedColorIndex(), -1);
  EXPECT_FALSE(
      manager->window_manager().IsWindowOpen(0, "palette.dungeon_main"));
  Click("Open comparison window");
  EXPECT_TRUE(
      manager->window_manager().IsWindowOpen(0, "palette.dungeon_main"));
  EXPECT_EQ(manager->GetCurrentRom()->vector(), before);
  const auto preset = LayoutPresets::GetDefaultPreset(EditorType::kPalette);
  EXPECT_EQ(preset.default_visible_panels,
            (std::vector<std::string>{"palette.control_panel"}));
}

TEST_F(PaletteWorkspaceTest, ColorInputUndoAndOriginalUseTheSameModel) {
  Click("Dungeon Main");
  Click("Palette color 3");
  auto& pm = gfx::PaletteManager::Get();
  const auto original = pm.GetColor("dungeon_main", 0, 3);
  const auto bytes = manager->GetCurrentRom()->vector();
  EnterColor("001F");
  ASSERT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x001F);
  EXPECT_EQ(pm.GetOriginalColor("dungeon_main", 0, 3).snes(), original.snes());
  EXPECT_EQ(SwatchColor("Current palette color"),
            ImGui::GetColorU32(
                gui::ConvertSnesColorToImVec4(gfx::SnesColor(0x001F))));
  Click("Undo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), original.snes());
  EXPECT_EQ(SwatchColor("Current palette color"),
            ImGui::GetColorU32(gui::ConvertSnesColorToImVec4(original)));
  Click("Redo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x001F);
  Click("Restore original palette color");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), original.snes());
  EXPECT_EQ(manager->GetCurrentRom()->vector(), bytes);
  EnterColor("FFFF");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), original.snes());
  EXPECT_EQ(manager->GetCurrentRom()->vector(), bytes);
}

TEST_F(PaletteWorkspaceTest, WheelDragCommitsOneUndoAndKeepsLiveColors) {
  Click("Dungeon Main");
  Click("Palette color 3");
  EnterColor("001F");
  auto& pm = gfx::PaletteManager::Get();
  pm.ClearHistory();
  const auto bytes = manager->GetCurrentRom()->vector();
  const auto start = pm.GetColor("dungeon_main", 0, 3).snes();
  const auto revision = pm.ColorRevision(&Session()->game_data);
  StartWheelDrag();
  ASSERT_TRUE(pm.HasColorEdit());
  // Render the normal picker in those same frames, retaining ImGui's active ID.
  visit_sessions_ = true;
  Frame();
  EXPECT_TRUE(pm.HasColorEdit());
  EXPECT_EQ(pm.GetUndoStackSize(), 0);
  for (float angle : {1.f, 1.5f, 2.f}) {
    MoveOnWheel(angle);
    EXPECT_EQ(pm.GetUndoStackSize(), 0);
    EXPECT_EQ(SwatchColor("Current palette color"),
              ImGui::GetColorU32(gui::ConvertSnesColorToImVec4(
                  pm.GetColor("dungeon_main", 0, 3))));
  }
  const auto end = pm.GetColor("dungeon_main", 0, 3).snes();
  ASSERT_NE(end, start);
  EXPECT_GT(pm.ColorRevision(&Session()->game_data), revision + 1);
  ReleaseWheel();
  EXPECT_FALSE(pm.HasColorEdit());
  EXPECT_EQ(pm.GetUndoStackSize(), 1);
  Click("Undo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), start);
  EXPECT_EQ(pm.GetUndoStackSize(), 0);
  Click("Redo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), end);
  StartWheelDrag();
  MoveOnWheel(4.f);
  ReleaseWheel();
  EXPECT_EQ(pm.GetUndoStackSize(), 2);
  Click("Undo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), end);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), bytes);
}

TEST_F(PaletteWorkspaceTest,
       EscapeCancelsHeldDragAndPreservesRedoAndDirtyState) {
  Click("Dungeon Main");
  Click("Palette color 3");
  EnterColor("001F");
  EnterColor("03E0");
  Click("Undo palette edit");
  auto& pm = gfx::PaletteManager::Get();
  const auto bytes = manager->GetCurrentRom()->vector();
  const auto undo = pm.GetUndoStackSize();
  const auto redo = pm.GetRedoStackSize();
  const auto dirty = pm.GetModifiedColorCount();
  ASSERT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x001F);
  StartWheelDrag();
  MoveOnWheel(2.f);
  ASSERT_NE(pm.GetColor("dungeon_main", 0, 3).snes(), 0x001F);
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
  Frame();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
  MoveOnWheel(4.f);  // Still held: must not start a replacement gesture.
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x001F);
  EXPECT_FALSE(pm.HasColorEdit());
  ReleaseWheel();
  EXPECT_EQ(pm.GetUndoStackSize(), undo);
  EXPECT_EQ(pm.GetRedoStackSize(), redo);
  EXPECT_EQ(pm.GetModifiedColorCount(), dirty);
  Click("Redo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x03E0);
  // Cancellation must not leave the picker disabled on its next use.
  StartWheelDrag();
  MoveOnWheel(4.f);
  ReleaseWheel();
  EXPECT_EQ(pm.GetUndoStackSize(), undo + 2);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), bytes);
}

TEST_F(PaletteWorkspaceTest,
       UnchangedGesturePreservesRedoAndCancelRestoresClean) {
  auto& pm = gfx::PaletteManager::Get();
  const auto original = pm.GetColor("dungeon_main", 0, 3);
  ASSERT_OK(pm.SetColor("dungeon_main", 0, 3, gfx::SnesColor(0x1234)));
  pm.Undo();
  ASSERT_FALSE(pm.HasUnsavedChanges());
  ASSERT_TRUE(pm.CanRedo());
  ASSERT_OK(pm.BeginColorEdit("dungeon_main", 0, 3));
  ASSERT_OK(pm.SetColor("dungeon_main", 0, 3, gfx::SnesColor(0x03E0)));
  ASSERT_OK(pm.SetColor("dungeon_main", 0, 3, original));
  pm.CommitColorEdit();
  EXPECT_FALSE(pm.HasUnsavedChanges());
  EXPECT_EQ(pm.GetUndoStackSize(), 0);
  EXPECT_EQ(pm.GetRedoStackSize(), 1);
  ASSERT_OK(pm.BeginColorEdit("dungeon_main", 0, 3, 17));
  ASSERT_OK(pm.SetColor("dungeon_main", 0, 3, gfx::SnesColor(0x001F)));
  pm.CommitColorEdit(18);
  pm.CancelColorEdit(18);
  EXPECT_TRUE(pm.HasColorEdit(17));
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x001F);
  pm.CancelColorEdit(17);
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), original.snes());
  EXPECT_FALSE(pm.HasUnsavedChanges());
  EXPECT_EQ(pm.GetUndoStackSize(), 0);
  EXPECT_EQ(pm.GetRedoStackSize(), 1);
}

TEST_F(PaletteWorkspaceTest, SessionSwitchCommitsOnlyTheOwningGesture) {
  Click("Dungeon Main");
  Click("Palette color 3");
  EnterColor("001F");
  auto& pm = gfx::PaletteManager::Get();
  pm.ClearHistory();
  StartWheelDrag();
  MoveOnWheel(2.f);
  const auto end = pm.GetColor("dungeon_main", 0, 3).snes();
  Switch(1);
  ASSERT_OK(manager->EnsureEditorAssetsLoaded(EditorType::kPalette));
  EXPECT_FALSE(pm.HasColorEdit());
  EXPECT_FALSE(pm.CanUndo());
  pm.CancelColorEdit();
  Switch(0);
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), end);
  EXPECT_EQ(pm.GetUndoStackSize(), 1);
  ReleaseWheel();
  pm.Undo();
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x001F);
}

TEST_F(PaletteWorkspaceTest, SavingDuringGestureCommitsOneStepAndReopensColor) {
  Click("Dungeon Main");
  Click("Palette color 3");
  EnterColor("001F");
  auto& pm = gfx::PaletteManager::Get();
  pm.ClearHistory();
  StartWheelDrag();
  MoveOnWheel(2.f);
  const auto end = pm.GetColor("dungeon_main", 0, 3).snes();
  ASSERT_TRUE(pm.HasColorEdit());
  core::FeatureFlags::get().kSaveAllPalettes = true;
  manager->GetCurrentProject()->feature_flags = core::FeatureFlags::get();
  const auto destination = root / "gesture-save.sfc";
  ASSERT_OK(manager->SaveRomAs(destination.string()));
  EXPECT_FALSE(pm.HasColorEdit());
  EXPECT_EQ(pm.GetUndoStackSize(), 1);
  EXPECT_FALSE(pm.HasUnsavedChanges());
  ReleaseWheel();
  EXPECT_EQ(pm.GetUndoStackSize(), 1);
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(destination.string()));
  gfx::PaletteGroupMap groups;
  ASSERT_OK(gfx::LoadAllPalettes(reopened.vector(), groups));
  EXPECT_EQ(groups.dungeon_main.palette_ref(0)[3].snes(), end);
  Click("Undo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x001F);
  EXPECT_TRUE(pm.HasUnsavedChanges());
}

TEST_F(PaletteWorkspaceTest,
       SaveButtonCommitsAndFreshPaletteLoaderReadsTheEdit) {
  Click("Dungeon Main");
  Click("Palette color 3");
  EnterColor("1234");
  auto& pm = gfx::PaletteManager::Get();
  ASSERT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x1234);
  auto before = manager->GetCurrentRom()->vector();
  const auto ranges = pm.GetModifiedColorWriteRanges();
  ASSERT_TRUE(ranges.ok()) << ranges.status();
  ASSERT_EQ(ranges->size(), 1);
  ASSERT_EQ((*ranges)[0].second - (*ranges)[0].first, 2);
  before[(*ranges)[0].first] = 0x34;
  before[(*ranges)[0].first + 1] = 0x12;
  core::FeatureFlags::get().kSaveAllPalettes = true;
  manager->GetCurrentProject()->feature_flags = core::FeatureFlags::get();
  const auto save_toast_begin = manager->toast_manager()->pending_count();
  Click("Save ROM");
  for (size_t i = save_toast_begin;
       i < manager->toast_manager()->pending_count(); ++i) {
    const auto* toast = manager->toast_manager()->GetPendingToast(i);
    EXPECT_NE(toast->type, ToastType::kError) << toast->message;
  }
  ASSERT_FALSE(pm.HasUnsavedChanges());
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(a.string()));
  EXPECT_EQ(reopened.vector(), before);
  gfx::PaletteGroupMap groups;
  ASSERT_OK(gfx::LoadAllPalettes(reopened.vector(), groups));
  EXPECT_EQ(groups.dungeon_main.palette_ref(0)[3].snes(), 0x1234);
  EXPECT_FALSE(pm.HasUnsavedChanges());
  EXPECT_FALSE(manager->GetCurrentRom()->dirty());
}

TEST_F(PaletteWorkspaceTest, DiscardConfirmationCancelPreservesPendingEdit) {
  Click("Dungeon Main");
  Click("Palette color 3");
  EnterColor("1234");
  Click("Palette tools");
  Click("Discard palette changes");
  ASSERT_TRUE(Widget("Cancel discard"));
  Click("Cancel discard");
  EXPECT_EQ(gfx::PaletteManager::Get().GetColor("dungeon_main", 0, 3).snes(),
            0x1234);
  EXPECT_TRUE(palette->CanUndo());
}

TEST_F(PaletteWorkspaceTest, DisabledSavePreservesColorAndDisk) {
  Click("Dungeon Main");
  Click("Palette color 3");
  EnterColor("1234");
  const auto disk_hash = util::ComputeFileSha1Hex(a.string());
  const auto bytes = manager->GetCurrentRom()->vector();
  Click("Save ROM");
  bool refusal_shown = false;
  for (size_t i = 0; i < manager->toast_manager()->pending_count(); ++i) {
    const auto* toast = manager->toast_manager()->GetPendingToast(i);
    refusal_shown |=
        toast->type == ToastType::kError &&
        toast->message.find("palette saving is disabled") != std::string::npos;
  }
  EXPECT_TRUE(refusal_shown);
  EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), disk_hash);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), bytes);
  EXPECT_TRUE(gfx::PaletteManager::Get().HasUnsavedChanges());
  EXPECT_TRUE(palette->CanUndo());
}

TEST_F(PaletteWorkspaceTest, FailedFileWriteRestoresBaselineAndAllowsRetry) {
  auto* dungeon_before =
      manager->GetCurrentEditorSet()->GetExistingEditor(EditorType::kDungeon);
  Click("Dungeon Main");
  Click("Palette color 3");
  auto& pm = gfx::PaletteManager::Get();
  const auto original = pm.GetOriginalColor("dungeon_main", 0, 3).snes();
  EnterColor("1234");
  const auto disk_hash = util::ComputeFileSha1Hex(a.string());
  const auto bytes = manager->GetCurrentRom()->vector();
  const auto baseline = *Session()->rom_baseline;
  // Force the new Palette transaction, not Dungeon's existing palette path.
  core::FeatureFlags::get().kSaveAllPalettes = true;
  manager->GetCurrentProject()->feature_flags = core::FeatureFlags::get();
  const auto parent_file = root / "not-a-directory";
  std::ofstream(parent_file) << "PRESERVE";
  const auto status = manager->SaveRomAs((parent_file / "fail.sfc").string());
  EXPECT_NE(
      std::string(status.message()).find("Could not create temp ROM file"),
      std::string::npos)
      << status;
  EXPECT_EQ(util::ComputeFileSha1Hex(a.string()), disk_hash);
  EXPECT_EQ(manager->GetCurrentRom()->vector(), bytes);
  ExpectBaseline(baseline);
  EXPECT_TRUE(pm.HasUnsavedChanges());
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x1234);
  EXPECT_EQ(pm.GetOriginalColor("dungeon_main", 0, 3).snes(), original);
  Click("Undo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), original);
  Click("Redo palette edit");
  EXPECT_EQ(pm.GetColor("dungeon_main", 0, 3).snes(), 0x1234);
  Click("Save ROM");
  EXPECT_FALSE(pm.HasUnsavedChanges());
  EXPECT_EQ(
      manager->GetCurrentEditorSet()->GetExistingEditor(EditorType::kDungeon),
      dungeon_before);
  Rom reopened;
  ASSERT_OK(reopened.LoadFromFile(a.string()));
  gfx::PaletteGroupMap groups;
  ASSERT_OK(gfx::LoadAllPalettes(reopened.vector(), groups));
  EXPECT_EQ(groups.dungeon_main.palette_ref(0)[3].snes(), 0x1234);
}

}  // namespace
}  // namespace yaze::editor
