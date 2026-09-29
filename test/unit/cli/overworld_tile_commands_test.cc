#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/strings/str_format.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "oracle_rom_fixture.h"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "zelda3/overworld/overworld.h"

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

  std::string rom_path_;
  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
};

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
  EXPECT_EQ(stats["screens_scanned"], 0xA0);
  EXPECT_EQ(stats["total_tiles"], 0xA0 * 1024);
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
  EXPECT_EQ(total, 0xA0 * 1024);

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

}  // namespace
}  // namespace yaze::cli
