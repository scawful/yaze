#include "core/hack_manifest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "rom/snes.h"
#include "util/json.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze::core {

std::string AddressOwnershipToString(AddressOwnership ownership) {
  switch (ownership) {
    case AddressOwnership::kVanillaSafe:
      return "vanilla_safe";
    case AddressOwnership::kHookPatched:
      return "hook_patched";
    case AddressOwnership::kAsmOwned:
      return "asm_owned";
    case AddressOwnership::kShared:
      return "shared";
    case AddressOwnership::kAsmExpansion:
      return "asm_expansion";
    case AddressOwnership::kRam:
      return "ram";
    case AddressOwnership::kMirror:
      return "mirror";
  }
  return "unknown";
}

namespace {

absl::StatusOr<uint32_t> ParseHexAddress(const std::string& str) {
  try {
    if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
      return static_cast<uint32_t>(std::stoul(str.substr(2), nullptr, 16));
    }
    return static_cast<uint32_t>(std::stoul(str, nullptr, 16));
  } catch (const std::exception& exc) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid hex address '%s': %s", str, exc.what()));
  }
}

absl::StatusOr<AddressOwnership> ParseOwnership(const std::string& str) {
  if (str == "vanilla_safe")
    return AddressOwnership::kVanillaSafe;
  if (str == "hook_patched")
    return AddressOwnership::kHookPatched;
  if (str == "asm_owned")
    return AddressOwnership::kAsmOwned;
  if (str == "shared")
    return AddressOwnership::kShared;
  if (str == "asm_expansion")
    return AddressOwnership::kAsmExpansion;
  if (str == "ram")
    return AddressOwnership::kRam;
  if (str == "mirror")
    return AddressOwnership::kMirror;
  return absl::InvalidArgumentError(
      absl::StrFormat("Unknown ownership string '%s'", str));
}

bool IsAsmOwned(AddressOwnership ownership) {
  switch (ownership) {
    case AddressOwnership::kHookPatched:
    case AddressOwnership::kAsmOwned:
    case AddressOwnership::kAsmExpansion:
    case AddressOwnership::kMirror:
      return true;
    case AddressOwnership::kVanillaSafe:
    case AddressOwnership::kShared:
    case AddressOwnership::kRam:
      return false;
  }
  return false;
}

uint32_t NormalizeSnesAddress(uint32_t address) {
  // Treat FastROM mirrors ($80-$FF) as equivalent to the canonical banks
  // ($00-$7F). The hack manifest is emitted from ASM `org $XXXXXX` directives
  // and typically uses canonical addresses.
  if (address >= 0x800000 && address <= 0xFFFFFF) {
    address &= 0x7FFFFF;
  }
  return address;
}

}  // namespace

void HackManifest::Reset() {
  loaded_ = false;
  manifest_version_ = 0;
  hack_name_.clear();
  total_hooks_ = 0;
  protected_regions_.clear();
  owned_banks_.clear();
  room_tag_map_.clear();
  room_tags_.clear();
  feature_flag_map_.clear();
  feature_flags_.clear();
  sram_map_.clear();
  sram_variables_.clear();
  message_layout_ = MessageLayout{};
  build_pipeline_ = BuildPipeline{};
  project_registry_ = ProjectRegistry{};
}

absl::Status HackManifest::LoadFromFile(const std::string& filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return absl::NotFoundError("Could not open manifest: " + filepath);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return LoadFromString(buffer.str());
}

absl::Status HackManifest::LoadFromString(const std::string& json_content) {
  Reset();

  Json root;
  try {
    root = Json::parse(json_content);
  } catch (const std::exception& exc) {
    return absl::InvalidArgumentError(
        std::string("Failed to parse manifest JSON: ") + exc.what());
  }

  manifest_version_ = root.value("manifest_version", 0);
  hack_name_ = root.value("hack_name", "");

  // Build pipeline
  if (root.contains("build_pipeline")) {
    auto& pipeline = root["build_pipeline"];
    build_pipeline_.dev_rom = pipeline.value("dev_rom", "");
    build_pipeline_.patched_rom = pipeline.value("patched_rom", "");
    build_pipeline_.assembler = pipeline.value("assembler", "");
    build_pipeline_.entry_point = pipeline.value("entry_point", "");
    build_pipeline_.build_script = pipeline.value("build_script", "");
  }

  // Protected regions
  if (root.contains("protected_regions")) {
    auto& protected_json = root["protected_regions"];
    total_hooks_ = protected_json.value("total_hooks", 0);
    if (protected_json.contains("regions") &&
        protected_json["regions"].is_array()) {
      for (auto& region_json : protected_json["regions"]) {
        ProtectedRegion region;
        ASSIGN_OR_RETURN(region.start, ParseHexAddress(region_json.value(
                                           "start", "0x000000")));
        ASSIGN_OR_RETURN(region.end,
                         ParseHexAddress(region_json.value("end", "0x000000")));
        region.start = NormalizeSnesAddress(region.start);
        region.end = NormalizeSnesAddress(region.end);
        region.hook_count = region_json.value("hook_count", 0);
        region.module = region_json.value("module", "");
        protected_regions_.push_back(region);
      }
    }
  }

  // Sort protected regions for binary search
  std::sort(protected_regions_.begin(), protected_regions_.end(),
            [](const ProtectedRegion& lhs, const ProtectedRegion& rhs) {
              return lhs.start < rhs.start;
            });

  // Owned banks
  if (root.contains("owned_banks") && root["owned_banks"].contains("banks")) {
    for (auto& bank_json : root["owned_banks"]["banks"]) {
      OwnedBank bank;
      uint32_t bank_u32 = 0;
      ASSIGN_OR_RETURN(bank_u32,
                       ParseHexAddress(bank_json.value("bank", "0x00")));
      bank.bank = static_cast<uint8_t>(bank_u32 & 0xFF);
      if (bank.bank >= 0x80) {
        bank.bank &= 0x7F;
      }
      ASSIGN_OR_RETURN(bank.bank_start, ParseHexAddress(bank_json.value(
                                            "bank_start", "0x000000")));
      ASSIGN_OR_RETURN(bank.bank_end, ParseHexAddress(bank_json.value(
                                          "bank_end", "0x000000")));
      bank.bank_start = NormalizeSnesAddress(bank.bank_start);
      bank.bank_end = NormalizeSnesAddress(bank.bank_end);
      ASSIGN_OR_RETURN(bank.ownership, ParseOwnership(bank_json.value(
                                           "ownership", "asm_owned")));
      bank.ownership_note = bank_json.value("ownership_note", "");
      owned_banks_[bank.bank] = bank;
    }
  }

  // Room tags
  if (root.contains("room_tags") && root["room_tags"].contains("tags")) {
    for (auto& tag_json : root["room_tags"]["tags"]) {
      RoomTagEntry tag;
      uint32_t tag_id_u32 = 0;
      ASSIGN_OR_RETURN(tag_id_u32,
                       ParseHexAddress(tag_json.value("tag_id", "0x00")));
      tag.tag_id = static_cast<uint8_t>(tag_id_u32 & 0xFF);
      ASSIGN_OR_RETURN(tag.address,
                       ParseHexAddress(tag_json.value("address", "0x000000")));
      tag.address = NormalizeSnesAddress(tag.address);
      tag.name = tag_json.value("name", "");
      tag.purpose = tag_json.value("purpose", "");
      tag.source = tag_json.value("source", "");
      tag.feature_flag = tag_json.value("feature_flag", "");
      tag.enabled = tag_json.value("enabled", true);
      room_tag_map_[tag.tag_id] = tag;
      room_tags_.push_back(tag);
    }
  }

  // Feature flags
  if (root.contains("feature_flags") &&
      root["feature_flags"].contains("flags")) {
    for (auto& flag_json : root["feature_flags"]["flags"]) {
      FeatureFlag flag;
      flag.name = flag_json.value("name", "");
      flag.value = flag_json.value("value", 0);
      flag.enabled = flag_json.value("enabled", false);
      flag.source = flag_json.value("source", "");
      feature_flag_map_[flag.name] = flag;
      feature_flags_.push_back(flag);
    }
  }

  // SRAM variables
  if (root.contains("sram") && root["sram"].contains("variables")) {
    for (auto& var_json : root["sram"]["variables"]) {
      SramVariable var;
      var.name = var_json.value("name", "");
      ASSIGN_OR_RETURN(var.address,
                       ParseHexAddress(var_json.value("address", "0x000000")));
      var.purpose = var_json.value("purpose", "");
      sram_map_[var.address] = var;
      sram_variables_.push_back(var);
    }
  }

  // Message layout
  if (root.contains("messages")) {
    auto& msg = root["messages"];
    if (msg.contains("hook_address") && msg["hook_address"].is_string()) {
      ASSIGN_OR_RETURN(message_layout_.hook_address,
                       ParseHexAddress(msg["hook_address"].get<std::string>()));
      message_layout_.hook_address =
          NormalizeSnesAddress(message_layout_.hook_address);
    }
    if (msg.contains("data_start")) {
      ASSIGN_OR_RETURN(message_layout_.data_start,
                       ParseHexAddress(msg.value("data_start", "0x000000")));
      message_layout_.data_start = NormalizeSnesAddress(message_layout_.data_start);
    }
    if (msg.contains("data_end")) {
      ASSIGN_OR_RETURN(message_layout_.data_end,
                       ParseHexAddress(msg.value("data_end", "0x000000")));
      message_layout_.data_end = NormalizeSnesAddress(message_layout_.data_end);
    }
    message_layout_.vanilla_count = msg.value("vanilla_count", 397);
    if (msg.contains("expanded_range")) {
      auto& expanded = msg["expanded_range"];
      uint32_t first_id = 0;
      uint32_t last_id = 0;
      ASSIGN_OR_RETURN(first_id,
                       ParseHexAddress(expanded.value("first", "0x000")));
      ASSIGN_OR_RETURN(last_id,
                       ParseHexAddress(expanded.value("last", "0x000")));
      message_layout_.first_expanded_id =
          static_cast<uint16_t>(first_id & 0xFFFF);
      message_layout_.last_expanded_id =
          static_cast<uint16_t>(last_id & 0xFFFF);
      message_layout_.expanded_count = expanded.value("count", 0);
    }
  }

  loaded_ = true;
  return absl::OkStatus();
}

AddressOwnership HackManifest::ClassifyAddress(uint32_t address) const {
  if (!loaded_)
    return AddressOwnership::kVanillaSafe;

  address = NormalizeSnesAddress(address);

  // Check bank ownership first
  uint8_t bank = static_cast<uint8_t>((address >> 16) & 0xFF);
  auto bank_it = owned_banks_.find(bank);
  if (bank_it != owned_banks_.end()) {
    return bank_it->second.ownership;
  }

  // Check protected regions (binary search since they're sorted)
  if (IsProtected(address)) {
    return AddressOwnership::kHookPatched;
  }

  return AddressOwnership::kVanillaSafe;
}

bool HackManifest::IsWriteOverwritten(uint32_t address) const {
  auto ownership = ClassifyAddress(address);
  return ownership == AddressOwnership::kHookPatched ||
         ownership == AddressOwnership::kAsmOwned ||
         ownership == AddressOwnership::kAsmExpansion;
}

bool HackManifest::IsProtected(uint32_t address) const {
  address = NormalizeSnesAddress(address);

  // Binary search for the region containing this address
  auto iter = std::upper_bound(
      protected_regions_.begin(), protected_regions_.end(), address,
      [](uint32_t addr, const ProtectedRegion& region) {
        return addr < region.start;
      });

  if (iter != protected_regions_.begin()) {
    --iter;
    if (address >= iter->start && address < iter->end) {
      return true;
    }
  }
  return false;
}

std::optional<AddressOwnership> HackManifest::GetBankOwnership(
    uint8_t bank) const {
  if (bank >= 0x80) {
    bank &= 0x7F;
  }
  auto iter = owned_banks_.find(bank);
  if (iter == owned_banks_.end())
    return std::nullopt;
  return iter->second.ownership;
}

std::string HackManifest::GetRoomTagLabel(uint8_t tag_id) const {
  auto iter = room_tag_map_.find(tag_id);
  if (iter == room_tag_map_.end())
    return "";
  return iter->second.name;
}

std::optional<RoomTagEntry> HackManifest::GetRoomTag(uint8_t tag_id) const {
  auto iter = room_tag_map_.find(tag_id);
  if (iter == room_tag_map_.end())
    return std::nullopt;
  return iter->second;
}

bool HackManifest::IsFeatureEnabled(const std::string& flag_name) const {
  auto iter = feature_flag_map_.find(flag_name);
  if (iter == feature_flag_map_.end())
    return false;
  return iter->second.enabled;
}

std::vector<WriteConflict> HackManifest::AnalyzeWriteRanges(
    const std::vector<std::pair<uint32_t, uint32_t>>& ranges) const {
  std::vector<WriteConflict> conflicts;
  if (!loaded_) {
    return conflicts;
  }

  for (const auto& range : ranges) {
    const uint32_t start = NormalizeSnesAddress(range.first);
    const uint32_t end = NormalizeSnesAddress(range.second);
    if (end <= start) {
      continue;
    }

    // 1) Conflicts at the bank level (asm_owned/asm_expansion/mirror/ram/shared)
    // Only report banks that are truly ASM-owned (not shared).
    uint32_t cur = start;
    while (cur < end) {
      const uint8_t bank = static_cast<uint8_t>((cur >> 16) & 0xFF);
      const uint32_t next_bank = (cur & 0xFF0000) + 0x010000;
      const uint32_t seg_end = std::min(end, next_bank);

      auto bank_it = owned_banks_.find(bank);
      if (bank_it != owned_banks_.end()) {
        const auto ownership = bank_it->second.ownership;
        if (IsAsmOwned(ownership) &&
            ownership != AddressOwnership::kHookPatched) {
          WriteConflict conflict;
          conflict.address = cur;
          conflict.ownership = ownership;
          conflict.module = bank_it->second.ownership_note;
          conflicts.push_back(std::move(conflict));
        }
      }

      cur = seg_end;
    }

    // 2) Conflicts from hook-patched protected regions (vanilla banks).
    // protected_regions_ is sorted by start, with [start, end) semantics.
    auto iter = std::upper_bound(
        protected_regions_.begin(), protected_regions_.end(), start,
        [](uint32_t addr, const ProtectedRegion& region) {
          return addr < region.start;
        });

    if (iter != protected_regions_.begin()) {
      auto prev = iter;
      --prev;
      if (prev->end > start) {
        iter = prev;
      }
    }

    for (; iter != protected_regions_.end() && iter->start < end; ++iter) {
      const uint32_t overlap_start = std::max(start, iter->start);
      const uint32_t overlap_end = std::min(end, iter->end);
      if (overlap_start < overlap_end) {
        WriteConflict conflict;
        conflict.address = overlap_start;
        conflict.ownership = AddressOwnership::kHookPatched;
        conflict.module = iter->module;
        conflicts.push_back(std::move(conflict));
      }
    }
  }

  return conflicts;
}

std::vector<WriteConflict> HackManifest::AnalyzePcWriteRanges(
    const std::vector<std::pair<uint32_t, uint32_t>>& pc_ranges) const {
  if (!loaded_) {
    return {};
  }

  std::vector<std::pair<uint32_t, uint32_t>> snes_ranges;
  snes_ranges.reserve(pc_ranges.size());

  for (const auto& range : pc_ranges) {
    uint32_t pc_start = range.first;
    const uint32_t pc_end = range.second;
    if (pc_end <= pc_start) {
      continue;
    }

    // Split at LoROM bank boundaries (0x8000 bytes per bank segment in PC
    // space). PcToSnes() is linear within these segments.
    while (pc_start < pc_end) {
      const uint32_t next_boundary = (pc_start & ~0x7FFFu) + 0x8000u;
      const uint32_t seg_end = std::min(pc_end, next_boundary);
      const uint32_t seg_len = seg_end - pc_start;
      const uint32_t snes_start = PcToSnes(pc_start);
      const uint32_t snes_end = snes_start + seg_len;
      snes_ranges.emplace_back(snes_start, snes_end);
      pc_start = seg_end;
    }
  }

  return AnalyzeWriteRanges(snes_ranges);
}

std::string HackManifest::GetSramVariableName(uint32_t address) const {
  auto iter = sram_map_.find(address);
  if (iter == sram_map_.end())
    return "";
  return iter->second.name;
}

bool HackManifest::IsExpandedMessage(uint16_t message_id) const {
  return message_id >= message_layout_.first_expanded_id &&
         message_id <= message_layout_.last_expanded_id;
}

// ============================================================================
// Project Registry Loading
// ============================================================================

absl::Status HackManifest::LoadProjectRegistry(
    const std::string& code_folder) {
  namespace fs = std::filesystem;

  project_registry_ = ProjectRegistry{};
  fs::path base(code_folder);

  // Look for data files in Docs/Dev/Planning/ (generated output location)
  fs::path planning = base / "Docs" / "Dev" / "Planning";

  // ── Load dungeons.json ──────────────────────────────────────────────────
  fs::path dungeons_path = planning / "dungeons.json";
  if (fs::exists(dungeons_path)) {
    std::ifstream file(dungeons_path);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      try {
        Json root = Json::parse(buffer.str());
        if (root.contains("dungeons") && root["dungeons"].is_array()) {
          for (const auto& dj : root["dungeons"]) {
            DungeonEntry entry;
            entry.id = dj.value("id", "");
            entry.name = dj.value("name", "");
            entry.vanilla_name = dj.value("vanilla_name", "");

            // Rooms
            if (dj.contains("rooms") && dj["rooms"].is_array()) {
              for (const auto& rj : dj["rooms"]) {
                DungeonRoom room;
                std::string id_str = rj.value("id", "0x00");
                auto parsed = ParseHexAddress(id_str);
                room.id = parsed.ok() ? static_cast<int>(*parsed) : 0;
                room.name = rj.value("name", "");
                room.grid_row = rj.value("grid_row", 0);
                room.grid_col = rj.value("grid_col", 0);
                room.type = rj.value("type", "normal");
                room.palette = rj.value("palette", 0);
                room.blockset = rj.value("blockset", 0);
                room.spriteset = rj.value("spriteset", 0);
                room.tag1 = static_cast<uint8_t>(rj.value("tag1", 0));
                room.tag2 = static_cast<uint8_t>(rj.value("tag2", 0));
                entry.rooms.push_back(std::move(room));
              }
            }

            // Stairs
            if (dj.contains("stairs") && dj["stairs"].is_array()) {
              for (const auto& sj : dj["stairs"]) {
                DungeonConnection conn;
                std::string from_str = sj.value("from", "0x00");
                std::string to_str = sj.value("to", "0x00");
                auto from_parsed = ParseHexAddress(from_str);
                auto to_parsed = ParseHexAddress(to_str);
                conn.from_room = from_parsed.ok()
                                     ? static_cast<int>(*from_parsed)
                                     : 0;
                conn.to_room =
                    to_parsed.ok() ? static_cast<int>(*to_parsed) : 0;
                conn.label = sj.value("label", "");
                entry.stairs.push_back(std::move(conn));
              }
            }

            // Holewarps
            if (dj.contains("holewarps") && dj["holewarps"].is_array()) {
              for (const auto& hj : dj["holewarps"]) {
                DungeonConnection conn;
                std::string from_str = hj.value("from", "0x00");
                std::string to_str = hj.value("to", "0x00");
                auto from_parsed = ParseHexAddress(from_str);
                auto to_parsed = ParseHexAddress(to_str);
                conn.from_room = from_parsed.ok()
                                     ? static_cast<int>(*from_parsed)
                                     : 0;
                conn.to_room =
                    to_parsed.ok() ? static_cast<int>(*to_parsed) : 0;
                conn.label = hj.value("label", "");
                entry.holewarps.push_back(std::move(conn));
              }
            }

            // Doors
            if (dj.contains("doors") && dj["doors"].is_array()) {
              for (const auto& doorj : dj["doors"]) {
                DungeonConnection conn;
                std::string from_str = doorj.value("from", "0x00");
                std::string to_str = doorj.value("to", "0x00");
                auto from_parsed = ParseHexAddress(from_str);
                auto to_parsed = ParseHexAddress(to_str);
                conn.from_room = from_parsed.ok()
                                     ? static_cast<int>(*from_parsed)
                                     : 0;
                conn.to_room =
                    to_parsed.ok() ? static_cast<int>(*to_parsed) : 0;
                conn.label = doorj.value("label", "");
                conn.direction = doorj.value("direction", "");
                entry.doors.push_back(std::move(conn));
              }
            }

            project_registry_.dungeons.push_back(std::move(entry));
          }
        }
      } catch (const std::exception& exc) {
        LOG_WARN("HackManifest", "Failed to parse dungeons.json: %s",
                 exc.what());
      }
    }
  }

  // ── Load overworld.json ─────────────────────────────────────────────────
  fs::path overworld_path = planning / "overworld.json";
  if (fs::exists(overworld_path)) {
    std::ifstream file(overworld_path);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      try {
        Json root = Json::parse(buffer.str());
        if (root.contains("areas") && root["areas"].is_array()) {
          for (const auto& aj : root["areas"]) {
            OverworldArea area;
            std::string id_str = aj.value("area_id", "0x00");
            auto parsed = ParseHexAddress(id_str);
            area.area_id = parsed.ok() ? static_cast<int>(*parsed) : 0;
            area.name = aj.value("name", "");
            area.world = aj.value("world", "");
            area.grid_row = aj.value("grid_row", 0);
            area.grid_col = aj.value("grid_col", 0);
            project_registry_.overworld_areas.push_back(std::move(area));
          }
        }
      } catch (const std::exception& exc) {
        LOG_WARN("HackManifest", "Failed to parse overworld.json: %s",
                 exc.what());
      }
    }
  }

  // ── Load oracle_room_labels.json ────────────────────────────────────────
  fs::path labels_path = planning / "oracle_room_labels.json";
  if (fs::exists(labels_path)) {
    std::ifstream file(labels_path);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      try {
        Json root = Json::parse(buffer.str());
        if (root.contains("resource_labels") &&
            root["resource_labels"].contains("room")) {
          for (auto& [key, value] : root["resource_labels"]["room"].items()) {
            project_registry_.room_labels[key] = value.get<std::string>();
          }
        }
      } catch (const std::exception& exc) {
        LOG_WARN("HackManifest", "Failed to parse oracle_room_labels.json: %s",
                 exc.what());
      }
    }
  }

  if (!project_registry_.dungeons.empty() ||
      !project_registry_.overworld_areas.empty() ||
      !project_registry_.room_labels.empty()) {
    LOG_DEBUG("HackManifest",
              "Loaded project registry: %zu dungeons, %zu overworld areas, "
              "%zu room labels",
              project_registry_.dungeons.size(),
              project_registry_.overworld_areas.size(),
              project_registry_.room_labels.size());
  }

  return absl::OkStatus();
}

}  // namespace yaze::core
