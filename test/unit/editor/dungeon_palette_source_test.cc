#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/graphics_editor.h"
#include "app/editor/graphics/palette_pixel_command.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/gfx/resource/arena.h"
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
        if (!context.ok() || context->targets.empty())
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
            if (plan.ok() && !plan->colors.empty() && !plan->sheets.empty())
              return SourceChoice{room_id, i, permutation};
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
