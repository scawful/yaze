#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/catalog_tile_browser.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_pixel_command.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/util/palette_manager.h"
#include "editor_test_support.h"
#include "imgui.h"
#include "oracle_rom_fixture.h"
#include "testing.h"

namespace yaze::editor {

class DungeonPaletteSourceTestPeer {
 public:
  static absl::Status LoadRoom(DungeonEditorV2& editor, int room_id) {
    return editor.room_loader_.LoadRoom(room_id, editor.rooms_[room_id]);
  }
  static void PrepareRoom(DungeonEditorV2& editor, int room_id) {
    editor.current_room_id_ = room_id;
    editor.ApplyEntranceRenderContext(room_id);
    editor.rooms_[room_id].PrepareForRender();
  }
  static void OpenGraphics(DungeonEditorV2& editor, int room_id,
                           const zelda3::RoomObject& object) {
    editor.OpenGraphicsEditorForObject(room_id, object);
  }
  static void ChangeEntrance(DungeonEditorV2& editor, int room_id,
                             uint8_t blockset) {
    const int slot = zelda3::kNumDungeonSpawnPoints;
    editor.entrances_[slot].room_ = room_id;
    editor.entrances_[slot].blockset_ = blockset;
    editor.SetGraphicsPreviewEntrance(slot);
  }
  static GraphicsEditorState& GraphicsState(GraphicsEditor& editor) {
    return editor.state_;
  }
  static absl::Status ApplyCatalogArt(GraphicsEditor& editor,
                                      CatalogTileBrowserState& browser) {
    return ApplyCatalogArtToObject(browser, editor.state_,
                                   editor.undo_manager_);
  }
};

namespace {

class DungeonPaletteSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gfx::Arena::Get().ClearTextureQueue();
    const auto fixture = test::FindOracleRomFixture();
    if (fixture.empty())
      GTEST_SKIP() << "Oracle ROM fixture is unavailable";
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 720);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* font_pixels = nullptr;
    int font_width = 0;
    int font_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
    root_ = test::UniqueTempPath("dungeon-palette-source");
    std::filesystem::create_directories(root_);
    const auto rom_copy = root_ / "source.sfc";
    std::filesystem::copy_file(fixture, rom_copy);
    manager_ = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, &renderer_);
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    ASSERT_OK(manager_->OpenRomOrProject(rom_copy.string()));
    ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kDungeon));
    ASSERT_OK(manager_->EnsureEditorAssetsLoaded(EditorType::kGraphics));
    auto* editors = manager_->GetCurrentEditorSet();
    ASSERT_NE(editors, nullptr);
    dungeon_ = editors->GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
    graphics_ = editors->GetGraphicsEditor();
    ASSERT_NE(dungeon_, nullptr);
    ASSERT_NE(graphics_, nullptr);
    for (int room_id : {0x89, 5}) {
      ASSERT_OK(DungeonPaletteSourceTestPeer::LoadRoom(*dungeon_, room_id));
    }
  }

  void TearDown() override {
    manager_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    if (context_) {
      ImGui::DestroyContext(context_);
      ImGui::SetCurrentContext(previous_context_);
    }
    if (!root_.empty()) {
      std::error_code ec;
      std::filesystem::remove_all(root_, ec);
    }
  }

  struct SourceChoice {
    int room_id;
    size_t index;
    PalettePermutation permutation;
  };

  std::optional<SourceChoice> ChooseSource() {
    const auto* owner = manager_->GetCurrentGameData();
    for (int room_id : {0x89, 5}) {
      DungeonPaletteSourceTestPeer::PrepareRoom(*dungeon_, room_id);
      auto& room = dungeon_->rooms()[room_id];
      const auto& objects = room.GetTileObjects();
      for (size_t i = 0; i < objects.size(); ++i) {
        const auto identity =
            std::make_tuple(objects[i].id_, objects[i].x_, objects[i].y_,
                            objects[i].size_, objects[i].GetLayerValue());
        const auto matches = std::count_if(
            objects.begin(), objects.end(), [&](const auto& candidate) {
              return std::make_tuple(candidate.id_, candidate.x_, candidate.y_,
                                     candidate.size_,
                                     candidate.GetLayerValue()) == identity;
            });
        if (matches != 1)
          continue;
        auto context = MakeDungeonGraphicsEditContext(room, objects[i], owner);
        if (!context.ok() || context->targets.empty() ||
            context->pattern_width <= 0 || context->pattern_height <= 0 ||
            context->pattern_cells.empty())
          continue;
        const auto& target = context->targets.front();
        if (!ValidateGraphicsPaletteAddressing(target, owner).ok())
          continue;
        for (uint8_t a = 1; a < 8; ++a) {
          for (uint8_t b = a + 1; b < 8; ++b) {
            if (target.palette[a].snes() == target.palette[b].snes())
              continue;
            PalettePermutation permutation{0, 1, 2, 3, 4, 5, 6, 7};
            std::swap(permutation[a], permutation[b]);
            auto plan = PlanPalettePixelPermutation(*context, {0}, permutation);
            if (plan.ok() && !plan->colors.empty() && !plan->sheets.empty()) {
              DungeonPaletteSourceTestPeer::OpenGraphics(*dungeon_, room_id,
                                                         objects[i]);
              auto& state =
                  DungeonPaletteSourceTestPeer::GraphicsState(*graphics_);
              if (!state.source_context)
                continue;
              CatalogTileBrowserState browser;
              PopulateObjectArt(browser, *state.source_context, permutation);
              auto asset =
                  AssembleCatalogAsset(browser.sources, "host.selected-object");
              if (!asset.ok())
                continue;
              const auto art_plan = PlanCatalogObjectTransfer(
                  state, *asset, {0, 0, asset->width, asset->height});
              if (art_plan.ok() && !art_plan->sheets.empty())
                return SourceChoice{room_id, i, permutation};
            }
          }
        }
      }
    }
    return std::nullopt;
  }

  std::vector<uint16_t> Appearance(int room_id, size_t index) {
    auto& room = dungeon_->rooms()[room_id];
    room.MarkGraphicsDirty();
    room.PrepareForRender();
    auto context = MakeDungeonGraphicsEditContext(
        room, room.GetTileObjects()[index], manager_->GetCurrentGameData());
    EXPECT_TRUE(context.ok()) << context.status();
    if (!context.ok() || context->targets.empty())
      return {};
    const auto& target = context->targets.front();
    const auto pixels =
        manager_->GetCurrentGameData()->sheet_store.Sheet(target.sheet_id);
    const int start =
        (target.tile_index / 16) * 8 * 128 + (target.tile_index % 16) * 8;
    std::vector<uint16_t> colors;
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x)
        colors.push_back(target.palette[pixels[start + y * 128 + x]].snes());
    return colors;
  }

  std::vector<uint16_t> BoundColors(const GraphicsEditTarget& target) {
    if (!target.writable_palette)
      return {};
    std::vector<uint16_t> colors;
    const auto* owner = manager_->GetCurrentGameData();
    for (int raw = 1; raw < 8; ++raw) {
      auto color = zelda3::ReadPaletteColor(
          *owner, *target.writable_palette->colors[raw]);
      EXPECT_TRUE(color.ok()) << color.status();
      if (color.ok())
        colors.push_back(color->snes());
    }
    return colors;
  }

  void PopulateObjectArt(CatalogTileBrowserState& browser,
                         const GraphicsEditContext& source,
                         const PalettePermutation& permutation) {
    ASSERT_GT(source.pattern_width, 0);
    ASSERT_GT(source.pattern_height, 0);
    ASSERT_FALSE(source.pattern_cells.empty());
    std::map<uint16_t, int> pages;
    std::vector<CatalogAssemblyCell> assembly;
    for (const auto& cell : source.pattern_cells) {
      ASSERT_LT(cell.target, source.targets.size());
      const auto& target = source.targets[cell.target];
      const auto [entry, inserted] =
          pages.emplace(target.sheet_id, static_cast<int>(pages.size()));
      assembly.push_back({cell.x, cell.y, entry->second, target.tile_index,
                          cell.hflip, cell.vflip});
    }
    browser.sources.resize(pages.size());
    for (const auto& [sheet, page] : pages) {
      auto& item = browser.sources[page];
      item.id = "host.selected-object.page-" + std::to_string(page);
      item.asset_id = "host.selected-object";
      item.title = "Placed object artwork";
      item.page = page;
      item.page_count = static_cast<int>(pages.size());
      item.pixel_width = source.pattern_width;
      item.pixel_height = source.pattern_height;
      item.assembly = assembly;
      const auto pixels =
          manager_->GetCurrentGameData()->sheet_store.Sheet(sheet);
      ASSERT_EQ(pixels.size(), 4096u);
      for (const auto pixel : pixels) {
        ASSERT_LT(pixel, permutation.size());
        item.indexed_pixels.push_back(permutation[pixel]);
      }
    }
    browser.source = 0;
  }

  std::filesystem::path root_;
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  gfx::NullRenderer renderer_;
  std::unique_ptr<EditorManager> manager_;
  DungeonEditorV2* dungeon_ = nullptr;
  GraphicsEditor* graphics_ = nullptr;
};

TEST_F(DungeonPaletteSourceTest,
       PlacedObjectCommandUsesLiveRoomAndAllowsSameSourceUndo) {
  const auto choice = ChooseSource();
  ASSERT_TRUE(choice)
      << "No cave/room-5 object has an addressable palette and used pixels";
  auto& room = dungeon_->rooms()[choice->room_id];
  const auto selected = room.GetTileObjects()[choice->index];
  DungeonPaletteSourceTestPeer::OpenGraphics(*dungeon_, choice->room_id,
                                             selected);
  auto context = MakeDungeonGraphicsEditContext(room, selected,
                                                manager_->GetCurrentGameData());
  ASSERT_OK(context.status());
  const auto target = context->targets.front();
  const auto sheet = target.sheet_id;
  const auto before = manager_->GetCurrentGameData()->sheet_store.Sheet(sheet);
  const std::vector<uint8_t> original(before.begin(), before.end());
  const auto colors_before = BoundColors(target);
  const auto appearance_before = Appearance(choice->room_id, choice->index);
  const auto undo_depth = graphics_->undo_manager().UndoStackSize();

  ASSERT_OK(graphics_->ReorderSourcePalette({0}, choice->permutation));
  EXPECT_EQ(graphics_->undo_manager().UndoStackSize(), undo_depth + 1);
  const auto changed = manager_->GetCurrentGameData()->sheet_store.Sheet(sheet);
  const std::vector<uint8_t> reordered(changed.begin(), changed.end());
  EXPECT_NE(reordered, original);
  EXPECT_NE(BoundColors(target), colors_before);
  EXPECT_EQ(Appearance(choice->room_id, choice->index), appearance_before);

  ASSERT_OK(graphics_->Undo());
  const auto undone = manager_->GetCurrentGameData()->sheet_store.Sheet(sheet);
  EXPECT_EQ(std::vector<uint8_t>(undone.begin(), undone.end()), original);
  EXPECT_EQ(BoundColors(target), colors_before);
  ASSERT_OK(graphics_->Redo());
  const auto redone = manager_->GetCurrentGameData()->sheet_store.Sheet(sheet);
  EXPECT_EQ(std::vector<uint8_t>(redone.begin(), redone.end()), reordered);
  EXPECT_EQ(Appearance(choice->room_id, choice->index), appearance_before);
}

TEST_F(DungeonPaletteSourceTest,
       PlacedObjectTryValidateApplyUsesDetachedPreviewAndNormalHistory) {
  const auto choice = ChooseSource();
  ASSERT_TRUE(choice) << "No addressable placed object has editable pixels";
  auto& room = dungeon_->rooms()[choice->room_id];
  const auto selected = room.GetTileObjects()[choice->index];
  DungeonPaletteSourceTestPeer::OpenGraphics(*dungeon_, choice->room_id,
                                             selected);
  auto& state = DungeonPaletteSourceTestPeer::GraphicsState(*graphics_);
  ASSERT_TRUE(state.source_context);
  const auto source = *state.source_context;
  ASSERT_TRUE(source.render_preview);
  ASSERT_TRUE(source.render_staged_preview);
  ASSERT_TRUE(source.preview_revision);
  ASSERT_GT(source.pattern_width, 0);
  ASSERT_GT(source.pattern_height, 0);
  ASSERT_FALSE(source.pattern_cells.empty());
  for (const auto& cell : source.pattern_cells) {
    EXPECT_GE(cell.x, 0);
    EXPECT_GE(cell.y, 0);
    EXPECT_LT(cell.x * 8, source.pattern_width);
    EXPECT_LT(cell.y * 8, source.pattern_height);
    EXPECT_LT(cell.target, source.targets.size());
  }

  const auto original_preview = source.render_preview();
  ASSERT_OK(original_preview.status());
  ASSERT_FALSE(original_preview->empty());
  const auto authored_before = CaptureDungeonRoomDocument(room);
  const auto pixels_before = manager_->GetCurrentGameData()->graphics_buffer;
  const auto colors_before = BoundColors(source.targets.front());
  const auto* rom = manager_->GetCurrentRom();
  const auto rom_before = rom->vector();
  const auto undo_before = graphics_->undo_manager().UndoStackSize();
  std::map<uint16_t, uint64_t> revisions;
  for (const auto& target : source.targets)
    revisions[target.sheet_id] =
        state.game_data->sheet_store.Revision(target.sheet_id);

  CatalogTileBrowserState browser;
  PopulateObjectArt(browser, source, choice->permutation);
  ASSERT_FALSE(browser.sources.empty());
  ASSERT_OK(TryCatalogArtOnObject(browser, state));
  ASSERT_TRUE(browser.object_pending);
  ASSERT_FALSE(browser.object_pending->sheets.empty());
  auto expected = browser.object_pending->before_sheets;
  for (const auto& diff : browser.object_pending->sheets)
    for (size_t i = 0; i < diff.offsets.size(); ++i)
      expected.at(diff.sheet)[diff.offsets[i]] = diff.after[i];

  EXPECT_EQ(state.game_data->graphics_buffer, pixels_before);
  EXPECT_EQ(graphics_->undo_manager().UndoStackSize(), undo_before);
  EXPECT_EQ(BoundColors(source.targets.front()), colors_before);
  EXPECT_TRUE(SameDungeonRoomDocument(authored_before,
                                      CaptureDungeonRoomDocument(room)));
  EXPECT_EQ(rom->vector(), rom_before);
  for (const auto& [sheet, revision] : revisions)
    EXPECT_EQ(state.game_data->sheet_store.Revision(sheet), revision);

  // A normal Graphics frame and source refresh must not make a detached
  // candidate stale merely by observing the same live object again.
  ImGui::NewFrame();
  EXPECT_OK(graphics_->Update());
  ImGui::Render();
  const auto observed = source.render_preview();
  ASSERT_OK(observed.status());
  EXPECT_EQ(observed->ToRgba(), original_preview->ToRgba());
  ASSERT_OK(ValidateCatalogArtPreview(browser, state));
  ASSERT_TRUE(browser.object_pending);
  ASSERT_OK(DungeonPaletteSourceTestPeer::ApplyCatalogArt(*graphics_, browser));
  EXPECT_FALSE(browser.object_pending);
  EXPECT_EQ(graphics_->undo_manager().UndoStackSize(), undo_before + 1);
  for (const auto& [sheet, pixels] : expected) {
    const auto current = state.game_data->sheet_store.Sheet(sheet);
    EXPECT_EQ(std::vector<uint8_t>(current.begin(), current.end()), pixels);
  }
  EXPECT_EQ(BoundColors(source.targets.front()), colors_before);
  EXPECT_TRUE(SameDungeonRoomDocument(authored_before,
                                      CaptureDungeonRoomDocument(room)));
  ASSERT_OK(graphics_->Undo());
  EXPECT_EQ(state.game_data->graphics_buffer, pixels_before);
  ASSERT_OK(graphics_->Redo());
  for (const auto& [sheet, pixels] : expected) {
    const auto current = state.game_data->sheet_store.Sheet(sheet);
    EXPECT_EQ(std::vector<uint8_t>(current.begin(), current.end()), pixels);
  }
}

TEST_F(DungeonPaletteSourceTest,
       PlacedObjectCandidateRejectsRoomPaletteAndUncommittedSheetChanges) {
  const auto choice = ChooseSource();
  ASSERT_TRUE(choice) << "No placed object supports a changed art plan";
  auto& room = dungeon_->rooms()[choice->room_id];
  const auto selected = room.GetTileObjects()[choice->index];
  DungeonPaletteSourceTestPeer::OpenGraphics(*dungeon_, choice->room_id,
                                             selected);
  auto& state = DungeonPaletteSourceTestPeer::GraphicsState(*graphics_);
  ASSERT_TRUE(state.source_context);
  const auto source = *state.source_context;
  CatalogTileBrowserState browser;
  PopulateObjectArt(browser, source, choice->permutation);
  ASSERT_FALSE(browser.sources.empty());
  const auto history_before = graphics_->undo_manager().UndoStackSize();

  // The set of loaded sheet bytes is unchanged by a slot permutation, but
  // their ordered use still changes the rendered room.
  const auto ordered_blocks = room.blocks();
  const auto different = std::find_if(
      ordered_blocks.begin() + 1, ordered_blocks.end(),
      [&](uint8_t sheet) { return sheet != ordered_blocks.front(); });
  ASSERT_NE(different, ordered_blocks.end());
  const auto before_slot_swap = state.source_context->preview_revision();
  ASSERT_OK(before_slot_swap.status());
  const auto slot = std::distance(ordered_blocks.begin(), different);
  std::swap(room.mutable_blocks()[0], room.mutable_blocks()[slot]);
  const auto after_slot_swap = state.source_context->preview_revision();
  ASSERT_OK(after_slot_swap.status());
  EXPECT_NE(*before_slot_swap, *after_slot_swap);
  room.mutable_blocks() = ordered_blocks;

  // An unrelated placement changes the room image even when the selected
  // object's source cells and every selected sheet remain the same.
  ASSERT_GT(room.GetTileObjects().size(), 1u);
  const auto other = choice->index == 0 ? 1u : 0u;
  ASSERT_OK(TryCatalogArtOnObject(browser, state));
  room.GetTileObjects()[other].x_ ^= 1;
  EXPECT_FALSE(ValidateCatalogArtPreview(browser, state).ok());
  EXPECT_FALSE(browser.object_pending);
  room.GetTileObjects()[other].x_ ^= 1;

  ASSERT_OK(TryCatalogArtOnObject(browser, state));
  auto& palettes = gfx::PaletteManager::Get();
  ASSERT_TRUE(palettes.IsManaging(state.game_data));
  const int bank = room.ResolveDungeonPaletteId();
  const auto color = palettes.GetColor("dungeon_main", bank, 0);
  ASSERT_OK(palettes.SetColor("dungeon_main", bank, 0,
                              gfx::SnesColor(color.snes() ^ 0x7fff)));
  EXPECT_FALSE(ValidateCatalogArtPreview(browser, state).ok());
  EXPECT_FALSE(browser.object_pending);
  ASSERT_OK(palettes.SetColor("dungeon_main", bank, 0, color));

  ASSERT_OK(TryCatalogArtOnObject(browser, state));
  const auto blocks = room.blocks();
  const auto unused =
      std::find_if(blocks.begin(), blocks.end(), [&](const auto sheet) {
        return state.game_data->sheet_store.HasSheet(sheet) &&
               !browser.object_pending->before_sheets.contains(sheet);
      });
  ASSERT_NE(unused, blocks.end())
      << "Fixture needs a room-used sheet outside the selected object's sheets";
  const auto sheet = *unused;
  const auto revision = state.game_data->sheet_store.Revision(sheet);
  {
    zelda3::GraphicsSheetStore::Edit stroke(state.game_data->sheet_store,
                                            sheet);
    ASSERT_TRUE(stroke.ok());
    const auto original = stroke.Get(0, 0);
    ASSERT_TRUE(stroke.Set(0, 0, static_cast<uint8_t>((original + 1) % 8)));
    EXPECT_EQ(state.game_data->sheet_store.Revision(sheet), revision);
    EXPECT_FALSE(ValidateCatalogArtPreview(browser, state).ok());
    EXPECT_FALSE(browser.object_pending);
    EXPECT_TRUE(stroke.Set(0, 0, original));
  }
  EXPECT_EQ(graphics_->undo_manager().UndoStackSize(), history_before);
}

TEST_F(DungeonPaletteSourceTest,
       PotAndStatueSourcesTraceTheirActualFixedPatterns) {
  constexpr int kRoom = 5;
  auto& room = dungeon_->rooms()[kRoom];
  struct Case {
    int id;
    uint8_t size;
    int width;
    int height;
    size_t cells;
  };
  for (const auto& item :
       {Case{0xFAF, 15, 16, 16, 4}, Case{0x11D, 0, 16, 24, 6}}) {
    SCOPED_TRACE(item.id);
    zelda3::RoomObject object(item.id, 4, 4, item.size, 0);
    object.SetRom(manager_->GetCurrentRom());
    object.EnsureTilesLoaded();
    room.SetTileObjects({object});
    DungeonPaletteSourceTestPeer::PrepareRoom(*dungeon_, kRoom);
    auto context = MakeDungeonGraphicsEditContext(
        room, room.GetTileObjects().front(), manager_->GetCurrentGameData());
    ASSERT_OK(context.status());
    EXPECT_EQ(context->pattern_width, item.width);
    EXPECT_EQ(context->pattern_height, item.height);
    EXPECT_EQ(context->pattern_cells.size(), item.cells);
    EXPECT_FALSE(context->targets.empty());
  }
}

TEST_F(DungeonPaletteSourceTest,
       ConditionalWallSourceUsesPreparedRoomLayoutForItsOpeningCap) {
  constexpr int kRoom = 5;
  DungeonPaletteSourceTestPeer::PrepareRoom(*dungeon_, kRoom);
  auto& room = dungeon_->rooms()[kRoom];
  zelda3::RoomObject wall(0x22, 4, 4, 0, 0);
  wall.SetRom(manager_->GetCurrentRom());
  wall.EnsureTilesLoaded();
  room.SetTileObjects({wall});
  room.bg1_buffer().SetTileAt(4, 4, 0x00e2);
  auto capped = MakeDungeonGraphicsEditContext(
      room, room.GetTileObjects().front(), manager_->GetCurrentGameData());
  ASSERT_OK(capped.status());
  EXPECT_EQ(capped->pattern_cells.size(), 3u);
  EXPECT_EQ(capped->pattern_width, 24);

  room.bg1_buffer().SetTileAt(4, 4, 0);
  auto uncapped = MakeDungeonGraphicsEditContext(
      room, room.GetTileObjects().front(), manager_->GetCurrentGameData());
  ASSERT_OK(uncapped.status());
  EXPECT_EQ(uncapped->pattern_cells.size(), 4u);
  EXPECT_EQ(uncapped->pattern_width, 32);
}

TEST_F(DungeonPaletteSourceTest,
       PlacementEntranceAndBlocksetDriftRefusePairedCommand) {
  const auto choice = ChooseSource();
  ASSERT_TRUE(choice)
      << "No cave/room-5 object has an addressable palette and used pixels";
  auto& room = dungeon_->rooms()[choice->room_id];
  DungeonPaletteSourceTestPeer::OpenGraphics(
      *dungeon_, choice->room_id, room.GetTileObjects()[choice->index]);
  auto context =
      MakeDungeonGraphicsEditContext(room, room.GetTileObjects()[choice->index],
                                     manager_->GetCurrentGameData());
  ASSERT_OK(context.status());
  const auto target = context->targets.front();
  const auto sheet = target.sheet_id;
  const auto original_sheet =
      manager_->GetCurrentGameData()->sheet_store.Sheet(sheet);
  const std::vector<uint8_t> pixels(original_sheet.begin(),
                                    original_sheet.end());
  const auto colors = BoundColors(target);
  const auto undo_depth = graphics_->undo_manager().UndoStackSize();
  const auto assert_unchanged = [&] {
    const auto current_sheet =
        manager_->GetCurrentGameData()->sheet_store.Sheet(sheet);
    EXPECT_EQ(std::vector<uint8_t>(current_sheet.begin(), current_sheet.end()),
              pixels);
    EXPECT_EQ(BoundColors(target), colors);
    EXPECT_EQ(graphics_->undo_manager().UndoStackSize(), undo_depth);
  };

  // The callback passed a copy; editing the original placement must refuse.
  room.GetTileObjects()[choice->index].x_ ^= 1;
  EXPECT_FALSE(graphics_->ReorderSourcePalette({0}, choice->permutation).ok());
  assert_unchanged();
  room.GetTileObjects()[choice->index].x_ ^= 1;

  const uint8_t old_blockset = room.blockset();
  room.SetBlockset(old_blockset ^ 1);
  EXPECT_FALSE(graphics_->ReorderSourcePalette({0}, choice->permutation).ok());
  assert_unchanged();
  room.SetBlockset(old_blockset);

  DungeonPaletteSourceTestPeer::ChangeEntrance(*dungeon_, choice->room_id,
                                               old_blockset ^ 1);
  EXPECT_FALSE(graphics_->ReorderSourcePalette({0}, choice->permutation).ok());
  assert_unchanged();
}

}  // namespace
}  // namespace yaze::editor
