#include "zelda3/overworld/overworld_allocation.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <sstream>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "nlohmann/json.hpp"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"

namespace yaze::zelda3 {

namespace {

using ordered_json = nlohmann::ordered_json;

constexpr uint32_t kAsmVersionPc = 0x140145;       // ZS "ASM applied" byte.
constexpr uint32_t kZsFunctionSpacePc = 0x141940;  // $28:9940.
constexpr int kMap32PerScreen = 256;
constexpr uint32_t kMap32HighPc = 0x1794D;
constexpr uint32_t kMap32LowPc = 0x17B2D;
constexpr uint32_t kPoolParentPc = 0x140998;
constexpr uint32_t kPoolOverlayPc = 0x140340;
constexpr uint32_t kAreaSizePc = 0x1788D;
constexpr uint32_t kVanillaParentPc = 0x125EC;
constexpr std::array<uint32_t, 3> kPoolSpritePointerPcs = {0x141438, 0x141578,
                                                           0x1416B8};
constexpr uint32_t kOverworldSpriteBankPc = 0x48000;  // $09:8000.
constexpr int kMaxSpritesPerList = 0x80;

// Subscreen overlay IDs that ZSCustomOverworld (or vanilla) code loads or
// tests by value, independent of any table entry.
constexpr std::array<int, 8> kZsCodeOverlayIds = {0x93, 0x94, 0x95, 0x96,
                                                  0x9C, 0x9D, 0x9E, 0x9F};
constexpr std::array<int, 9> kVanillaCodeOverlayIds = {
    0x93, 0x94, 0x95, 0x96, 0x97, 0x9C, 0x9D, 0x9E, 0x9F};

uint32_t LoRomSnesToPc(uint32_t snes) {
  return ((snes & 0x7F0000) >> 1) | (snes & 0x7FFF);
}

uint32_t LoRomPcToSnes(uint32_t pc) {
  return ((pc << 1) & 0x7F0000) | (pc & 0x7FFF) | 0x8000;
}

std::string SnesText(uint32_t snes) {
  return absl::StrFormat("$%02X:%04X", (snes >> 16) & 0xFF, snes & 0xFFFF);
}

std::string HexByte(int value) {
  return absl::StrFormat("0x%02X", value);
}

std::string AreaText(int area) {
  return absl::StrFormat("$%02X", area);
}

std::string RangeText(int first, int last) {
  return absl::StrFormat("$%02X-$%02X", first, last);
}

bool IsZsV3(uint8_t asm_version) {
  return asm_version != 0xFF && asm_version != 0x00 && asm_version >= 3;
}

uint8_t ReadAsmVersion(const Rom& rom) {
  return rom.size() > kAsmVersionPc ? rom.data()[kAsmVersionPc] : 0xFF;
}

bool InRom(const Rom& rom, uint32_t pc, size_t length) {
  return static_cast<size_t>(pc) + length <= rom.size();
}

uint32_t ReadLong(const Rom& rom, uint32_t pc) {
  const uint8_t* d = rom.data();
  return d[pc] | (d[pc + 1] << 8) | (d[pc + 2] << 16);
}

uint16_t ReadWord(const Rom& rom, uint32_t pc) {
  const uint8_t* d = rom.data();
  return static_cast<uint16_t>(d[pc] | (d[pc + 1] << 8));
}

std::string HexBytes(const uint8_t* data, size_t count) {
  std::string out;
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      out += ' ';
    }
    out += absl::StrFormat("%02X", data[i]);
  }
  return out;
}

AreaTableInfo RomTable(const char* id, const char* label, const char* group,
                       uint32_t pc, int entry_bytes, int first_area,
                       int last_area, std::string past_end,
                       std::string note = "") {
  AreaTableInfo table;
  table.id = id;
  table.label = label;
  table.group = group;
  table.in_rom = true;
  table.pc = pc;
  table.snes = LoRomPcToSnes(pc);
  table.entry_bytes = entry_bytes;
  table.first_area = first_area;
  table.last_area = last_area;
  table.past_end = std::move(past_end);
  table.note = std::move(note);
  return table;
}

AreaTableInfo RamTable(const char* id, const char* label, const char* group,
                       uint32_t snes, int entry_bytes, int first_area,
                       int last_area, std::string past_end,
                       std::string note = "") {
  AreaTableInfo table;
  table.id = id;
  table.label = label;
  table.group = group;
  table.in_rom = false;
  table.pc = 0;
  table.snes = snes;
  table.entry_bytes = entry_bytes;
  table.first_area = first_area;
  table.last_area = last_area;
  table.past_end = std::move(past_end);
  table.note = std::move(note);
  return table;
}

std::vector<AreaTableInfo> ZsCustomV3Tables() {
  std::vector<AreaTableInfo> t;
  // Map data.
  t.push_back(RomTable("map32_high", "Map32 pointers (high bytes)", "map",
                       kMap32HighPc, 3, 0x00, 0x9F,
                       "Map32 pointers (low bytes) at $02:FB2D",
                       "TailMapExpansion.asm copies this table to $28:A400 "
                       "with 192 entries"));
  t.push_back(RomTable("map32_low", "Map32 pointers (low bytes)", "map",
                       kMap32LowPc, 3, 0x00, 0x9F,
                       "code (LoadSubscreenOverlay) at $02:FD0D",
                       "TailMapExpansion.asm copies this table to $28:A640 "
                       "with 192 entries"));
  t.push_back(RomTable("area_size", "Area size", "map", kAreaSizePc, 1, 0x00,
                       0xBF, "Map32 pointers (high bytes) at $02:F94D",
                       "0 small, 1 large, 2 wide, 3 tall"));
  t.push_back(RomTable("pool_parent", "Parent area", "map", kPoolParentPc, 1,
                       0x00, 0x9F, "Pool ByScreen1 at $28:8A38"));
  // Graphics and palettes.
  t.push_back(RomTable(
      "area_gfx", "Area graphics ($0AA2, GFX0AA2ValsOW)", "gfx", 0x7C9C, 1,
      0x00, 0x9F, "the old palette table at $00:FD3C (unused after ZS v3)"));
  t.push_back(RomTable(
      "palette_set", "Area palette set (OverworldPalettesScreenToSet_New)",
      "gfx", 0x4C635, 1, 0x00, 0xBF, "", "yaze writes entries $00-$9F only"));
  t.push_back(RomTable("pool_bg_color", "BG color", "gfx", 0x140000, 2, 0x00,
                       0x9F, "Pool enable flags at $28:8140"));
  t.push_back(RomTable("pool_main_palette", "Main palette", "gfx", 0x140160, 1,
                       0x00, 0x9F, "Pool mosaic table at $28:8200"));
  t.push_back(RomTable("pool_mosaic", "Mosaic", "gfx", 0x140200, 1, 0x00, 0x9F,
                       "Pool animated GFX table at $28:82A0"));
  t.push_back(RomTable("pool_animated_gfx", "Animated GFX", "gfx", 0x1402A0, 1,
                       0x00, 0x9F, "Pool subscreen overlay table at $28:8340"));
  t.push_back(RomTable("pool_subscreen_overlay", "Subscreen overlay", "gfx",
                       kPoolOverlayPc, 2, 0x00, 0x9F,
                       "Pool tile GFX groups at $28:8480"));
  t.push_back(RomTable("pool_tile_gfx_groups", "Tile GFX groups (8 sheets)",
                       "gfx", 0x140480, 8, 0x00, 0x9F,
                       "Pool default GFX groups at $28:8980, then the parent "
                       "table"));
  t.push_back(RomTable("sw_sprite_gfx", "Special-world sprite GFX set", "gfx",
                       0x166E1, 1, 0x80, 0x9F,
                       "special-world sprite palettes at $02:E701"));
  t.push_back(RomTable(
      "sw_sprite_palette", "Special-world sprite palette", "gfx", 0x16701, 1,
      0x80, 0xBF, "",
      "Sprite_LoadGfxProperties copies 0x40 bytes to $7E:FDC0; nothing "
      "checks that entries past the authored ones hold palette data"));
  t.push_back(RomTable("sw_entry_gfx",
                       "Special-exit entry GFX ($0AA2, LoadSpecialOverworld)",
                       "gfx", 0x16821, 1, 0x80, 0x8F,
                       "palette_prop_a at $02:E831, then palette_prop_b",
                       "read only when entering through a special exit"));
  // Transitions.
  t.push_back(RomTable("pool_by_screen_1", "Transition ByScreen1 (right)",
                       "transition", 0x140A38, 2, 0x00, 0x9F,
                       "Pool ByScreen2 at $28:8B78"));
  t.push_back(RomTable("pool_by_screen_2", "Transition ByScreen2 (left)",
                       "transition", 0x140B78, 2, 0x00, 0x9F,
                       "Pool ByScreen3 at $28:8CB8"));
  t.push_back(RomTable("pool_by_screen_3", "Transition ByScreen3 (down)",
                       "transition", 0x140CB8, 2, 0x00, 0x9F,
                       "Pool ByScreen4 at $28:8DF8"));
  t.push_back(RomTable("pool_by_screen_4", "Transition ByScreen4 (up)",
                       "transition", 0x140DF8, 2, 0x00, 0x9F,
                       "Pool transition position Y at $28:8F38"));
  t.push_back(RomTable("pool_trans_pos_y", "Transition position Y",
                       "transition", 0x140F38, 2, 0x00, 0x9F,
                       "Pool transition position X at $28:9078"));
  t.push_back(RomTable("pool_trans_pos_x", "Transition position X",
                       "transition", 0x141078, 2, 0x00, 0x9F,
                       "Pool transition target north at $28:91B8"));
  t.push_back(RomTable("pool_trans_target_north", "Transition target north",
                       "transition", 0x1411B8, 2, 0x00, 0x9F,
                       "Pool transition target west at $28:92F8"));
  t.push_back(RomTable("pool_trans_target_west", "Transition target west",
                       "transition", 0x1412F8, 2, 0x00, 0x9F,
                       "Pool sprite pointers (phase 0) at $28:9438"));
  // Sprites.
  t.push_back(RomTable("pool_sprites_phase_0", "Sprite list pointers, phase 0",
                       "sprites", kPoolSpritePointerPcs[0], 2, 0x00, 0x9F,
                       "Pool sprite pointers (phase 1) at $28:9578"));
  t.push_back(RomTable("pool_sprites_phase_1", "Sprite list pointers, phase 1",
                       "sprites", kPoolSpritePointerPcs[1], 2, 0x00, 0x9F,
                       "Pool sprite pointers (phase 2) at $28:96B8"));
  t.push_back(RomTable("pool_sprites_phase_2", "Sprite list pointers, phase 2",
                       "sprites", kPoolSpritePointerPcs[2], 2, 0x00, 0x9F,
                       "Pool sign messages at $28:97F8 (read as bank $09 "
                       "sprite pointers)"));
  // Text.
  t.push_back(RomTable("pool_sign_message", "Sign message", "text", 0x1417F8, 2,
                       0x00, 0x9F,
                       "8 bytes of padding, then ZS code at $28:9940"));
  // Music.
  t.push_back(RomTable("music_lw", "Music, light world (per game state)",
                       "music", 0x14303, 1, 0x00, 0x3F,
                       "the next game state's copy (+$40)",
                       "4 copies, one per game state"));
  t.push_back(RomTable("music_dw_sw", "Music, dark and special world", "music",
                       0x14403, 1, 0x40, 0x9F,
                       "code (AdjustOverworldAmbiance) at $02:C463",
                       "copied to WRAM $7F:5B40+ with the light-world table"));
  // Items and doors.
  t.push_back(RomTable("bomb_doors", "Bombable door locations", "items",
                       0x12644, 2, 0x00, 0x9F,
                       "hidden item pointers at $02:A784"));
  t.push_back(RomTable("hidden_items", "Hidden item pointers", "items", 0x12784,
                       2, 0x00, 0x9F,
                       "data after the table at "
                       "$02:A8C4"));
  // Save data.
  t.push_back(RamTable("area_flags", "Overworld area flags (SRAM)", "sram",
                       0x7EF280, 1, 0x00, 0x7F,
                       "$7E:F300+, save data past the 0x80-byte area block",
                       "overlay and bomb-door bits are set for every area "
                       "visited"));
  return t;
}

std::vector<AreaTableInfo> VanillaTables() {
  std::vector<AreaTableInfo> t;
  t.push_back(RomTable("map32_high", "Map32 pointers (high bytes)", "map",
                       kMap32HighPc, 3, 0x00, 0x9F,
                       "Map32 pointers (low bytes) at $02:FB2D"));
  t.push_back(RomTable("map32_low", "Map32 pointers (low bytes)", "map",
                       kMap32LowPc, 3, 0x00, 0x9F,
                       "code (LoadSubscreenOverlay) at $02:FD0D"));
  t.push_back(RomTable("area_size", "Area size (vanilla: 0 large, 1 small)",
                       "map", kAreaSizePc, 1, 0x00, 0xBF,
                       "Map32 pointers (high bytes) at $02:F94D"));
  t.push_back(RomTable("parent", "Parent area (Overworld_ActualScreenID)",
                       "map", kVanillaParentPc, 1, 0x00, 0x7F,
                       "OverworldScreenTilemapChange at $02:A62C",
                       "dark world reads the light-world entry and adds $40; "
                       "special-world parents are fixed in code"));
  t.push_back(RomTable("area_gfx", "Area graphics ($0AA2, GFXAA2ValsOW)", "gfx",
                       0x7C9C, 1, 0x00, 0x7F,
                       "the area palette table at $00:FD1C"));
  t.push_back(RomTable("area_palette",
                       "Area palette (OverworldPalettesScreen"
                       "ToSet)",
                       "gfx", 0x7D1C, 1, 0x00, 0x87,
                       "code (ResetStarTileGraphics) at $00:FDA4"));
  t.push_back(RomTable("sw_sprite_gfx", "Special-world sprite GFX ($0AA3)",
                       "gfx", 0x16811, 1, 0x80, 0x8F,
                       "special-world area GFX at $02:E821"));
  t.push_back(RomTable("sw_entry_gfx", "Special-world area GFX ($0AA2)", "gfx",
                       0x16821, 1, 0x80, 0x8F, "palette_prop_a at $02:E831"));
  t.push_back(RomTable("sw_palette_a", "Special-world palette (prop a)", "gfx",
                       0x16831, 1, 0x80, 0x8F, "palette_prop_b at $02:E841"));
  t.push_back(RomTable("sw_palette_b", "Special-world palette (prop b)", "gfx",
                       0x16841, 1, 0x80, 0x8F,
                       "code (LoadSpecialOverworld) at $02:E851"));
  t.push_back(RomTable("sign_message", "Sign message (Overworld_SignText)",
                       "text", 0x3F51D, 2, 0x00, 0x8F,
                       "data after the table at $07:F63D"));
  t.push_back(RomTable("music_lw", "Music, light world (per game state)",
                       "music", 0x14303, 1, 0x00, 0x3F,
                       "the next game state's copy (+$40)",
                       "4 copies, one per game state"));
  t.push_back(RomTable("music_dw_sw", "Music, dark and special world", "music",
                       0x14403, 1, 0x40, 0x9F,
                       "code (AdjustOverworldAmbiance) at $02:C463"));
  t.push_back(RomTable("hidden_items", "Hidden item pointers", "items", 0xDC2F9,
                       2, 0x00, 0x7F, "hidden item data"));
  t.push_back(RamTable("area_flags", "Overworld area flags (SRAM)", "sram",
                       0x7EF280, 1, 0x00, 0x7F,
                       "$7E:F300+, save data past the 0x80-byte area block"));
  return t;
}

AllocationAreaSize DecodeAreaSize(uint8_t value, bool zs_v3) {
  if (zs_v3) {
    switch (value) {
      case 1:
        return AllocationAreaSize::kLarge;
      case 2:
        return AllocationAreaSize::kWide;
      case 3:
        return AllocationAreaSize::kTall;
      default:
        return AllocationAreaSize::kSmall;
    }
  }
  return value == 0 ? AllocationAreaSize::kLarge : AllocationAreaSize::kSmall;
}

// Vanilla special-world parents are fixed in code: Zora's Domain
// ($81/$82/$89/$8A) is one large area, every other ID is its own parent.
int VanillaSpecialParent(int area) {
  if (area == 0x81 || area == 0x82 || area == 0x89 || area == 0x8A) {
    return 0x81;
  }
  return area;
}

void ScanAreaMap(const Rom& rom, uint32_t high_table_pc, uint32_t low_table_pc,
                 int index, AreaAllocation* area) {
  const uint32_t high_entry = high_table_pc + 3 * index;
  const uint32_t low_entry = low_table_pc + 3 * index;
  if (!InRom(rom, high_entry, 3) || !InRom(rom, low_entry, 3)) {
    return;
  }
  area->high.snes = ReadLong(rom, high_entry);
  area->low.snes = ReadLong(rom, low_entry);
  auto scan = [&rom](uint32_t snes) {
    const uint32_t pc = LoRomSnesToPc(snes);
    MapStreamScan result;
    if (pc < rom.size()) {
      result = ScanMapStream(rom.data(), rom.size(), pc);
    } else {
      result.pc = pc;
      result.error = "pointer is past the end of the ROM";
    }
    result.snes = snes;
    return result;
  };
  area->high = scan(area->high.snes);
  area->low = scan(area->low.snes);
  area->map_pointer_valid = area->high.in_rom && area->low.in_rom &&
                            area->high.terminated && area->low.terminated &&
                            area->high.decoded_bytes >= kMap32PerScreen &&
                            area->low.decoded_bytes >= kMap32PerScreen;
  if (area->map_pointer_valid) {
    std::set<uint16_t> ids;
    for (int i = 0; i < kMap32PerScreen; ++i) {
      ids.insert(static_cast<uint16_t>((area->high.decoded[i] << 8) |
                                       area->low.decoded[i]));
    }
    area->unique_tile32 = static_cast<int>(ids.size());
  }
  area->high.decoded.clear();
  area->low.decoded.clear();
}

int CountSpriteList(const Rom& rom, uint16_t pointer, bool* valid) {
  *valid = false;
  if (pointer < 0x8000) {
    return 0;
  }
  uint32_t pc = kOverworldSpriteBankPc + (pointer & 0x7FFF);
  int count = 0;
  while (pc < rom.size()) {
    if (rom.data()[pc] == 0xFF) {
      *valid = true;
      return count;
    }
    if (++count > kMaxSpritesPerList) {
      return count;
    }
    pc += 3;
  }
  return count;
}

std::string EvidenceCount(int count, const char* singular, const char* plural) {
  return absl::StrFormat("%d %s", count, count == 1 ? singular : plural);
}

std::string JoinAreas(const std::vector<int>& areas) {
  std::vector<std::string> parts;
  parts.reserve(areas.size());
  for (int area : areas) {
    parts.push_back(AreaText(area));
  }
  return absl::StrJoin(parts, ",");
}

}  // namespace

const char* AllocationWorldName(AllocationWorld world) {
  switch (world) {
    case AllocationWorld::kLight:
      return "light";
    case AllocationWorld::kDark:
      return "dark";
    case AllocationWorld::kSpecial:
      return "special";
  }
  return "special";
}

const char* AllocationAreaSizeName(AllocationAreaSize size) {
  switch (size) {
    case AllocationAreaSize::kSmall:
      return "small";
    case AllocationAreaSize::kLarge:
      return "large";
    case AllocationAreaSize::kWide:
      return "wide";
    case AllocationAreaSize::kTall:
      return "tall";
  }
  return "small";
}

AllocationWorld AllocationWorldForArea(int area) {
  if (area >= 0x80) {
    return AllocationWorld::kSpecial;
  }
  return area >= 0x40 ? AllocationWorld::kDark : AllocationWorld::kLight;
}

std::vector<AreaTableInfo> OverworldAreaTableCatalogue(uint8_t asm_version) {
  std::vector<AreaTableInfo> tables =
      IsZsV3(asm_version) ? ZsCustomV3Tables() : VanillaTables();
  for (auto& table : tables) {
    if (table.id.rfind("sw_", 0) == 0) {
      table.applies_first = 0x80;  // Indexed with the area ID minus $80.
    } else if (table.id == "music_lw") {
      table.applies_last = 0x3F;
    } else if (table.id == "music_dw_sw") {
      table.applies_first = 0x40;
    }
  }
  return tables;
}

MapStreamScan ScanMapStream(const uint8_t* data, size_t size, uint32_t pc,
                            size_t max_output) {
  MapStreamScan scan;
  scan.pc = pc;
  scan.snes = LoRomPcToSnes(pc);
  scan.in_rom = pc < size;
  if (!scan.in_rom) {
    scan.error = "stream starts past the end of the data";
    return scan;
  }
  size_t src = pc;
  std::vector<uint8_t>& out = scan.decoded;
  auto need = [&](size_t count) {
    return src + count <= size;
  };
  while (true) {
    if (!need(1)) {
      scan.error = "ran past the end of the data before $FF";
      break;
    }
    const uint8_t header = data[src++];
    if (header == 0xFF) {
      scan.terminated = true;
      break;
    }
    int command = header >> 5;
    int length = 0;
    if (command == 7) {
      if (!need(1)) {
        scan.error = "ran past the end of the data in a long header";
        break;
      }
      command = (header >> 2) & 7;
      length = ((header & 0x03) << 8) | data[src++];
    } else {
      length = header & 0x1F;
    }
    ++length;
    bool ok = true;
    switch (command) {
      case 0:  // Direct copy.
        if (!need(length)) {
          ok = false;
          break;
        }
        out.insert(out.end(), data + src, data + src + length);
        src += length;
        break;
      case 1:  // Byte fill.
        if (!need(1)) {
          ok = false;
          break;
        }
        out.insert(out.end(), length, data[src++]);
        break;
      case 2: {  // Word fill.
        if (!need(2)) {
          ok = false;
          break;
        }
        const uint8_t a = data[src];
        const uint8_t b = data[src + 1];
        src += 2;
        for (int i = 0; i < length; ++i) {
          out.push_back((i & 1) ? b : a);
        }
        break;
      }
      case 3: {  // Increasing fill.
        if (!need(1)) {
          ok = false;
          break;
        }
        uint8_t value = data[src++];
        for (int i = 0; i < length; ++i) {
          out.push_back(value++);
        }
        break;
      }
      default: {  // Copy from output, big-endian offset (map streams).
        if (!need(2)) {
          ok = false;
          break;
        }
        size_t offset = (static_cast<size_t>(data[src]) << 8) | data[src + 1];
        src += 2;
        for (int i = 0; i < length; ++i) {
          if (offset >= out.size()) {
            scan.error = "copy offset points past the decoded bytes";
            ok = false;
            break;
          }
          out.push_back(out[offset++]);
        }
        break;
      }
    }
    if (!ok) {
      if (scan.error.empty()) {
        scan.error = "ran past the end of the data inside a command";
      }
      break;
    }
    if (out.size() > max_output) {
      scan.error = absl::StrFormat("decoded more than %d bytes without $FF",
                                   static_cast<int>(max_output));
      break;
    }
  }
  scan.compressed_bytes = static_cast<int>(src - pc);
  scan.decoded_bytes = static_cast<int>(out.size());
  return scan;
}

std::vector<std::string> OverworldAllocationReport::TablesStoppingBeforeTail()
    const {
  std::vector<std::string> ids;
  for (const auto& table : tables) {
    if (table.Misses(kSpecialTailFirstArea)) {
      ids.push_back(table.id);
    }
  }
  return ids;
}

absl::StatusOr<OverworldAllocationReport> BuildOverworldAllocationReport(
    Rom* rom, const OverworldAllocationOptions& options) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (rom->size() < 0x80000) {
    return absl::FailedPreconditionError(
        absl::StrFormat("ROM is %d bytes; an ALTTP ROM is at least 512 KB",
                        static_cast<int>(rom->size())));
  }

  OverworldAllocationReport report;
  report.rom_name = std::filesystem::path(rom->filename()).filename().string();
  report.rom_size = rom->size();
  report.asm_version = ReadAsmVersion(*rom);
  const bool zs_v3 = IsZsV3(report.asm_version);
  report.catalogue = zs_v3 ? "zscustom_v3" : "vanilla";
  report.tables = OverworldAreaTableCatalogue(report.asm_version);
  const TailMapExpansionPlan plan;
  const uint32_t marker_pc = LoRomSnesToPc(plan.marker_snes);
  report.tail_marker_present =
      marker_pc < rom->size() && rom->data()[marker_pc] == plan.marker_value;

  report.areas.resize(kAllocationAreaCount);
  for (int id = 0; id < kAllocationAreaCount; ++id) {
    AreaAllocation& area = report.areas[id];
    area.area = id;
    area.world = AllocationWorldForArea(id);
    if (auto it = options.area_names.find(id); it != options.area_names.end()) {
      area.name = it->second;
    }
    for (const auto& table : report.tables) {
      if (table.Misses(id)) {
        area.uncovered_tables.push_back(table.id);
      }
    }

    // Parent.
    if (zs_v3) {
      const uint32_t pc = kPoolParentPc + id;
      if (pc < rom->size()) {
        if (id <= 0x9F) {
          area.parent = rom->data()[pc];
        } else {
          area.parent_past_end = rom->data()[pc];
        }
      }
    } else if (id < 0x80) {
      const int lw = id & 0x3F;
      const uint8_t parent = rom->data()[kVanillaParentPc + lw];
      area.parent = id < 0x40 ? parent : parent + 0x40;
    } else if (id <= 0x9F) {
      area.parent = VanillaSpecialParent(id);
    }

    // Size. Vanilla fixes special-world sizes in code (only Zora's Domain
    // is large); ZS v3 reads all 0xC0 entries of the table.
    if (zs_v3 || id < 0x80) {
      area.size = DecodeAreaSize(rom->data()[kAreaSizePc + id], zs_v3);
    } else {
      area.size = VanillaSpecialParent(id) == 0x81 ? AllocationAreaSize::kLarge
                                                   : AllocationAreaSize::kSmall;
    }

    // Map data. Once the tail marker is present the game reads the
    // relocated 192-entry tables; otherwise only $00-$9F have pointers.
    if (report.tail_marker_present) {
      area.map_pointer_source = "tail_table";
      ScanAreaMap(*rom, LoRomSnesToPc(plan.high_table_snes),
                  LoRomSnesToPc(plan.low_table_snes), id, &area);
    } else if (id <= 0x9F) {
      area.map_pointer_source = "vanilla_table";
      ScanAreaMap(*rom, kMap32HighPc, kMap32LowPc, id, &area);
    }
    if (area.map_pointer_source != "none") {
      for (int lower = 0; lower < id; ++lower) {
        const AreaAllocation& other = report.areas[lower];
        if (other.map_pointer_source != "none" &&
            other.high.snes == area.high.snes &&
            other.low.snes == area.low.snes) {
          area.duplicate_of = lower;
          break;
        }
      }
    }
  }

  // Subscreen overlay users (the game reads the parent's entry).
  if (zs_v3) {
    for (int id = 0; id <= 0x9F; ++id) {
      const AreaAllocation& area = report.areas[id];
      if (area.parent.has_value() && *area.parent != id) {
        continue;
      }
      const uint16_t overlay = ReadWord(*rom, kPoolOverlayPc + 2 * id);
      if (overlay >= 0x80 && overlay < kAllocationAreaCount) {
        report.areas[overlay].overlay_users.push_back(id);
      }
    }
    for (int overlay : kZsCodeOverlayIds) {
      report.areas[overlay].overlay_named_in_code = true;
    }
  } else {
    for (int overlay : kVanillaCodeOverlayIds) {
      report.areas[overlay].overlay_named_in_code = true;
    }
  }

  // Entities.
  if (options.scan_entities) {
    if (auto entrances = LoadEntrances(rom); entrances.ok()) {
      for (const auto& entrance : *entrances) {
        // LoadEntrances passes its deleted flag into is_hole_, so test the
        // position: $FFFF marks an unused slot.
        if (entrance.map_pos_ != 0xFFFF &&
            entrance.map_id_ < kAllocationAreaCount) {
          report.areas[entrance.map_id_].entrances++;
        }
      }
    }
    if (auto holes = LoadHoles(rom); holes.ok()) {
      for (const auto& hole : *holes) {
        if (hole.map_id_ < kAllocationAreaCount) {
          report.areas[hole.map_id_].holes++;
        }
      }
    }
    if (auto exits = LoadExits(rom); exits.ok()) {
      for (const auto& exit : *exits) {
        if (!exit.deleted_ && exit.map_id_ < kAllocationAreaCount) {
          report.areas[exit.map_id_].exits++;
        }
      }
    }
  }

  // Overworld sprites (ZS v3 Pool pointer tables, bank $09 lists).
  if (options.scan_sprites && zs_v3) {
    for (int id = 0; id <= 0x9F; ++id) {
      for (uint32_t table_pc : kPoolSpritePointerPcs) {
        const uint32_t entry = table_pc + 2 * id;
        if (!InRom(*rom, entry, 2)) {
          continue;
        }
        bool valid = false;
        const int count = CountSpriteList(*rom, ReadWord(*rom, entry), &valid);
        if (valid) {
          report.areas[id].sprites += count;
        }
      }
    }
  }

  // Evidence and summary.
  for (auto& area : report.areas) {
    if (area.unique_tile32 > 1) {
      area.evidence.push_back(
          absl::StrFormat("map data (%d unique tile32)", area.unique_tile32));
    }
    const bool multi_area =
        area.parent.has_value() &&
        (*area.parent != area.area || area.size != AllocationAreaSize::kSmall);
    if (multi_area) {
      area.evidence.push_back(absl::StrFormat("%s area, parent %s",
                                              AllocationAreaSizeName(area.size),
                                              AreaText(*area.parent)));
    }
    if (area.entrances > 0) {
      area.evidence.push_back(
          EvidenceCount(area.entrances, "entrance", "entrances"));
    }
    if (area.holes > 0) {
      area.evidence.push_back(EvidenceCount(area.holes, "hole", "holes"));
    }
    if (area.exits > 0) {
      area.evidence.push_back(EvidenceCount(area.exits, "exit", "exits"));
    }
    if (area.sprites > 0) {
      area.evidence.push_back(EvidenceCount(area.sprites, "sprite", "sprites"));
    }
    if (!area.overlay_users.empty()) {
      area.evidence.push_back(
          absl::StrCat("subscreen overlay of ", JoinAreas(area.overlay_users)));
    }
    if (area.overlay_named_in_code) {
      area.evidence.push_back("overlay ID named in code");
    }
    area.used = !area.evidence.empty();
  }

  const std::array<std::pair<const char*, std::pair<int, int>>, 4> worlds = {
      {{"light", {0x00, 0x3F}},
       {"dark", {0x40, 0x7F}},
       {"special", {0x80, 0x9F}},
       {"special_tail", {0xA0, 0xBF}}}};
  for (const auto& [key, range] : worlds) {
    WorldAllocationSummary summary;
    summary.key = key;
    summary.first_area = range.first;
    summary.last_area = range.second;
    for (int id = range.first; id <= range.second; ++id) {
      const AreaAllocation& area = report.areas[id];
      if (area.used) {
        summary.used++;
      } else {
        summary.free++;
        if (area.map_pointer_valid) {
          summary.free_with_map_data++;
        } else {
          summary.free_without_map_data++;
        }
      }
    }
    report.summary.push_back(summary);
  }
  return report;
}

// ---------------------------------------------------------------------------
// TailMapExpansion check
// ---------------------------------------------------------------------------

const char* TailCheckVerdictName(TailCheckVerdict verdict) {
  switch (verdict) {
    case TailCheckVerdict::kSafe:
      return "safe";
    case TailCheckVerdict::kUnsafe:
      return "unsafe";
    case TailCheckVerdict::kAlreadyApplied:
      return "already_applied";
  }
  return "unsafe";
}

const char* TailCheckScopeName(TailCheckScope scope) {
  switch (scope) {
    case TailCheckScope::kRom:
      return "rom";
    case TailCheckScope::kPatch:
      return "patch";
    case TailCheckScope::kYaze:
      return "yaze";
  }
  return "rom";
}

std::string TailMapExpansionCheck::BlockingSummary() const {
  std::vector<std::string> lines;
  for (const auto& finding : findings) {
    if (finding.blocking) {
      lines.push_back(absl::StrFormat(
          "[%s] %s", TailCheckScopeName(finding.scope), finding.message));
    }
  }
  return absl::StrJoin(lines, "\n");
}

namespace {

TailCheckRegion MakeRegion(const Rom& rom, const TailCheckAnnotator& annotator,
                           const char* id, const char* label, uint32_t snes,
                           int length) {
  TailCheckRegion region;
  region.id = id;
  region.label = label;
  region.snes = snes;
  region.pc = LoRomSnesToPc(snes);
  region.length = length;
  region.in_rom = InRom(rom, region.pc, length);
  if (region.pc < rom.size()) {
    const size_t available = std::min<size_t>(length, rom.size() - region.pc);
    const uint8_t* data = rom.data() + region.pc;
    for (size_t i = 0; i < available; ++i) {
      if (data[i] != 0) {
        region.nonzero_bytes++;
      }
    }
    region.first_bytes = HexBytes(data, std::min<size_t>(available, 16));
  }
  if (annotator.ownership) {
    if (auto owner = annotator.ownership(snes); owner.has_value()) {
      region.owner = owner->first;
    }
  }
  if (annotator.symbol) {
    region.symbol = annotator.symbol(snes);
  }
  return region;
}

TailStreamCheck CheckBlankStream(const Rom& rom, const char* id, uint32_t snes,
                                 const std::vector<uint8_t>& planned,
                                 bool applied) {
  TailStreamCheck check;
  check.id = id;
  check.snes = snes;
  const uint32_t pc = LoRomSnesToPc(snes);
  // Decode the stream as the game would see it after the patch: the planned
  // bytes, followed by whatever the ROM holds after them. Once applied, the
  // ROM already holds the stream.
  std::vector<uint8_t> image;
  const size_t window = 0x10000;
  if (pc < rom.size()) {
    const size_t available = std::min<size_t>(window, rom.size() - pc);
    image.assign(rom.data() + pc, rom.data() + pc + available);
  }
  if (!applied) {
    check.stream_bytes = static_cast<int>(planned.size());
    if (image.size() < planned.size()) {
      image.resize(planned.size(), 0x00);
    }
    std::copy(planned.begin(), planned.end(), image.begin());
  }
  const MapStreamScan scan = ScanMapStream(image.data(), image.size(), 0);
  check.terminated = scan.terminated;
  check.decoded_bytes = scan.decoded_bytes;
  check.consumed_bytes = scan.compressed_bytes;
  check.error = scan.error;
  check.stopped_at = SnesText(LoRomPcToSnes(pc + scan.compressed_bytes));
  check.terminated_within_stream =
      scan.terminated &&
      (applied || scan.compressed_bytes <= check.stream_bytes);
  check.valid =
      check.terminated_within_stream && scan.decoded_bytes >= kMap32PerScreen;
  return check;
}

}  // namespace

TailMapExpansionCheck CheckTailMapExpansion(
    const Rom& rom, const TailMapExpansionPlan& plan,
    const TailCheckAnnotator& annotator) {
  TailMapExpansionCheck check;
  check.rom_size = rom.size();
  check.asm_version = ReadAsmVersion(rom);
  const uint32_t marker_pc = LoRomSnesToPc(plan.marker_snes);
  if (marker_pc < rom.size()) {
    check.marker_value = rom.data()[marker_pc];
  }
  check.marker_present =
      marker_pc < rom.size() && check.marker_value == plan.marker_value;
  const int table_bytes = plan.table_entries * 3;

  check.regions.push_back(MakeRegion(rom, annotator, "marker",
                                     "Detection marker", plan.marker_snes, 1));
  check.regions.push_back(MakeRegion(rom, annotator, "high_table",
                                     "Map32 pointers, high (192 entries)",
                                     plan.high_table_snes, table_bytes));
  check.regions.push_back(MakeRegion(rom, annotator, "low_table",
                                     "Map32 pointers, low (192 entries)",
                                     plan.low_table_snes, table_bytes));
  check.regions.push_back(MakeRegion(
      rom, annotator, "blank_high", "Blank map stream, high bytes",
      plan.blank_high_snes, static_cast<int>(plan.blank_high.size())));
  check.regions.push_back(
      MakeRegion(rom, annotator, "blank_low", "Blank map stream, low bytes",
                 plan.blank_low_snes, static_cast<int>(plan.blank_low.size())));

  check.streams.push_back(
      CheckBlankStream(rom, "blank_high", plan.blank_high_snes, plan.blank_high,
                       check.marker_present));
  check.streams.push_back(CheckBlankStream(rom, "blank_low",
                                           plan.blank_low_snes, plan.blank_low,
                                           check.marker_present));

  for (size_t i = 0; i < plan.operand_snes.size(); ++i) {
    TailCheckOperand operand;
    operand.snes = plan.operand_snes[i];
    operand.pc = LoRomSnesToPc(operand.snes);
    operand.vanilla = plan.operand_vanilla[i];
    operand.patched = plan.operand_patched[i];
    operand.in_rom = operand.pc >= 1 && InRom(rom, operand.pc, 3);
    if (!operand.in_rom) {
      operand.state = "missing";
    } else {
      operand.opcode = rom.data()[operand.pc - 1];
      operand.current = ReadLong(rom, operand.pc);
      if (operand.current == operand.vanilla) {
        operand.state = "vanilla";
      } else if (operand.current == operand.patched) {
        operand.state = "patched";
      } else {
        operand.state = "other";
      }
    }
    check.operands.push_back(operand);
  }

  // End of the ZS function space ($28:9940 up to the marker).
  if (IsZsV3(check.asm_version) && marker_pc <= rom.size()) {
    for (uint32_t pc = marker_pc; pc > kZsFunctionSpacePc; --pc) {
      if (rom.data()[pc - 1] != 0) {
        check.zs_code_found = true;
        check.zs_code_end_snes = LoRomPcToSnes(pc - 1);
        check.free_bytes_before_marker = static_cast<int>(marker_pc - pc);
        break;
      }
    }
  }

  const auto tables = OverworldAreaTableCatalogue(check.asm_version);
  for (const auto& table : tables) {
    if (table.Misses(kSpecialTailFirstArea)) {
      check.tables_stopping_before_tail.push_back(table.id);
    }
  }

  auto add = [&check](TailCheckScope scope, const char* id, std::string message,
                      bool blocking) {
    check.findings.push_back({scope, id, std::move(message), blocking});
  };

  // ROM findings.
  if (!IsZsV3(check.asm_version)) {
    add(TailCheckScope::kRom, "zscustom_v3_required",
        absl::StrFormat("ZSCustomOverworld v3 is required; this ROM reports "
                        "ASM version 0x%02X at $28:8145",
                        check.asm_version),
        true);
  }
  if (!check.marker_present) {
    for (const auto& region : check.regions) {
      if (region.nonzero_bytes > 0) {
        std::string message = absl::StrFormat(
            "the patch would overwrite %d non-zero byte%s at %s (%s)",
            region.nonzero_bytes, region.nonzero_bytes == 1 ? "" : "s",
            SnesText(region.snes), region.label);
        if (!region.symbol.empty()) {
          absl::StrAppend(&message, ", symbol ", region.symbol);
        }
        add(TailCheckScope::kRom, "target_in_use", message, true);
      }
    }
    if (!check.regions.empty() &&
        check.rom_size < static_cast<size_t>(check.regions.back().pc +
                                             check.regions.back().length)) {
      add(TailCheckScope::kRom, "rom_too_small",
          absl::StrFormat(
              "the ROM is %d bytes; the patch writes up to PC "
              "0x%06X, so asar would expand it",
              static_cast<int>(check.rom_size),
              check.regions.back().pc + check.regions.back().length),
          false);
    }
  }
  for (const auto& region : check.regions) {
    if (!annotator.ownership) {
      break;
    }
    if (auto owner = annotator.ownership(region.snes);
        owner.has_value() && owner->second) {
      add(TailCheckScope::kRom, "target_owned",
          absl::StrFormat("hack manifest: %s (%s) is %s", SnesText(region.snes),
                          region.label, owner->first),
          true);
    }
  }
  for (const auto& operand : check.operands) {
    if (operand.state == "other") {
      add(TailCheckScope::kRom, "operand_changed",
          absl::StrFormat("operand at %s holds %s, not the vanilla %s; "
                          "another patch changed the map loader",
                          SnesText(operand.snes), SnesText(operand.current),
                          SnesText(operand.vanilla)),
          true);
    } else if (operand.state == "patched" && !check.marker_present) {
      add(TailCheckScope::kRom, "operand_without_marker",
          absl::StrFormat("operand at %s already points to %s but the marker "
                          "at %s is missing",
                          SnesText(operand.snes), SnesText(operand.current),
                          SnesText(plan.marker_snes)),
          true);
    } else if (operand.state == "missing") {
      add(TailCheckScope::kRom, "operand_missing",
          absl::StrFormat("operand at %s is past the end of the ROM",
                          SnesText(operand.snes)),
          true);
    }
    if (annotator.ownership) {
      if (auto owner = annotator.ownership(operand.snes);
          owner.has_value() && owner->second) {
        add(TailCheckScope::kRom, "operand_owned",
            absl::StrFormat("hack manifest: operand at %s is %s",
                            SnesText(operand.snes), owner->first),
            true);
      }
    }
  }
  if (check.zs_code_found && check.free_bytes_before_marker < 0x200) {
    add(TailCheckScope::kRom, "marker_near_zs_code",
        absl::StrFormat("ZS code ends at %s, %d bytes before the marker at "
                        "%s; asar does not reserve %s-%s, so growing ZS code "
                        "would run into the relocated tables",
                        SnesText(check.zs_code_end_snes),
                        check.free_bytes_before_marker,
                        SnesText(plan.marker_snes), SnesText(plan.marker_snes),
                        SnesText(plan.low_table_snes + table_bytes - 1)),
        false);
  }

  // Patch findings.
  for (const auto& stream : check.streams) {
    if (stream.valid) {
      continue;
    }
    std::string message;
    if (!stream.terminated_within_stream && stream.stream_bytes > 0) {
      message = absl::StrFormat(
          "blank map stream at %s is %d bytes with no $FF terminator; "
          "decoding reads past it and stops at %s after %d decoded bytes",
          SnesText(stream.snes), stream.stream_bytes, stream.stopped_at,
          stream.decoded_bytes);
      if (!stream.error.empty()) {
        absl::StrAppend(&message, " (", stream.error, ")");
      }
    } else {
      message = absl::StrFormat(
          "blank map stream at %s decodes to %d bytes; a map needs %d",
          SnesText(stream.snes), stream.decoded_bytes, kMap32PerScreen);
    }
    add(TailCheckScope::kPatch, "blank_stream_invalid", message, true);
  }
  std::vector<std::string> unpatched_tables;
  for (const auto& id : check.tables_stopping_before_tail) {
    if (id != "map32_high" && id != "map32_low") {
      unpatched_tables.push_back(id);
    }
  }
  if (!unpatched_tables.empty()) {
    add(TailCheckScope::kPatch, "tail_tables_missing",
        absl::StrFormat(
            "the patch extends only the map32 pointers; %d other per-area "
            "tables stop before $A0, so a tail area reads the data after "
            "each of them (%s)",
            static_cast<int>(unpatched_tables.size()),
            absl::StrJoin(unpatched_tables, ", ")),
        true);
  }

  // yaze findings.
  if (!plan.yaze_saves_relocated_tables) {
    add(TailCheckScope::kYaze, "pointer_save_desync",
        "after the patch the game reads map pointers from $28:A400/$28:A640, "
        "but yaze's overworld save writes 0x1794D/0x17B2D, so the next save "
        "leaves maps $00-$9F stale in game",
        true);
  }
  if (plan.yaze_map_count < plan.table_entries) {
    add(TailCheckScope::kYaze, "tail_maps_not_editable",
        absl::StrFormat("yaze loads and saves %d maps ($00-$%02X); maps "
                        "$A0-$BF cannot be edited",
                        plan.yaze_map_count, plan.yaze_map_count - 1),
        false);
  }

  bool blocking = false;
  bool rom_blocking = false;
  for (const auto& finding : check.findings) {
    blocking = blocking || finding.blocking;
    rom_blocking = rom_blocking ||
                   (finding.blocking && finding.scope == TailCheckScope::kRom);
  }
  check.rom_compatible = !rom_blocking;
  if (check.marker_present) {
    check.verdict = TailCheckVerdict::kAlreadyApplied;
  } else {
    check.verdict =
        blocking ? TailCheckVerdict::kUnsafe : TailCheckVerdict::kSafe;
  }
  return check;
}

absl::Status TailMapExpansionApplyGuard(const Rom& rom) {
  const TailMapExpansionCheck check = CheckTailMapExpansion(rom);
  if (check.verdict != TailCheckVerdict::kUnsafe) {
    return absl::OkStatus();
  }
  return absl::FailedPreconditionError(absl::StrCat(
      "Refusing to apply TailMapExpansion.asm: the compatibility check says "
      "unsafe.\n",
      check.BlockingSummary(),
      "\nRun `z3ed overworld-allocation --rom <rom> --tail-check` for the "
      "full report."));
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

namespace {

ordered_json TableJson(const AreaTableInfo& table) {
  ordered_json j;
  j["id"] = table.id;
  j["label"] = table.label;
  j["group"] = table.group;
  j["location"] =
      table.in_rom
          ? absl::StrFormat("%s (PC 0x%06X)", SnesText(table.snes), table.pc)
          : absl::StrFormat("%s (RAM)", SnesText(table.snes));
  j["entry_bytes"] = table.entry_bytes;
  j["covers"] = RangeText(table.first_area, table.last_area);
  j["read_for"] = RangeText(table.applies_first, table.applies_last);
  j["misses_tail"] = table.Misses(kSpecialTailFirstArea);
  j["past_end"] = table.past_end;
  if (!table.note.empty()) {
    j["note"] = table.note;
  }
  return j;
}

ordered_json StreamJson(const MapStreamScan& scan) {
  ordered_json j;
  j["pointer"] = SnesText(scan.snes);
  j["pc"] = absl::StrFormat("0x%06X", scan.pc);
  j["terminated"] = scan.terminated;
  j["compressed_bytes"] = scan.compressed_bytes;
  j["decoded_bytes"] = scan.decoded_bytes;
  if (!scan.error.empty()) {
    j["error"] = scan.error;
  }
  return j;
}

ordered_json AreaJson(const AreaAllocation& area) {
  ordered_json j;
  j["id"] = HexByte(area.area);
  j["world"] = AllocationWorldName(area.world);
  if (!area.name.empty()) {
    j["name"] = area.name;
  }
  j["used"] = area.used;
  j["evidence"] = area.evidence;
  j["parent"] = area.parent.has_value() ? ordered_json(HexByte(*area.parent))
                                        : ordered_json(nullptr);
  if (area.parent_past_end.has_value()) {
    j["parent_read_past_table"] = HexByte(*area.parent_past_end);
  }
  j["size"] = AllocationAreaSizeName(area.size);
  ordered_json map;
  map["source"] = area.map_pointer_source;
  map["valid"] = area.map_pointer_valid;
  if (area.map_pointer_source != "none") {
    map["high"] = StreamJson(area.high);
    map["low"] = StreamJson(area.low);
    map["unique_tile32"] = area.unique_tile32;
    map["duplicate_of"] = area.duplicate_of.has_value()
                              ? ordered_json(HexByte(*area.duplicate_of))
                              : ordered_json(nullptr);
  }
  j["map"] = map;
  j["entrances"] = area.entrances;
  j["holes"] = area.holes;
  j["exits"] = area.exits;
  j["sprites"] = area.sprites;
  j["uncovered_tables"] = area.uncovered_tables;
  return j;
}

ordered_json TailJson(const TailMapExpansionCheck& check) {
  ordered_json j;
  j["verdict"] = TailCheckVerdictName(check.verdict);
  j["rom_compatible"] = check.rom_compatible;
  j["asm_version"] = HexByte(check.asm_version);
  j["marker"] = {{"address", "$28:A3FF"},
                 {"value", HexByte(check.marker_value)},
                 {"present", check.marker_present}};
  ordered_json regions = ordered_json::array();
  for (const auto& region : check.regions) {
    ordered_json r;
    r["id"] = region.id;
    r["label"] = region.label;
    r["address"] = SnesText(region.snes);
    r["pc"] = absl::StrFormat("0x%06X", region.pc);
    r["length"] = region.length;
    r["in_rom"] = region.in_rom;
    r["nonzero_bytes"] = region.nonzero_bytes;
    r["first_bytes"] = region.first_bytes;
    if (!region.owner.empty()) {
      r["owner"] = region.owner;
    }
    if (!region.symbol.empty()) {
      r["symbol"] = region.symbol;
    }
    regions.push_back(r);
  }
  j["regions"] = regions;
  ordered_json streams = ordered_json::array();
  for (const auto& stream : check.streams) {
    ordered_json s;
    s["id"] = stream.id;
    s["address"] = SnesText(stream.snes);
    s["stream_bytes"] = stream.stream_bytes;
    s["terminated"] = stream.terminated;
    s["terminated_within_stream"] = stream.terminated_within_stream;
    s["decoded_bytes"] = stream.decoded_bytes;
    s["consumed_bytes"] = stream.consumed_bytes;
    s["stopped_at"] = stream.stopped_at;
    if (!stream.error.empty()) {
      s["error"] = stream.error;
    }
    s["valid"] = stream.valid;
    streams.push_back(s);
  }
  j["blank_streams"] = streams;
  ordered_json operands = ordered_json::array();
  for (const auto& operand : check.operands) {
    ordered_json o;
    o["address"] = SnesText(operand.snes);
    o["pc"] = absl::StrFormat("0x%06X", operand.pc);
    o["opcode"] = HexByte(operand.opcode);
    o["current"] = SnesText(operand.current);
    o["vanilla"] = SnesText(operand.vanilla);
    o["patched"] = SnesText(operand.patched);
    o["state"] = operand.state;
    operands.push_back(o);
  }
  j["operands"] = operands;
  if (check.zs_code_found) {
    j["zs_code_end"] = SnesText(check.zs_code_end_snes);
    j["free_bytes_before_marker"] = check.free_bytes_before_marker;
  } else {
    j["zs_code_end"] = nullptr;
    j["free_bytes_before_marker"] = nullptr;
  }
  j["tables_stopping_before_tail"] = check.tables_stopping_before_tail;
  ordered_json blockers = ordered_json::array();
  ordered_json warnings = ordered_json::array();
  for (const auto& finding : check.findings) {
    ordered_json f = {{"scope", TailCheckScopeName(finding.scope)},
                      {"id", finding.id},
                      {"message", finding.message}};
    (finding.blocking ? blockers : warnings).push_back(f);
  }
  j["blockers"] = blockers;
  j["warnings"] = warnings;
  return j;
}

std::string MapColumn(const AreaAllocation& area) {
  if (area.map_pointer_source == "none") {
    return "none";
  }
  if (!area.map_pointer_valid) {
    return "invalid";
  }
  if (area.duplicate_of.has_value()) {
    return absl::StrCat("=", AreaText(*area.duplicate_of));
  }
  return area.unique_tile32 > 1 ? "data" : "blank";
}

}  // namespace

std::string OverworldAllocationToJson(const OverworldAllocationReport& report,
                                      const TailMapExpansionCheck* tail_check) {
  ordered_json j;
  j["rom"] = report.rom_name;
  j["rom_size"] = report.rom_size;
  j["asm_version"] = HexByte(report.asm_version);
  j["catalogue"] = report.catalogue;
  j["tail_marker_present"] = report.tail_marker_present;
  ordered_json summary = ordered_json::array();
  for (const auto& world : report.summary) {
    summary.push_back({{"world", world.key},
                       {"range", RangeText(world.first_area, world.last_area)},
                       {"used", world.used},
                       {"free", world.free},
                       {"free_with_map_data", world.free_with_map_data},
                       {"free_without_map_data", world.free_without_map_data}});
  }
  j["summary"] = summary;
  j["tables_stopping_before_tail"] = report.TablesStoppingBeforeTail();
  ordered_json tables = ordered_json::array();
  for (const auto& table : report.tables) {
    tables.push_back(TableJson(table));
  }
  j["tables"] = tables;
  ordered_json areas = ordered_json::array();
  for (const auto& area : report.areas) {
    areas.push_back(AreaJson(area));
  }
  j["areas"] = areas;
  if (tail_check != nullptr) {
    j["tail_check"] = TailJson(*tail_check);
  }
  return j.dump(2);
}

std::string FormatOverworldAllocationText(
    const OverworldAllocationReport& report,
    const TailMapExpansionCheck* tail_check) {
  std::ostringstream out;
  out << absl::StrFormat("Overworld allocation: %s (%d bytes)\n",
                         report.rom_name.empty() ? "<memory>" : report.rom_name,
                         static_cast<int>(report.rom_size));
  out << absl::StrFormat(
      "Layout: ASM version 0x%02X, %s tables; TailMapExpansion marker %s\n\n",
      report.asm_version, report.catalogue,
      report.tail_marker_present ? "present" : "absent");

  out << "Summary\n";
  out << absl::StrFormat("  %-13s %-8s %5s %5s %9s %11s\n", "world", "range",
                         "used", "free", "free+map", "free-no-map");
  for (const auto& world : report.summary) {
    out << absl::StrFormat("  %-13s %-8s %5d %5d %9d %11d\n", world.key,
                           RangeText(world.first_area, world.last_area),
                           world.used, world.free, world.free_with_map_data,
                           world.free_without_map_data);
  }

  out << "\nPer-area tables (a read past the last entry returns the data "
         "after the table)\n";
  out << absl::StrFormat("  %-24s %-19s %5s  %-8s %-8s %s\n", "table",
                         "location", "bytes", "covers", "read for",
                         "past the end");
  for (const auto& table : report.tables) {
    const std::string location =
        table.in_rom
            ? absl::StrFormat("%s 0x%06X", SnesText(table.snes), table.pc)
            : absl::StrFormat("%s RAM", SnesText(table.snes));
    out << absl::StrFormat("  %-24s %-19s %5d  %-8s %-8s %s\n", table.id,
                           location, table.entry_bytes,
                           RangeText(table.first_area, table.last_area),
                           RangeText(table.applies_first, table.applies_last),
                           table.past_end.empty() ? "-" : table.past_end);
  }
  const auto stopping = report.TablesStoppingBeforeTail();
  out << absl::StrFormat(
      "  %d of %d tables are read for $A0 but stop before it.\n",
      static_cast<int>(stopping.size()),
      static_cast<int>(report.tables.size()));

  out << "\nAreas (map: data/blank/=dup/invalid/none; miss = tables read for "
         "the ID that have no entry for it)\n";
  out << absl::StrFormat(
      "  %-4s %-7s %-4s %-6s %-5s %-7s %5s %4s %4s %4s %4s "
      "%4s  %s\n",
      "id", "world", "used", "parent", "size", "map", "tile32", "ent", "hole",
      "exit", "spr", "miss", "evidence");
  for (const auto& area : report.areas) {
    std::string parent = "--";
    if (area.parent.has_value()) {
      parent = AreaText(*area.parent);
    }
    std::string evidence = absl::StrJoin(area.evidence, "; ");
    if (!area.name.empty()) {
      evidence = evidence.empty()
                     ? absl::StrCat("[", area.name, "]")
                     : absl::StrCat("[", area.name, "] ", evidence);
    }
    out << absl::StrFormat(
        "  %-4s %-7s %-4s %-6s %-5s %-7s %5s %4d %4d %4d %4d %4d  %s\n",
        AreaText(area.area), AllocationWorldName(area.world),
        area.used ? "yes" : "no", parent, AllocationAreaSizeName(area.size),
        MapColumn(area),
        area.unique_tile32 >= 0 ? absl::StrCat(area.unique_tile32) : "-",
        area.entrances, area.holes, area.exits, area.sprites,
        static_cast<int>(area.uncovered_tables.size()),
        evidence.empty() ? "-" : evidence);
  }

  if (tail_check != nullptr) {
    const TailMapExpansionCheck& c = *tail_check;
    out << "\nTailMapExpansion check "
           "(assets/patches/Overworld/TailMapExpansion.asm)\n";
    out << absl::StrFormat("  Verdict: %s (ROM compatible: %s)\n",
                           TailCheckVerdictName(c.verdict),
                           c.rom_compatible ? "yes" : "no");
    out << absl::StrFormat("  Marker $28:A3FF: 0x%02X (%s)\n", c.marker_value,
                           c.marker_present ? "present" : "absent");
    if (c.zs_code_found) {
      out << absl::StrFormat(
          "  ZS code ends at %s; %d bytes free before $28:A3FF\n",
          SnesText(c.zs_code_end_snes), c.free_bytes_before_marker);
    } else {
      out << "  ZS code end: not found (not a ZS v3 ROM)\n";
    }
    out << "  Target regions:\n";
    for (const auto& region : c.regions) {
      std::string extra;
      if (!region.symbol.empty()) {
        absl::StrAppend(&extra, " symbol ", region.symbol);
      }
      if (!region.owner.empty()) {
        absl::StrAppend(&extra, " owner ", region.owner);
      }
      out << absl::StrFormat(
          "    %-10s %s  PC 0x%06X %4d B  %s  non-zero %3d  [%s]%s\n",
          region.id, SnesText(region.snes), region.pc, region.length,
          region.in_rom ? "in ROM " : "past end", region.nonzero_bytes,
          region.first_bytes, extra);
    }
    out << "  Blank streams (decoded as the game would read them after the "
           "patch):\n";
    for (const auto& stream : c.streams) {
      out << absl::StrFormat(
          "    %-10s %s  %3d B written  $FF in stream: %-3s  decoded %d B, "
          "stopped at %s  %s\n",
          stream.id, SnesText(stream.snes), stream.stream_bytes,
          stream.terminated_within_stream ? "yes" : "no", stream.decoded_bytes,
          stream.stopped_at, stream.valid ? "valid" : "INVALID");
    }
    out << "  Patched operands (LDA.l table,X):\n";
    for (const auto& operand : c.operands) {
      out << absl::StrFormat(
          "    %s  PC 0x%06X  opcode 0x%02X  now %s  vanilla %s  patch %s  "
          "%s\n",
          SnesText(operand.snes), operand.pc, operand.opcode,
          SnesText(operand.current), SnesText(operand.vanilla),
          SnesText(operand.patched), operand.state);
    }
    out << "  Blockers:\n";
    bool any = false;
    for (const auto& finding : c.findings) {
      if (finding.blocking) {
        any = true;
        out << absl::StrFormat("    [%s] %s: %s\n",
                               TailCheckScopeName(finding.scope), finding.id,
                               finding.message);
      }
    }
    if (!any) {
      out << "    none\n";
    }
    out << "  Warnings:\n";
    any = false;
    for (const auto& finding : c.findings) {
      if (!finding.blocking) {
        any = true;
        out << absl::StrFormat("    [%s] %s: %s\n",
                               TailCheckScopeName(finding.scope), finding.id,
                               finding.message);
      }
    }
    if (!any) {
      out << "    none\n";
    }
  }
  return out.str();
}

}  // namespace yaze::zelda3
