#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/editor/overworld/maps/map_properties.h"
#include "app/editor/overworld/overworld_undo_actions.h"
#include "e2e/rom_dependent/editor_save_test_base.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::test {
namespace {

enum class MapSaveRomClass { kVanilla, kExpanded, kOracle };

class SaveTrustTile32Test
    : public EditorSaveTestBase,
      public ::testing::WithParamInterface<MapSaveRomClass> {
 protected:
  using Range = std::pair<uint32_t, uint32_t>;

  static std::vector<uint8_t> FileBytes(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
  }

  void SetUp() override {
    // Reuse EditorSaveTestBase's file writer, without its vanilla-only setup.
    if (std::getenv("YAZE_SKIP_ROM_TESTS")) {
      GTEST_SKIP() << "ROM tests disabled by YAZE_SKIP_ROM_TESTS";
    }
    if (GetParam() == MapSaveRomClass::kOracle) {
      const char* source = std::getenv("YAZE_TEST_ROM_OOS");
      if (source != nullptr)
        source_path_ = source;
    } else {
      source_path_ = TestRomManager::GetRomPath(
          GetParam() == MapSaveRomClass::kVanilla ? RomRole::kVanilla
                                                  : RomRole::kExpanded);
    }
    if (source_path_.empty() || !std::filesystem::exists(source_path_)) {
      GTEST_SKIP() << "Missing ROM class " << static_cast<int>(GetParam())
                   << "; Oracle requires YAZE_TEST_ROM_OOS";
    }
    if (GetParam() == MapSaveRomClass::kExpanded) {
      if (const char* oracle = std::getenv("YAZE_TEST_ROM_OOS");
          oracle != nullptr && std::filesystem::exists(oracle) &&
          std::filesystem::equivalent(source_path_, oracle)) {
        GTEST_SKIP() << "Expanded role aliases Oracle; supply a distinct "
                        "ZSCustomOverworld v3 ROM";
      }
    }

    source_bytes_ = FileBytes(source_path_);
    ASSERT_FALSE(source_bytes_.empty());
    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze_save_trust_map32_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    test_rom_path_ = (directory_ / "working.sfc").string();
    ASSERT_TRUE(std::filesystem::copy_file(source_path_, test_rom_path_));
    ASSERT_EQ(FileBytes(source_path_), source_bytes_);
    ASSERT_EQ(FileBytes(test_rom_path_), source_bytes_);

    ASSERT_TRUE(rom_.LoadFromFile(test_rom_path_).ok());
    const auto profile = zelda3::DetectOverworldRomProfile(rom_);
    if (GetParam() == MapSaveRomClass::kVanilla) {
      ASSERT_TRUE(profile.is_vanilla);
    } else if (GetParam() == MapSaveRomClass::kExpanded) {
      ASSERT_GE(profile.asm_version, 3);
      ASSERT_NE(profile.asm_version, 0xFF);
      ASSERT_TRUE(profile.has_expanded_tile32);
    }
    ASSERT_TRUE(zelda3::LoadGameData(rom_, data_).ok());
    world_ = std::make_unique<zelda3::Overworld>(&rom_, &data_);
    ASSERT_TRUE(world_->Load(&rom_).ok());
  }

  void TearDown() override {
    world_.reset();
    if (!source_bytes_.empty()) {
      EXPECT_EQ(FileBytes(source_path_), source_bytes_)
          << "Read-only source ROM changed";
    }
    // These are only the uniquely created files owned by this fixture.
    std::error_code error;
    if (!test_rom_path_.empty())
      std::filesystem::remove(test_rom_path_, error);
    if (!directory_.empty())
      std::filesystem::remove(directory_, error);
  }

  absl::Status SerializeMaps() {
    RETURN_IF_ERROR(world_->CreateTile32Tilemap());
    if (zelda3::DetectOverworldRomProfile(rom_).has_expanded_tile32) {
      RETURN_IF_ERROR(world_->SaveMap32Expanded());
    } else {
      RETURN_IF_ERROR(world_->SaveMap32Tiles());
    }
    return world_->SaveOverworldMaps();
  }

  absl::Status SaveMaps() {
    RETURN_IF_ERROR(SerializeMaps());
    RETURN_IF_ERROR(SaveRomToFile(&rom_, test_rom_path_));
    world_->CommitSavedMapTiles();
    return absl::OkStatus();
  }

  std::unique_ptr<editor::OverworldTilePaintAction> PaintOneTile() {
    auto& tiles = world_->GetMapTiles(0);
    const auto before = tiles[0][0];
    const uint16_t after = before == 0 ? 1 : 0;
    auto action = std::make_unique<editor::OverworldTilePaintAction>(
        0, 0, std::vector<editor::OverworldTileChange>{{0, 0, before, after}},
        world_.get(), [] {});
    EXPECT_TRUE(action->Redo().ok());
    return action;
  }

  absl::Status LoadFresh(Rom& rom, zelda3::GameData& data,
                         std::unique_ptr<zelda3::Overworld>& world) {
    RETURN_IF_ERROR(rom.LoadFromFile(test_rom_path_));
    RETURN_IF_ERROR(zelda3::LoadGameData(rom, data));
    world = std::make_unique<zelda3::Overworld>(&rom, &data);
    return world->Load(&rom);
  }

  ::testing::AssertionResult FreshTileEquals(uint16_t expected) {
    Rom fresh;
    zelda3::GameData data;
    std::unique_ptr<zelda3::Overworld> world;
    const auto status = LoadFresh(fresh, data, world);
    if (!status.ok())
      return ::testing::AssertionFailure() << status;
    const auto actual = world->GetMapTiles(0)[0][0];
    if (actual != expected) {
      return ::testing::AssertionFailure()
             << "Fresh-file tile expected " << expected << ", got " << actual;
    }
    return ::testing::AssertionSuccess();
  }

  ::testing::AssertionResult Contained(const std::vector<uint8_t>& before,
                                       const std::vector<uint8_t>& after,
                                       const std::vector<Range>& ranges) {
    if (before.size() != after.size()) {
      return ::testing::AssertionFailure() << "ROM size changed";
    }
    for (size_t index = 0; index < before.size(); ++index) {
      if (before[index] == after[index])
        continue;
      if (std::none_of(ranges.begin(), ranges.end(),
                       [index](const Range& range) {
                         return index >= range.first && index < range.second;
                       })) {
        return ::testing::AssertionFailure()
               << "Write outside declared domain at PC 0x" << std::hex << index;
      }
    }
    return ::testing::AssertionSuccess();
  }

  std::vector<Range> MapWriteRanges() {
    // The production projection declares Tile32, compressed streams, and
    // pointers. Tile16 is a different domain and is deliberately excluded.
    auto ranges = world_->GetProjectedWriteRanges();
    const auto profile = zelda3::DetectOverworldRomProfile(rom_);
    const uint32_t tile16 = profile.has_expanded_tile16
                                ? zelda3::GetMap16TilesExpanded()
                                : zelda3::kMap16Tiles;
    std::erase_if(
        ranges, [tile16](const Range& range) { return range.first == tile16; });
    const auto add = [&ranges](uint32_t start, uint32_t size) {
      ranges.emplace_back(start, start + size);
    };
    if (profile.has_expanded_tile32) {
      for (uint32_t address :
           {0x176EC, 0x176F3, 0x176FA, 0x17701, 0x17708, 0x1771A, 0x1772C,
            0x17733, 0x1773A, 0x17741, 0x17748, 0x1775A, 0x1776C, 0x17773,
            0x1777A, 0x17781, 0x17788, 0x1779A}) {
        add(address, 3);
      }
    }
    // SaveOverworldMaps includes SaveLargeMaps, even when grids are unchanged.
    if (profile.supports_area_enum) {
      add(zelda3::GetOverworldMapParentIdExpanded(), zelda3::kNumOverworldMaps);
      for (int address : {zelda3::transition_target_northExpanded,
                          zelda3::transition_target_westExpanded,
                          zelda3::GetOverworldTransitionPositionXExpanded(),
                          zelda3::GetOverworldTransitionPositionYExpanded(),
                          zelda3::GetOverworldScreenChange1Expanded(),
                          zelda3::GetOverworldScreenChange2Expanded(),
                          zelda3::GetOverworldScreenChange3Expanded(),
                          zelda3::GetOverworldScreenChange4Expanded()}) {
        add(address, zelda3::kNumOverworldMaps * 2);
      }
    } else {
      add(zelda3::kOverworldMapParentId, 64);
      add(zelda3::kOverworldMapSize, 64);
      add(zelda3::kOverworldMapSizeHighByte, 64);
      add(zelda3::kOverworldScreenSize, 128);
      add(zelda3::kOverworldScreenSizeForLoading, zelda3::kNumOverworldMaps);
      add(0x1262C, 8);
      for (int address :
           {zelda3::kTransitionTargetNorth, zelda3::kTransitionTargetWest,
            zelda3::kOverworldTransitionPositionX,
            zelda3::kOverworldTransitionPositionY,
            zelda3::kOverworldScreenTileMapChangeByScreen1,
            zelda3::kOverworldScreenTileMapChangeByScreen2,
            zelda3::kOverworldScreenTileMapChangeByScreen3,
            zelda3::kOverworldScreenTileMapChangeByScreen4}) {
        add(address, 128);
      }
    }
    return ranges;
  }

  int PropertyMap() const {
    for (int map = 0; map < 0x80; ++map) {
      if (world_->overworld_map(map)->parent() == map)
        return map;
    }
    return -1;
  }

  ::testing::AssertionResult FreshPropertyEquals(int map, int expected) {
    Rom fresh;
    zelda3::GameData data;
    std::unique_ptr<zelda3::Overworld> world;
    const auto status = LoadFresh(fresh, data, world);
    if (!status.ok())
      return ::testing::AssertionFailure() << status;
    const int actual = world->overworld_map(map)->area_graphics_for_save();
    if (actual != expected) {
      return ::testing::AssertionFailure()
             << "Fresh-file area graphics expected " << expected << ", got "
             << actual;
    }
    return ::testing::AssertionSuccess();
  }

  Rom rom_;
  zelda3::GameData data_;
  std::unique_ptr<zelda3::Overworld> world_;
  std::filesystem::path directory_;
  std::string source_path_;
  std::vector<uint8_t> source_bytes_;
};

TEST_P(SaveTrustTile32Test, I1NoOpMapsPreserveFile) {
  ASSERT_TRUE(SaveMaps().ok());
  EXPECT_EQ(FileBytes(test_rom_path_), source_bytes_);
}

TEST_P(SaveTrustTile32Test, I2MapsRoundTripThroughFreshReader) {
  const auto action = PaintOneTile();
  const uint16_t expected = world_->GetMapTiles(0)[0][0];
  const auto light = world_->GetMapTiles(0);
  const auto dark = world_->GetMapTiles(1);
  const auto special = world_->GetMapTiles(2);
  ASSERT_TRUE(SaveMaps().ok());
  EXPECT_TRUE(FreshTileEquals(expected));
  Rom fresh;
  zelda3::GameData data;
  std::unique_ptr<zelda3::Overworld> world;
  ASSERT_TRUE(LoadFresh(fresh, data, world).ok());
  EXPECT_EQ(world->GetMapTiles(0), light);
  EXPECT_EQ(world->GetMapTiles(1), dark);
  EXPECT_EQ(world->GetMapTiles(2), special);
}

TEST_P(SaveTrustTile32Test, I3MapsUndoBeforeSavePreservesFile) {
  const auto action = PaintOneTile();
  ASSERT_TRUE(action->Undo().ok());
  ASSERT_TRUE(SaveMaps().ok());
  EXPECT_EQ(FileBytes(test_rom_path_), source_bytes_);
}

TEST_P(SaveTrustTile32Test, I4MapsStayWithinDeclaredRegions) {
  const auto before = rom_.vector();
  const auto ranges = MapWriteRanges();
  const auto action = PaintOneTile();
  ASSERT_TRUE(SaveMaps().ok());
  Rom fresh;
  ASSERT_TRUE(fresh.LoadFromFile(test_rom_path_).ok());
  EXPECT_TRUE(Contained(before, fresh.vector(), ranges));
}

TEST_P(SaveTrustTile32Test, OmittedMapDiskSaveFailsSameFreshAssertion) {
  const uint16_t original = world_->GetMapTiles(0)[0][0];
  const auto action = PaintOneTile();
  const uint16_t expected = world_->GetMapTiles(0)[0][0];
  ASSERT_NE(original, expected);
  ASSERT_TRUE(SerializeMaps().ok());
  // Only SaveRomToFile is omitted; the same fresh-reader assertion must fail.
  EXPECT_FALSE(FreshTileEquals(expected));
  EXPECT_TRUE(FreshTileEquals(original));
}

TEST_P(SaveTrustTile32Test, I1NoOpPropertiesPreserveFile) {
  ASSERT_TRUE(world_->SaveMapProperties().ok());
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  EXPECT_EQ(FileBytes(test_rom_path_), source_bytes_);
}

TEST_P(SaveTrustTile32Test, I2PropertiesRoundTripThroughFreshReader) {
  const int map = PropertyMap();
  ASSERT_GE(map, 0);
  const int before = world_->overworld_map(map)->area_graphics_for_save();
  const int after = before == 0 ? 1 : 0;
  editor::MapPropertiesSystem properties(world_.get(), &rom_);
  ASSERT_TRUE(
      properties
          .ApplyPropertyEditDirect(
              {map, editor::OverworldPropertyField::kAreaGraphics, 0, after})
          .ok());
  ASSERT_TRUE(world_->SaveMapProperties().ok());
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  EXPECT_TRUE(FreshPropertyEquals(map, after));
}

TEST_P(SaveTrustTile32Test, I3PropertiesUndoBeforeSavePreservesFile) {
  const int map = PropertyMap();
  ASSERT_GE(map, 0);
  const int original = world_->overworld_map(map)->area_graphics_for_save();
  const int edited = original == 0 ? 1 : 0;
  editor::MapPropertiesSystem properties(world_.get(), &rom_);
  editor::OverworldMapPropertyEditAction action(
      {map, editor::OverworldPropertyField::kAreaGraphics, 0, original},
      {map, editor::OverworldPropertyField::kAreaGraphics, 0, edited},
      [&properties](const editor::OverworldPropertyEdit& edit) {
        return properties.ApplyPropertyEditDirect(edit);
      },
      "Edit area graphics");
  ASSERT_TRUE(action.Redo().ok());
  ASSERT_TRUE(action.Undo().ok());
  ASSERT_TRUE(world_->SaveMapProperties().ok());
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  EXPECT_EQ(FileBytes(test_rom_path_), source_bytes_);
}

TEST_P(SaveTrustTile32Test, I4PropertyEditChangesOnlyItsField) {
  const auto before = rom_.vector();
  const int map = PropertyMap();
  ASSERT_GE(map, 0);
  const int original = world_->overworld_map(map)->area_graphics_for_save();
  editor::MapPropertiesSystem properties(world_.get(), &rom_);
  ASSERT_TRUE(properties
                  .ApplyPropertyEditDirect(
                      {map, editor::OverworldPropertyField::kAreaGraphics, 0,
                       original == 0 ? 1 : 0})
                  .ok());
  ASSERT_TRUE(world_->SaveMapProperties().ok());
  ASSERT_TRUE(SaveRomToFile(&rom_, test_rom_path_).ok());
  Rom fresh;
  ASSERT_TRUE(fresh.LoadFromFile(test_rom_path_).ok());
  const uint32_t field = zelda3::kAreaGfxIdPtr + map;
  EXPECT_TRUE(Contained(before, fresh.vector(), {{field, field + 1}}));
}

TEST_P(SaveTrustTile32Test, OmittedPropertyDiskSaveFailsSameFreshAssertion) {
  const int map = PropertyMap();
  ASSERT_GE(map, 0);
  const int before = world_->overworld_map(map)->area_graphics_for_save();
  const int after = before == 0 ? 1 : 0;
  editor::MapPropertiesSystem properties(world_.get(), &rom_);
  ASSERT_TRUE(
      properties
          .ApplyPropertyEditDirect(
              {map, editor::OverworldPropertyField::kAreaGraphics, 0, after})
          .ok());
  ASSERT_TRUE(world_->SaveMapProperties().ok());
  EXPECT_FALSE(FreshPropertyEquals(map, after));
  EXPECT_TRUE(FreshPropertyEquals(map, before));
}

INSTANTIATE_TEST_SUITE_P(
    SaveTrustRoles, SaveTrustTile32Test,
    ::testing::Values(MapSaveRomClass::kVanilla, MapSaveRomClass::kExpanded,
                      MapSaveRomClass::kOracle),
    [](const ::testing::TestParamInfo<MapSaveRomClass>& info) {
      switch (info.param) {
        case MapSaveRomClass::kVanilla:
          return "Vanilla";
        case MapSaveRomClass::kExpanded:
          return "Expanded";
        case MapSaveRomClass::kOracle:
          return "Oracle";
      }
      return "Unknown";
    });

}  // namespace
}  // namespace yaze::test
