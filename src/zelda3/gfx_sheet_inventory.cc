#include "zelda3/gfx_sheet_inventory.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/gfx/util/compression.h"
#include "nlohmann/json.hpp"
#include "rom/snes.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {
namespace {

constexpr uint32_t kSpritesetTable = 0x5B57;
constexpr int kSpritesetCount = 144;
constexpr uint32_t kRoomBlocksetTable = 0x5D97;
constexpr int kRoomBlocksetCount = 82;
constexpr uint32_t kMainBlocksetPointer = 0x6237;
constexpr int kMainBlocksetCount = 37;
constexpr size_t kTileBytes3bpp = 24;
constexpr size_t kMeasureLimit = 0x8000;

template <size_t N>
bool Contains(const std::array<uint8_t, N>& values, int value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

// Splits CSV text into rows of fields (RFC 4180 quoting).
std::vector<std::vector<std::string>> ParseCsv(const std::string& text) {
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string field;
  bool quoted = false;
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (quoted) {
      if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
        field.push_back('"');
        ++i;
      } else if (c == '"') {
        quoted = false;
      } else {
        field.push_back(c);
      }
    } else if (c == '"') {
      quoted = true;
    } else if (c == ',') {
      row.push_back(std::move(field));
      field.clear();
    } else if (c == '\n' || c == '\r') {
      if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
        ++i;
      }
      row.push_back(std::move(field));
      field.clear();
      rows.push_back(std::move(row));
      row.clear();
    } else {
      field.push_back(c);
    }
  }
  if (!field.empty() || !row.empty()) {
    row.push_back(std::move(field));
    rows.push_back(std::move(row));
  }
  return rows;
}

}  // namespace

std::vector<int> GfxSheetInventoryEntry::FreeBlocks() const {
  std::vector<int> free;
  if (reserved || engine_loaded || !has_block_stats) {
    return free;
  }
  for (int block : empty_16x16_blocks) {
    if (std::find(reserved_blocks.begin(), reserved_blocks.end(), block) ==
        reserved_blocks.end()) {
      free.push_back(block);
    }
  }
  return free;
}

absl::StatusOr<GfxSheetInventory> BuildGfxSheetInventory(
    const Rom& rom, const std::map<int, OverworldAreaGfxInfo>& areas,
    const std::map<int, RoomGfxInfo>& rooms,
    const GfxSheetInventoryOptions& options) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  const auto& bytes = rom.vector();
  GfxSheetInventory inventory;
  inventory.overworld_areas = areas;
  inventory.rooms = rooms;

  // Sheets.
  inventory.sheets.resize(kGfxSheetCount);
  for (uint16_t gfx = 0; gfx < kGfxSheetCount; ++gfx) {
    auto& entry = inventory.sheets[gfx];
    entry.gfx = gfx;
    entry.kind = GetGfxSheetStorageKind(gfx);
    if (gfx >= kSpriteSheetBase &&
        gfx - kSpriteSheetBase < kSpriteSheetValueCount) {
      entry.sprite_value = static_cast<uint8_t>(gfx - kSpriteSheetBase);
    }
    entry.reserved = options.reserved_sheets.count(gfx) != 0;
    entry.flagged = options.flagged_sheets.count(gfx) != 0;
    if (auto it = options.reserved_blocks.find(gfx);
        it != options.reserved_blocks.end()) {
      entry.reserved_blocks = it->second;
    }
    entry.engine_loaded = entry.kind != GfxSheetStorageKind::kCompressed3bpp;

    auto pc = ReadGfxSheetPc(rom, gfx);
    if (!pc.ok()) {
      entry.error = std::string(pc.status().message());
      continue;
    }
    entry.pc = *pc;

    std::vector<uint8_t> data;
    if (entry.kind == GfxSheetStorageKind::kRaw3bpp) {
      if (entry.pc + kGfxSheet3bppBytes > bytes.size()) {
        entry.error = "raw sheet runs past the ROM end";
        continue;
      }
      data.assign(bytes.begin() + entry.pc,
                  bytes.begin() + entry.pc + kGfxSheet3bppBytes);
      entry.stored_bytes = kGfxSheet3bppBytes;
    } else if (entry.kind == GfxSheetStorageKind::kCompressed3bpp) {
      auto decoded = gfx::lc_lz2::DecompressExact(bytes.data(), bytes.size(),
                                                  entry.pc, kMeasureLimit,
                                                  /*big_endian_copy=*/false);
      if (!decoded.ok()) {
        entry.error = std::string(decoded.status().message());
        continue;
      }
      entry.stored_bytes = decoded->compressed_size;
      data = std::move(decoded->data);
      data.resize(kGfxSheet3bppBytes, 0);
    } else {
      continue;  // 2bpp: engine-loaded, no block statistics.
    }

    entry.has_block_stats = true;
    for (int tile = 0; tile < 64; ++tile) {
      const auto begin = data.begin() + tile * kTileBytes3bpp;
      if (std::all_of(begin, begin + kTileBytes3bpp,
                      [](uint8_t b) { return b == 0; })) {
        ++entry.empty_8x8;
      }
    }
    for (int block = 0; block < kGfxSheetBlockCount; ++block) {
      if (IsGfxSheetBlockEmpty(data, block, 3)) {
        entry.empty_16x16_blocks.push_back(block);
      }
    }
    entry.all_empty = entry.empty_8x8 == 64;
    entry.sha1 = util::ComputeSha1Hex(data.data(), data.size()).substr(0, 12);
  }

  // Shared pointers and identical contents.
  std::map<uint32_t, std::vector<uint16_t>> by_pc;
  std::map<std::string, std::vector<uint16_t>> by_sha;
  for (const auto& entry : inventory.sheets) {
    if (entry.error.empty() || entry.pc != 0) {
      by_pc[entry.pc].push_back(entry.gfx);
    }
    if (!entry.sha1.empty() && !entry.all_empty) {
      by_sha[entry.sha1].push_back(entry.gfx);
    }
  }
  for (auto& entry : inventory.sheets) {
    for (uint16_t other : by_pc[entry.pc]) {
      if (other != entry.gfx) {
        entry.shares_pointer_with.push_back(other);
      }
    }
    if (!entry.sha1.empty() && !entry.all_empty) {
      for (uint16_t other : by_sha[entry.sha1]) {
        if (other != entry.gfx) {
          entry.same_content_as.push_back(other);
        }
      }
    }
  }

  // Group tables.
  auto read = [&bytes](size_t pc) -> uint8_t {
    return pc < bytes.size() ? bytes[pc] : 0;
  };
  for (int n = 0; n < kSpritesetCount; ++n) {
    std::array<uint8_t, 4> slots{};
    for (int j = 0; j < 4; ++j) {
      slots[j] = read(kSpritesetTable + n * 4 + j);
    }
    inventory.spritesets.push_back(slots);
  }
  for (int n = 0; n < kRoomBlocksetCount; ++n) {
    std::array<uint8_t, 4> slots{};
    for (int j = 0; j < 4; ++j) {
      slots[j] = read(kRoomBlocksetTable + n * 4 + j);
    }
    inventory.room_blocksets.push_back(slots);
  }
  const uint32_t main_pc = SnesToPc(read(kMainBlocksetPointer) |
                                    (read(kMainBlocksetPointer + 1) << 8));
  for (int n = 0; n < kMainBlocksetCount; ++n) {
    std::array<uint8_t, 8> slots{};
    for (int j = 0; j < 8; ++j) {
      slots[j] = read(main_pc + n * 8 + j);
    }
    inventory.main_blocksets.push_back(slots);
  }

  // Usage per sheet.
  for (auto& entry : inventory.sheets) {
    const int gfx = entry.gfx;
    auto& use = entry.usage;
    for (int n = 0; n < kMainBlocksetCount; ++n) {
      if (Contains(inventory.main_blocksets[n], gfx)) {
        use.main_blocksets.push_back(n);
      }
    }
    for (int n = 0; n < kRoomBlocksetCount; ++n) {
      if (Contains(inventory.room_blocksets[n], gfx)) {
        use.room_blocksets.push_back(n);
      }
    }
    for (const auto& [area, info] : areas) {
      if (Contains(info.static_graphics, gfx)) {
        use.ow_areas_static.push_back(area);
      }
    }
    if (entry.sprite_value.has_value()) {
      const int value = *entry.sprite_value;
      for (int n = 0; n < kSpritesetCount; ++n) {
        if (Contains(inventory.spritesets[n], value)) {
          use.spritesets.push_back(n);
        }
      }
      auto uses_set = [&use](int set) {
        return std::binary_search(use.spritesets.begin(), use.spritesets.end(),
                                  set);
      };
      for (const auto& [area, info] : areas) {
        if (std::any_of(info.sprite_graphics.begin(),
                        info.sprite_graphics.end(), uses_set)) {
          use.ow_areas_sprite.push_back(area);
        }
      }
      for (const auto& [room, info] : rooms) {
        if (uses_set(info.spriteset + kDungeonSpritesetBase)) {
          use.rooms_sprite.push_back(room);
        }
      }
      if (auto it = options.sprite_value_labels.find(value);
          it != options.sprite_value_labels.end()) {
        entry.labels.assign(it->second.begin(), it->second.end());
      }
    }
    entry.unreferenced = !use.Any() && !entry.engine_loaded;
  }
  return inventory;
}

std::map<int, OverworldAreaGfxInfo> CollectOverworldAreaGfx(
    Rom& rom, GameData* game_data) {
  std::map<int, OverworldAreaGfxInfo> areas;
  if (!rom.is_loaded()) {
    return areas;
  }
  for (int index = 0; index < kNumOverworldAreas; ++index) {
    OverworldMap map(index, &rom, game_data);
    map.set_game_state(0);
    map.LoadAreaGraphics();
    OverworldAreaGfxInfo info;
    info.world = index < 0x40 ? 0 : (index < 0x80 ? 1 : 2);
    info.parent = map.parent();
    info.area_graphics = map.area_graphics();
    for (int i = 0; i < 3; ++i) {
      info.sprite_graphics[i] = map.sprite_graphics(i);
    }
    for (int i = 0; i < 16; ++i) {
      info.static_graphics[i] = map.static_graphics(i);
    }
    areas[index] = info;
  }
  return areas;
}

std::map<int, RoomGfxInfo> CollectRoomGfx(const Rom& rom) {
  std::map<int, RoomGfxInfo> rooms;
  const auto& bytes = rom.vector();
  auto in_range = [&bytes](int64_t pc, int64_t count) {
    return pc >= 0 && pc + count <= static_cast<int64_t>(bytes.size());
  };
  if (!in_range(kRoomHeaderPointer, 3) ||
      !in_range(kRoomHeaderPointerBank, 1)) {
    return rooms;
  }
  const uint32_t table = SnesToPc((bytes[kRoomHeaderPointer + 2] << 16) |
                                  (bytes[kRoomHeaderPointer + 1] << 8) |
                                  bytes[kRoomHeaderPointer]);
  for (int room = 0; room < kNumDungeonRooms; ++room) {
    const int64_t slot = static_cast<int64_t>(table) + room * 2;
    if (!in_range(slot, 2)) {
      continue;
    }
    const uint32_t header = SnesToPc((bytes[kRoomHeaderPointerBank] << 16) |
                                     (bytes[slot + 1] << 8) | bytes[slot]);
    // LoadRoomHeaderFromRom requires 14 readable header bytes.
    if (!in_range(header, 14)) {
      continue;
    }
    rooms[room] = {bytes[header + 2], bytes[header + 3]};
  }
  return rooms;
}

GfxGroupUsage FindSpritesetUsage(
    int spriteset, const std::map<int, OverworldAreaGfxInfo>& areas,
    const std::map<int, RoomGfxInfo>& rooms) {
  GfxGroupUsage usage;
  for (const auto& [area, info] : areas) {
    for (int state = 0; state < 3; ++state) {
      if (info.sprite_graphics[state] == spriteset) {
        usage.ow_areas_by_state[state].push_back(area);
      }
    }
  }
  for (const auto& [room, info] : rooms) {
    if (info.spriteset + kDungeonSpritesetBase == spriteset) {
      usage.rooms.push_back(room);
    }
  }
  return usage;
}

GfxGroupUsage FindRoomsetUsage(int roomset,
                               const std::map<int, OverworldAreaGfxInfo>& areas,
                               const std::map<int, RoomGfxInfo>& rooms) {
  GfxGroupUsage usage;
  for (const auto& [area, info] : areas) {
    if (info.area_graphics == roomset) {
      usage.ow_areas.push_back(area);
    }
  }
  for (const auto& [room, info] : rooms) {
    if (info.blockset == roomset) {
      usage.rooms.push_back(room);
    }
  }
  return usage;
}

const char* GfxSheetInventoryKindName(GfxSheetStorageKind kind) {
  switch (kind) {
    case GfxSheetStorageKind::kCompressed3bpp:
      return "3bpp-lz2";
    case GfxSheetStorageKind::kRaw3bpp:
      return "3bpp-raw";
    case GfxSheetStorageKind::kCompressed2bpp:
      return "2bpp";
  }
  return "unknown";
}

nlohmann::json GfxSheetInventoryToJson(const GfxSheetInventory& inventory) {
  using nlohmann::json;
  json out = json::object();

  json sheets = json::array();
  for (const auto& entry : inventory.sheets) {
    json sheet = json::object();
    sheet["gfx"] = entry.gfx;
    sheet["hex"] = absl::StrFormat("0x%02X", entry.gfx);
    sheet["pc"] = absl::StrFormat("0x%06X", entry.pc);
    sheet["kind"] = GfxSheetInventoryKindName(entry.kind);
    if (entry.sprite_value.has_value()) {
      sheet["sprite_value"] = absl::StrFormat("0x%02X", *entry.sprite_value);
    }
    if (entry.error.empty()) {
      sheet["stored_bytes"] = entry.stored_bytes.has_value()
                                  ? json(*entry.stored_bytes)
                                  : json(nullptr);
    }
    if (entry.has_block_stats) {
      sheet["empty_8x8"] = entry.empty_8x8;
      sheet["empty_16x16"] = entry.empty_16x16_blocks.size();
      sheet["empty_16x16_blocks"] = entry.empty_16x16_blocks;
      sheet["all_empty"] = entry.all_empty;
      sheet["sha1"] = entry.sha1;
      sheet["free_16x16_blocks"] = entry.FreeBlocks();
    }
    if (!entry.error.empty()) {
      sheet["error"] = entry.error;
    }
    if (entry.reserved) {
      sheet["reserved"] = "reserved by project config";
    }
    if (entry.flagged) {
      sheet["flagged"] = "needs owner approval before use";
    }
    if (!entry.reserved_blocks.empty()) {
      sheet["reserved_blocks"] = entry.reserved_blocks;
    }
    if (entry.engine_loaded) {
      sheet["engine_loaded"] = true;
    }
    sheet["shares_pointer_with"] = entry.shares_pointer_with;
    if (!entry.sha1.empty() && !entry.all_empty) {
      sheet["same_content_as"] = entry.same_content_as;
    }
    json usage = json::object();
    usage["main_blocksets"] = entry.usage.main_blocksets;
    usage["room_blocksets"] = entry.usage.room_blocksets;
    usage["ow_areas_static"] = entry.usage.ow_areas_static;
    if (entry.sprite_value.has_value()) {
      usage["spritesets"] = entry.usage.spritesets;
      usage["ow_areas_sprite"] = entry.usage.ow_areas_sprite;
      usage["rooms_sprite"] = entry.usage.rooms_sprite;
    }
    sheet["usage"] = std::move(usage);
    if (!entry.labels.empty()) {
      sheet["labels"] = entry.labels;
    }
    sheet["unreferenced"] = entry.unreferenced;
    sheets.push_back(std::move(sheet));
  }
  out["sheets"] = std::move(sheets);

  auto table = [](const auto& rows) {
    json object = json::object();
    for (size_t n = 0; n < rows.size(); ++n) {
      object[std::to_string(n)] =
          std::vector<int>(rows[n].begin(), rows[n].end());
    }
    return object;
  };
  out["spritesets"] = table(inventory.spritesets);
  out["main_blocksets"] = table(inventory.main_blocksets);
  out["room_blocksets"] = table(inventory.room_blocksets);

  static constexpr const char* kWorlds[] = {"Light", "Dark", "Special"};
  json areas = json::object();
  for (const auto& [index, info] : inventory.overworld_areas) {
    areas[std::to_string(index)] = {
        {"world", kWorlds[std::clamp(info.world, 0, 2)]},
        {"parent", absl::StrFormat("0x%02X", info.parent)},
        {"static_graphics", std::vector<int>(info.static_graphics.begin(),
                                             info.static_graphics.end())},
        {"sprite_graphics", std::vector<int>(info.sprite_graphics.begin(),
                                             info.sprite_graphics.end())},
        {"area_graphics", absl::StrFormat("0x%02X", info.area_graphics)},
    };
  }
  out["overworld_areas"] = std::move(areas);

  json rooms = json::object();
  for (const auto& [index, info] : inventory.rooms) {
    rooms[std::to_string(index)] = {{"blockset", info.blockset},
                                    {"spriteset", info.spriteset}};
  }
  out["rooms"] = std::move(rooms);
  return out;
}

std::map<uint8_t, std::set<std::string>> ParseSpritesetLabelCsv(
    const std::string& csv_text) {
  static const std::regex kCell(R"(^\s*0x([0-9A-Fa-f]{2})\s+([^\n]+))");
  std::map<uint8_t, std::set<std::string>> labels;
  for (const auto& row : ParseCsv(csv_text)) {
    for (size_t column = 2; column < 6 && column < row.size(); ++column) {
      std::smatch match;
      if (!std::regex_search(row[column], match, kCell)) {
        continue;
      }
      std::string name = match[2].str();
      while (!name.empty() &&
             std::isspace(static_cast<unsigned char>(name.back()))) {
        name.pop_back();
      }
      const auto value =
          static_cast<uint8_t>(std::stoi(match[1].str(), nullptr, 16));
      labels[value].insert(std::move(name));
    }
  }
  return labels;
}

}  // namespace yaze::zelda3
