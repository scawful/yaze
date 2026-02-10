#include <gtest/gtest.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_palette.h"
#include "testing.h"

namespace yaze::test {
namespace {

std::filesystem::path FindRepoFileUpwards(const std::string& relative_path,
                                          int max_parent_hops = 8) {
  std::filesystem::path base = std::filesystem::current_path();
  for (int i = 0; i <= max_parent_hops; i++) {
    std::filesystem::path candidate = base / relative_path;
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
    if (!base.has_parent_path()) {
      break;
    }
    base = base.parent_path();
  }
  return {};
}

static bool IsHex4(const std::string& s) {
  if (s.size() != 4) {
    return false;
  }
  for (char ch : s) {
    if (!std::isxdigit(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  return true;
}

absl::StatusOr<std::vector<uint16_t>> ParsePaletteWordsFromUsdasm(
    const std::filesystem::path& asm_path, const std::string& label,
    size_t expected_words) {
  std::ifstream file(asm_path);
  if (!file) {
    return absl::NotFoundError(
        absl::StrCat("Failed to open usdasm file: ", asm_path.string()));
  }

  const std::string section_marker = absl::StrCat(".", label);
  bool in_section = false;
  std::vector<uint16_t> words;

  std::string line;
  while (std::getline(file, line)) {
    if (!in_section) {
      if (line.rfind(section_marker, 0) == 0) {
        in_section = true;
      }
      continue;
    }

    // Stop when we hit the next labeled block (".something").
    if (!line.empty() && line[0] == '.') {
      break;
    }

    if (line.find("dw") == std::string::npos) {
      continue;
    }

    // Parse "$XXXX" tokens from "dw" lines.
    size_t pos = 0;
    while (true) {
      pos = line.find('$', pos);
      if (pos == std::string::npos) {
        break;
      }
      if (pos + 5 <= line.size()) {
        const std::string hex4 = line.substr(pos + 1, 4);
        if (IsHex4(hex4)) {
          const uint16_t value =
              static_cast<uint16_t>(std::stoul(hex4, nullptr, 16));
          words.push_back(value);
          if (words.size() == expected_words) {
            return words;
          }
        }
      }
      pos++;
    }
  }

  if (!in_section) {
    return absl::NotFoundError(
        absl::StrFormat("Label '.%s' not found in usdasm (%s)", label,
                        asm_path.string()));
  }

  return absl::FailedPreconditionError(absl::StrFormat(
      "Label '.%s' had %zu words, expected %zu (%s)", label, words.size(),
      expected_words, asm_path.string()));
}

absl::Status WriteWordsLE(std::vector<uint8_t>& rom, size_t offset,
                          const std::vector<uint16_t>& words) {
  const size_t bytes = words.size() * 2;
  if (offset + bytes > rom.size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "WriteWordsLE out of bounds: off=%zu bytes=%zu rom_size=%zu", offset,
        bytes, rom.size()));
  }

  for (size_t i = 0; i < words.size(); i++) {
    const size_t idx = offset + (i * 2);
    const uint16_t word = words[i];
    rom[idx] = static_cast<uint8_t>(word & 0xFF);
    rom[idx + 1] = static_cast<uint8_t>((word >> 8) & 0xFF);
  }

  return absl::OkStatus();
}

}  // namespace

TEST(UsdasmPaletteLoadingTest, Bank1BPalettesLoadWithCorrectShapeAndValues) {
  const std::filesystem::path asm_path =
      FindRepoFileUpwards("assets/asm/usdasm/bank_1B.asm");
  ASSERT_FALSE(asm_path.empty())
      << "Could not locate assets/asm/usdasm/bank_1B.asm from cwd="
      << std::filesystem::current_path();

  std::vector<uint16_t> ow_main_00;
  std::vector<uint16_t> ow_aux_00;
  std::vector<uint16_t> ow_anim_00;
  std::vector<uint16_t> dungeon_00;

  ASSERT_OK_AND_ASSIGN(
      ow_main_00,
      ParsePaletteWordsFromUsdasm(asm_path, "owmain_00", /*expected_words=*/35));
  ASSERT_OK_AND_ASSIGN(
      ow_aux_00,
      ParsePaletteWordsFromUsdasm(asm_path, "owaux_00", /*expected_words=*/21));
  ASSERT_OK_AND_ASSIGN(
      ow_anim_00,
      ParsePaletteWordsFromUsdasm(asm_path, "owanim_00", /*expected_words=*/7));
  ASSERT_OK_AND_ASSIGN(
      dungeon_00,
      ParsePaletteWordsFromUsdasm(asm_path, "dungeon_00", /*expected_words=*/90));

  // Synthetic 1MB LoROM image. LoadAllPalettes bounds-checks offsets against
  // the ROM size, so this must cover all palette tables read by that loader.
  std::vector<uint8_t> rom_data(0x100000, 0x00);

  ASSERT_OK(WriteWordsLE(rom_data, gfx::kOverworldPaletteMain, ow_main_00));
  ASSERT_OK(WriteWordsLE(rom_data, gfx::kOverworldPaletteAux, ow_aux_00));
  ASSERT_OK(WriteWordsLE(rom_data, gfx::kOverworldPaletteAnimated, ow_anim_00));
  ASSERT_OK(WriteWordsLE(rom_data, gfx::kDungeonMainPalettes, dungeon_00));

  gfx::PaletteGroupMap groups;
  ASSERT_OK(gfx::LoadAllPalettes(rom_data, groups));

  // Shapes (counts + per-palette sizes) derived from usdasm bank_1B.
  ASSERT_EQ(groups.overworld_main.size(), 6u);
  ASSERT_EQ(groups.overworld_aux.size(), 20u);
  ASSERT_EQ(groups.overworld_animated.size(), 14u);
  ASSERT_EQ(groups.dungeon_main.size(), 20u);

  ASSERT_EQ(groups.overworld_main.palette_ref(0).size(), ow_main_00.size());
  ASSERT_EQ(groups.overworld_aux.palette_ref(0).size(), ow_aux_00.size());
  ASSERT_EQ(groups.overworld_animated.palette_ref(0).size(), ow_anim_00.size());
  ASSERT_EQ(groups.dungeon_main.palette_ref(0).size(), dungeon_00.size());

  // Values: verify the first palette in each group matches usdasm words exactly.
  for (size_t i = 0; i < ow_main_00.size(); i++) {
    EXPECT_EQ(groups.overworld_main.palette_ref(0)[i].snes(), ow_main_00[i])
        << "ow_main[0][" << i << "]";
  }
  for (size_t i = 0; i < ow_aux_00.size(); i++) {
    EXPECT_EQ(groups.overworld_aux.palette_ref(0)[i].snes(), ow_aux_00[i])
        << "ow_aux[0][" << i << "]";
  }
  for (size_t i = 0; i < ow_anim_00.size(); i++) {
    EXPECT_EQ(groups.overworld_animated.palette_ref(0)[i].snes(), ow_anim_00[i])
        << "ow_animated[0][" << i << "]";
  }
  for (size_t i = 0; i < dungeon_00.size(); i++) {
    EXPECT_EQ(groups.dungeon_main.palette_ref(0)[i].snes(), dungeon_00[i])
        << "dungeon_main[0][" << i << "]";
  }
}

}  // namespace yaze::test
