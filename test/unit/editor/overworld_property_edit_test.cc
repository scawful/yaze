#include "app/editor/overworld/map_properties.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/overworld_undo_actions.h"
#include "rom/rom.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::editor {
namespace {

class OverworldPropertyEditTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(rom_->LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    (*rom_)[zelda3::OverworldCustomASMHasBeenApplied] = 0xFF;
    overworld_ = std::make_unique<zelda3::Overworld>(rom_.get());
    PopulateOverworldMaps();
  }

  void PopulateOverworldMaps() {
    auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
        overworld_->overworld_maps());
    maps.clear();
    maps.reserve(zelda3::kNumOverworldMaps);
    for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
      maps.emplace_back(i, rom_.get());
      maps.back().SetAsSmallMap(i);
    }
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
};

int CountEditsForField(const std::vector<OverworldPropertyEdit>& edits,
                       OverworldPropertyField field) {
  return static_cast<int>(std::count_if(
      edits.begin(), edits.end(), [field](const OverworldPropertyEdit& edit) {
        return edit.field == field;
      }));
}

bool HasEdit(const std::vector<OverworldPropertyEdit>& edits,
             OverworldPropertyField field, int index, int value) {
  return std::any_of(edits.begin(), edits.end(),
                     [field, index, value](const OverworldPropertyEdit& edit) {
                       return edit.field == field && edit.index == index &&
                              edit.value == value;
                     });
}

TEST_F(OverworldPropertyEditTest, MessageEditTargetsParentMap) {
  auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
      overworld_->overworld_maps());
  maps[0x40].set_message_id(0x0111);
  maps[0x42].SetAsLargeMap(0x40, 2);

  MapPropertiesSystem properties(overworld_.get(), rom_.get());
  ASSERT_TRUE(properties
                  .ApplyPropertyEditDirect(
                      {0x42, OverworldPropertyField::kMessageId, 0, 0x0222})
                  .ok());

  EXPECT_EQ(overworld_->overworld_map(0x40)->message_id(), 0x0222);
  EXPECT_EQ(overworld_->overworld_map(0x42)->message_id(), 0x0000);

  auto read_value =
      properties.ReadPropertyValue({0x42, OverworldPropertyField::kMessageId});
  ASSERT_TRUE(read_value.ok());
  EXPECT_EQ(*read_value, 0x0222);
}

TEST_F(OverworldPropertyEditTest, MusicEditWritesLightAndDarkWorldTables) {
  MapPropertiesSystem properties(overworld_.get(), rom_.get());

  ASSERT_TRUE(properties
                  .ApplyPropertyEditDirect(
                      {0x02, OverworldPropertyField::kMusic, 2, 0x2A})
                  .ok());
  EXPECT_EQ((*rom_)[zelda3::kOverworldMusicMasterSword + 0x02], 0x2A);

  ASSERT_TRUE(properties
                  .ApplyPropertyEditDirect(
                      {0x40, OverworldPropertyField::kMusic, 0, 0x35})
                  .ok());
  EXPECT_EQ((*rom_)[zelda3::kOverworldMusicDarkWorld], 0x35);
}

TEST_F(OverworldPropertyEditTest, VanillaRomRejectsZSCustomOnlyFields) {
  MapPropertiesSystem properties(overworld_.get(), rom_.get());

  EXPECT_FALSE(
      properties
          .ApplyPropertyEditDirect(
              {0x00, OverworldPropertyField::kAnimatedGraphics, 0, 0x12})
          .ok());
  EXPECT_FALSE(properties
                   .ApplyPropertyEditDirect(
                       {0x00, OverworldPropertyField::kMosaicExpanded, 0, 1})
                   .ok());
}

TEST_F(OverworldPropertyEditTest, ZSCustomV2AllowsDirectionalMosaic) {
  (*rom_)[zelda3::OverworldCustomASMHasBeenApplied] = 0x02;
  MapPropertiesSystem properties(overworld_.get(), rom_.get());

  ASSERT_TRUE(properties
                  .ApplyPropertyEditDirect(
                      {0x05, OverworldPropertyField::kMosaicExpanded, 3, 1})
                  .ok());

  EXPECT_TRUE(overworld_->overworld_map(0x05)->mosaic_expanded()[3]);
}

TEST_F(OverworldPropertyEditTest,
       PropertyBatchSkipsUnsupportedFieldsAndAppliesSupportedFields) {
  (*rom_)[zelda3::OverworldCustomASMHasBeenApplied] = 0xFF;
  auto* map = overworld_->mutable_overworld_map(0x08);
  ASSERT_NE(map, nullptr);
  map->set_area_graphics(0x10);
  map->set_main_palette(0x20);

  MapPropertiesSystem properties(overworld_.get(), rom_.get());
  ASSERT_TRUE(properties
                  .ApplyPropertyEdits({
                      {0x08, OverworldPropertyField::kAreaGraphics, 0, 0x33},
                      {0x08, OverworldPropertyField::kMainPalette, 0, 0x44},
                  })
                  .ok());

  EXPECT_EQ(map->area_graphics(), 0x33);
  EXPECT_EQ(map->main_palette(), 0x20);
}

TEST(OverworldMetadataPasteEditTest, ScopedPalettePasteOnlyBuildsPaletteEdits) {
  OverworldMapMetadataClipboard clipboard;
  clipboard.valid = true;
  clipboard.scope = OverworldMapMetadataClipboardScope::kPalettes;
  clipboard.area_palette = 0x12;
  clipboard.main_palette = 0x34;
  clipboard.area_specific_bg_color = 0x4567;
  clipboard.sprite_palette = {0x20, 0x21, 0x22};
  clipboard.area_graphics = 0x55;
  clipboard.message_id = 0x1234;

  EXPECT_TRUE(CanPasteOverworldMapMetadata(
      clipboard, OverworldMapMetadataClipboardScope::kPalettes));
  EXPECT_FALSE(CanPasteOverworldMapMetadata(
      clipboard, OverworldMapMetadataClipboardScope::kGraphics));
  EXPECT_TRUE(
      BuildOverworldMetadataPasteEdits(
          0x08, clipboard, OverworldMapMetadataClipboardScope::kGraphics)
          .empty());

  const auto edits = BuildOverworldMetadataPasteEdits(
      0x08, clipboard, OverworldMapMetadataClipboardScope::kPalettes);

  EXPECT_EQ(edits.size(), 6u);
  EXPECT_TRUE(HasEdit(edits, OverworldPropertyField::kAreaPalette, 0, 0x12));
  EXPECT_TRUE(HasEdit(edits, OverworldPropertyField::kMainPalette, 0, 0x34));
  EXPECT_TRUE(
      HasEdit(edits, OverworldPropertyField::kAreaSpecificBgColor, 0, 0x4567));
  EXPECT_TRUE(HasEdit(edits, OverworldPropertyField::kSpritePalette, 2, 0x22));
  EXPECT_EQ(CountEditsForField(edits, OverworldPropertyField::kAreaGraphics),
            0);
  EXPECT_EQ(CountEditsForField(edits, OverworldPropertyField::kMessageId), 0);
}

TEST(OverworldMetadataPasteEditTest, FullClipboardCanPasteScopedMetadata) {
  OverworldMapMetadataClipboard clipboard;
  clipboard.valid = true;
  clipboard.scope = OverworldMapMetadataClipboardScope::kAll;
  clipboard.area_size = static_cast<int>(zelda3::AreaSizeEnum::LargeArea);
  clipboard.area_graphics = 0x11;
  clipboard.animated_graphics = 0x12;
  clipboard.custom_tilesets[7] = 0x37;
  clipboard.sprite_graphics = {0x24, 0x25, 0x26};
  clipboard.message_id = 0x0203;
  clipboard.music = {0x04, 0x05, 0x06, 0x07};

  EXPECT_TRUE(CanPasteOverworldMapMetadata(
      clipboard, OverworldMapMetadataClipboardScope::kGraphics));
  EXPECT_TRUE(CanPasteOverworldMapMetadata(
      clipboard, OverworldMapMetadataClipboardScope::kMusicMessages));

  const auto graphics_edits = BuildOverworldMetadataPasteEdits(
      0x09, clipboard, OverworldMapMetadataClipboardScope::kGraphics);
  EXPECT_EQ(graphics_edits.size(), 13u);
  EXPECT_TRUE(
      HasEdit(graphics_edits, OverworldPropertyField::kAreaGraphics, 0, 0x11));
  EXPECT_TRUE(HasEdit(graphics_edits, OverworldPropertyField::kAnimatedGraphics,
                      0, 0x12));
  EXPECT_TRUE(HasEdit(graphics_edits, OverworldPropertyField::kSpriteGraphics,
                      2, 0x26));
  EXPECT_TRUE(
      HasEdit(graphics_edits, OverworldPropertyField::kCustomTileset, 7, 0x37));
  EXPECT_EQ(
      CountEditsForField(graphics_edits, OverworldPropertyField::kAreaSize), 0);
  EXPECT_EQ(
      CountEditsForField(graphics_edits, OverworldPropertyField::kMessageId),
      0);

  const auto music_edits = BuildOverworldMetadataPasteEdits(
      0x09, clipboard, OverworldMapMetadataClipboardScope::kMusicMessages);
  EXPECT_EQ(music_edits.size(), 5u);
  EXPECT_TRUE(
      HasEdit(music_edits, OverworldPropertyField::kMessageId, 0, 0x0203));
  EXPECT_TRUE(HasEdit(music_edits, OverworldPropertyField::kMusic, 3, 0x07));
  EXPECT_EQ(
      CountEditsForField(music_edits, OverworldPropertyField::kAreaGraphics),
      0);
}

TEST(OverworldMetadataPasteEditTest, ClipboardDescriptionIncludesScopeAndMap) {
  OverworldMapMetadataClipboard clipboard;
  EXPECT_EQ(DescribeOverworldMapMetadataClipboard(clipboard),
            "No map metadata");

  clipboard.valid = true;
  clipboard.scope = OverworldMapMetadataClipboardScope::kGraphics;
  clipboard.source_map_id = 0x42;

  EXPECT_EQ(DescribeOverworldMapMetadataClipboard(clipboard),
            "graphics metadata from map 0x42");
}

TEST(OverworldPropertyBatchEditActionTest, UndoRedoRestoresBatchAsOneAction) {
  std::array<int, 3> values = {0x10, 0x20, 0x30};
  auto apply = [&values](const OverworldPropertyEdit& edit) -> absl::Status {
    if (edit.index < 0 || edit.index >= static_cast<int>(values.size())) {
      return absl::InvalidArgumentError("invalid index");
    }
    values[edit.index] = edit.value;
    return absl::OkStatus();
  };

  std::vector<OverworldPropertyEdit> before = {
      {0x00, OverworldPropertyField::kAreaGraphics, 0, 0x10},
      {0x00, OverworldPropertyField::kAreaPalette, 1, 0x20},
      {0x00, OverworldPropertyField::kMessageId, 2, 0x30},
  };
  std::vector<OverworldPropertyEdit> after = {
      {0x00, OverworldPropertyField::kAreaGraphics, 0, 0x11},
      {0x00, OverworldPropertyField::kAreaPalette, 1, 0x22},
      {0x00, OverworldPropertyField::kMessageId, 2, 0x33},
  };

  values = {0x11, 0x22, 0x33};
  UndoManager manager;
  manager.Push(std::make_unique<OverworldMapPropertyBatchEditAction>(
      before, after, apply, "Paste map metadata"));

  EXPECT_EQ(manager.UndoStackSize(), 1u);
  EXPECT_EQ(manager.GetUndoDescription(), "Paste map metadata");

  ASSERT_TRUE(manager.Undo().ok());
  EXPECT_EQ(values[0], 0x10);
  EXPECT_EQ(values[1], 0x20);
  EXPECT_EQ(values[2], 0x30);

  ASSERT_TRUE(manager.Redo().ok());
  EXPECT_EQ(values[0], 0x11);
  EXPECT_EQ(values[1], 0x22);
  EXPECT_EQ(values[2], 0x33);
}

}  // namespace
}  // namespace yaze::editor
