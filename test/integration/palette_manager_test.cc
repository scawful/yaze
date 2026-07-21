#include "app/gfx/util/palette_manager.h"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <optional>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/widgets/palette_editor_widget.h"
#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace gfx {
namespace {

struct DungeonSaveFlagsGuard {
  decltype(core::FeatureFlags::get().dungeon) previous =
      core::FeatureFlags::get().dungeon;
  ~DungeonSaveFlagsGuard() { core::FeatureFlags::get().dungeon = previous; }
};

void ConfigurePaletteOnlyDungeonSave() {
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = false;
  flags.kSaveSprites = false;
  flags.kSaveRoomHeaders = false;
  flags.kSaveTorches = false;
  flags.kSavePits = false;
  flags.kSaveBlocks = false;
  flags.kSaveCollision = false;
  flags.kSaveWaterFillZones = false;
  flags.kSaveChests = false;
  flags.kSavePotItems = false;
  flags.kSaveEntrances = false;
  flags.kSavePalettes = true;
}

void SeedPaletteGroup(PaletteGroup* group, int palette_count, int color_count,
                      uint16_t seed) {
  ASSERT_NE(group, nullptr);
  group->clear();
  for (int palette_index = 0; palette_index < palette_count; ++palette_index) {
    SnesPalette palette;
    for (int color_index = 0; color_index < color_count; ++color_index) {
      palette.AddColor(SnesColor(static_cast<uint16_t>(
          (seed + palette_index * color_count + color_index) & 0x7FFF)));
    }
    group->AddPalette(palette);
  }
}

// Test fixture for PaletteManager integration tests
class PaletteManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // PaletteManager is a singleton, so reset it for test isolation
    PaletteManager::Get().ResetForTesting();
  }

  void TearDown() override {
    // Clean up any test state
    PaletteManager::Get().ClearHistory();
  }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(PaletteManagerTest, InitializationState) {
  auto& manager = PaletteManager::Get();

  // Before initialization, should not be initialized
  // Note: This might fail if other tests have already initialized it
  // In production, we'd need a Reset() method for testing

  // After initialization with null ROM, should handle gracefully
  manager.Initialize(static_cast<Rom*>(nullptr));
  EXPECT_FALSE(manager.IsInitialized());
}

TEST_F(PaletteManagerTest, HasNoUnsavedChangesInitially) {
  auto& manager = PaletteManager::Get();

  // Should have no unsaved changes initially
  EXPECT_FALSE(manager.HasUnsavedChanges());
  EXPECT_EQ(manager.GetModifiedColorCount(), 0);
}

// ============================================================================
// Dirty Tracking Tests
// ============================================================================

TEST_F(PaletteManagerTest, TracksModifiedGroups) {
  auto& manager = PaletteManager::Get();

  // Initially, no groups should be modified
  auto modified_groups = manager.GetModifiedGroups();
  EXPECT_TRUE(modified_groups.empty());
}

TEST_F(PaletteManagerTest, GetModifiedColorCount) {
  auto& manager = PaletteManager::Get();

  // Initially, no colors modified
  EXPECT_EQ(manager.GetModifiedColorCount(), 0);

  // After initialization and making changes, count should increase
  // (This would require a valid ROM to test properly)
}

// ============================================================================
// Undo/Redo Tests
// ============================================================================

TEST_F(PaletteManagerTest, UndoRedoInitialState) {
  auto& manager = PaletteManager::Get();

  // Initially, should not be able to undo or redo
  EXPECT_FALSE(manager.CanUndo());
  EXPECT_FALSE(manager.CanRedo());
  EXPECT_EQ(manager.GetUndoStackSize(), 0);
  EXPECT_EQ(manager.GetRedoStackSize(), 0);
}

TEST_F(PaletteManagerTest, ClearHistoryResetsStacks) {
  auto& manager = PaletteManager::Get();

  // Clear history should reset both stacks
  manager.ClearHistory();

  EXPECT_FALSE(manager.CanUndo());
  EXPECT_FALSE(manager.CanRedo());
  EXPECT_EQ(manager.GetUndoStackSize(), 0);
  EXPECT_EQ(manager.GetRedoStackSize(), 0);
}

TEST_F(PaletteManagerTest, UndoWithoutChangesIsNoOp) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.CanUndo());

  // Should not crash
  manager.Undo();

  EXPECT_FALSE(manager.CanUndo());
}

TEST_F(PaletteManagerTest, RedoWithoutUndoIsNoOp) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.CanRedo());

  // Should not crash
  manager.Redo();

  EXPECT_FALSE(manager.CanRedo());
}

// ============================================================================
// Batch Operations Tests
// ============================================================================

TEST_F(PaletteManagerTest, BatchModeTracking) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.InBatch());

  manager.BeginBatch();
  EXPECT_TRUE(manager.InBatch());

  manager.EndBatch();
  EXPECT_FALSE(manager.InBatch());
}

TEST_F(PaletteManagerTest, NestedBatchOperations) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.InBatch());

  manager.BeginBatch();
  EXPECT_TRUE(manager.InBatch());

  manager.BeginBatch();  // Nested
  EXPECT_TRUE(manager.InBatch());

  manager.EndBatch();
  EXPECT_TRUE(manager.InBatch());  // Still in batch (outer)

  manager.EndBatch();
  EXPECT_FALSE(manager.InBatch());  // Now out of batch
}

TEST_F(PaletteManagerTest, EndBatchWithoutBeginIsNoOp) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.InBatch());

  // Should not crash
  manager.EndBatch();

  EXPECT_FALSE(manager.InBatch());
}

// ============================================================================
// Change Notification Tests
// ============================================================================

TEST_F(PaletteManagerTest, RegisterAndUnregisterListener) {
  auto& manager = PaletteManager::Get();

  int callback_count = 0;
  auto callback = [&callback_count](const PaletteChangeEvent& event) {
    callback_count++;
  };

  // Register listener
  int id = manager.RegisterChangeListener(callback);
  EXPECT_GT(id, 0);

  // Unregister listener
  manager.UnregisterChangeListener(id);

  // After unregistering, callback should not be called
  // (Would need to trigger an event to test this properly)
}

TEST_F(PaletteManagerTest, MultipleListeners) {
  auto& manager = PaletteManager::Get();

  int callback1_count = 0;
  int callback2_count = 0;

  auto callback1 = [&callback1_count](const PaletteChangeEvent& event) {
    callback1_count++;
  };

  auto callback2 = [&callback2_count](const PaletteChangeEvent& event) {
    callback2_count++;
  };

  int id1 = manager.RegisterChangeListener(callback1);
  int id2 = manager.RegisterChangeListener(callback2);

  EXPECT_NE(id1, id2);

  // Clean up
  manager.UnregisterChangeListener(id1);
  manager.UnregisterChangeListener(id2);
}

// ============================================================================
// Color Query Tests (without ROM)
// ============================================================================

TEST_F(PaletteManagerTest, ResetColorWithoutInitializationReturnsError) {
  auto& manager = PaletteManager::Get();

  auto status = manager.ResetColor("ow_main", 0, 0);

  // Should return an error or default color
  // Exact behavior depends on implementation
}

TEST_F(PaletteManagerTest, ResetPaletteWithoutInitializationFails) {
  auto& manager = PaletteManager::Get();

  auto status = manager.ResetPalette("ow_main", 0);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

// ============================================================================
// Save/Discard Tests (without ROM)
// ============================================================================

TEST_F(PaletteManagerTest, SaveGroupWithoutInitializationFails) {
  auto& manager = PaletteManager::Get();

  auto status = manager.SaveGroup("ow_main");

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(PaletteManagerTest, SaveAllWithoutInitializationFails) {
  auto& manager = PaletteManager::Get();

  auto status = manager.SaveAllToRom();

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(PaletteManagerTest, SaveTransactionRollbackRestoresRetryState) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
  zelda3::GameData game_data(&rom);
  auto* group = game_data.palette_groups.get_group("ow_main");
  ASSERT_NE(group, nullptr);
  group->clear();
  SnesPalette palette;
  palette.AddColor(SnesColor(0x0000));
  group->AddPalette(palette);

  auto& manager = PaletteManager::Get();
  manager.Initialize(&game_data);
  ASSERT_TRUE(manager.SetColor("ow_main", 0, 0, SnesColor(0x001F)).ok());
  ASSERT_TRUE(manager.HasUnsavedChanges());
  ASSERT_TRUE(manager.BeginSaveTransaction().ok());
  ASSERT_TRUE(manager.SaveAllToRom().ok());
  ASSERT_FALSE(manager.HasUnsavedChanges());

  manager.RollbackSaveTransaction();
  EXPECT_TRUE(manager.HasUnsavedChanges());
  EXPECT_TRUE(manager.IsColorModified("ow_main", 0, 0));

  ASSERT_TRUE(manager.ResetColor("ow_main", 0, 0).ok());
  EXPECT_EQ(manager.GetColor("ow_main", 0, 0).snes(), 0x0000);
}

TEST_F(PaletteManagerTest, DungeonRenderEditsMapToManagedHudAndDungeonColors) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
  zelda3::GameData game_data(&rom);
  SeedPaletteGroup(game_data.palette_groups.get_group("hud"), 1, 32, 0x0100);
  SeedPaletteGroup(game_data.palette_groups.get_group("dungeon_main"), 2, 90,
                   0x0200);

  auto& manager = PaletteManager::Get();
  manager.Initialize(&game_data);

  gui::PaletteEditorWidget widget;
  widget.Initialize(&game_data);
  widget.SetDungeonRenderPaletteMode(true);
  widget.SetCurrentPaletteId(1);
  int callback_count = 0;
  int callback_palette = -1;
  widget.SetOnPaletteChanged([&](int palette_id) {
    ++callback_count;
    callback_palette = palette_id;
  });

  constexpr int kHudDisplayIndex = 17;
  constexpr int kDungeonDisplayIndex = 99;
  constexpr int kDungeonColorIndex = 62;
  const SnesColor original_dungeon_color =
      manager.GetColor("dungeon_main", 1, kDungeonColorIndex);
  const SnesColor new_hud_color(0x001F);
  const SnesColor new_dungeon_color(0x7C00);

  ASSERT_TRUE(
      widget.ApplyDungeonRenderColorEdit(kHudDisplayIndex, new_hud_color).ok());
  ASSERT_TRUE(
      widget
          .ApplyDungeonRenderColorEdit(kDungeonDisplayIndex, new_dungeon_color)
          .ok());

  EXPECT_TRUE(manager.IsColorModified("hud", 0, kHudDisplayIndex));
  EXPECT_TRUE(manager.IsColorModified("dungeon_main", 1, kDungeonColorIndex));
  EXPECT_EQ(manager.GetColor("hud", 0, kHudDisplayIndex).snes(),
            new_hud_color.snes());
  EXPECT_EQ(manager.GetColor("dungeon_main", 1, kDungeonColorIndex).snes(),
            new_dungeon_color.snes());
  EXPECT_EQ(callback_count, 2);
  EXPECT_EQ(callback_palette, 1);

  std::array<uint16_t, 256> cgram{};
  const auto& dungeon_palette =
      game_data.palette_groups.dungeon_main.palette_ref(1);
  const auto& hud_palette = game_data.palette_groups.hud.palette_ref(0);
  zelda3::LoadDungeonRenderPaletteToCgram(cgram, dungeon_palette, &hud_palette);
  EXPECT_EQ(cgram[kHudDisplayIndex], new_hud_color.snes());
  EXPECT_EQ(cgram[kDungeonDisplayIndex], new_dungeon_color.snes());

  ASSERT_TRUE(
      widget.ApplyDungeonRenderColorEdit(kDungeonDisplayIndex, std::nullopt)
          .ok());
  EXPECT_EQ(manager.GetColor("dungeon_main", 1, kDungeonColorIndex).snes(),
            original_dungeon_color.snes());
  EXPECT_EQ(callback_count, 3);

  constexpr int kReservedRowStart = 96;
  const absl::Status reserved_status =
      widget.ApplyDungeonRenderColorEdit(kReservedRowStart, new_dungeon_color);
  EXPECT_FALSE(reserved_status.ok());
  EXPECT_EQ(reserved_status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(callback_count, 3);
}

TEST_F(PaletteManagerTest, DungeonRenderEditPersistsThroughDungeonEditorSave) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());
  zelda3::GameData game_data(&rom);
  SeedPaletteGroup(game_data.palette_groups.get_group("dungeon_main"), 2, 90,
                   0x0200);

  auto& manager = PaletteManager::Get();
  manager.Initialize(&game_data);

  gui::PaletteEditorWidget widget;
  widget.Initialize(&game_data);
  widget.SetDungeonRenderPaletteMode(true);
  widget.SetCurrentPaletteId(1);

  constexpr int kDungeonDisplayIndex = 99;
  constexpr int kDungeonColorIndex = 62;
  const SnesColor edited_color(0x1234);
  const uint32_t color_address =
      GetPaletteAddress("dungeon_main", 1, kDungeonColorIndex);
  ASSERT_TRUE(rom.ReadWord(color_address - 2).ok());
  ASSERT_TRUE(rom.ReadWord(color_address + 2).ok());
  const uint16_t previous_color = *rom.ReadWord(color_address - 2);
  const uint16_t next_color = *rom.ReadWord(color_address + 2);

  ASSERT_TRUE(
      widget.ApplyDungeonRenderColorEdit(kDungeonDisplayIndex, edited_color)
          .ok());
  ASSERT_TRUE(manager.HasUnsavedChanges());
  ASSERT_TRUE(manager.IsColorModified("dungeon_main", 1, kDungeonColorIndex));

  DungeonSaveFlagsGuard flags_guard;
  ConfigurePaletteOnlyDungeonSave();
  auto dungeon_editor = std::make_unique<editor::DungeonEditorV2>(&rom);
  editor::EditorDependencies dependencies;
  dependencies.rom = &rom;
  dependencies.game_data = &game_data;
  dungeon_editor->SetDependencies(dependencies);
  dungeon_editor->SetGameData(&game_data);

  const absl::Status save_status = dungeon_editor->Save();
  ASSERT_TRUE(save_status.ok()) << save_status.message();

  ASSERT_TRUE(rom.ReadWord(color_address).ok());
  EXPECT_EQ(*rom.ReadWord(color_address), edited_color.snes());
  EXPECT_EQ(*rom.ReadWord(color_address - 2), previous_color);
  EXPECT_EQ(*rom.ReadWord(color_address + 2), next_color);

  PaletteGroupMap reloaded_groups;
  const absl::Status reload_status =
      LoadAllPalettes(rom.vector(), reloaded_groups);
  ASSERT_TRUE(reload_status.ok()) << reload_status.message();
  ASSERT_GT(reloaded_groups.dungeon_main.size(), 1u);
  ASSERT_GT(reloaded_groups.dungeon_main.palette_ref(1).size(),
            static_cast<size_t>(kDungeonColorIndex));
  EXPECT_EQ(
      reloaded_groups.dungeon_main.palette_ref(1)[kDungeonColorIndex].snes(),
      edited_color.snes());
  EXPECT_FALSE(manager.HasUnsavedChanges());
}

TEST_F(PaletteManagerTest, DiscardGroupWithoutInitializationIsNoOp) {
  auto& manager = PaletteManager::Get();

  // Should not crash
  manager.DiscardGroup("ow_main");

  // No unsaved changes
  EXPECT_FALSE(manager.HasUnsavedChanges());
}

TEST_F(PaletteManagerTest, DiscardAllWithoutInitializationIsNoOp) {
  auto& manager = PaletteManager::Get();

  // Should not crash
  manager.DiscardAllChanges();

  // No unsaved changes
  EXPECT_FALSE(manager.HasUnsavedChanges());
}

// ============================================================================
// Group Modification Query Tests
// ============================================================================

TEST_F(PaletteManagerTest, IsGroupModifiedInitiallyFalse) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.IsGroupModified("ow_main"));
  EXPECT_FALSE(manager.IsGroupModified("dungeon_main"));
  EXPECT_FALSE(manager.IsGroupModified("global_sprites"));
}

TEST_F(PaletteManagerTest, IsPaletteModifiedInitiallyFalse) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.IsPaletteModified("ow_main", 0));
  EXPECT_FALSE(manager.IsPaletteModified("ow_main", 5));
}

TEST_F(PaletteManagerTest, IsColorModifiedInitiallyFalse) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.IsColorModified("ow_main", 0, 0));
  EXPECT_FALSE(manager.IsColorModified("ow_main", 0, 7));
}

// ============================================================================
// Invalid Input Tests
// ============================================================================

TEST_F(PaletteManagerTest, SetColorInvalidGroupName) {
  auto& manager = PaletteManager::Get();

  SnesColor color(0x7FFF);
  auto status = manager.SetColor("invalid_group", 0, 0, color);

  EXPECT_FALSE(status.ok());
}

TEST_F(PaletteManagerTest, GetColorInvalidGroupName) {
  auto& manager = PaletteManager::Get();

  SnesColor color = manager.GetColor("invalid_group", 0, 0);

  // Should return default color
  auto rgb = color.rgb();
  EXPECT_FLOAT_EQ(rgb.x, 0.0f);
  EXPECT_FLOAT_EQ(rgb.y, 0.0f);
  EXPECT_FLOAT_EQ(rgb.z, 0.0f);
}

TEST_F(PaletteManagerTest, IsGroupModifiedInvalidGroupName) {
  auto& manager = PaletteManager::Get();

  EXPECT_FALSE(manager.IsGroupModified("invalid_group"));
}

}  // namespace
}  // namespace gfx
}  // namespace yaze
