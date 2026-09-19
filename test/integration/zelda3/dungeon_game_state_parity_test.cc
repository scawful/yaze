// Compares what yaze loads for each dungeon room with the full machine state
// the real game had right after loading it: graphics (VRAM tile data), BG
// palette (the $7EC300 target buffer) and sprites (the sprite slot tables).
//
// Needs a vanilla ROM and a folder written by
//   scripts/agents/capture-game-room-tilemaps.py --full
// for that ROM:
//   YAZE_GAME_CAPTURE_DIR=<dir> yaze_test_integration \
//       --gtest_filter='DungeonGameStateParityTest.*'
// Without the folder the tests skip. Captures are ROM-derived data and must
// not be committed.

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::test {
namespace {

std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

uint16_t Word(const std::vector<uint8_t>& bytes, size_t offset) {
  return static_cast<uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

// WRAM dump offsets ($7E0000 base).
constexpr size_t kMainBlockset = 0x0AA1;    // $0AA1, from the entrance
constexpr size_t kPaletteTarget = 0xC300;   // $7EC300, palette target buffer
constexpr size_t kAnimatedFrame0 = 0xA680;  // $7EA680, animated tile frame 0
// VRAM: BG character data starts at word $2000 (BG12NBA=$22).
constexpr size_t kBgCharBytes = 0x4000;

struct CapturedRoom {
  int room_id;
  std::vector<uint8_t> wram;
  std::vector<uint8_t> vram;
};

class DungeonGameStateParityTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const char* dir = std::getenv("YAZE_GAME_CAPTURE_DIR");
    if (dir == nullptr) {
      GTEST_SKIP() << "Set YAZE_GAME_CAPTURE_DIR to a --full capture folder.";
    }
    capture_dir_ = dir;
    YAZE_SKIP_IF_ROM_MISSING(RomRole::kVanilla, "DungeonGameStateParityTest");
    rom_ = std::make_unique<Rom>();
    ASSERT_TRUE(
        rom_->LoadFromFile(TestRomManager::GetRomPath(RomRole::kVanilla)).ok());
    ASSERT_TRUE(zelda3::LoadGameData(*rom_, game_data_).ok());
    std::ifstream manifest_in(capture_dir_ / "manifest.json");
    manifest_ = nlohmann::json::parse(manifest_in, nullptr, false);
  }

  // Rooms with a complete --full capture, ascending.
  std::vector<CapturedRoom> CapturedRooms() const {
    std::vector<CapturedRoom> rooms;
    for (int room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
      const auto base = capture_dir_ / absl::StrFormat("room_%03X", room_id);
      auto wram = ReadFile(base.string() + ".wram");
      auto vram = ReadFile(base.string() + ".vram");
      if (wram.size() != 0x20000 || vram.size() != 0x10000) {
        continue;
      }
      rooms.push_back({room_id, std::move(wram), std::move(vram)});
    }
    return rooms;
  }

  // The entrance-selection method the capture used for a room ("direct",
  // "dungeon-map N", "neighbour 0xNNN", "fixed", ...).
  std::string EntranceMethod(int room_id) const {
    const std::string key = absl::StrFormat("0x%03X", room_id);
    if (!manifest_.is_object() || !manifest_.contains("rooms") ||
        !manifest_["rooms"].contains(key)) {
      return "";
    }
    return manifest_["rooms"][key].value("entrance_method", "");
  }

  std::filesystem::path capture_dir_;
  std::unique_ptr<Rom> rom_;
  zelda3::GameData game_data_;
  nlohmann::json manifest_;
};

// The main graphics set comes from the entrance, which yaze now infers per
// room (room_default_entrance.h). Only meaningful for captures made with
// --entrance auto; a fixed-entrance capture reports its fixed set.
TEST_F(DungeonGameStateParityTest, DefaultMainBlocksetMatchesCapture) {
  int compared = 0;
  int matching = 0;
  for (const auto& room : CapturedRooms()) {
    const std::string method = EntranceMethod(room.room_id);
    if (method.empty() || method == "fixed") {
      continue;
    }
    ++compared;
    const uint8_t game = room.wram[kMainBlockset];
    const uint8_t yaze =
        game_data_.room_default_entrances[room.room_id].main_blockset;
    if (game == yaze) {
      ++matching;
    } else {
      ADD_FAILURE() << absl::StrFormat(
          "room 0x%03X: game main blockset %d, yaze default %d (capture "
          "entrance method: %s)",
          room.room_id, game, yaze, method);
    }
  }
  if (compared == 0) {
    GTEST_SKIP() << "No --entrance auto captures in " << capture_dir_;
  }
  std::cout << absl::StrFormat("main blockset: %d/%d rooms match\n", matching,
                               compared);
}

// Every BG tile the game loaded must decode to the same 8x8 color indices as
// yaze's room graphics buffer, given the same main blockset.
TEST_F(DungeonGameStateParityTest, BackgroundTileGraphicsMatchCapture) {
  int rooms = 0;
  int rooms_exact = 0;
  long tiles_different = 0;
  std::map<int, int> sheet_mismatches;  // block index -> rooms
  for (const auto& captured : CapturedRooms()) {
    const uint8_t main_blockset = captured.wram[kMainBlockset];
    zelda3::Room room = zelda3::LoadRoomFromRom(rom_.get(), captured.room_id);
    room.SetGameData(&game_data_);
    const std::string method = EntranceMethod(captured.room_id);
    if (method.empty() || method == "fixed") {
      // A fixed-entrance capture: render with the same main set it used.
      room.SetRenderEntranceBlockset(main_blockset);
      room.LoadRoomGraphics(main_blockset);
    } else {
      // Per-room entrances: yaze must pick the main set on its own, as the
      // editor does when no entrance is selected.
      room.LoadRoomGraphics();
    }
    room.CopyRoomGraphicsToBuffer();
    const auto& gfx = room.get_gfx_buffer();

    int room_differences = 0;
    for (int tile = 0; tile < 0x200; ++tile) {
      const bool animated = tile >= 0x1B0 && tile < 0x1D0;
      // Animated tiles cycle; compare with WRAM frame 0, which yaze models.
      const uint8_t* planar =
          animated ? &captured.wram[kAnimatedFrame0 + (tile - 0x1B0) * 32]
                   : &captured.vram[kBgCharBytes + tile * 32];
      bool same = true;
      for (int y = 0; y < 8 && same; ++y) {
        for (int x = 0; x < 8; ++x) {
          const int bit = 7 - x;
          const int index = ((planar[y * 2] >> bit) & 1) |
                            (((planar[y * 2 + 1] >> bit) & 1) << 1) |
                            (((planar[16 + y * 2] >> bit) & 1) << 2) |
                            (((planar[16 + y * 2 + 1] >> bit) & 1) << 3);
          const uint8_t yaze =
              gfx[(tile >> 4) * 1024 + (tile & 15) * 8 + y * 128 + x];
          if (index != yaze) {
            same = false;
            break;
          }
        }
      }
      if (!same) {
        ++room_differences;
        ++sheet_mismatches[tile / 64];
      }
    }
    ++rooms;
    tiles_different += room_differences;
    if (room_differences == 0) {
      ++rooms_exact;
    } else {
      ADD_FAILURE() << absl::StrFormat(
          "room 0x%03X (main blockset %d): %d of 512 BG tiles differ",
          captured.room_id, main_blockset, room_differences);
    }
  }
  if (rooms == 0) {
    GTEST_SKIP() << "No --full captures in " << capture_dir_;
  }
  std::cout << absl::StrFormat(
      "BG graphics: %d/%d rooms exact, %ld differing tiles\n", rooms_exact,
      rooms, tiles_different);
  for (const auto& [sheet, count] : sheet_mismatches) {
    std::cout << absl::StrFormat("  sheet slot %d: %d tile differences\n",
                                 sheet, count);
  }
}

// CGRAM rows 2-7, colors 1-15 come from the room header's palette set
// (Palettes_Load_UnderworldSet). Color 0 of each row is transparent for BG.
TEST_F(DungeonGameStateParityTest, BackgroundPaletteMatchesCapture) {
  int rooms = 0;
  int rooms_exact = 0;
  for (const auto& captured : CapturedRooms()) {
    zelda3::Room room =
        zelda3::LoadRoomHeaderFromRom(rom_.get(), captured.room_id);
    room.SetGameData(&game_data_);
    const int palette_id = room.ResolveDungeonPaletteId();
    ASSERT_GE(palette_id, 0);
    std::array<uint16_t, 256> cgram{};
    zelda3::LoadDungeonRenderPaletteToCgram(
        cgram, game_data_.palette_groups.dungeon_main[palette_id]);
    int differences = 0;
    for (int row = 2; row < 8; ++row) {
      for (int color = 1; color < 16; ++color) {
        const int index = row * 16 + color;
        const uint16_t game =
            Word(captured.wram, kPaletteTarget + index * 2) & 0x7FFF;
        if (cgram[index] != game) {
          ++differences;
        }
      }
    }
    ++rooms;
    if (differences == 0) {
      ++rooms_exact;
    } else {
      ADD_FAILURE() << absl::StrFormat(
          "room 0x%03X (palette %d): %d of 90 BG colors differ",
          captured.room_id, palette_id, differences);
    }
  }
  if (rooms == 0) {
    GTEST_SKIP() << "No --full captures in " << capture_dir_;
  }
  std::cout << absl::StrFormat("BG palette: %d/%d rooms exact\n", rooms_exact,
                               rooms);
}

}  // namespace
}  // namespace yaze::test
