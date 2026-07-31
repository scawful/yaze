#include "gtest/gtest.h"

#include <utility>

#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {
namespace {

constexpr uint32_t kObject11fDescriptorAddress = 0x842E;
constexpr uint16_t kObject11fDescriptorWord = 0x0E9A;
constexpr uint32_t kObject11fSourceAddress = 0x29EC;

void StoreWord(std::vector<uint8_t>& data, uint32_t address, uint16_t word) {
  data[address] = static_cast<uint8_t>(word & 0xFF);
  data[address + 1] = static_cast<uint8_t>(word >> 8);
}

std::vector<uint8_t> MakeObjectTileRomData(uint16_t first_word) {
  std::vector<uint8_t> data(0x200000, 0);
  StoreWord(data, kObject11fDescriptorAddress, kObject11fDescriptorWord);
  for (size_t index = 0; index < 4; ++index) {
    StoreWord(data, kObject11fSourceAddress + index * 2,
              static_cast<uint16_t>(first_word + index));
  }
  return data;
}

}  // namespace

TEST(RoomObjectSetIdTest, RecomputesAllBgsAndInvalidatesTileCache) {
  RoomObject obj(/*id=*/0x21, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);

  // Seed state so we can validate that changing the ID invalidates caches.
  obj.tiles_loaded_ = true;
  obj.tile_count_ = 123;
  obj.tile_data_ptr_ = 456;
  obj.tiles_.push_back(gfx::TileInfo{});
  obj.all_bgs_ = false;

  obj.set_id(/*id=*/0x0C);

  EXPECT_EQ(obj.id_, 0x0C);
  EXPECT_TRUE(obj.all_bgs_);
  EXPECT_FALSE(obj.tiles_loaded_);
  EXPECT_TRUE(obj.tiles_.empty());
  EXPECT_EQ(obj.tile_count_, 0);
  EXPECT_EQ(obj.tile_data_ptr_, -1);
}

TEST(RoomObjectSetIdTest, NoOpWhenIdUnchanged) {
  RoomObject obj(/*id=*/0x21, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);

  obj.tiles_loaded_ = true;
  obj.tile_count_ = 1;
  obj.tile_data_ptr_ = 1;
  obj.tiles_.push_back(gfx::TileInfo{});
  obj.all_bgs_ = true;  // simulate custom/manual override

  obj.set_id(/*id=*/0x21);

  EXPECT_EQ(obj.id_, 0x21);
  EXPECT_TRUE(obj.tiles_loaded_);
  EXPECT_FALSE(obj.tiles_.empty());
  EXPECT_EQ(obj.tile_count_, 1);
  EXPECT_EQ(obj.tile_data_ptr_, 1);
  EXPECT_TRUE(obj.all_bgs_);
}

TEST(RoomObjectTileCacheTest, EveryAccessorRefreshesTrackedCacheRevision) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MakeObjectTileRomData(/*first_word=*/1)).ok());
  RoomObject object(/*id=*/0x11F, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);
  object.SetRom(&rom);

  auto initial_tiles_or = object.GetTiles();
  ASSERT_TRUE(initial_tiles_or.ok()) << initial_tiles_or.status();
  ASSERT_FALSE(initial_tiles_or->empty());
  EXPECT_EQ((*initial_tiles_or)[0].id_, 1);

  StoreWord(rom.mutable_vector(), kObject11fSourceAddress, /*word=*/2);
  rom.AdvanceObjectTileRevision();
  auto refreshed_tiles_or = object.GetTiles();
  ASSERT_TRUE(refreshed_tiles_or.ok()) << refreshed_tiles_or.status();
  EXPECT_EQ((*refreshed_tiles_or)[0].id_, 2);

  StoreWord(rom.mutable_vector(), kObject11fSourceAddress, /*word=*/3);
  rom.AdvanceObjectTileRevision();
  auto refreshed_tile_or = object.GetTile(/*index=*/0);
  ASSERT_TRUE(refreshed_tile_or.ok()) << refreshed_tile_or.status();
  EXPECT_EQ((*refreshed_tile_or)->id_, 3);

  StoreWord(rom.mutable_vector(), kObject11fSourceAddress, /*word=*/4);
  rom.AdvanceObjectTileRevision();
  ASSERT_FALSE(object.tiles().empty());
  EXPECT_EQ(object.tiles()[0].id_, 4);

  object.tile_count_ = 999;
  rom.AdvanceObjectTileRevision();
  EXPECT_EQ(object.GetTileCount(), 4);
}

TEST(RoomObjectTileCacheTest,
     InPlaceRomReplacementCannotReuseEqualRevisionCache) {
  Rom active_rom;
  Rom replacement;
  ASSERT_TRUE(
      active_rom.LoadFromData(MakeObjectTileRomData(/*first_word=*/1)).ok());
  ASSERT_TRUE(
      replacement.LoadFromData(MakeObjectTileRomData(/*first_word=*/9)).ok());
  ASSERT_EQ(active_rom.object_tile_revision(),
            replacement.object_tile_revision());

  RoomObject object(/*id=*/0x11F, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);
  object.SetRom(&active_rom);
  auto initial_tile_or = object.GetTile(/*index=*/0);
  ASSERT_TRUE(initial_tile_or.ok()) << initial_tile_or.status();
  EXPECT_EQ((*initial_tile_or)->id_, 1);

  active_rom = std::move(replacement);
  auto replaced_tile_or = object.GetTile(/*index=*/0);
  ASSERT_TRUE(replaced_tile_or.ok()) << replaced_tile_or.status();
  EXPECT_EQ((*replaced_tile_or)->id_, 9);
}

TEST(RoomObjectTileCacheTest, CopiedTrackedCachesCheckRevisionAndRomIdentity) {
  Rom first_rom;
  Rom second_rom;
  ASSERT_TRUE(
      first_rom.LoadFromData(MakeObjectTileRomData(/*first_word=*/1)).ok());
  ASSERT_TRUE(
      second_rom.LoadFromData(MakeObjectTileRomData(/*first_word=*/9)).ok());
  ASSERT_EQ(first_rom.object_tile_revision(),
            second_rom.object_tile_revision());

  RoomObject source(/*id=*/0x11F, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);
  source.SetRom(&first_rom);
  auto source_tile_or = source.GetTile(/*index=*/0);
  ASSERT_TRUE(source_tile_or.ok()) << source_tile_or.status();
  EXPECT_EQ((*source_tile_or)->id_, 1);

  RoomObject same_rom_copy = source;
  StoreWord(first_rom.mutable_vector(), kObject11fSourceAddress, /*word=*/2);
  first_rom.AdvanceObjectTileRevision();
  auto refreshed_copy_tile_or = same_rom_copy.GetTile(/*index=*/0);
  ASSERT_TRUE(refreshed_copy_tile_or.ok()) << refreshed_copy_tile_or.status();
  EXPECT_EQ((*refreshed_copy_tile_or)->id_, 2);

  // Equal revision numbers are insufficient: a copied cache moved to another
  // ROM must reload from that ROM rather than reuse the source object's tiles.
  RoomObject other_rom_copy = source;
  other_rom_copy.SetRom(&second_rom);
  auto other_tile_or = other_rom_copy.GetTile(/*index=*/0);
  ASSERT_TRUE(other_tile_or.ok()) << other_tile_or.status();
  EXPECT_EQ((*other_tile_or)->id_, 9);
}

TEST(RoomObjectTileCacheTest,
     CopiedInjectedCacheRemainsUntrackedAcrossRomChanges) {
  Rom first_rom;
  Rom second_rom;
  ASSERT_TRUE(
      first_rom.LoadFromData(MakeObjectTileRomData(/*first_word=*/1)).ok());
  ASSERT_TRUE(
      second_rom.LoadFromData(MakeObjectTileRomData(/*first_word=*/9)).ok());

  RoomObject injected(/*id=*/0x11F, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);
  injected.SetRom(&first_rom);
  ASSERT_TRUE(injected.GetTile(/*index=*/0).ok());
  injected.mutable_tiles().assign(
      1, gfx::TileInfo(/*id=*/777, /*palette=*/0, /*v=*/false, /*h=*/false,
                       /*o=*/false));
  injected.tiles_loaded_ = true;
  injected.tile_count_ = 1;

  RoomObject copied = injected;
  copied.SetRom(&second_rom);
  second_rom.AdvanceObjectTileRevision();

  auto tiles_or = copied.GetTiles();
  ASSERT_TRUE(tiles_or.ok()) << tiles_or.status();
  ASSERT_EQ(tiles_or->size(), 1u);
  EXPECT_EQ((*tiles_or)[0].id_, 777);
  auto tile_or = copied.GetTile(/*index=*/0);
  ASSERT_TRUE(tile_or.ok()) << tile_or.status();
  EXPECT_EQ((*tile_or)->id_, 777);
  EXPECT_EQ(copied.GetTileCount(), 1);
}

}  // namespace zelda3
}  // namespace yaze
