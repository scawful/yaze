#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/tile16/tile16_edit_session.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "gtest/gtest.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "test_utils/overworld_model_test_peer.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace {
using editor::Tile16Commit;
using editor::Tile16DefinitionOwner;
using editor::Tile16EditSession;

gfx::Tile16 MakeSolidTile16(uint8_t palette) {
  const gfx::TileInfo info(0, palette, false, false, false);
  return gfx::Tile16(info, info, info, info);
}

class PageTile16DefinitionEditTest : public ::testing::Test {
 protected:
  void SetUp() override {
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
    zelda3::OverworldModelTestPeer::InitializeMapSlots(
        *overworld_, zelda3::kNumOverworldMaps);
    auto* global = overworld_->mutable_tiles16();
    global->assign(4096, MakeSolidTile16(0));

    std::vector<gfx::SnesColor> colors(256);
    gfx::SnesPalette palette(colors);
    atlas_.Create(128, 8192, 8, std::vector<uint8_t>(128 * 8192, 0));
    atlas_.SetPalette(palette);
    tilemap_.atlas.Create(128, 8192, 8, atlas_.vector());
    tilemap_.atlas.SetPalette(palette);
    std::vector<uint8_t> source(128 * 512, 1);
    graphics_.Create(128, 512, 8, source);
    graphics_.SetPalette(palette);

    BindMap(0x90);
    session_.set_document_publisher(
        [this](const std::vector<Tile16Commit>& edits) {
          return Publish(edits);
        });
    session_.set_commit_owner_stamper(
        [this](Tile16Commit* before, Tile16Commit* after) {
          Stamp(before, after);
        });
    ASSERT_TRUE(session_.InitializeBitmaps(atlas_, graphics_, types_).ok());
    session_.set_palette(palette);
    rom_.set_dirty(false);
  }

  void TearDown() override { gfx::Arena::Get().ClearTextureQueue(); }

  void BindMap(int map_id) {
    current_map_ = map_id;
    document_ = overworld_->Tiles16ForMap(map_id);
    session_.BindDocument(&document_, &history_, [] {}, map_id);
  }

  void Stamp(Tile16Commit* before, Tile16Commit* after) {
    if (after->owner != Tile16DefinitionOwner::kUnbound) {
      if (before->owner == Tile16DefinitionOwner::kUnbound) {
        before->owner_map_id = after->owner_map_id;
        before->owner =
            after->owner == Tile16DefinitionOwner::kClearPageOverride
                ? Tile16DefinitionOwner::kPageLocal
                : after->owner;
      }
      return;
    }
    before->owner_map_id = current_map_;
    after->owner_map_id = current_map_;
    if (current_map_ < 0x80 || !page_local_) {
      before->owner = Tile16DefinitionOwner::kGlobal;
      after->owner = Tile16DefinitionOwner::kGlobal;
      return;
    }
    after->owner = Tile16DefinitionOwner::kPageLocal;
    before->owner = overworld_->HasPageTile16(current_map_, after->tile_id)
                        ? Tile16DefinitionOwner::kPageLocal
                        : Tile16DefinitionOwner::kClearPageOverride;
  }

  // Mirrors PublishTile16DocumentEdits owner routing without rev5 Import.
  absl::Status Publish(const std::vector<Tile16Commit>& edits) {
    if (reject_publish_) {
      return absl::FailedPreconditionError("publisher refused");
    }
    auto* global = overworld_->mutable_tiles16();
    std::map<uint16_t, std::vector<rom::NativePageTile16Definition>> plans;
    std::vector<std::pair<int, gfx::Tile16>> global_writes;
    for (const auto& edit : edits) {
      auto owner = edit.owner;
      int owner_map = edit.owner_map_id;
      if (owner == Tile16DefinitionOwner::kUnbound) {
        owner_map = current_map_;
        owner = (current_map_ >= 0x80 && page_local_)
                    ? Tile16DefinitionOwner::kPageLocal
                    : Tile16DefinitionOwner::kGlobal;
      }
      if (owner == Tile16DefinitionOwner::kGlobal) {
        if (owner_map >= 0x80 &&
            overworld_->HasPageTile16(owner_map, edit.tile_id)) {
          return absl::FailedPreconditionError("hidden by page override");
        }
        global_writes.emplace_back(edit.tile_id, edit.tile_data);
        continue;
      }
      auto& rows = plans[static_cast<uint16_t>(owner_map)];
      if (rows.empty()) {
        for (const auto& row : overworld_->page_tile16()) {
          if (row.map_id == static_cast<uint16_t>(owner_map))
            rows.push_back(row);
        }
      }
      rows.erase(std::remove_if(rows.begin(), rows.end(),
                                [&](const auto& row) {
                                  return row.tile_id ==
                                         static_cast<uint16_t>(edit.tile_id);
                                }),
                 rows.end());
      if (owner == Tile16DefinitionOwner::kPageLocal &&
          (*global)[edit.tile_id] != edit.tile_data) {
        rows.push_back(zelda3::Overworld::MakePageTile16Definition(
            static_cast<uint16_t>(owner_map),
            static_cast<uint16_t>(edit.tile_id), edit.tile_data));
      }
    }
    std::vector<rom::NativePageTile16Definition> next;
    std::set<uint16_t> touched;
    for (const auto& [map, _] : plans)
      touched.insert(map);
    for (const auto& row : overworld_->page_tile16()) {
      if (!touched.contains(row.map_id))
        next.push_back(row);
    }
    for (const auto& [_, rows] : plans)
      next.insert(next.end(), rows.begin(), rows.end());
    zelda3::OverworldModelTestPeer::SetPageTile16(*overworld_, next);
    for (const auto& [id, data] : global_writes)
      (*global)[id] = data;
    ++publish_count_;
    return absl::OkStatus();
  }

  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
  editor::UndoManager history_;
  gfx::Tilemap tilemap_;
  gfx::Bitmap atlas_, graphics_;
  std::array<uint8_t, 0x200> types_{};
  Tile16EditSession session_{&rom_, &tilemap_};
  std::vector<gfx::Tile16> document_;
  int current_map_ = 0x90;
  bool page_local_ = true;
  bool reject_publish_ = false;
  int publish_count_ = 0;
};

TEST_F(PageTile16DefinitionEditTest, HasPageTile16AndEncodeRoundTrip) {
  const auto encoded = zelda3::Overworld::MakePageTile16Definition(
      0x90, 0xFD2, MakeSolidTile16(1));
  EXPECT_EQ(encoded.map_id, 0x90);
  EXPECT_EQ(encoded.tile_id, 0xFD2);
  zelda3::OverworldModelTestPeer::SetPageTile16(*overworld_, {encoded});
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_FALSE(overworld_->HasPageTile16(0x91, 0xFD2));
  EXPECT_NE(overworld_->Tiles16ForMap(0x90)[0xFD2],
            overworld_->tiles16()[0xFD2]);
  EXPECT_EQ(overworld_->Tiles16ForMap(0x91)[0xFD2],
            overworld_->tiles16()[0xFD2]);
}

TEST_F(PageTile16DefinitionEditTest,
       CrossMapUndoRestoresOwnerAndLeavesNeighborUntouched) {
  page_local_ = true;
  BindMap(0x90);
  ASSERT_TRUE(session_.SetCurrentTile(0xFD2).ok());
  ASSERT_TRUE(session_.ApplyPaletteToAll(3).ok());
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(3));
  EXPECT_EQ(overworld_->tiles16()[0xFD2], MakeSolidTile16(0));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x91)[0xFD2], MakeSolidTile16(0));

  BindMap(0x91);
  page_local_ = false;
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_FALSE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(0));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x91)[0xFD2], MakeSolidTile16(0));
  EXPECT_EQ(overworld_->tiles16()[0xFD2], MakeSolidTile16(0));
  // Bound map-91 document must not absorb map-90's undo payload.
  EXPECT_EQ(document_[0xFD2], MakeSolidTile16(0));

  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(3));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x91)[0xFD2], MakeSolidTile16(0));
  EXPECT_EQ(overworld_->tiles16()[0xFD2], MakeSolidTile16(0));
}

TEST_F(PageTile16DefinitionEditTest,
       RejectingPublisherLeavesSessionAndModelUnchanged) {
  BindMap(0x90);
  ASSERT_TRUE(session_.SetCurrentTile(0xFD2).ok());
  const auto pages_before = overworld_->page_tile16();
  const auto global_before = overworld_->tiles16()[0xFD2];
  const auto doc_before = document_[0xFD2];
  const bool dirty_before = rom_.dirty();
  const auto history_before = history_.UndoStackSize();
  reject_publish_ = true;
  EXPECT_FALSE(session_.ApplyPaletteToAll(5).ok());
  EXPECT_EQ(overworld_->page_tile16().size(), pages_before.size());
  EXPECT_EQ(overworld_->tiles16()[0xFD2], global_before);
  EXPECT_EQ(document_[0xFD2], doc_before);
  EXPECT_EQ(rom_.dirty(), dirty_before);
  EXPECT_EQ(history_.UndoStackSize(), history_before);
  EXPECT_EQ(publish_count_, 0);
}

TEST_F(PageTile16DefinitionEditTest,
       ClearOverrideUsesSharedUndoAndRestoresExactPageRow) {
  zelda3::OverworldModelTestPeer::SetPageTile16(
      *overworld_, {zelda3::Overworld::MakePageTile16Definition(
                        0x90, 0xFD2, MakeSolidTile16(2)),
                    zelda3::Overworld::MakePageTile16Definition(
                        0x90, 0x010, MakeSolidTile16(4))});
  (*overworld_->mutable_tiles16())[0xFD2] = MakeSolidTile16(7);
  BindMap(0x90);
  ASSERT_TRUE(session_.SetCurrentTile(0xFD2).ok());

  Tile16Commit clear;
  clear.tile_id = 0xFD2;
  clear.tile_data = MakeSolidTile16(7);
  clear.owner = Tile16DefinitionOwner::kClearPageOverride;
  clear.owner_map_id = 0x90;
  ASSERT_TRUE(
      session_.ImportDefinitions({clear}, "Clear page-local Tile16 override")
          .ok());
  EXPECT_FALSE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0x010));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(7));
  EXPECT_EQ(history_.UndoStackSize(), 1u);

  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0x010));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(2));
  EXPECT_EQ(overworld_->tiles16()[0xFD2], MakeSolidTile16(7));

  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_FALSE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0x010));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(7));
}

TEST_F(PageTile16DefinitionEditTest,
       ScopeSwitchDoesNotRedirectExistingHistoryOwner) {
  page_local_ = true;
  BindMap(0x90);
  ASSERT_TRUE(session_.SetCurrentTile(0xFD2).ok());
  ASSERT_TRUE(session_.ApplyPaletteToAll(4).ok());
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0xFD2));

  page_local_ = false;  // live scope flip must not retarget prior Undo
  ASSERT_TRUE(history_.Undo().ok());
  EXPECT_FALSE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_EQ(overworld_->tiles16()[0xFD2], MakeSolidTile16(0));
  ASSERT_TRUE(history_.Redo().ok());
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_EQ(overworld_->tiles16()[0xFD2], MakeSolidTile16(0));
}

}  // namespace
}  // namespace yaze
