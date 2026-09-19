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
    // "retried" rooms fell back to entrance 0x34 in the harness, so their
    // main set is 0x34's, not the room's own.
    if (method.empty() || method == "fixed" ||
        method.find("retried") != std::string::npos) {
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
    if (method.empty() || method == "fixed" ||
        method.find("retried") != std::string::npos) {
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

// The game loads every sprite of a room at once (Underworld_LoadSprites):
// slot N holds the Nth non-overlord entry and overlords fill overlord slots
// 7, 6, ... A slot still holds a room-list sprite when $0DD0 != 0 and
// $0BC0 == N. Type, layer and key drop are stable after load except for the
// SpritePrep rewrites below; positions can move a little from init offsets
// and AI, so they get a 3-tile tolerance.
TEST_F(DungeonGameStateParityTest, SpritesMatchCapture) {
  constexpr size_t kState = 0x0DD0, kSlotMarker = 0x0BC0, kType = 0x0E20;
  constexpr size_t kLayer = 0x0F20, kDrop = 0x0CBA;
  constexpr size_t kYLo = 0x0D00, kXLo = 0x0D10, kYHi = 0x0D20, kXHi = 0x0D30;
  constexpr size_t kOverlordType = 0x0B00, kOverlordXLo = 0x0B08;
  constexpr size_t kOverlordXHi = 0x0B10, kOverlordYLo = 0x0B18;
  constexpr size_t kOverlordYHi = 0x0B20, kOverlordLayer = 0x0B40;
  int rooms = 0, rooms_exact = 0, sprites_checked = 0, overlords_checked = 0;
  int sprites_not_loaded = 0;
  for (const auto& captured : CapturedRooms()) {
    const auto& w = captured.wram;
    zelda3::Room room = zelda3::LoadRoomFromRom(rom_.get(), captured.room_id);
    room.LoadSprites();
    const int base_x = (captured.room_id & 0x0F) * 0x200;
    const int base_y = (captured.room_id >> 4) * 0x200;
    int slot = 0;
    int overlord_slot = 7;
    int differences = 0;
    for (const auto& sprite : room.GetSprites()) {
      const int x = base_x + sprite.x() * 16;
      const int y = base_y + sprite.y() * 16;
      if (sprite.IsOverlord()) {
        if (overlord_slot < 0) {
          continue;
        }
        const size_t s = static_cast<size_t>(overlord_slot--);
        int game_x = w[kOverlordXLo + s] | (w[kOverlordXHi + s] << 8);
        const int game_y = w[kOverlordYLo + s] | (w[kOverlordYHi + s] << 8);
        if (w[kOverlordType + s] == 0x03) {
          game_x += 8;  // Underworld_LoadSingleOverlord moves type 3 by -8.
        }
        ++overlords_checked;
        if (w[kOverlordType + s] != sprite.id() || game_x != x || game_y != y ||
            w[kOverlordLayer + s] != sprite.layer()) {
          ++differences;
          ADD_FAILURE() << absl::StrFormat(
              "room 0x%03X overlord slot %zu: game type %02X (%d,%d) layer %d, "
              "yaze %02X (%d,%d) layer %d",
              captured.room_id, s, w[kOverlordType + s], game_x, game_y,
              w[kOverlordLayer + s], sprite.id(), x, y, sprite.layer());
        }
        continue;
      }
      const size_t s = static_cast<size_t>(slot++);
      if (s >= 16) {
        continue;
      }
      if (w[kState + s] == 0 || w[kSlotMarker + s] != s) {
        ++sprites_not_loaded;  // Removed by game state or replaced at runtime.
        continue;
      }
      ++sprites_checked;
      // SpritePrep code that rewrites the loaded values (usdasm bank_06):
      // SpritePrep_Debirando turns 0x64 into its pit 0x63 and spawns the 0x64
      // separately; SpritePrep_BonkItem puts bonk items (0x3B) on layer 2
      // indoors.
      int expected_type = sprite.id();
      int expected_layer = sprite.layer();
      if (expected_type == 0x64) {
        expected_type = 0x63;
      }
      if (expected_type == 0x3B) {
        expected_layer = 2;
      }
      const int game_x = w[kXLo + s] | (w[kXHi + s] << 8);
      const int game_y = w[kYLo + s] | (w[kYHi + s] << 8);
      const bool near =
          std::abs(game_x - x) <= 48 && std::abs(game_y - y) <= 48;
      if (w[kType + s] != expected_type || w[kLayer + s] != expected_layer ||
          w[kDrop + s] != sprite.key_drop() || !near) {
        ++differences;
        ADD_FAILURE() << absl::StrFormat(
            "room 0x%03X slot %zu: game type %02X (%d,%d) layer %d drop %d, "
            "yaze %02X (%d,%d) layer %d drop %d",
            captured.room_id, s, w[kType + s], game_x, game_y, w[kLayer + s],
            w[kDrop + s], sprite.id(), x, y, sprite.layer(), sprite.key_drop());
      }
    }
    ++rooms;
    rooms_exact += differences == 0;
  }
  if (rooms == 0) {
    GTEST_SKIP() << "No --full captures in " << capture_dir_;
  }
  std::cout << absl::StrFormat(
      "sprites: %d/%d rooms exact; %d sprites and %d overlords checked, %d "
      "not loaded by the game in this state\n",
      rooms_exact, rooms, sprites_checked, overlords_checked,
      sprites_not_loaded);
}

}  // namespace
}  // namespace yaze::test
