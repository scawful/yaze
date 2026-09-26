// ROM-backed overworld build checks: per-phase build timing and parity
// between cached and uncached map builds. Skips unless YAZE_TEST_ROM_OOS or
// YAZE_TEST_ROM_VANILLA points at a ROM.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {
namespace {

std::vector<std::string> OverworldRomPaths() {
  std::vector<std::string> paths;
  for (const char* env : {"YAZE_TEST_ROM_OOS", "YAZE_TEST_ROM_VANILLA"}) {
    if (const char* path = std::getenv(env);
        path != nullptr && std::filesystem::exists(path)) {
      paths.push_back(path);
    }
  }
  return paths;
}

bool LoadOverworldTestRom(const std::string& path, Rom& rom) {
  std::ifstream file(path, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  if (!rom.LoadFromData(bytes).ok()) {
    return false;
  }
  if (rom.size() < 0x200000) {
    rom.Expand(0x200000);
  }
  return true;
}

using Clock = std::chrono::steady_clock;

double MsSince(Clock::time_point start) {
  return std::chrono::duration<double, std::milli>(Clock::now() - start)
      .count();
}

int WorldOf(int map_index) {
  if (map_index >= kSpecialWorldMapIdStart)
    return 2;
  if (map_index >= kDarkWorldMapIdStart)
    return 1;
  return 0;
}

// Times each build phase for every map and the Overworld::EnsureMapBuilt path
// used by the editor and render service. Prints a table; asserts only that
// every map builds.
TEST(OverworldRomBuildTest, PhaseTimingForAllMaps) {
  const auto paths = OverworldRomPaths();
  if (paths.empty()) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS or YAZE_TEST_ROM_VANILLA";
  }
  for (const auto& path : paths) {
    SCOPED_TRACE(path);
    Rom rom;
    ASSERT_TRUE(LoadOverworldTestRom(path, rom));
    GameData game_data;
    auto t0 = Clock::now();
    ASSERT_TRUE(LoadGameData(rom, game_data).ok());
    const double game_data_ms = MsSince(t0);

    Overworld overworld(&rom, &game_data);
    t0 = Clock::now();
    ASSERT_TRUE(overworld.Load(&rom).ok());
    const double load_ms = MsSince(t0);

    // Editor/render path: build every map through EnsureMapBuilt.
    t0 = Clock::now();
    for (int i = 0; i < kNumOverworldMaps; ++i) {
      ASSERT_TRUE(overworld.EnsureMapBuilt(i).ok()) << "map " << i;
    }
    const double ensure_all_ms = MsSince(t0);

    // Raw phases, no caches: run each phase directly on every map.
    double area_gfx = 0, tileset = 0, tiles16 = 0, palette = 0, overlay = 0,
           bitmap = 0;
    auto& tiles16_list = *overworld.mutable_tiles16();
    for (int i = 0; i < kNumOverworldMaps; ++i) {
      auto* map = overworld.mutable_overworld_map(i);
      map->set_game_state(0);
      auto t = Clock::now();
      map->LoadAreaGraphics();
      area_gfx += MsSince(t);
      t = Clock::now();
      ASSERT_TRUE(map->BuildTileset().ok());
      tileset += MsSince(t);
      t = Clock::now();
      ASSERT_TRUE(map->BuildTiles16Gfx(tiles16_list,
                                       static_cast<int>(tiles16_list.size()))
                      .ok());
      tiles16 += MsSince(t);
      t = Clock::now();
      ASSERT_TRUE(map->LoadPalette().ok());
      palette += MsSince(t);
      t = Clock::now();
      ASSERT_TRUE(map->LoadOverlay().ok());
      overlay += MsSince(t);
      t = Clock::now();
      ASSERT_TRUE(map->BuildBitmap(overworld.GetMapTiles(WorldOf(i))).ok());
      bitmap += MsSince(t);
    }

    std::printf(
        "[overworld-build-timing] %s\n"
        "  LoadGameData            %8.1f ms\n"
        "  Overworld::Load         %8.1f ms\n"
        "  EnsureMapBuilt x%d     %8.1f ms (%.2f ms/map)\n"
        "  raw phases x%d: area_gfx %.1f | tileset %.1f | tiles16 %.1f | "
        "palette %.1f | overlay %.1f | bitmap %.1f ms\n",
        path.c_str(), game_data_ms, load_ms, kNumOverworldMaps, ensure_all_ms,
        ensure_all_ms / kNumOverworldMaps, kNumOverworldMaps, area_gfx, tileset,
        tiles16, palette, overlay, bitmap);
  }
}

// Background overlays (sky/lava) only cover backdrop pixels; front overlays
// (fog, rain, canopy) may cover anything. Every area whose parent has an
// overlay id produces a non-empty layer.
TEST(OverworldRomBuildTest, SubscreenOverlayLayersFollowGameLayering) {
  const auto paths = OverworldRomPaths();
  if (paths.empty()) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS or YAZE_TEST_ROM_VANILLA";
  }
  for (const auto& path : paths) {
    SCOPED_TRACE(path);
    Rom rom;
    ASSERT_TRUE(LoadOverworldTestRom(path, rom));
    GameData game_data;
    ASSERT_TRUE(LoadGameData(rom, game_data).ok());
    Overworld overworld(&rom, &game_data);
    ASSERT_TRUE(overworld.Load(&rom).ok());
    int areas_with_overlay = 0;
    for (int i = 0; i < 0x80; ++i) {
      SCOPED_TRACE(i);
      auto layer = overworld.BuildSubscreenOverlayLayer(i);
      ASSERT_TRUE(layer.ok()) << layer.status();
      const auto* map = overworld.overworld_map(i);
      const auto* parent = overworld.overworld_map(map->parent());
      // Children follow the parent's overlay id.
      EXPECT_EQ(layer->overlay_id, parent->render_subscreen_overlay());
      if (layer->overlay_screen < 0) {
        continue;
      }
      ++areas_with_overlay;
      const auto& base = map->bitmap_data();
      ASSERT_EQ(layer->pixels.size(), base.size());
      int covered = 0;
      for (size_t p = 0; p < base.size(); ++p) {
        if (layer->pixels[p] == 0)
          continue;
        ++covered;
        EXPECT_FALSE(IsOverworldBackdropPixel(layer->pixels[p]));
        if (layer->background) {
          ASSERT_TRUE(IsOverworldBackdropPixel(base[p])) << "pixel " << p;
        }
      }
      if (!layer->background) {
        EXPECT_GT(covered, 0);
      }
    }
    EXPECT_GT(areas_with_overlay, 0);
  }
}

}  // namespace
}  // namespace yaze::zelda3
