#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/palette_manager.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "cli/handlers/game/overworld_sprite_edit_commands.h"
#include "cli/handlers/game/terrain_catalog_commands.h"
#include "cli/handlers/tools/overworld_validate_commands.h"
#include "core/features.h"
#include "core/source_artifact_publisher.h"
#include "fixtures/lyra/native_directory_fixture.h"
#include "framework/rom_save_fault.h"
#include "oracle_rom_fixture.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "rom/transaction.h"
#include "rom/write_fence.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_allocation.h"
#include "zelda3/overworld/overworld_version_helper.h"
#include "zelda3/overworld/terrain_graphics.h"

namespace yaze::cli {
namespace {

using overworld::AreaTileLocation;
using overworld::ResolveAreaTileLocation;
using ::testing::HasSubstr;

// ---------------------------------------------------------------------------
// Address math (no ROM)
// ---------------------------------------------------------------------------

TEST(OverworldTileAddressTest, SmallLightWorldScreenOffsetsIntoWorldGrid) {
  // LW $04 is column 4 of row 0: its 32x32 block starts at world x=128.
  auto location = ResolveAreaTileLocation(0x04, 0x04, 1, 1, 5, 6);
  ASSERT_TRUE(location.ok()) << location.status();
  EXPECT_EQ(location->world, 0);
  EXPECT_EQ(location->area_width, 32);
  EXPECT_EQ(location->area_height, 32);
  EXPECT_EQ(location->screen_id, 0x04);
  EXPECT_EQ(location->screen_x, 5);
  EXPECT_EQ(location->screen_y, 6);
  EXPECT_EQ(location->world_x, 133);
  EXPECT_EQ(location->world_y, 6);
}

TEST(OverworldTileAddressTest, NativeCapacityBoundsParentChildAndLastScreen) {
  const auto child = ResolveAreaTileLocation(0xB1, 0xB0, 2, 2, 40, 2, 192);
  ASSERT_TRUE(child.ok()) << child.status();
  EXPECT_EQ(child->parent_map, 0xB0);
  EXPECT_EQ(child->screen_id, 0xB1);
  EXPECT_EQ(child->screen_x, 8);
  EXPECT_EQ(child->world_x, 40);
  EXPECT_TRUE(ResolveAreaTileLocation(0xBF, 0xBF, 1, 1, 31, 31, 192).ok());
  EXPECT_FALSE(ResolveAreaTileLocation(0xB1, 0xB0, 2, 2, 40, 2).ok());
  EXPECT_FALSE(ResolveAreaTileLocation(0xA0, 0xA0, 1, 1, 0, 0, 160).ok());
  EXPECT_FALSE(ResolveAreaTileLocation(0xC0, 0xC0, 1, 1, 0, 0, 192).ok());
  EXPECT_FALSE(ResolveAreaTileLocation(0xB8, 0xB8, 1, 2, 0, 0, 192).ok());
  EXPECT_FALSE(ResolveAreaTileLocation(0xB1, 0xB0, 2, 2, 64, 2, 192).ok());
}

TEST(OverworldTileAddressTest, LargeDarkWorldAreaUsesParentRelativeCoords) {
  // DW $40 is a 2x2 area; (40,10) lands in child $41 at screen (8,10).
  auto location = ResolveAreaTileLocation(0x40, 0x40, 2, 2, 40, 10);
  ASSERT_TRUE(location.ok()) << location.status();
  EXPECT_EQ(location->world, 1);
  EXPECT_EQ(location->area_width, 64);
  EXPECT_EQ(location->screen_id, 0x41);
  EXPECT_EQ(location->screen_x, 8);
  EXPECT_EQ(location->screen_y, 10);
  EXPECT_EQ(location->world_x, 40);
  EXPECT_EQ(location->world_y, 10);

  // Naming the child screen resolves to the same tile.
  auto via_child = ResolveAreaTileLocation(0x49, 0x40, 2, 2, 40, 10);
  ASSERT_TRUE(via_child.ok()) << via_child.status();
  EXPECT_EQ(via_child->world_x, 40);
  EXPECT_EQ(via_child->world_y, 10);
  EXPECT_EQ(via_child->screen_id, 0x41);
}

TEST(OverworldTileAddressTest, LargeAreaAwayFromOriginAndSpecialWorld) {
  // DW $5B: local $1B = column 3, row 3.
  auto dark = ResolveAreaTileLocation(0x5B, 0x5B, 2, 2, 33, 40);
  ASSERT_TRUE(dark.ok()) << dark.status();
  EXPECT_EQ(dark->screen_id, 0x5B + 1 + 8);
  EXPECT_EQ(dark->world_x, 3 * 32 + 33);
  EXPECT_EQ(dark->world_y, 3 * 32 + 40);

  // SW $81 (Zora's Domain) spans $81,$82,$89,$8A.
  auto special = ResolveAreaTileLocation(0x8A, 0x81, 2, 2, 63, 63);
  ASSERT_TRUE(special.ok()) << special.status();
  EXPECT_EQ(special->world, 2);
  EXPECT_EQ(special->screen_id, 0x8A);
  EXPECT_EQ(special->world_x, 32 + 63);
  EXPECT_EQ(special->world_y, 63);
}

TEST(OverworldTileAddressTest, WideAndTallAreasBoundEachAxis) {
  auto wide = ResolveAreaTileLocation(0x10, 0x10, 2, 1, 63, 31);
  ASSERT_TRUE(wide.ok()) << wide.status();
  EXPECT_EQ(wide->screen_id, 0x11);
  EXPECT_TRUE(absl::IsOutOfRange(
      ResolveAreaTileLocation(0x10, 0x10, 2, 1, 10, 32).status()));

  auto tall = ResolveAreaTileLocation(0x10, 0x10, 1, 2, 31, 63);
  ASSERT_TRUE(tall.ok()) << tall.status();
  EXPECT_EQ(tall->screen_id, 0x18);
  EXPECT_TRUE(absl::IsOutOfRange(
      ResolveAreaTileLocation(0x10, 0x10, 1, 2, 32, 0).status()));
}

TEST(OverworldTileAddressTest, RejectsCoordinatesOutsideTheArea) {
  const auto small = ResolveAreaTileLocation(0x04, 0x04, 1, 1, 32, 0);
  EXPECT_TRUE(absl::IsOutOfRange(small.status()));
  EXPECT_THAT(std::string(small.status().message()),
              HasSubstr("x must be 0-31"));
  EXPECT_TRUE(absl::IsOutOfRange(
      ResolveAreaTileLocation(0x40, 0x40, 2, 2, 64, 0).status()));
  EXPECT_TRUE(absl::IsOutOfRange(
      ResolveAreaTileLocation(0x40, 0x40, 2, 2, 0, -1).status()));
  // 63 is valid on a large area: the old command's fixed bound hid this.
  EXPECT_TRUE(ResolveAreaTileLocation(0x40, 0x40, 2, 2, 63, 63).ok());
}

TEST(OverworldTileAddressTest, RejectsInconsistentParentData) {
  // Parent in another world.
  EXPECT_TRUE(absl::IsFailedPrecondition(
      ResolveAreaTileLocation(0x44, 0x04, 1, 1, 0, 0).status()));
  // Screen outside its parent's span.
  EXPECT_TRUE(absl::IsFailedPrecondition(
      ResolveAreaTileLocation(0x06, 0x04, 1, 1, 0, 0).status()));
  // Wide area in the last column would wrap into the next row.
  EXPECT_TRUE(absl::IsFailedPrecondition(
      ResolveAreaTileLocation(0x07, 0x07, 2, 1, 0, 0).status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      ResolveAreaTileLocation(0xA0, 0xA0, 1, 1, 0, 0).status()));
}

// ---------------------------------------------------------------------------
// Blockset indexing on a synthetic overworld grid
// ---------------------------------------------------------------------------

void FillWorld(zelda3::OverworldBlockset& world) {
  world.assign(0x200, std::vector<uint16_t>(0x200, 0));
}

TEST(OverworldTileBlocksetTest, ReadsAndWritesTheSelectedWorldCell) {
  Rom rom;
  zelda3::Overworld overworld(&rom);
  auto* tiles = overworld.mutable_map_tiles();
  FillWorld(tiles->light_world);
  FillWorld(tiles->dark_world);
  FillWorld(tiles->special_world);
  tiles->light_world[5][6] = 0x0BAD;      // what the old command read for $04
  tiles->light_world[133][6] = 0x0123;    // LW $04 (5,6)
  tiles->dark_world[133][6] = 0x0DDD;     // same cell, wrong world
  tiles->dark_world[40][10] = 0x0255;     // DW $40 (40,10)
  tiles->special_world[95][63] = 0x0777;  // SW $81 area (63,63)

  auto lw = ResolveAreaTileLocation(0x04, 0x04, 1, 1, 5, 6);
  ASSERT_TRUE(lw.ok());
  auto lw_tile = overworld::ReadAreaTile(overworld, *lw);
  ASSERT_TRUE(lw_tile.ok()) << lw_tile.status();
  EXPECT_EQ(*lw_tile, 0x0123);

  auto dw = ResolveAreaTileLocation(0x40, 0x40, 2, 2, 40, 10);
  ASSERT_TRUE(dw.ok());
  EXPECT_EQ(*overworld::ReadAreaTile(overworld, *dw), 0x0255);

  auto sw = ResolveAreaTileLocation(0x81, 0x81, 2, 2, 63, 63);
  ASSERT_TRUE(sw.ok());
  EXPECT_EQ(*overworld::ReadAreaTile(overworld, *sw), 0x0777);

  ASSERT_TRUE(overworld::WriteAreaTile(overworld, *lw, 0x0456).ok());
  EXPECT_EQ(tiles->light_world[133][6], 0x0456);
  EXPECT_EQ(tiles->light_world[5][6], 0x0BAD);
  EXPECT_EQ(tiles->dark_world[133][6], 0x0DDD);
}

TEST(OverworldTileBlocksetTest, UnloadedGridFailsClosed) {
  Rom rom;
  zelda3::Overworld overworld(&rom);
  auto location = ResolveAreaTileLocation(0x00, 0x00, 1, 1, 0, 0);
  ASSERT_TRUE(location.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(
      overworld::ReadAreaTile(overworld, *location).status()));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      overworld::WriteAreaTile(overworld, *location, 1)));
}

// ---------------------------------------------------------------------------
// Tile usage statistics on a synthetic overworld grid
// ---------------------------------------------------------------------------

class OverworldTileUsageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
    auto* tiles = overworld_->mutable_map_tiles();
    FillWorld(tiles->light_world);
    FillWorld(tiles->dark_world);
    FillWorld(tiles->special_world);
    // LW $04 (column 4): ten 0x0123 and three 0x0456.
    for (int i = 0; i < 10; ++i) {
      tiles->light_world[128 + i][0] = 0x0123;
    }
    for (int i = 0; i < 3; ++i) {
      tiles->light_world[128 + i][31] = 0x0456;
    }
    // DW $40: five 0x0123.
    for (int i = 0; i < 5; ++i) {
      tiles->dark_world[i][i] = 0x0123;
    }
    // SW $81 (column 1): two 0x0777.
    tiles->special_world[32][0] = 0x0777;
    tiles->special_world[63][31] = 0x0777;
    // Below the special world's four screen rows: not a screen, not counted.
    tiles->special_world[0][200] = 0x0999;
  }

  static const overworld::TileUsageCount* Find(
      const overworld::TileUsageSummary& summary, uint16_t tile_id) {
    for (const auto& usage : summary.tiles) {
      if (usage.tile_id == tile_id) {
        return &usage;
      }
    }
    return nullptr;
  }

  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
};

TEST_F(OverworldTileUsageTest, CountsEveryScreenOfEveryWorld) {
  auto summary = overworld::CollectTileUsage(*overworld_, {});
  ASSERT_TRUE(summary.ok()) << summary.status();
  EXPECT_EQ(summary->screens_scanned, 0xA0);
  EXPECT_EQ(summary->total_tiles, 0xA0 * 1024);
  ASSERT_EQ(summary->tiles.size(), 4u);

  // Most placed first.
  EXPECT_EQ(summary->tiles[0].tile_id, 0x0000);
  EXPECT_EQ(summary->tiles[0].count, 0xA0 * 1024 - 20);
  EXPECT_EQ(summary->tiles[0].screens, 0xA0);
  EXPECT_EQ(summary->tiles[1].tile_id, 0x0123);
  EXPECT_EQ(summary->tiles[1].count, 15);
  EXPECT_EQ(summary->tiles[1].screens, 2);
  const auto* bush = Find(*summary, 0x0456);
  ASSERT_NE(bush, nullptr);
  EXPECT_EQ(bush->count, 3);
  const auto* cloud = Find(*summary, 0x0777);
  ASSERT_NE(cloud, nullptr);
  EXPECT_EQ(cloud->count, 2);
  EXPECT_EQ(cloud->screens, 1);
  EXPECT_EQ(Find(*summary, 0x0999), nullptr);

  int total = 0;
  for (const auto& usage : summary->tiles) {
    total += usage.count;
  }
  EXPECT_EQ(total, summary->total_tiles);

  ASSERT_EQ(summary->worlds.size(), 3u);
  EXPECT_EQ(summary->worlds[0].screens, 0x40);
  EXPECT_EQ(summary->worlds[0].unique_tiles, 3);
  EXPECT_EQ(summary->worlds[1].unique_tiles, 2);
  EXPECT_EQ(summary->worlds[2].screens, 0x20);
  EXPECT_EQ(summary->worlds[2].total_tiles, 0x20 * 1024);
  EXPECT_EQ(summary->worlds[2].unique_tiles, 2);

  ASSERT_EQ(summary->screens.size(), 0xA0u);
  const auto& lw04 = summary->screens[0x04];
  EXPECT_EQ(lw04.map_id, 0x04);
  EXPECT_EQ(lw04.unique_tiles, 3);
  EXPECT_EQ(lw04.top_tile, 0x0000);
  EXPECT_EQ(lw04.top_tile_count, 1024 - 13);
  EXPECT_EQ(summary->screens[0x81].unique_tiles, 2);
  EXPECT_EQ(summary->screens[0x80].unique_tiles, 1);
}

TEST_F(OverworldTileUsageTest, ScopesToOneScreenOrWorld) {
  overworld::TileUsageQuery screen_query;
  screen_query.map_id = 0x04;
  auto screen = overworld::CollectTileUsage(*overworld_, screen_query);
  ASSERT_TRUE(screen.ok()) << screen.status();
  EXPECT_EQ(screen->screens_scanned, 1);
  EXPECT_EQ(screen->total_tiles, 1024);
  ASSERT_EQ(screen->tiles.size(), 3u);
  EXPECT_EQ(screen->tiles[1].tile_id, 0x0123);
  EXPECT_EQ(screen->tiles[1].count, 10);
  ASSERT_EQ(screen->worlds.size(), 1u);
  EXPECT_EQ(screen->worlds[0].world, 0);

  overworld::TileUsageQuery world_query;
  world_query.world = 1;
  world_query.tile_id = 0x0123;
  auto dark = overworld::CollectTileUsage(*overworld_, world_query);
  ASSERT_TRUE(dark.ok()) << dark.status();
  EXPECT_EQ(dark->screens_scanned, 0x40);
  ASSERT_FALSE(dark->screens.empty());
  EXPECT_EQ(dark->screens[0].map_id, 0x40);
  EXPECT_EQ(dark->screens[0].tile_count, 5);
  EXPECT_EQ(dark->screens[1].tile_count, 0);
  EXPECT_EQ(Find(*dark, 0x0456), nullptr);
}

TEST_F(OverworldTileUsageTest, RejectsInconsistentFiltersAndUnloadedGrids) {
  overworld::TileUsageQuery mismatch;
  mismatch.map_id = 0x04;
  mismatch.world = 1;
  EXPECT_TRUE(absl::IsInvalidArgument(
      overworld::CollectTileUsage(*overworld_, mismatch).status()));

  overworld::TileUsageQuery bad_map;
  bad_map.map_id = 0xA0;
  EXPECT_TRUE(absl::IsInvalidArgument(
      overworld::CollectTileUsage(*overworld_, bad_map).status()));

  Rom rom;
  zelda3::Overworld empty(&rom);
  EXPECT_TRUE(absl::IsFailedPrecondition(
      overworld::CollectTileUsage(empty, {}).status()));
}

// ---------------------------------------------------------------------------
// Argument parsing (fails before the overworld loads)
// ---------------------------------------------------------------------------

TEST(OverworldTileCommandArgsTest, TileStatsRejectsBadFilters) {
  handlers::OverworldTileStatsCommandHandler handler;
  std::string output;
  auto status = handler.Run({"--mock-rom", "--top=-1", "--format=json"},
                            nullptr, &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  status = handler.Run(
      {"--mock-rom", "--screen=0x04", "--map=0x05", "--format=json"}, nullptr,
      &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  status = handler.Run({"--mock-rom", "--map=0xA0", "--format=json"}, nullptr,
                       &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  status = handler.Run({"--mock-rom", "--details", "--format=json"}, nullptr,
                       &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
}

TEST(OverworldTileCommandArgsTest, RejectsNonNumericCoordinates) {
  handlers::OverworldGetTileCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--map=0x40", "--x=abc", "--y=1", "--format=json"},
      nullptr, &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("--x must be a decimal"));
}

TEST(OverworldTileCommandArgsTest, FindTileRejectsIdsPastSixteenBits) {
  // 0x10000 used to truncate to tile 0 and report its matches.
  handlers::OverworldFindTileCommandHandler handler;
  std::string output;
  const auto status = handler.Run(
      {"--mock-rom", "--tile=0x10000", "--format=json"}, nullptr, &output);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()), HasSubstr("Tile ID out of range"));
}

// ---------------------------------------------------------------------------
// ROM-gated checks: set YAZE_TEST_ROM_OOS to a COPY of the Oracle ROM.
// Read-only tests load it in memory; write tests copy it again first.
// ---------------------------------------------------------------------------

class OverworldTileRomTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rom_path_ = test::FindOracleRomFixture();
    if (rom_path_.empty()) {
      GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to a copy of an Oracle ROM";
    }
    ASSERT_TRUE(rom_.LoadFromFile(rom_path_).ok());
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
    ASSERT_TRUE(overworld_->Load(&rom_).ok());
  }

  nlohmann::json RunJson(resources::CommandHandler& handler,
                         const std::vector<std::string>& args, Rom* rom,
                         absl::Status* status_out = nullptr) {
    std::string output;
    auto all_args = args;
    all_args.push_back("--format=json");
    const auto status = handler.Run(all_args, rom, &output);
    if (status_out != nullptr) {
      *status_out = status;
    } else {
      EXPECT_TRUE(status.ok()) << status << "\n" << output;
    }
    return nlohmann::json::parse(output, nullptr, /*allow_exceptions=*/false);
  }

  uint16_t RawWorldTile(int world, int world_x, int world_y) {
    return overworld_->GetMapTiles(world)[world_x][world_y];
  }

  static int HexField(const nlohmann::json& value) {
    return std::stoi(value.get<std::string>(), nullptr, 16);
  }

  void SeedValidationFixture(
      test::native_overworld::NativeDirectoryFixture* fixture) {
    // The shared seed helper takes a 2MiB source. A patched Oracle fixture can
    // append streams/graphics after that prefix. Retain that complete trailer
    // in the resulting model fixture, without copying it over the new tables.
    constexpr size_t kSeedPrefix = 0x200000;
    ASSERT_LE(rom_.size(), fixture->Entry(1).pc_address);
    if (rom_.size() <= kSeedPrefix) {
      ASSERT_NO_FATAL_FAILURE(
          fixture->SeedLoadedLegacyForModelTest(rom_, *overworld_));
    } else {
      const auto& layout = overworld_->resolved_layout();
      ASSERT_LE(layout.tile16_pc.value + 4096 * 8, kSeedPrefix);
      for (const auto& address : layout.tile32_pcs)
        ASSERT_LE(address.value + 4432 * 6, kSeedPrefix);
      ASSERT_LE(layout.terrain_high_table_pc.value + 160 * 3, kSeedPrefix);
      ASSERT_LE(layout.terrain_low_table_pc.value + 160 * 3, kSeedPrefix);
      const auto& original = rom_.vector();
      Rom prefix;
      ASSERT_TRUE(prefix
                      .LoadFromData(std::vector<uint8_t>(
                          original.begin(), original.begin() + kSeedPrefix))
                      .ok());
      ASSERT_NO_FATAL_FAILURE(
          fixture->SeedLoadedLegacyForModelTest(prefix, *overworld_));
      std::copy(original.begin() + kSeedPrefix, original.end(),
                fixture->bytes.begin() + kSeedPrefix);
      ASSERT_TRUE(std::equal(original.begin() + kSeedPrefix, original.end(),
                             fixture->bytes.begin() + kSeedPrefix));
    }
    // Vanilla has 128 item slots; its following code is not a special-world
    // item table. The complete native model needs explicit empty lists for
    // those newly materialized slots, just as the shared helper supplies tail
    // lists when starting from a v3 source.
    const auto version = zelda3::OverworldVersionHelper::GetVersion(rom_);
    if (!zelda3::OverworldVersionHelper::SupportsAreaEnum(version)) {
      const auto root = rom_.ReadLong(zelda3::overworldItemsAddress);
      const auto bank = rom_.ReadByte(zelda3::overworldItemsAddressBank);
      ASSERT_TRUE(root.ok()) << root.status();
      ASSERT_TRUE(bank.ok()) << bank.status();
      std::optional<uint16_t> empty;
      for (int map = 0; map < 128 && !empty; ++map) {
        const auto word = rom_.ReadWord(SnesToPc(*root) + map * 2);
        ASSERT_TRUE(word.ok()) << word.status();
        if (*word < 0x8000)
          continue;
        const auto end = rom_.ReadWord(SnesToPc((*bank << 16) | *word));
        if (end.ok() && *end == 0xFFFF)
          empty = *word;
      }
      ASSERT_TRUE(empty.has_value());
      for (int map = 128; map < fixture->Entry(23).count; ++map)
        fixture->Word(fixture->Entry(23).pc_address + map * 2, *empty);
    }
    // Native phases all have 160 prefix entries. Vanilla's phase-specific
    // 64/144/144 tables need explicit empty lists beyond their real extents.
    const auto sprites = zelda3::GetOverworldSpriteLayout(rom_);
    std::optional<uint16_t> empty_sprites;
    for (int phase = 0; phase < 3 && !empty_sprites; ++phase) {
      for (int map = 0; map < sprites.counts[phase]; ++map) {
        const auto word = rom_.ReadWord(sprites.tables[phase] + map * 2);
        ASSERT_TRUE(word.ok()) << word.status();
        if (*word < 0xC881)
          continue;
        const auto end = rom_.ReadByte(0x40000 + *word);
        if (end.ok() && *end == 0xFF) {
          empty_sprites = *word;
          break;
        }
      }
    }
    ASSERT_TRUE(empty_sprites.has_value());
    for (int phase = 0; phase < 3; ++phase)
      for (int map = sprites.counts[phase]; map < 160; ++map)
        fixture->Word(fixture->Entry(16 + phase).pc_address + map * 2,
                      *empty_sprites);
    fixture->Seal();
  }

  std::string rom_path_;
  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
};

TEST_F(OverworldTileRomTest,
       NativeValidationReadsCanonicalTablesAndSkipsUnmaterializedTail) {
  for (uint16_t capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    test::native_overworld::NativeDirectoryFixture fixture(capacity);
    ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
    fixture.UpgradeToRevision4();
    Rom native;
    ASSERT_TRUE(native.LoadFromData(fixture.bytes).ok());
    OverworldValidateCommandHandler handler;
    absl::Status status;
    const auto json = RunJson(handler, {"--include-tail"}, &native, &status);
    ASSERT_TRUE(status.ok()) << status << "\n" << json.dump();
    ASSERT_TRUE(json.is_object()) << json.dump();
    for (const auto* key :
         {"total_maps", "valid_maps", "invalid_maps", "skipped_maps", "maps"})
      ASSERT_TRUE(json.contains(key)) << key << "\n" << json.dump();
    ASSERT_TRUE(json["maps"].is_array());
    ASSERT_EQ(json["maps"].size(), 192);
    EXPECT_EQ(json["total_maps"], 192);
    EXPECT_EQ(json["valid_maps"], capacity);
    EXPECT_EQ(json["invalid_maps"], 0);
    EXPECT_EQ(json["skipped_maps"], 192 - capacity);
    for (int map : {0xA0, 0xBF}) {
      ASSERT_TRUE(json["maps"][map].is_string());
      const auto row =
          nlohmann::json::parse(json["maps"][map].get<std::string>());
      for (const auto* key : {"skipped", "error", "snes_low", "snes_high",
                              "decomp_low", "decomp_high"})
        ASSERT_TRUE(row.contains(key)) << key << "\n" << row.dump();
      if (capacity == 160) {
        EXPECT_EQ(row["skipped"], true);
        EXPECT_EQ(row["error"], "tail not materialized");
        EXPECT_EQ(row["snes_low"], "0x000000");
      } else {
        EXPECT_EQ(row["skipped"], false);
        EXPECT_EQ(row["decomp_low"], true);
        EXPECT_EQ(row["decomp_high"], true);
        const auto pointer =
            native.ReadLong(fixture.Entry(20).pc_address + map * 3);
        ASSERT_TRUE(pointer.ok());
        EXPECT_EQ(HexField(row["snes_high"]), *pointer);
      }
    }
  }
}

TEST_F(OverworldTileRomTest, LegacyValidationDoesNotInventTailPointerRows) {
  if (overworld_->materialized_map_count() != 160) {
    GTEST_SKIP() << "This control requires a legacy160 fixture";
  }
  OverworldValidateCommandHandler handler;
  absl::Status status;
  const auto json = RunJson(handler, {"--include-tail"}, &rom_, &status);
  ASSERT_TRUE(status.ok()) << status << "\n" << json.dump();
  ASSERT_TRUE(json.is_object()) << json.dump();
  for (const auto* key : {"total_maps", "skipped_maps", "maps"})
    ASSERT_TRUE(json.contains(key)) << key << "\n" << json.dump();
  ASSERT_TRUE(json["maps"].is_array());
  ASSERT_EQ(json["maps"].size(), 192);
  EXPECT_EQ(json["total_maps"], 192);
  EXPECT_EQ(json["skipped_maps"], 32);
  EXPECT_FALSE(json.contains("baseline_maps_checked"));
  for (int map = 0xA0; map < 0xC0; ++map) {
    ASSERT_TRUE(json["maps"][map].is_string());
    const auto row =
        nlohmann::json::parse(json["maps"][map].get<std::string>());
    for (const auto* key : {"skipped", "error", "snes_low", "snes_high"})
      ASSERT_TRUE(row.contains(key)) << key << "\n" << row.dump();
    EXPECT_EQ(row["skipped"], true);
    EXPECT_EQ(row["error"], "tail not materialized");
    EXPECT_EQ(row["snes_low"], "0x000000");
    EXPECT_EQ(row["snes_high"], "0x000000");
  }
}

TEST_F(OverworldTileRomTest,
       NativeValidationBaselineRejectsValidChangedOriginalTerrain) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
  fixture.UpgradeToRevision4();
  Rom native;
  ASSERT_TRUE(native.LoadFromData(fixture.bytes).ok());
  OverworldValidateCommandHandler handler;
  const std::vector<std::string> args = {"--include-tail",
                                         "--baseline=" + rom_path_};
  absl::Status status;
  auto json = RunJson(handler, args, &native, &status);
  ASSERT_TRUE(status.ok()) << status << "\n" << json.dump();
  ASSERT_TRUE(json.is_object()) << json.dump();
  for (const auto* key :
       {"valid_maps", "baseline_maps_checked", "baseline_mismatches"})
    ASSERT_TRUE(json.contains(key)) << key << "\n" << json.dump();
  EXPECT_EQ(json["valid_maps"], 192);
  EXPECT_EQ(json["baseline_maps_checked"], 160);
  EXPECT_EQ(json["baseline_mismatches"], 0);

  // A syntactically valid original-page pointer change leaves every tail page
  // untouched. All streams still decompress, so only the model comparison can
  // reject this semantic loss of saved terrain.
  zelda3::Overworld before(&native);
  ASSERT_TRUE(before.Load(&native).ok());
  fixture.Long(fixture.Entry(20).pc_address, fixture.Snes(0x240010));
  fixture.Long(fixture.Entry(21).pc_address, fixture.Snes(0x240010));
  fixture.Seal();
  ASSERT_TRUE(native.LoadFromData(fixture.bytes).ok());
  zelda3::Overworld changed(&native);
  ASSERT_TRUE(changed.Load(&native).ok());
  const auto& old_special = before.GetMapTiles(2);
  const auto& new_special = changed.GetMapTiles(2);
  for (int map = 0xA0; map < 0xC0; ++map) {
    const int left = (map % 8) * 32;
    const int top = ((map % 64) / 8) * 32;
    for (int x = left; x < left + 32; ++x)
      for (int y = top; y < top + 32; ++y)
        EXPECT_EQ(old_special[x][y], new_special[x][y]);
  }
  json = RunJson(handler, args, &native, &status);
  ASSERT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("baseline terrain changed"));
  ASSERT_TRUE(json.is_object()) << json.dump();
  for (const auto* key :
       {"valid_maps", "baseline_maps_checked", "baseline_mismatches"})
    ASSERT_TRUE(json.contains(key)) << key << "\n" << json.dump();
  EXPECT_EQ(json["valid_maps"], 192);
  EXPECT_EQ(json["baseline_maps_checked"], 160);
  EXPECT_GT(json["baseline_mismatches"].get<int>(), 0);
}

TEST_F(OverworldTileRomTest, GetTileMatchesEditorGridForDarkAndLightWorld) {
  handlers::OverworldGetTileCommandHandler handler;

  // DW $40 sits at the Dark World origin: area (40,10) is canvas (40,10).
  // Decimal and 0x-hex coordinates are the same value.
  for (const std::string x_arg : {"--x=40", "--x=0x28"}) {
    auto json = RunJson(handler, {"--map=0x40", x_arg, "--y=10"}, &rom_);
    ASSERT_TRUE(json.is_object());
    auto& tile = json["Overworld Tile"];
    EXPECT_EQ(tile["x"], 40);
    EXPECT_EQ(tile["world"], "Dark");
    EXPECT_EQ(HexField(tile["tile_id"]), RawWorldTile(1, 40, 10));
  }

  // LW $04: column 4, so canvas x = 128 + area x.
  auto location = overworld::ResolveAreaTileForMap(*overworld_, 0x04, 10, 10);
  ASSERT_TRUE(location.ok()) << location.status();
  auto json = RunJson(handler, {"--map=0x04", "--x=10", "--y=10"}, &rom_);
  auto& tile = json["Overworld Tile"];
  EXPECT_EQ(tile["world_x"], location->world_x);
  EXPECT_EQ(HexField(tile["tile_id"]),
            RawWorldTile(0, location->world_x, location->world_y));
  if (location->parent_map == 0x04) {
    EXPECT_EQ(location->world_x, 138);
  }
}

TEST_F(OverworldTileRomTest, ChildScreenAndBoundsFollowAreaSize) {
  handlers::OverworldGetTileCommandHandler handler;
  const auto* dw40 = overworld_->overworld_map(0x40);
  ASSERT_NE(dw40, nullptr);
  const auto [columns, rows] = overworld::AreaScreenSpan(*dw40);
  const int width = columns * 32;
  const int height = rows * 32;

  // The last valid tile works; one past it fails with OutOfRange.
  auto edge = RunJson(handler,
                      {"--map=0x40", "--x=" + std::to_string(width - 1),
                       "--y=" + std::to_string(height - 1)},
                      &rom_);
  EXPECT_EQ(HexField(edge["Overworld Tile"]["tile_id"]),
            RawWorldTile(1, width - 1, height - 1));
  absl::Status status;
  RunJson(handler, {"--map=0x40", "--x=" + std::to_string(width), "--y=0"},
          &rom_, &status);
  EXPECT_TRUE(absl::IsOutOfRange(status)) << status;

  if (columns == 2) {
    // The child screen resolves to the parent: same tile, same address.
    auto parent = RunJson(handler, {"--map=0x40", "--x=40", "--y=10"}, &rom_);
    auto child = RunJson(handler, {"--map=0x41", "--x=40", "--y=10"}, &rom_);
    EXPECT_EQ(parent["Overworld Tile"]["tile_id"],
              child["Overworld Tile"]["tile_id"]);
    EXPECT_EQ(child["Overworld Tile"]["parent_area"], "0x40");
    EXPECT_EQ(child["Overworld Tile"]["screen"], "0x41");
    EXPECT_EQ(child["Overworld Tile"]["screen_x"], 8);
  }
}

TEST_F(OverworldTileRomTest, FindTileMatchesFeedBackIntoGetTile) {
  handlers::OverworldGetTileCommandHandler get_handler;
  handlers::OverworldFindTileCommandHandler find_handler;
  auto probe = RunJson(get_handler, {"--map=0x40", "--x=40", "--y=10"}, &rom_);
  const std::string tile_id =
      probe["Overworld Tile"]["tile_id"].get<std::string>();

  auto found =
      RunJson(find_handler, {"--tile=" + tile_id, "--map=0x41"}, &rom_);
  auto& matches = found["Overworld Tile Search"]["matches"];
  ASSERT_TRUE(matches.is_array());
  bool saw_probe = false;
  for (auto& match : matches) {
    const int x = match["x"].get<int>();
    const int y = match["y"].get<int>();
    auto again =
        RunJson(get_handler,
                {"--map=" + match["map_id"].get<std::string>(),
                 "--x=" + std::to_string(x), "--y=" + std::to_string(y)},
                &rom_);
    EXPECT_EQ(again["Overworld Tile"]["tile_id"], tile_id);
    saw_probe |= (match["map_id"] == "0x41" && x == 40 && y == 10);
  }
  const auto* dw40 = overworld_->overworld_map(0x40);
  if (overworld::AreaScreenSpan(*dw40).first == 2) {
    EXPECT_TRUE(saw_probe);
  }
}

TEST_F(OverworldTileRomTest, TileStatsCountsMatchTheWorldGrid) {
  handlers::OverworldTileStatsCommandHandler handler;
  auto json = RunJson(handler, {"--top=0"}, &rom_);
  auto& stats = json["Overworld Tile Statistics"];
  ASSERT_TRUE(stats.is_object()) << json.dump();
  const int screens = overworld_->materialized_map_count();
  EXPECT_EQ(stats["screens_scanned"], screens);
  EXPECT_EQ(stats["total_tiles"], screens * 1024);
  auto& counts = stats["tile_counts"];
  ASSERT_TRUE(counts.is_array());
  EXPECT_EQ(stats["unique_tiles"].get<int>(), static_cast<int>(counts.size()));
  EXPECT_GT(counts.size(), 1u) << "a real overworld uses many tile16s";
  int total = 0;
  int previous = counts[0]["count"].get<int>();
  for (auto& entry : counts) {
    const int count = entry["count"].get<int>();
    EXPECT_LE(count, previous);
    previous = count;
    total += count;
  }
  EXPECT_EQ(total, screens * 1024);

  // One screen: recount it straight from the grid.
  auto screen = RunJson(handler, {"--map=0x40", "--top=0"}, &rom_);
  auto& screen_stats = screen["Overworld Tile Statistics"];
  EXPECT_EQ(screen_stats["screens_scanned"], 1);
  std::map<uint16_t, int> expected;
  for (int x = 0; x < 32; ++x) {
    for (int y = 0; y < 32; ++y) {
      ++expected[RawWorldTile(1, x, y)];
    }
  }
  auto& screen_counts = screen_stats["tile_counts"];
  ASSERT_EQ(screen_counts.size(), expected.size());
  for (auto& entry : screen_counts) {
    const int tile_id = HexField(entry["tile_id"]);
    EXPECT_EQ(entry["count"].get<int>(), expected[tile_id]) << tile_id;
  }

  // --tile agrees with overworld-find-tile on the same screen.
  const std::string top_tile =
      screen_stats["screens"][0]["top_tile"].get<std::string>();
  auto tile = RunJson(handler, {"--map=0x40", "--tile=" + top_tile}, &rom_);
  handlers::OverworldFindTileCommandHandler find_handler;
  auto found =
      RunJson(find_handler, {"--tile=" + top_tile, "--map=0x40"}, &rom_);
  EXPECT_EQ(tile["Overworld Tile Statistics"]["tile"]["count"],
            found["Overworld Tile Search"]["matches_found"]);
  EXPECT_EQ(tile["Overworld Tile Statistics"]["screens"][0]["tile_count"],
            found["Overworld Tile Search"]["matches_found"]);
}

TEST_F(OverworldTileRomTest, TileStatsDetailsGroupScreensByGraphicsSource) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
  fixture.UpgradeToRevision4();
  fixture.Seal();
  Rom seeded;
  ASSERT_TRUE(seeded.LoadFromData(fixture.bytes).ok());
  zelda3::Overworld world(&seeded);
  ASSERT_TRUE(world.Load(&seeded).ok());

  // A Tile16 placed in two special-world areas, with a quadrant in a slot
  // below the animated slot 7.
  overworld::TileUsageQuery special;
  special.world = 2;
  const auto usage_or = overworld::CollectTileUsage(world, special);
  ASSERT_TRUE(usage_or.ok()) << usage_or.status();
  const auto& usage = *usage_or;
  uint16_t tile_id = 0;
  int quadrant = -1, slot = -1, area_a = -1, area_b = -1, screen_a = -1,
      screen_b = -1;
  for (size_t i = 0; i < usage.tiles.size() && i < 32 && area_b < 0; ++i) {
    const auto& tile = world.tiles16()[usage.tiles[i].tile_id];
    const gfx::TileInfo infos[] = {tile.tile0_, tile.tile1_, tile.tile2_,
                                   tile.tile3_};
    int q = 0;
    while (q < 4 && infos[q].id_ / 64 >= 7)
      ++q;
    if (q == 4)
      continue;
    auto query = special;
    query.tile_id = usage.tiles[i].tile_id;
    const auto one = overworld::CollectTileUsage(world, query);
    ASSERT_TRUE(one.ok()) << one.status();
    for (const auto& screen : one->screens) {
      if (screen.tile_count == 0)
        continue;
      const int area = world.overworld_map(screen.map_id)->parent();
      if (area_a < 0) {
        area_a = area;
        screen_a = screen.map_id;
      } else if (area != area_a) {
        area_b = area;
        screen_b = screen.map_id;
        break;
      }
    }
    if (area_b >= 0) {
      tile_id = usage.tiles[i].tile_id;
      quadrant = q;
      slot = infos[q].id_ / 64;
    } else {
      area_a = screen_a = -1;
    }
  }
  ASSERT_GE(area_b, 0) << "no Tile16 placed in two areas";

  const auto& groups = fixture.Entry(6);
  auto details_with_sheets = [&](int sheet_a, int sheet_b) {
    std::copy_n(fixture.bytes.begin() + groups.pc_address + area_a * 8, 8,
                fixture.bytes.begin() + groups.pc_address + area_b * 8);
    fixture.bytes[groups.pc_address + area_a * 8 + slot] = sheet_a;
    fixture.bytes[groups.pc_address + area_b * 8 + slot] = sheet_b;
    fixture.Seal();
    Rom rom;
    EXPECT_TRUE(rom.LoadFromData(fixture.bytes).ok());
    handlers::OverworldTileStatsCommandHandler handler;
    return RunJson(
        handler, {"--tile=" + absl::StrFormat("0x%04X", tile_id), "--details"},
        &rom);
  };
  const std::string name_a = absl::StrFormat("0x%02X", screen_a);
  const std::string name_b = absl::StrFormat("0x%02X", screen_b);
  auto group_of = [](const nlohmann::json& tile, const std::string& screen) {
    for (const auto& group : tile["source_groups"]) {
      for (const auto& listed : group["screens"]) {
        if (listed == screen)
          return group;
      }
    }
    return nlohmann::json();
  };

  // Control: same sheets, so both areas resolve to one source group.
  auto same = details_with_sheets(0x10, 0x10);
  auto& same_tile = same["Overworld Tile Statistics"]["tile"];
  ASSERT_TRUE(same_tile.contains("source_groups")) << same.dump();
  int listed = 0;
  for (const auto& group : same_tile["source_groups"])
    listed += static_cast<int>(group["screens"].size());
  EXPECT_EQ(listed, same_tile["screens"].get<int>());
  ASSERT_FALSE(group_of(same_tile, name_a).is_null());
  EXPECT_EQ(group_of(same_tile, name_a), group_of(same_tile, name_b));

  const auto& tile = world.tiles16()[tile_id];
  const gfx::TileInfo infos[] = {tile.tile0_, tile.tile1_, tile.tile2_,
                                 tile.tile3_};
  const auto types = world.all_tiles_types();
  for (int q = 0; q < 4; ++q) {
    const auto& detail = same_tile["quadrants"][q];
    EXPECT_EQ(HexField(detail["word"]), gfx::TileInfoToWord(infos[q]));
    EXPECT_EQ(HexField(detail["tile8_id"]), infos[q].id_);
    EXPECT_EQ(detail["palette"], infos[q].palette_);
    EXPECT_EQ(detail["priority"], infos[q].over_);
    EXPECT_EQ(detail["flip_x"], infos[q].horizontal_mirror_);
    EXPECT_EQ(detail["flip_y"], infos[q].vertical_mirror_);
    if (infos[q].id_ < types.size())
      EXPECT_EQ(HexField(detail["tile_type"]), types[infos[q].id_]);
    else
      EXPECT_EQ(detail["tile_type"], "unresolved");
  }

  // Same words, different sheet in one area: the areas separate.
  auto split = details_with_sheets(0x10, 0x11);
  auto& split_tile = split["Overworld Tile Statistics"]["tile"];
  EXPECT_EQ(split_tile["quadrants"], same_tile["quadrants"]);
  const auto group_a = group_of(split_tile, name_a);
  const auto group_b = group_of(split_tile, name_b);
  ASSERT_FALSE(group_a.is_null());
  ASSERT_FALSE(group_b.is_null());
  EXPECT_NE(group_a, group_b);
  EXPECT_EQ(group_a["quadrants"][quadrant]["sheet"], 0x10);
  EXPECT_EQ(group_b["quadrants"][quadrant]["sheet"], 0x11);

  // Without --details the output is the existing count-only shape.
  fixture.Seal();
  Rom plain_rom;
  ASSERT_TRUE(plain_rom.LoadFromData(fixture.bytes).ok());
  handlers::OverworldTileStatsCommandHandler handler;
  auto plain = RunJson(
      handler, {"--tile=" + absl::StrFormat("0x%04X", tile_id)}, &plain_rom);
  auto expected = split;
  for (const char* key : {"quadrants", "graphics_preview", "source_group_count",
                          "source_groups"}) {
    expected["Overworld Tile Statistics"]["tile"].erase(key);
  }
  EXPECT_EQ(plain, expected);
}

TEST_F(OverworldTileRomTest, TileStatsAndFindTileCoverTheNativeTail) {
  for (uint16_t capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    test::native_overworld::NativeDirectoryFixture fixture(capacity);
    ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
    fixture.UpgradeToRevision4();
    Rom native;
    ASSERT_TRUE(native.LoadFromData(fixture.bytes).ok());
    zelda3::Overworld world(&native);
    ASSERT_TRUE(world.Load(&native).ok());
    ASSERT_EQ(world.materialized_map_count(), capacity);

    handlers::OverworldTileStatsCommandHandler stats_handler;
    auto all = RunJson(stats_handler, {"--top=0"}, &native);
    auto& stats = all["Overworld Tile Statistics"];
    ASSERT_TRUE(stats.is_object()) << all.dump();
    EXPECT_EQ(stats["screens_scanned"], capacity);
    EXPECT_EQ(stats["total_tiles"], capacity * 1024);
    ASSERT_EQ(stats["screens"].size(), capacity);
    EXPECT_EQ(stats["screens"].back()["map_id"],
              absl::StrFormat("0x%02X", capacity - 1));
    ASSERT_EQ(stats["worlds"].size(), 3u);
    EXPECT_EQ(stats["worlds"][2]["screens"], capacity - 0x80);

    // $BF is the last tail screen: local 0x3F at grid (224,224).
    const uint16_t tile_id = world.GetMapTiles(2)[224][224];
    const std::string tile = absl::StrFormat("0x%04X", tile_id);
    handlers::OverworldFindTileCommandHandler find_handler;
    absl::Status stats_status;
    absl::Status find_status;
    auto one = RunJson(stats_handler, {"--map=0xBF", "--tile=" + tile}, &native,
                       &stats_status);
    auto found = RunJson(find_handler, {"--map=0xBF", "--tile=" + tile},
                         &native, &find_status);
    if (capacity == 160) {
      EXPECT_FALSE(stats_status.ok());
      EXPECT_FALSE(find_status.ok());
      continue;
    }
    ASSERT_TRUE(stats_status.ok()) << stats_status;
    ASSERT_TRUE(find_status.ok()) << find_status;
    auto& screen = one["Overworld Tile Statistics"];
    EXPECT_EQ(screen["screens_scanned"], 1);
    const int count = screen["tile"]["count"].get<int>();
    EXPECT_GE(count, 1);
    EXPECT_EQ(found["Overworld Tile Search"]["matches_found"], count);
    for (auto& match : found["Overworld Tile Search"]["matches"]) {
      EXPECT_EQ(match["map_id"], "0xBF");
    }
  }
}

// Catalog one area into a fresh file; `out` receives the export on success.
absl::Status RunTerrainCatalog(Rom* rom, const std::string& map,
                               nlohmann::json* out = nullptr) {
  const auto path = test::UniqueTempPath("terrain_catalog", ".json");
  handlers::OverworldTerrainCatalogCommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--map=" + map, "--out=" + path.string(), "--format=json"},
                  rom, &output);
  if (status.ok() && out != nullptr) {
    std::ifstream file(path);
    *out = nlohmann::json::parse(file);
  }
  std::error_code ec;
  std::filesystem::remove(path, ec);
  return status;
}

TEST_F(OverworldTileRomTest, TerrainCatalogCoversTheNativeTail) {
  // A standard ROM's $A0-$BF slots are blank fill, not catalog data.
  ASSERT_FALSE(rom::IsNativeSpecialOverworldLayout(rom_.InspectLyraLayout()));
  auto status = RunTerrainCatalog(&rom_, "0xA0");
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_THAT(std::string(status.message()), HasSubstr("160-map overworld"));

  for (uint16_t capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    test::native_overworld::NativeDirectoryFixture fixture(capacity);
    ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
    fixture.UpgradeToRevision4();
    Rom native;
    ASSERT_TRUE(native.LoadFromData(fixture.bytes).ok());
    nlohmann::json catalog;
    status = RunTerrainCatalog(&native, "0xBF", &catalog);
    if (capacity == 160) {
      EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
      continue;
    }
    ASSERT_TRUE(status.ok()) << status;
    zelda3::Overworld world(&native);
    ASSERT_TRUE(world.Load(&native).ok());
    EXPECT_EQ(catalog["parent_area"], 0xBF);
    EXPECT_EQ(catalog["profile"]["world"], 2);
    // $BF is local 0x3F of the special world: grid origin (224,224).
    const auto& grid = world.GetMapTiles(2);
    int cells = 0;
    for (const auto& record : catalog["records"]) {
      for (const auto& at : record["occurrences"]) {
        EXPECT_EQ(record["tile_id"],
                  grid[224 + at[0].get<int>()][224 + at[1].get<int>()]);
        ++cells;
      }
      for (const auto& quadrant : record["quadrants"]) {
        EXPECT_NE(quadrant["graphics_source"]["kind"], "none");
      }
    }
    EXPECT_EQ(cells, 32 * 32);
  }
}

TEST_F(OverworldTileRomTest, TerrainCatalogExportsAnExplicitNativeSheetZero) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
  fixture.UpgradeToRevision4();
  Rom native;
  ASSERT_TRUE(native.LoadFromData(fixture.bytes).ok());
  zelda3::Overworld world(&native);
  ASSERT_TRUE(world.Load(&native).ok());
  const auto& tile = world.tiles16()[world.GetMapTiles(2)[224][224]];
  // A main slot below the animated slot 7 that $BF's first Tile16 reads.
  int slot = -1;
  for (const auto& quadrant :
       {tile.tile0_, tile.tile1_, tile.tile2_, tile.tile3_}) {
    if (quadrant.id_ / 64 < 7) {
      slot = quadrant.id_ / 64;
      break;
    }
  }
  ASSERT_GE(slot, 0);

  // Control: the seeded custom tile group names a nonzero sheet.
  nlohmann::json control;
  ASSERT_TRUE(RunTerrainCatalog(&native, "0xBF", &control).ok());
  ASSERT_NE(control["profile"]["sheets"][slot], 0);

  // Tile group ID $00 is an explicit sheet 0 ($FF would inherit).
  fixture.bytes[fixture.Entry(6).pc_address + 0xBF * 8 + slot] = 0;
  fixture.Seal();
  Rom zeroed;
  ASSERT_TRUE(zeroed.LoadFromData(fixture.bytes).ok());
  nlohmann::json catalog;
  const auto status = RunTerrainCatalog(&zeroed, "0xBF", &catalog);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(catalog["profile"]["sheets"][slot], 0);

  auto data = std::make_unique<zelda3::GameData>();
  zelda3::LoadOptions options;
  options.expand_rom = false;
  ASSERT_TRUE(zelda3::LoadGameData(zeroed, *data, options).ok());
  int checked = 0;
  for (const auto& record : catalog["records"]) {
    for (const auto& quadrant : record["quadrants"]) {
      const int id = quadrant["tile8_id"];
      if (id / 64 != slot)
        continue;
      EXPECT_EQ(quadrant["graphics_source"]["kind"], "sheet");
      EXPECT_EQ(quadrant["graphics_source"]["sheet"], 0);
      const auto mask = zelda3::Tile8SourceTransparency(
          data->graphics_buffer, 0, id, quadrant["flip_x"].get<bool>(),
          quadrant["flip_y"].get<bool>());
      ASSERT_TRUE(mask.ok()) << mask.status();
      EXPECT_EQ(quadrant["transparent_pixels"],
                std::count(mask->begin(), mask->end(), 1));
      ++checked;
    }
  }
  EXPECT_GT(checked, 0);
}

TEST_F(OverworldTileRomTest, TerrainCatalogRefusesACharacterWithoutASource) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
  fixture.UpgradeToRevision4();
  int slot = -1;
  {
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
    zelda3::Overworld world(&rom);
    ASSERT_TRUE(world.Load(&rom).ok());
    const auto& tile = world.tiles16()[world.GetMapTiles(2)[224][224]];
    for (const auto& quadrant :
         {tile.tile0_, tile.tile1_, tile.tile2_, tile.tile3_}) {
      if (quadrant.id_ / 64 < 7) {
        slot = quadrant.id_ / 64;
        break;
      }
    }
  }
  ASSERT_GE(slot, 0);

  // Tile group ID $FE names a sheet past the decoded graphics store.
  fixture.bytes[fixture.Entry(6).pc_address + 0xBF * 8 + slot] = 0xFE;
  fixture.Seal();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  {
    auto data = std::make_unique<zelda3::GameData>();
    zelda3::LoadOptions options;
    options.expand_rom = false;
    ASSERT_TRUE(zelda3::LoadGameData(rom, *data, options).ok());
    ASSERT_LT(data->graphics_buffer.size(), 0xFFu * 0x1000);
    auto world = std::make_unique<zelda3::Overworld>(&rom);
    world->SetGameData(data.get());
    ASSERT_TRUE(world->Load(&rom).ok());
    ASSERT_TRUE(world->EnsureMapBuilt(0xBF).ok());
    const auto* map = world->overworld_map(0xBF);
    ASSERT_EQ(map->GraphicsSourceForCharacter(slot * 64).kind,
              zelda3::OverworldMap::GraphicsCharacterSource::Kind::kNone);
  }
  const auto status = RunTerrainCatalog(&rom, "0xBF");
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr(absl::StrFormat(
                  "from slot %d, which has no graphics source", slot)));
}

TEST_F(OverworldTileRomTest, TerrainCatalogAdmitsOnlyPagesThatMatchTheArea) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
  fixture.UpgradeToRevision4();
  // Appearance flags are valid only from $80; find a multi-page special area.
  int parent = -1;
  int page = -1;
  for (int map = 0x80; map < 0xA0 && page < 0; ++map) {
    const auto* area = overworld_->overworld_map(map);
    if (area->parent() != map)
      continue;
    parent = map;
    if (area->area_size() == zelda3::AreaSizeEnum::LargeArea ||
        area->area_size() == zelda3::AreaSizeEnum::WideArea) {
      page = map + 1;
    } else if (area->area_size() == zelda3::AreaSizeEnum::TallArea) {
      page = map + 8;
    }
  }
  ASSERT_GE(page, 0);
  const std::string map = absl::StrFormat("0x%02X", page);
  auto catalog_of = [&](nlohmann::json* out) {
    fixture.Seal();
    Rom rom;
    EXPECT_TRUE(rom.LoadFromData(fixture.bytes).ok());
    return RunTerrainCatalog(&rom, map, out);
  };
  nlohmann::json control;
  ASSERT_TRUE(catalog_of(&control).ok());

  // The page takes its own rows, copied from the parent: same build, same
  // profile as the inherited control.
  for (uint16_t id : {1, 2, 3, 4, 5, 6, 26, 27, 31}) {
    const auto& entry = fixture.Entry(id);
    std::copy_n(
        fixture.bytes.begin() + entry.pc_address + parent * entry.stride,
        entry.stride,
        fixture.bytes.begin() + entry.pc_address + page * entry.stride);
  }
  fixture.bytes[fixture.Entry(410).pc_address + page] = 1;
  nlohmann::json same;
  const auto same_status = catalog_of(&same);
  ASSERT_TRUE(same_status.ok()) << same_status;
  EXPECT_EQ(same["profile_fingerprint"], control["profile_fingerprint"]);
  EXPECT_EQ(same["records"], control["records"]);

  // A different main sheet on that page is not described by the area profile.
  const uint32_t slot0 = fixture.Entry(6).pc_address;
  fixture.bytes[slot0 + parent * 8] = 0x10;
  fixture.bytes[slot0 + page * 8] = 0x11;
  auto status = catalog_of(nullptr);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("differs from the parent's"));

  // Point sheet $11 at sheet $10's data: the page now builds pixel-identical
  // graphics, yet its characters still come from another sheet.
  {
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
    auto data = std::make_unique<zelda3::GameData>();
    zelda3::LoadOptions options;
    options.expand_rom = false;
    ASSERT_TRUE(zelda3::LoadGameData(rom, *data, options).ok());
    const auto& table = data->gfx_sheet_table;
    ASSERT_GT(table.count, 0x11u);
    for (uint32_t pc : {table.bank_pc, table.high_pc, table.low_pc}) {
      fixture.bytes[pc + 0x11] = fixture.bytes[pc + 0x10];
    }
  }
  fixture.Seal();
  {
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());
    auto data = std::make_unique<zelda3::GameData>();
    zelda3::LoadOptions options;
    options.expand_rom = false;
    ASSERT_TRUE(zelda3::LoadGameData(rom, *data, options).ok());
    auto world_owner = std::make_unique<zelda3::Overworld>(&rom);
    auto& world = *world_owner;
    world.SetGameData(data.get());
    ASSERT_TRUE(world.Load(&rom).ok());
    ASSERT_TRUE(world.EnsureMapBuilt(parent).ok());
    ASSERT_TRUE(world.EnsureMapBuilt(page).ok());
    const auto* a = world.overworld_map(parent);
    const auto* b = world.overworld_map(page);
    ASSERT_EQ(a->static_graphics(0), 0x10);
    ASSERT_EQ(b->static_graphics(0), 0x11);
    ASSERT_EQ(a->current_graphics(), b->current_graphics());
    ASSERT_TRUE(a->current_palette() == b->current_palette());
  }
  status = catalog_of(nullptr);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(std::string(status.message()),
              HasSubstr("differs from the parent's"));
}

std::filesystem::path CopyRomTo(const std::string& source,
                                const std::filesystem::path& dir) {
  std::filesystem::create_directories(dir);
  const auto target = dir / "work.sfc";
  std::filesystem::copy_file(source, target,
                             std::filesystem::copy_options::overwrite_existing);
  return target;
}

std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

TEST_F(OverworldTileRomTest, SetTileDryRunLeavesRomAndFileUnchanged) {
  const auto dir = test::UniqueTempPath("ow_tile_dry");
  const auto work = CopyRomTo(rom_path_, dir);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(work.string()).ok());
  const auto memory_before = rom.vector();
  const auto file_before = ReadBytes(work);

  const uint16_t current = RawWorldTile(1, 40, 10);
  const uint16_t replacement = current == 0x0001 ? 0x0002 : 0x0001;
  handlers::OverworldSetTileCommandHandler handler;
  auto json = RunJson(handler,
                      {"--map=0x40", "--x=40", "--y=10",
                       absl::StrFormat("--tile=0x%04X", replacement)},
                      &rom);
  auto& write = json["Overworld Tile Write"];
  EXPECT_EQ(write["mode"], "dry-run");
  EXPECT_EQ(write["write_status"], "dry-run");
  EXPECT_EQ(write["verified_grid"], true);
  EXPECT_EQ(write["bytes_outside_save_ranges"], 0);
  ASSERT_TRUE(write["changed_bytes"].is_number());
  EXPECT_GT(write["changed_bytes"].get<int>(), 0);
  EXPECT_EQ(rom.vector(), memory_before);
  EXPECT_EQ(ReadBytes(work), file_before);
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_F(OverworldTileRomTest, SetTileWriteChangesOnlyTheRequestedTile) {
  const auto dir = test::UniqueTempPath("ow_tile_write");
  const auto work = CopyRomTo(rom_path_, dir);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(work.string()).ok());

  auto location = overworld::ResolveAreaTileForMap(*overworld_, 0x04, 10, 10);
  ASSERT_TRUE(location.ok()) << location.status();
  const uint16_t current =
      RawWorldTile(0, location->world_x, location->world_y);
  const uint16_t replacement = current == 0x0001 ? 0x0002 : 0x0001;

  handlers::OverworldSetTileCommandHandler handler;
  auto json =
      RunJson(handler,
              {"--map=0x04", "--x=10", "--y=10",
               absl::StrFormat("--tile=0x%04X", replacement), "--write"},
              &rom);
  auto& write = json["Overworld Tile Write"];
  ASSERT_EQ(write["save_status"], "saved") << json.dump();
  EXPECT_EQ(write["verified_reload"], true);

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(work.string()).ok());
  zelda3::Overworld after(&reopened);
  ASSERT_TRUE(after.Load(&reopened).ok());
  const auto want = overworld_->map_tiles();
  const auto got = after.map_tiles();
  int differences = 0;
  for (int x = 0; x < 256; ++x) {
    for (int y = 0; y < 256; ++y) {
      differences += want.light_world[x][y] != got.light_world[x][y];
      differences += want.dark_world[x][y] != got.dark_world[x][y];
      differences += want.special_world[x][y] != got.special_world[x][y];
    }
  }
  EXPECT_EQ(differences, 1);
  EXPECT_EQ(got.light_world[location->world_x][location->world_y], replacement);
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

TEST_F(OverworldTileRomTest, SetTileWriteRefusesOracleCheckoutRom) {
  const auto root = test::UniqueTempPath("ow_tile_guard");
  const auto work = CopyRomTo(rom_path_, root / "Roms");
  std::ofstream(root / "Oracle_main.asm") << "; main\n";
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(work.string()).ok());
  const auto file_before = ReadBytes(work);

  handlers::OverworldSetTileCommandHandler handler;
  absl::Status status;
  RunJson(handler, {"--map=0x40", "--x=1", "--y=1", "--tile=0x0001", "--write"},
          &rom, &status);
  EXPECT_TRUE(absl::IsPermissionDenied(status)) << status;
  EXPECT_EQ(ReadBytes(work), file_before);
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

TEST_F(OverworldTileRomTest,
       NativeSetTileUsesNormalWriterAndPreservesDryRunFailureAndReopen) {
  const auto flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = flags; });
  core::FeatureFlags::get().overworld.kSaveOverworldMaps = true;
  for (uint16_t capacity : {160, 192}) {
    SCOPED_TRACE(capacity);
    test::native_overworld::NativeDirectoryFixture fixture(capacity);
    fixture.SeedLoadedLegacyForModelTest(rom_, *overworld_);
    fixture.UpgradeToRevision4();
    if (capacity == 192) {
      for (int map : {0xB0, 0xB1, 0xB8, 0xB9}) {
        fixture.bytes[fixture.Entry(7).pc_address + map] = 0xB0;
        fixture.bytes[fixture.Entry(30).pc_address + map] = 1;
      }
      fixture.Seal();
    }
    const auto dir = test::UniqueTempPath("ow_native_tile");
    std::filesystem::create_directories(dir);
    auto cleanup = absl::MakeCleanup([&] {
      std::error_code ec;
      std::filesystem::remove_all(dir, ec);
    });
    const auto path = dir / "edit.sfc";
    {
      std::ofstream file(path, std::ios::binary);
      file.write(reinterpret_cast<const char*>(fixture.bytes.data()),
                 fixture.bytes.size());
    }
    Rom rom;
    ASSERT_TRUE(rom.LoadFromFile(path.string()).ok());
    zelda3::Overworld before(&rom);
    ASSERT_TRUE(before.Load(&rom).ok());
    const int map = capacity == 192 ? 0xB1 : 0x00;
    const int x = capacity == 192 ? 40 : 1;
    const auto location = overworld::ResolveAreaTileForMap(before, map, x, 2);
    ASSERT_TRUE(location.ok()) << location.status();
    const auto old_tile = overworld::ReadAreaTile(before, *location);
    ASSERT_TRUE(old_tile.ok());
    const uint16_t replacement = *old_tile == 1 ? 2 : 1;
    std::vector<std::string> args = {
        absl::StrFormat("--map=%02X", map), absl::StrFormat("--x=%d", x),
        "--y=2", absl::StrFormat("--tile=%03X", replacement)};
    handlers::OverworldGetTileCommandHandler get;
    const auto read = RunJson(get, {args[0], args[1], "--y=2"}, &rom);
    ASSERT_TRUE(read.is_object()) << read.dump();
    EXPECT_EQ(HexField(read["Overworld Tile"]["tile_id"]), *old_tile);
    if (capacity == 192) {
      const auto last = RunJson(get, {"--map=BF", "--x=31", "--y=31"}, &rom);
      EXPECT_EQ(HexField(last["Overworld Tile"]["map_id"]), 0xBF);
    }
    handlers::OverworldSetTileCommandHandler set;
    const auto initial = rom.vector();
    const bool dirty = rom.dirty();
    absl::Status status;
    const auto dry = RunJson(set, args, &rom, &status);
    ASSERT_TRUE(status.ok()) << status << dry.dump();
    EXPECT_EQ(dry["Overworld Tile Write"]["write_status"], "dry-run");
    EXPECT_EQ(rom.vector(), initial);
    EXPECT_EQ(rom.dirty(), dirty);
    EXPECT_EQ(ReadBytes(path), initial);
    auto write_args = args;
    write_args.push_back("--write");
    {
      ScopedRomReadOnly read_only(rom);
      RunJson(set, write_args, &rom, &status);
      EXPECT_TRUE(absl::IsPermissionDenied(status)) << status;
      EXPECT_EQ(rom.vector(), initial);
      EXPECT_EQ(ReadBytes(path), initial);
    }
    {
      test::ScopedRomStagingFailure failure;
      RunJson(set, write_args, &rom, &status);
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(rom.vector(), initial);
      EXPECT_EQ(rom.dirty(), dirty);
      EXPECT_EQ(ReadBytes(path), initial);
    }
    const auto saved = RunJson(set, write_args, &rom, &status);
    ASSERT_TRUE(status.ok()) << status << saved.dump();
    EXPECT_EQ(saved["Overworld Tile Write"]["verified_reload"], true);
    Rom reopened;
    ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
    zelda3::Overworld after(&reopened);
    ASSERT_TRUE(after.Load(&reopened).ok());
    size_t differences = 0;
    for (int world = 0; world < 3; ++world) {
      const auto& old_grid = before.GetMapTiles(world);
      const auto& new_grid = after.GetMapTiles(world);
      ASSERT_EQ(new_grid.size(), old_grid.size());
      for (size_t col = 0; col < old_grid.size(); ++col) {
        ASSERT_EQ(new_grid[col].size(), old_grid[col].size());
        for (size_t row = 0; row < old_grid[col].size(); ++row)
          differences += new_grid[col][row] != old_grid[col][row];
      }
    }
    EXPECT_EQ(differences, 1);
    EXPECT_EQ(*overworld::ReadAreaTile(after, *location), replacement);
    const auto saved_bytes = reopened.vector();
    RunJson(get, {capacity == 160 ? "--map=A0" : "--map=C0", "--x=0", "--y=0"},
            &reopened, &status);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_EQ(reopened.vector(), saved_bytes);
    RunJson(set,
            {capacity == 160 ? "--map=A0" : "--map=C0", "--x=0", "--y=0",
             "--tile=001", "--write"},
            &reopened, &status);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_EQ(reopened.vector(), saved_bytes);
    EXPECT_EQ(ReadBytes(path), saved_bytes);
  }
}

TEST_F(OverworldTileRomTest, NativeSpriteEditUsesNormalSaveAndReopens) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  fixture.SeedLoadedLegacyForModelTest(rom_, *overworld_);
  fixture.UpgradeToRevision4();
  const auto dir = test::UniqueTempPath("ow_native_sprite");
  std::filesystem::create_directories(dir);
  auto cleanup = absl::MakeCleanup([&] {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  });
  const auto path = dir / "edit.sfc";
  {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(fixture.bytes.data()),
               fixture.bytes.size());
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path.string()).ok());
  auto live_rows = [](zelda3::Overworld& world, int map) {
    std::vector<zelda3::Sprite> rows;
    for (const auto& sprite : *world.mutable_sprites(1))
      if (!sprite.deleted() && sprite.map_id() == map)
        rows.push_back(sprite);
    return rows;
  };
  zelda3::Overworld before(&rom);
  ASSERT_TRUE(before.Load(&rom).ok());
  const auto old_rows = live_rows(before, 0x00);
  ASSERT_FALSE(old_rows.empty());
  const auto& last = old_rows.back();
  ASSERT_EQ(last.overworld_x_flags() | last.overworld_y_flags(), 0);

  // The seeded original list region is full, so free a row before adding.
  handlers::OverworldSpriteEditCommandHandler remove(
      handlers::OverworldSpriteEditAction::kRemove);
  const std::vector<std::string> args = {
      "--screen=0x00", "--phase=1",
      absl::StrFormat("--index=%d", static_cast<int>(old_rows.size()) - 1),
      absl::StrFormat("--expect-id=0x%02X", last.id())};
  const auto initial = rom.vector();
  absl::Status status;
  const auto dry = RunJson(remove, args, &rom, &status);
  ASSERT_TRUE(status.ok()) << status << dry.dump();
  EXPECT_EQ(dry["strategy"], "native-save");
  EXPECT_EQ(dry["write_status"], "dry-run");
  EXPECT_EQ(rom.vector(), initial);
  EXPECT_EQ(ReadBytes(path), initial);

  auto write_args = args;
  write_args.push_back("--write");
  const auto saved = RunJson(remove, write_args, &rom, &status);
  ASSERT_TRUE(status.ok()) << status << saved.dump();
  EXPECT_EQ(saved["verified_reload"], true);
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
  ASSERT_TRUE(
      rom::IsNativeSpecialOverworldLayout(reopened.InspectLyraLayout()));
  zelda3::Overworld after(&reopened);
  ASSERT_TRUE(after.Load(&reopened).ok());
  EXPECT_EQ(live_rows(after, 0x00).size(), old_rows.size() - 1);

  handlers::OverworldSpriteEditCommandHandler add(
      handlers::OverworldSpriteEditAction::kAdd);
  RunJson(
      add,
      {"--screen=0x00", "--phase=1", absl::StrFormat("--id=0x%02X", last.id()),
       absl::StrFormat("--x=%d", last.x() / 16),
       absl::StrFormat("--y=%d", last.y() / 16), "--write"},
      &reopened, &status);
  ASSERT_TRUE(status.ok()) << status;
  Rom restored;
  ASSERT_TRUE(restored.LoadFromFile(path.string()).ok());
  zelda3::Overworld final_world(&restored);
  ASSERT_TRUE(final_world.Load(&restored).ok());
  const auto final_rows = live_rows(final_world, 0x00);
  ASSERT_EQ(final_rows.size(), old_rows.size());
  for (size_t i = 0; i < old_rows.size(); ++i) {
    EXPECT_EQ(final_rows[i].id(), old_rows[i].id());
    EXPECT_EQ(final_rows[i].x(), old_rows[i].x());
    EXPECT_EQ(final_rows[i].y(), old_rows[i].y());
  }
}

TEST_F(OverworldTileRomTest,
       SelectedImportStagesNative192AndPublishesOnlyNewVerifiedOutput) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  fixture.SeedLoadedLegacyForModelTest(rom_, *overworld_);
  fixture.UpgradeToRevision4();
  fixture.AddAuthoringMetadata();
  const auto dir = test::UniqueTempPath("ow_selected_import");
  std::filesystem::create_directories(dir);
  auto cleanup = absl::MakeCleanup([&] {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  });
  const auto donor_path = dir / "donor.sfc";
  const auto target_path = dir / "native.sfc";
  const auto output_path = dir / "imported.sfc";
  auto write_bytes = [](const auto& path, const std::vector<uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  };
  Rom native;
  ASSERT_TRUE(native.LoadFromData(fixture.bytes).ok());
  zelda3::Overworld target(&native);
  ASSERT_TRUE(target.Load(&native).ok());
  // Choose an ordinary complete area with explicit palette selectors. The
  // fixture contributes actual legacy resources; tail pages and authoring
  // metadata come from the maintained native model fixture.
  int parent = -1;
  for (int map = 0; map < 128; ++map) {
    const auto* page = overworld_->overworld_map(map);
    const int pc = overworld_->version_constants().kOverworldMapPaletteGroup +
                   page->area_palette() * 4;
    if (page->parent() == map && page->main_palette() != 0xFF &&
        page->roomset_source() ==
            zelda3::OverworldRoomsetSource::kCustomTileGroups &&
        rom_.data()[pc] != 0xFF && rom_.data()[pc + 1] != 0xFF &&
        rom_.data()[pc + 2] != 0xFF) {
      parent = map;
      break;
    }
  }
  ASSERT_GE(parent, 0) << "Fixture must contain an explicit ordinary area";
  std::string maps;
  std::set<int> selected;
  for (int map = 0; map < 160; ++map) {
    if (overworld_->overworld_map(map)->parent() != parent)
      continue;
    selected.insert(map);
    if (!maps.empty())
      maps += ',';
    maps += absl::StrFormat("%02X", map);
  }
  const int physical = *selected.rbegin();  // exercise a child when available
  const auto location = overworld::LocateScreenTile(target, physical, 5, 6);
  ASSERT_TRUE(location.ok()) << location.status();
  const auto old_tile = overworld::ReadAreaTile(target, *location);
  ASSERT_TRUE(old_tile.ok());
  ASSERT_TRUE(
      overworld::WriteAreaTile(target, *location, *old_tile == 1 ? 2 : 1).ok());
  const auto flags = core::FeatureFlags::get();
  auto restore_flags =
      absl::MakeCleanup([&] { core::FeatureFlags::get() = flags; });
  core::FeatureFlags::get().overworld.kSaveOverworldMaps = true;
  core::FeatureFlags::get().kSaveAllPalettes = true;
  Rom donor;
  ASSERT_TRUE(donor.LoadFromData(rom_.vector()).ok());
  auto donor_data_owner = std::make_unique<zelda3::GameData>(&donor);
  auto& donor_data = *donor_data_owner;
  zelda3::LoadOptions data_options;
  data_options.load_graphics = false;
  data_options.expand_rom = false;
  ASSERT_TRUE(zelda3::LoadGameData(donor, donor_data, data_options).ok());
  zelda3::Overworld donor_world(&donor, &donor_data);
  ASSERT_TRUE(donor_world.Load(&donor).ok());
  std::set<int> used_tiles, used_groups, used_aux, room_palette_sets;
  for (int map = 0; map < 192; ++map) {
    const auto* page = target.overworld_map(map);
    used_groups.insert(page->area_palette());
    const int pc = target.version_constants().kOverworldMapPaletteGroup +
                   page->area_palette() * 4;
    used_aux.insert(native.data()[pc]);
    used_aux.insert(native.data()[pc + 1]);
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 32; ++x) {
        const auto cell = overworld::LocateScreenTile(target, map, x, y);
        ASSERT_TRUE(cell.ok());
        used_tiles.insert(*overworld::ReadAreaTile(target, *cell));
      }
  }
  for (int room = 0; room < zelda3::kNumberOfRooms; ++room)
    room_palette_sets.insert(
        zelda3::LoadRoomHeaderFromRom(&donor, room).palette());
  int private_tile = 4095;
  while (private_tile >= 0 && used_tiles.count(private_tile))
    --private_tile;
  ASSERT_GE(private_tile, 0);
  int private_group = -1, private_aux = -1;
  const int palette_base =
      (target.version_constants().kOverworldMapPaletteGroup -
       target.version_constants().kDungeonPalettesGroups) /
      4;
  for (int group = 0; group < 31; ++group)
    if (!used_groups.count(group) &&
        !room_palette_sets.count(palette_base + group)) {
      private_group = group;
      break;
    }
  for (int aux = 0; aux < donor_data.palette_groups.overworld_aux.size(); ++aux)
    if (!used_aux.count(aux)) {
      private_aux = aux;
      break;
    }
  ASSERT_GE(private_group, 0) << "Fixture needs an unowned OW palette group";
  ASSERT_GE(private_aux, 0) << "Fixture needs an unowned auxiliary palette";
  auto definition = donor_world.tiles16()[*old_tile];
  definition.tile0_.horizontal_mirror_ = !definition.tile0_.horizontal_mirror_;
  if (definition == target.tiles16()[private_tile])
    definition.tile1_.vertical_mirror_ = !definition.tile1_.vertical_mirror_;
  (*donor_world.mutable_tiles16())[private_tile] = definition;
  ASSERT_TRUE(
      overworld::WriteAreaTile(donor_world, *location, private_tile).ok());
  const auto old_group =
      donor_data
          .paletteset_ids[palette_base +
                          donor_world.overworld_map(parent)->area_palette()];
  donor_data.paletteset_ids[palette_base + private_group] = old_group;
  donor_data.paletteset_ids[palette_base + private_group][0] = private_aux;
  auto* aux =
      donor_data.palette_groups.overworld_aux.mutable_palette(private_aux);
  *aux = donor_data.palette_groups.overworld_aux.palette_ref(old_group[0]);
  (*aux)[0].set_snes((*aux)[0].snes() ^ 1);
  for (int color = 0; color < aux->size(); ++color)
    (*aux)[color].set_modified(true);
  const uint8_t sheet =
      target.overworld_map(parent)->custom_tileset(0) == 0x3A ? 0x3B : 0x3A;
  for (int map : selected) {
    donor_world.mutable_overworld_map(map)->set_area_palette(private_group);
    donor_world.mutable_overworld_map(map)->set_custom_tileset(0, sheet);
  }
  ASSERT_TRUE(donor_world.SaveMapProperties().ok());
  ASSERT_TRUE(donor_world.SaveCustomOverworldData().ok());
  ASSERT_TRUE(donor_world.SaveMap16Expanded().ok());
  ASSERT_TRUE(donor_world.CreateTile32Tilemap().ok());
  ASSERT_TRUE(donor_world.SaveMap32Expanded().ok());
  ASSERT_TRUE(donor_world.SaveOverworldMaps().ok());
  ASSERT_TRUE(zelda3::SaveGfxGroups(donor, donor_data).ok());
  ASSERT_TRUE(zelda3::SaveGameData(donor, donor_data).ok());
  write_bytes(donor_path, donor.vector());
  const auto donor_before = donor.vector();
  ASSERT_TRUE(target.SaveNativeTerrain().ok());
  write_bytes(target_path, native.vector());
  ASSERT_TRUE(native.LoadFromFile(target_path.string()).ok());
  const auto original = native.vector();
  // This source contains unavailable dormant special-area sprite selectors
  // (map $90 is $C2). They are outside this BG import and remain byte-exact.
  const auto sprite_selector_pc = fixture.Entry(25).pc_address + 0x90;
  ASSERT_GE(original[sprite_selector_pc], 24);
  handlers::OverworldImportMapsCommandHandler handler;
  const std::vector<std::string> args = {"--donor=" + donor_path.string(),
                                         "--maps=" + maps,
                                         "--output=" + output_path.string()};
  absl::Status status;
  const auto dry = RunJson(handler, args, &native, &status);
  ASSERT_TRUE(status.ok()) << status << dry.dump();
  EXPECT_EQ(dry["Overworld Map Import"]["write_status"], "dry-run");
  EXPECT_EQ(dry["Overworld Map Import"]["changed_definitions"], 1);
  EXPECT_GE(dry["Overworld Map Import"]["changed_palette_records"].get<int>(),
            1);
  EXPECT_FALSE(std::filesystem::exists(output_path));
  EXPECT_EQ(native.vector(), original);
  EXPECT_EQ(ReadBytes(target_path), original);
  EXPECT_EQ(ReadBytes(donor_path), donor_before);

  for (const auto& alias : {target_path, donor_path}) {
    auto alias_args = args;
    alias_args[2] = "--output=" + alias.string();
    alias_args.push_back("--write");
    RunJson(handler, alias_args, &native, &status);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(native.vector(), original);
    EXPECT_EQ(ReadBytes(target_path), original);
  }
  auto write_args = args;
  write_args.push_back("--write");
  const auto written = RunJson(handler, write_args, &native, &status);
  ASSERT_TRUE(status.ok()) << status << written.dump();
  EXPECT_EQ(native.vector(), original);
  EXPECT_EQ(ReadBytes(target_path), original);
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(output_path.string()).ok());
  zelda3::Overworld actual(&reopened);
  ASSERT_TRUE(actual.Load(&reopened).ok());
  EXPECT_EQ(actual.materialized_map_count(), 192);
  EXPECT_EQ(*overworld::ReadAreaTile(actual, *location), private_tile);
  EXPECT_EQ(actual.tiles16()[private_tile], definition);
  EXPECT_EQ(actual.overworld_map(parent)->custom_tileset(0), sheet);
  EXPECT_EQ(actual.overworld_map(parent)->area_palette(), private_group);
  auto actual_data_owner = std::make_unique<zelda3::GameData>(&reopened);
  auto& actual_data = *actual_data_owner;
  ASSERT_TRUE(zelda3::LoadGameData(reopened, actual_data, data_options).ok());
  EXPECT_EQ(actual_data.paletteset_ids[palette_base + private_group],
            donor_data.paletteset_ids[palette_base + private_group]);
  EXPECT_EQ(actual_data.palette_groups.overworld_aux.palette_ref(private_aux),
            *aux);
  for (int table : {25, 37, 38}) {
    const auto& entry = fixture.Entry(table);
    EXPECT_TRUE(std::equal(
        original.begin() + entry.pc_address,
        original.begin() + entry.pc_address + entry.count * entry.stride,
        reopened.vector().begin() + entry.pc_address));
  }
  EXPECT_EQ(reopened.vector()[sprite_selector_pc],
            original[sprite_selector_pc]);
  zelda3::Overworld control(&native);
  ASSERT_TRUE(control.Load(&native).ok());
  for (int map = 0; map < 192; ++map) {
    if (selected.count(map))
      continue;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        const auto cell = overworld::LocateScreenTile(control, map, x, y);
        ASSERT_TRUE(cell.ok()) << cell.status();
        EXPECT_EQ(*overworld::ReadAreaTile(actual, *cell),
                  *overworld::ReadAreaTile(control, *cell));
      }
    }
  }
  const auto output_bytes = ReadBytes(output_path);
  RunJson(handler, write_args, &native, &status);
  EXPECT_TRUE(absl::IsAlreadyExists(status)) << status;
  EXPECT_EQ(ReadBytes(output_path), output_bytes);
  EXPECT_EQ(native.vector(), original);

  // Shared palette-owner lifecycle: native commits remain staged until the
  // outer file save succeeds. Invalid batches and later publication failure
  // leave bytes and palette dirty/original tracking intact.
  auto palette_data_owner = std::make_unique<zelda3::GameData>(&native);
  auto& palette_data = *palette_data_owner;
  ASSERT_TRUE(zelda3::LoadGameData(native, palette_data, data_options).ok());
  auto& manager = gfx::PaletteManager::Get();
  manager.Initialize(&palette_data);
  auto release_palettes =
      absl::MakeCleanup([&] { manager.ReleaseSession(&palette_data); });
  const auto old_color = manager.GetColor("ow_aux", private_aux, 0);
  auto new_color = old_color;
  new_color.set_snes(new_color.snes() ^ 1);
  ASSERT_TRUE(manager.SetColor("ow_aux", private_aux, 0, new_color).ok());
  const auto planned = zelda3::PrepareNativeOverworldPaletteSave(
      native, palette_data, false, true);
  ASSERT_TRUE(planned.ok()) << planned.status();
  ASSERT_EQ(planned->colors.size(), 1);
  auto reject_batch = [&](const rom::NativeOverworldPaletteEdits& plan) {
    EXPECT_FALSE(native
                     .CommitNativeTerrainImage(original, original, {}, {},
                                               nullptr, nullptr, &plan)
                     .ok());
    EXPECT_EQ(native.vector(), original);
    EXPECT_TRUE(manager.HasUnsavedChanges());
    EXPECT_EQ(manager.GetOriginalColor("ow_aux", private_aux, 0).snes(),
              old_color.snes());
  };
  auto invalid = *planned;
  invalid.colors[0].value = 0x8000;
  reject_batch(invalid);
  invalid = *planned;
  invalid.colors[0].expected ^= 1;
  reject_batch(invalid);
  invalid = *planned;
  invalid.colors.push_back(invalid.colors.front());
  reject_batch(invalid);
  invalid = *planned;
  invalid.colors[0].palette_index = 20;  // unbound in the legacy session layout
  reject_batch(invalid);
  invalid = *planned;
  invalid.colors[0].kind = static_cast<rom::NativeOverworldPaletteKind>(255);
  reject_batch(invalid);
  palette_data.main_blockset_ids[0][0] ^= 1;
  EXPECT_FALSE(zelda3::PrepareNativeOverworldPaletteSave(native, palette_data,
                                                         true, true)
                   .ok());
  EXPECT_EQ(native.vector(), original);
  palette_data.main_blockset_ids[0][0] ^= 1;
  auto& foreign_color =
      (*palette_data.palette_groups.hud.mutable_palette(0))[0];
  const auto old_foreign_color = foreign_color;
  foreign_color.set_snes(foreign_color.snes() ^ 1);
  foreign_color.set_modified(true);
  EXPECT_FALSE(zelda3::PrepareNativeOverworldPaletteSave(native, palette_data,
                                                         false, true)
                   .ok());
  EXPECT_EQ(native.vector(), original);
  foreign_color = old_foreign_color;
  invalid = *planned;
  const int row_pc = 0x75504 + private_group * 4;
  rom::NativeOverworldPaletteSelectorEdit row;
  row.group_index = private_group;
  std::copy_n(original.begin() + row_pc, 4, row.expected.begin());
  row.value = row.expected;
  row.value[3] ^= 1;
  invalid.selectors.push_back(row);
  reject_batch(invalid);
  core::RomAddressOverrides overlap;
  overlap.addresses[core::RomAddressKey::kOverworldAuxPaletteExtensionPc] =
      fixture.Entry(411).pc_address;
  overlap.addresses[core::RomAddressKey::kOverworldAuxPaletteExtensionCount] =
      32;
  overlap.addresses[core::RomAddressKey::kOverworldAuxPaletteExtensionAbi] = 1;
  const auto overlapping =
      gfx::palette_group_internal::OverworldPaletteLayout::FromOverrides(
          overlap, native.size());
  ASSERT_TRUE(overlapping.ok()) << overlapping.status();
  invalid = *planned;
  invalid.layout = *overlapping;
  invalid.colors[0].palette_index = 20;
  invalid.colors[0].expected = *native.ReadWord(fixture.Entry(411).pc_address);
  reject_batch(invalid);
  {
    ScopedRomReadOnly policy(native);
    reject_batch(*planned);
  }
  {
    rom::WriteFence deny;
    rom::ScopedWriteFence fence(&native, &deny);
    reject_batch(*planned);
  }
  EXPECT_FALSE(
      native.WriteWord(0xDE86C + private_aux * 42, new_color.snes()).ok());
  ASSERT_TRUE(manager.BeginSaveTransaction().ok());
  {
    ScopedRomTransaction transaction(native);
    ASSERT_TRUE(manager.SaveAllToRom().ok());
    EXPECT_TRUE(manager.HasUnsavedChanges());
    EXPECT_EQ(manager.GetOriginalColor("ow_aux", private_aux, 0).snes(),
              old_color.snes());
    test::ScopedRomStagingFailure failure;
    EXPECT_FALSE(
        native
            .SaveToFile({.save_new = false,
                         .filename = (dir / "failed-palette.sfc").string()})
            .ok());
    manager.RollbackSaveTransaction();
  }
  EXPECT_EQ(native.vector(), original);
  EXPECT_TRUE(manager.HasUnsavedChanges());
  ASSERT_TRUE(manager.BeginSaveTransaction().ok());
  {
    ScopedRomTransaction transaction(native);
    ASSERT_TRUE(manager.SaveAllToRom().ok());
    ASSERT_TRUE(
        native
            .SaveToFile({.save_new = false,
                         .filename = (dir / "palette-reopen.sfc").string()})
            .ok());
    transaction.Commit();
    manager.CommitSaveTransaction();
  }
  EXPECT_FALSE(manager.HasUnsavedChanges());
  EXPECT_EQ(manager.GetOriginalColor("ow_aux", private_aux, 0).snes(),
            new_color.snes());
  Rom palette_reopened;
  ASSERT_TRUE(
      palette_reopened.LoadFromFile((dir / "palette-reopen.sfc").string())
          .ok());
  EXPECT_EQ(*palette_reopened.ReadWord(0xDE86C + private_aux * 42),
            new_color.snes());
  auto staged_color = new_color;
  staged_color.set_snes(staged_color.snes() ^ 2);
  auto later_color = staged_color;
  later_color.set_snes(later_color.snes() ^ 4);
  ASSERT_TRUE(manager.SetColor("ow_aux", private_aux, 0, staged_color).ok());
  ASSERT_TRUE(manager.BeginSaveTransaction().ok());
  {
    ScopedRomTransaction transaction(native);
    ASSERT_TRUE(manager.SaveAllToRom().ok());
    ASSERT_TRUE(manager.SetColor("ow_aux", private_aux, 0, later_color).ok());
    ASSERT_TRUE(
        native
            .SaveToFile({.save_new = false,
                         .filename = (dir / "palette-post-stage.sfc").string()})
            .ok());
    transaction.Commit();
    manager.CommitSaveTransaction();
  }
  EXPECT_TRUE(manager.HasUnsavedChanges());
  EXPECT_EQ(manager.GetOriginalColor("ow_aux", private_aux, 0).snes(),
            staged_color.snes());
  EXPECT_EQ(manager.GetColor("ow_aux", private_aux, 0).snes(),
            later_color.snes());
  EXPECT_EQ(*native.ReadWord(0xDE86C + private_aux * 42), staged_color.snes());
  manager.ReleaseSession(&palette_data);

  fixture.bytes = original;
  fixture.bytes[fixture.Entry(31).pc_address + 0xBF] = private_group;
  fixture.Seal();
  write_bytes(target_path, fixture.bytes);
  ASSERT_TRUE(native.LoadFromFile(target_path.string()).ok());
  auto palette_foreign_args = write_args;
  const auto palette_foreign_output = dir / "foreign-palette.sfc";
  palette_foreign_args[2] = "--output=" + palette_foreign_output.string();
  RunJson(handler, palette_foreign_args, &native, &status);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(status.message(), HasSubstr("unselected owner"));
  EXPECT_FALSE(std::filesystem::exists(palette_foreign_output));
  EXPECT_EQ(native.vector(), fixture.bytes);
  // Restore the original model before the independent shared-definition case.
  ASSERT_TRUE(native.LoadFromData(original).ok());

  // A changed definition used by both an imported and a preserved page must
  // reject the entire transaction before publishing even one selected cell.
  std::set<uint16_t> imported_tiles, foreign_tiles;
  for (int map = 0; map < 192; ++map) {
    auto& ids = selected.count(map) ? imported_tiles : foreign_tiles;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        const auto cell = overworld::LocateScreenTile(control, map, x, y);
        ASSERT_TRUE(cell.ok());
        ids.insert(*overworld::ReadAreaTile(
            selected.count(map) ? donor_world : control, *cell));
      }
    }
  }
  std::vector<uint16_t> shared;
  std::set_intersection(imported_tiles.begin(), imported_tiles.end(),
                        foreign_tiles.begin(), foreign_tiles.end(),
                        std::back_inserter(shared));
  ASSERT_FALSE(shared.empty());
  fixture.bytes = original;
  fixture.bytes[fixture.Entry(400).pc_address + shared.front() * 8 + 1] ^= 0x40;
  fixture.Seal();
  write_bytes(target_path, fixture.bytes);
  ASSERT_TRUE(native.LoadFromFile(target_path.string()).ok());
  const auto conflict_input = native.vector();
  auto conflict_args = write_args;
  const auto conflict_output = dir / "foreign-owner.sfc";
  conflict_args[2] = "--output=" + conflict_output.string();
  RunJson(handler, conflict_args, &native, &status);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_THAT(status.message(), HasSubstr("unselected owner"));
  EXPECT_FALSE(std::filesystem::exists(conflict_output));
  EXPECT_EQ(native.vector(), conflict_input);
  EXPECT_EQ(ReadBytes(target_path), conflict_input);
  EXPECT_EQ(ReadBytes(donor_path), donor_before);
}

TEST_F(OverworldTileRomTest, AllocationTakesNativeTailOccupancyFromTheModel) {
  test::native_overworld::NativeDirectoryFixture fixture(192);
  ASSERT_NO_FATAL_FAILURE(SeedValidationFixture(&fixture));
  fixture.UpgradeToRevision4();
  rom::NativeOverworldAuthoringMetadata metadata;
  metadata.next_placement_id = 2;
  metadata.placements = {
      {1, rom::NativePlacementState::kPending,
       rom::NativeAuthoringDoorPlacement{0xA5, 9, 9, 0x0B, std::nullopt}}};
  fixture.AddAuthoringMetadata(metadata);
  Rom native;
  ASSERT_TRUE(native.LoadFromData(fixture.bytes).ok());
  zelda3::Overworld world(&native);
  ASSERT_TRUE(world.Load(&native).ok());
  ASSERT_TRUE(world.native_authoring_metadata().has_value());
  // Stage entities on blank tail pages the way the editor holds them.
  world.mutable_all_items()->emplace_back(0x01, 0xAC, 16, 16, false);
  world.mutable_sprites(1)->emplace_back(std::vector<uint8_t>{}, 0xB2, 0x08, 4,
                                         4, 64, 64);
  world.mutable_overworld_map(0xA3)->set_subscreen_overlay(0xB5);

  zelda3::OverworldAllocationOptions options;
  options.overworld = &world;
  const auto with_model =
      zelda3::BuildOverworldAllocationReport(&native, options);
  ASSERT_TRUE(with_model.ok()) << with_model.status();
  const auto without_model = zelda3::BuildOverworldAllocationReport(&native);
  ASSERT_TRUE(without_model.ok()) << without_model.status();
  const auto& areas = with_model->areas;
  EXPECT_EQ(areas[0xA5].authoring_placements, 1);
  EXPECT_EQ(areas[0xAC].items, 1);
  EXPECT_EQ(areas[0xB2].sprites, 1);
  EXPECT_THAT(areas[0xB5].overlay_users, ::testing::Contains(0xA3));
  for (int id : {0xA5, 0xAC, 0xB2, 0xB5}) {
    EXPECT_TRUE(areas[id].used) << id;
    EXPECT_FALSE(without_model->areas[id].used) << id;
    EXPECT_TRUE(without_model->areas[id].unverified) << id;
  }
  // No evidence, yet inbound transitions and code references are unscanned.
  EXPECT_FALSE(areas[0xB6].used);
  EXPECT_TRUE(areas[0xB6].unverified);
  EXPECT_EQ(
      areas[0xB6].unscanned,
      (std::vector<std::string>{"inbound_transitions", "code_references"}));
  for (const auto* report : {&*with_model, &*without_model}) {
    for (const auto& summary : report->summary) {
      if (summary.key == "special_tail") {
        EXPECT_EQ(summary.free, 0);
        EXPECT_EQ(summary.used + summary.unverified, 32);
      }
    }
  }
  // $00-$9F keep the legacy readers; the model adds nothing there.
  for (int id = 0; id <= 0x9F; ++id) {
    const auto& legacy = without_model->areas[id];
    EXPECT_EQ(areas[id].entrances, legacy.entrances) << id;
    EXPECT_EQ(areas[id].sprites, legacy.sprites) << id;
    EXPECT_EQ(areas[id].items, 0) << id;
    EXPECT_TRUE(areas[id].unscanned.empty()) << id;
  }
}

class OverworldNativeSourceImportTest : public ::testing::Test {
 protected:
  static constexpr int kSelectedMap = 0x8D;
  static constexpr int kKeptMap = 0x90;

  void SetUp() override {
    fixture_.SeedNativeSpritePrefix();
    fixture_.bytes[zelda3::overworldItemsAddressBank] = 0x1B;
    fixture_.Word(0xD8000, 0xFFFF);
    for (int map = 0; map < 192; ++map) {
      fixture_.Word(fixture_.Entry(23).pc_address + map * 2, 0x8000);
      fixture_.bytes[fixture_.Entry(7).pc_address + map] = map;
    }
    for (int map : {kSelectedMap, kSelectedMap + 1}) {
      fixture_.bytes[fixture_.Entry(7).pc_address + map] = kSelectedMap;
      fixture_.bytes[fixture_.Entry(30).pc_address + map] =
          static_cast<uint8_t>(zelda3::AreaSizeEnum::WideArea);
    }
    fixture_.UpgradeToRevision4();
    fixture_.AddAuthoringMetadata();
    fixture_.AddPageTile16();
    const std::vector<rom::NativePageTile16Definition> kept = {
        {kKeptMap, 0x10, {9, 9, 9, 9}}};
    const auto encoded = rom::EncodeNativePageTile16(kept, 192, 16);
    ASSERT_TRUE(encoded.ok()) << encoded.status();
    std::copy(encoded->begin(), encoded->end(),
              fixture_.bytes.begin() + fixture_.Entry(413).pc_address);
    fixture_.Seal();

    dir_ = test::UniqueTempPath("ow_native_source_import");
    std::filesystem::create_directories(dir_);
    target_path_ = dir_ / "native.sfc";
    source_path_ = dir_ / "source.json";
    output_path_ = dir_ / "imported.sfc";
    WriteBytes(target_path_, fixture_.bytes);
    ASSERT_TRUE(target_.LoadFromFile(target_path_.string()).ok());
    original_ = target_.vector();

    model_rom_.LoadFromData(fixture_.bytes);
    model_rom_.EnableExperimentalPageTile16();
    model_ = std::make_unique<zelda3::Overworld>(&model_rom_);
    ASSERT_TRUE(model_->Load(&model_rom_).ok());
    grid_ = model_->map_tiles().special_world;
    const int parent = model_->overworld_map(kSelectedMap)->parent();
    for (int map = 0; map < 192; ++map) {
      if (model_->overworld_map(map)->parent() == parent) {
        area_.push_back(map);
      }
    }
    ASSERT_EQ(std::count(area_.begin(), area_.end(), kKeptMap), 0);
    source_ = BuildSource();
  }

  void TearDown() override {
    std::error_code ignored;
    std::filesystem::remove_all(dir_, ignored);
  }

  static void WriteBytes(const std::filesystem::path& path,
                         const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }

  uint16_t Cell(const zelda3::OverworldBlockset& grid, int map, int x,
                int y) const {
    return grid[(map % 8) * 32 + x][((map % 64) / 8) * 32 + y];
  }

  std::string DictionarySha() const {
    std::string words;
    for (const auto& tile : model_->tiles16()) {
      for (const auto& quadrant : tile.tiles_info) {
        const uint16_t word = gfx::TileInfoToWord(quadrant);
        words.push_back(static_cast<char>(word & 0xFF));
        words.push_back(static_cast<char>(word >> 8));
      }
    }
    return core::ComputeSourceArtifactSha256(words);
  }

  nlohmann::json BuildSource() {
    const auto snapshot = model_->CaptureNativeAreaSnapshot();
    EXPECT_TRUE(snapshot.ok()) << snapshot.status();
    rows_ = {{kSelectedMap, 0x10, {1, 2, 3, 4}},
             {kSelectedMap, 0x11, {5, 6, 7, 8}},
             {kKeptMap, 0x10, {7, 7, 7, 7}}};
    const auto table = rom::EncodeNativePageTile16(rows_, 192, 16);
    EXPECT_TRUE(table.ok()) << table.status();
    auto rows = nlohmann::json::array();
    for (const auto& row : rows_) {
      rows.push_back({{"map_id", row.map_id},
                      {"tile_id", row.tile_id},
                      {"words", row.words}});
    }
    std::vector<int> maps = area_;
    maps.push_back(kKeptMap);
    auto pages = nlohmann::json::array();
    for (const int map : maps) {
      std::vector<uint16_t> tiles(1024);
      for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
          tiles[y * 32 + x] = Cell(grid_, map, x, y);
        }
      }
      tiles[4 * 32 + 3] = tiles[4 * 32 + 3] == 1 ? 2 : 1;
      pages.push_back({{"map_id", map},
                       {"parent_map_id", model_->overworld_map(map)->parent()},
                       {"tiles", tiles},
                       {"source_profile",
                        zelda3::NativeSourcePageProfile(*snapshot, 5, map)}});
    }
    return {{"schema", "yaze.special-overworld-native-source.experimental-v2"},
            {"directory_revision", 5},
            {"materialized_map_count", 192},
            {"runtime_supported", false},
            {"tile16_dictionary", {{"sha256", DictionarySha()}}},
            {"page_tile16_definitions",
             {{"table_id", rom::kNativePageTile16TableId},
              {"stride", 12},
              {"count", 16},
              {"sha256", core::ComputeSourceArtifactSha256(
                             std::string(table->begin(), table->end()))},
              {"rows", rows}}},
            {"pages", pages}};
  }

  std::string MapsArg(const std::vector<int>& maps) const {
    std::vector<std::string> parts;
    for (const int map : maps) {
      parts.push_back(absl::StrFormat("%02X", map));
    }
    return absl::StrJoin(parts, ",");
  }

  std::vector<std::string> Args(const std::string& text) const {
    return {"--native-source=" + source_path_.string(),
            "--native-source-sha256=" + core::ComputeSourceArtifactSha256(text),
            "--experimental-page-tile16",
            "--maps=" + MapsArg(area_),
            "--output=" + output_path_.string(),
            "--format=json"};
  }

  absl::Status Run(const nlohmann::json& source,
                   std::vector<std::string> args = {},
                   nlohmann::json* result = nullptr) {
    const std::string text = source.dump();
    WriteBytes(source_path_, std::vector<uint8_t>(text.begin(), text.end()));
    if (args.empty()) {
      args = Args(text);
    }
    handlers::OverworldImportMapsCommandHandler handler;
    std::string output;
    const auto status = handler.Run(args, &target_, &output);
    if (result != nullptr) {
      *result = nlohmann::json::parse(output, nullptr, false)
                    .value("Overworld Map Import", nlohmann::json::object());
    }
    return status;
  }

  void ExpectRefused(const absl::Status& status, absl::string_view message) {
    EXPECT_FALSE(status.ok());
    EXPECT_THAT(std::string(status.message()), HasSubstr(message)) << status;
    EXPECT_FALSE(std::filesystem::exists(output_path_));
    EXPECT_EQ(target_.vector(), original_);
    EXPECT_EQ(ReadBytes(target_path_), original_);
  }

  test::native_overworld::NativeDirectoryFixture fixture_;
  std::filesystem::path dir_;
  std::filesystem::path target_path_;
  std::filesystem::path source_path_;
  std::filesystem::path output_path_;
  Rom target_;
  std::vector<uint8_t> original_;
  Rom model_rom_;
  std::unique_ptr<zelda3::Overworld> model_;
  zelda3::OverworldBlockset grid_;
  std::vector<int> area_;
  std::vector<rom::NativePageTile16Definition> rows_;
  nlohmann::json source_;
};

TEST_F(OverworldNativeSourceImportTest, DryRunThenWriteImportsSelectedPages) {
  ASSERT_EQ(area_, (std::vector<int>{kSelectedMap, kSelectedMap + 1}));
  nlohmann::json dry;
  const auto dry_status = Run(source_, {}, &dry);
  ASSERT_TRUE(dry_status.ok()) << dry_status;
  EXPECT_EQ(dry["write_status"], "dry-run");
  EXPECT_EQ(dry["source"], "native-source");
  EXPECT_EQ(dry["selected_page_rows"], 2);
  EXPECT_EQ(dry["changed_cells"], static_cast<int>(area_.size()));
  EXPECT_EQ(dry["profile_writes"], false);
  EXPECT_EQ(dry["verified_reload"], true);
  EXPECT_FALSE(std::filesystem::exists(output_path_));
  EXPECT_EQ(target_.vector(), original_);

  auto args = Args(source_.dump());
  args.push_back("--write");
  nlohmann::json wrote;
  const auto write_status = Run(source_, args, &wrote);
  ASSERT_TRUE(write_status.ok()) << write_status;
  EXPECT_EQ(wrote["write_status"], "new-output");
  ASSERT_TRUE(std::filesystem::exists(output_path_));
  EXPECT_EQ(target_.vector(), original_);
  EXPECT_EQ(ReadBytes(target_path_), original_);

  Rom reopened;
  reopened.LoadFromData(ReadBytes(output_path_));
  reopened.EnableExperimentalPageTile16();
  zelda3::Overworld world(&reopened);
  ASSERT_TRUE(world.Load(&reopened).ok());
  EXPECT_EQ(world.page_tile16(),
            (std::vector<rom::NativePageTile16Definition>{
                rows_[0], rows_[1], {kKeptMap, 0x10, {9, 9, 9, 9}}}));
  const auto grid = world.map_tiles().special_world;
  for (int map = 0x80; map < 0xC0; ++map) {
    const bool selected = std::count(area_.begin(), area_.end(), map) != 0;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        const uint16_t before = Cell(grid_, map, x, y);
        const uint16_t expected =
            selected && x == 3 && y == 4 ? (before == 1 ? 2 : 1) : before;
        ASSERT_EQ(Cell(grid, map, x, y), expected)
            << absl::StrFormat("map $%02X (%d,%d)", map, x, y);
      }
    }
  }
  EXPECT_EQ(world.map_tiles().light_world, model_->map_tiles().light_world);
  EXPECT_EQ(world.map_tiles().dark_world, model_->map_tiles().dark_world);
  ASSERT_EQ(world.tiles16().size(), model_->tiles16().size());
  for (size_t i = 0; i < world.tiles16().size(); ++i) {
    ASSERT_EQ(world.tiles16()[i].tiles_info, model_->tiles16()[i].tiles_info)
        << i;
  }

  EXPECT_EQ(Run(source_).code(), absl::StatusCode::kAlreadyExists);
}

TEST_F(OverworldNativeSourceImportTest, RefusesBeforeProducingOutput) {
  const std::string text = source_.dump();
  auto args = Args(text);
  args[1] = "--native-source-sha256=" + std::string(64, '0');
  ExpectRefused(Run(source_, args), "sha256");

  args = Args(text);
  args.erase(args.begin() + 2);
  ExpectRefused(Run(source_, args), "--experimental-page-tile16");

  args = Args(text);
  args[4] = "--output=" + source_path_.string();
  EXPECT_FALSE(Run(source_, args).ok());
  args[4] = "--output=" + target_path_.string();
  ExpectRefused(Run(source_, args), "");

  auto source = source_;
  source["pages"][0]["source_profile"]["palette_set"] =
      source["pages"][0]["source_profile"]["palette_set"].get<int>() ^ 1;
  ExpectRefused(Run(source), "palette_set");

  source = source_;
  source["pages"].back()["tiles"].erase(0);
  ExpectRefused(Run(source), "tiles");

  source = source_;
  source["pages"][0]["tiles"][0] = model_->tiles16().size();
  ExpectRefused(Run(source), "Invalid native source");

  source = source_;
  source["pages"].push_back(source["pages"][0]);
  ExpectRefused(Run(source), "repeats page");

  source = source_;
  source["pages"].erase(0);
  ExpectRefused(Run(source), "");

  source = source_;
  source["directory_revision"] = 4;
  ExpectRefused(Run(source), "");

  source = source_;
  source["tile16_dictionary"]["sha256"] = std::string(64, '0');
  ExpectRefused(Run(source), "");

  source = source_;
  source["page_tile16_definitions"]["rows"][2]["words"][0] = 8;
  ExpectRefused(Run(source), "");

  args = Args(text);
  args[3] = "--maps=" + MapsArg({kSelectedMap});
  ExpectRefused(Run(source_, args), "every physical page");
}

TEST(OverworldNativeSourceImportArgsTest, RequiresExactlyOneSource) {
  handlers::OverworldImportMapsCommandHandler handler;
  Rom rom;
  std::string output;
  EXPECT_EQ(
      handler
          .Run({"--donor=a.sfc", "--native-source=b.json",
                "--native-source-sha256=00", "--maps=8D", "--output=c.sfc"},
               &rom, &output)
          .code(),
      absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(handler.Run({"--maps=8D", "--output=c.sfc"}, &rom, &output).code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(handler
                .Run({"--native-source=b.json", "--maps=8D", "--output=c.sfc"},
                     &rom, &output)
                .code(),
            absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace yaze::cli
