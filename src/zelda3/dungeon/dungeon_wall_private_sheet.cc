#include "zelda3/dungeon/dungeon_wall_private_sheet.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <tuple>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/compression.h"
#include "core/source_artifact_publisher.h"
#include "rom/rom.h"
#include "util/macro.h"

namespace yaze::zelda3 {
namespace {

using Json = nlohmann::json;

absl::StatusOr<std::string> ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open private-sheet file: %s", path.string()));
  }
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

absl::StatusOr<int> ParseHexOrInt(const Json& value, const char* field) {
  if (value.is_number_integer()) {
    return static_cast<int>(value.get<int64_t>());
  }
  if (value.is_string()) {
    const auto& text = value.get_ref<const std::string&>();
    // Oracle kit/profile JSON uses unprefixed hex IDs like "054". Prefer base
    // 16 for plain hex digit strings so leading zeros are not read as octal.
    int base = 0;
    const bool prefixed =
        text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    if (!prefixed) {
      const bool hex_digits =
          !text.empty() &&
          std::all_of(text.begin(), text.end(),
                      [](unsigned char c) { return std::isxdigit(c) != 0; });
      if (hex_digits)
        base = 16;
    }
    try {
      return static_cast<int>(std::stoul(text, nullptr, base));
    } catch (...) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid %s value '%s'", field, text));
    }
  }
  return absl::InvalidArgumentError(absl::StrFormat(
      "Private-sheet field '%s' must be int or hex string", field));
}

absl::StatusOr<std::string> RequireString(const Json& object, const char* key) {
  if (!object.contains(key) || !object.at(key).is_string() ||
      object.at(key).get<std::string>().empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Private-sheet kit missing string '%s'", key));
  }
  return object.at(key).get<std::string>();
}

}  // namespace

absl::StatusOr<WallPrivateSheetBinding> LoadWallPrivateSheetBinding(
    const std::filesystem::path& kit_json,
    const std::filesystem::path& sheet_3bpp) {
  ASSIGN_OR_RETURN(const std::string kit_text, ReadFile(kit_json));
  Json root = Json::parse(kit_text, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return absl::InvalidArgumentError("Private-sheet kit is not valid JSON");
  }
  if (!root.contains("sheets") || !root["sheets"].is_array() ||
      root["sheets"].empty()) {
    return absl::InvalidArgumentError("Private-sheet kit has no sheets[]");
  }

  const Json* private_sheet = nullptr;
  for (const auto& sheet : root["sheets"]) {
    if (!sheet.is_object())
      continue;
    if (sheet.value("private_pointer", false)) {
      private_sheet = &sheet;
      break;
    }
  }
  if (private_sheet == nullptr) {
    return absl::FailedPreconditionError(
        "Private-sheet kit has no private_pointer sheet entry");
  }

  WallPrivateSheetBinding binding;
  binding.kit_path = kit_json.string();
  binding.sheet_path = sheet_3bpp.string();
  ASSIGN_OR_RETURN(binding.kit_id, RequireString(root, "kit"));
  ASSIGN_OR_RETURN(binding.profile_id, RequireString(root, "profile"));
  ASSIGN_OR_RETURN(binding.decoded_sha256,
                   RequireString(*private_sheet, "decoded_sha256"));

  if (private_sheet->contains("sheet")) {
    ASSIGN_OR_RETURN(const int sheet,
                     ParseHexOrInt((*private_sheet)["sheet"], "sheet"));
    binding.bg_request = static_cast<uint8_t>(sheet);
  }
  if (private_sheet->contains("copy_of")) {
    ASSIGN_OR_RETURN(const int copy_of,
                     ParseHexOrInt((*private_sheet)["copy_of"], "copy_of"));
    if (static_cast<uint8_t>(copy_of) != binding.bg_request) {
      return absl::FailedPreconditionError(
          "Private-sheet copy_of must equal logical BG request");
    }
  }
  if (private_sheet->contains("sprite_relative")) {
    ASSIGN_OR_RETURN(
        const int sprite,
        ParseHexOrInt((*private_sheet)["sprite_relative"], "sprite_relative"));
    binding.sprite_relative = static_cast<uint8_t>(sprite);
  }
  if (private_sheet->contains("slot")) {
    ASSIGN_OR_RETURN(const int slot,
                     ParseHexOrInt((*private_sheet)["slot"], "slot"));
    binding.slot = static_cast<uint8_t>(slot);
  }
  if (root.contains("private_row")) {
    ASSIGN_OR_RETURN(const int row,
                     ParseHexOrInt(root["private_row"], "private_row"));
    binding.required_blockset = static_cast<uint8_t>(row);
  }
  if (root.contains("rooms") && root["rooms"].is_array() &&
      !root["rooms"].empty()) {
    ASSIGN_OR_RETURN(const int room,
                     ParseHexOrInt(root["rooms"][0], "rooms[0]"));
    binding.room = static_cast<uint16_t>(room);
  }
  if (binding.slot != kWallPrivateSlot) {
    return absl::FailedPreconditionError(
        "Private-sheet binding requires slot 3");
  }
  if (binding.bg_request != kWallPrivateBgRequest ||
      binding.sprite_relative != kWallPrivateSpriteRelative) {
    return absl::FailedPreconditionError(
        "Private-sheet binding must keep BG 0x08 and sprite-relative 0x36");
  }

  ASSIGN_OR_RETURN(const std::string sheet_bytes, ReadFile(sheet_3bpp));
  if (sheet_bytes.size() != kWallPrivateDecoded3bppBytes) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Private-sheet 3bpp must be exactly %zu bytes (got %zu)",
        kWallPrivateDecoded3bppBytes, sheet_bytes.size()));
  }
  const std::string digest = core::ComputeSourceArtifactSha256(sheet_bytes);
  if (digest != binding.decoded_sha256) {
    return absl::FailedPreconditionError(
        "Private-sheet 3bpp identity mismatch vs kit decoded_sha256");
  }
  binding.decoded_3bpp.assign(sheet_bytes.begin(), sheet_bytes.end());
  binding.decoded_8bpp =
      gfx::SnesTo8bppSheet(std::span<const uint8_t>(binding.decoded_3bpp), 3);
  if (binding.decoded_8bpp.size() != kWallPrivateDecoded8bppBytes) {
    return absl::FailedPreconditionError(
        "Private-sheet 3bpp did not expand to one 8bpp dungeon sheet");
  }
  binding.required_profile = kWallPrivateArmoryProfile;
  binding.dungeon = kWallPrivateArmoryDungeon;
  return binding;
}

absl::Status AttachWallPrivateSheetObjectBindings(
    WallPrivateSheetBinding* binding, const Json& contract) {
  if (binding == nullptr) {
    return absl::InvalidArgumentError("Private-sheet binding is null");
  }
  if (!contract.contains("bindings") || !contract["bindings"].is_array()) {
    return absl::InvalidArgumentError("Contract missing bindings[]");
  }
  binding->objects.clear();
  for (const auto& item : contract["bindings"]) {
    if (!item.is_object())
      continue;
    WallPrivateSheetObjectExpectation expected;
    ASSIGN_OR_RETURN(const int object_id,
                     ParseHexOrInt(item.at("object_id"), "object_id"));
    ASSIGN_OR_RETURN(const int subtype,
                     ParseHexOrInt(item.at("subtype"), "subtype"));
    ASSIGN_OR_RETURN(expected.asset_file, RequireString(item, "file"));
    ASSIGN_OR_RETURN(expected.asset_sha256, RequireString(item, "sha256"));
    if (!item.contains("position_tile8") ||
        !item["position_tile8"].is_array() ||
        item["position_tile8"].size() != 2) {
      return absl::InvalidArgumentError(
          "Object binding requires position_tile8 [x,y]");
    }
    ASSIGN_OR_RETURN(expected.x,
                     ParseHexOrInt(item["position_tile8"][0], "position_x"));
    ASSIGN_OR_RETURN(expected.y,
                     ParseHexOrInt(item["position_tile8"][1], "position_y"));
    expected.id = object_id;
    expected.size = subtype;
    binding->objects.push_back(std::move(expected));
  }
  if (binding->objects.size() < 2) {
    return absl::FailedPreconditionError(
        "Armory private-sheet recipe expects at least worktable+drawers pins");
  }
  return absl::OkStatus();
}

absl::StatusOr<WallPrivateSheetResolution> ResolveWallPrivateSheet(
    const WallPrivateSheetBinding& binding,
    const WallPrivateSheetContext& context) {
  WallPrivateSheetResolution result;
  result.decoded_sha256 = binding.decoded_sha256;
  result.bg_sheet = binding.bg_request;
  result.sprite_sheet = static_cast<uint8_t>(binding.sprite_relative + 0x73);

  auto refuse = [&](std::string reason) -> WallPrivateSheetResolution {
    result.applied = false;
    result.reason = std::move(reason);
    result.sheet_overrides_8bpp.clear();
    return result;
  };

  if (!context.indoor) {
    return refuse("private sheet requires indoor dungeon context");
  }
  if (context.room != static_cast<int>(binding.room)) {
    return refuse(
        absl::StrFormat("private sheet refuses room 0x%03X (requires 0x%03X)",
                        context.room, binding.room));
  }
  if (context.profile != binding.required_profile) {
    return refuse(
        absl::StrFormat("private sheet refuses profile %d (requires %d)",
                        context.profile, binding.required_profile));
  }
  if (context.blockset != binding.required_blockset) {
    return refuse(absl::StrFormat(
        "private sheet refuses blockset 0x%02X (requires 0x%02X)",
        context.blockset, binding.required_blockset));
  }
  if (context.dungeon >= 0 && context.dungeon != binding.dungeon) {
    return refuse(
        absl::StrFormat("private sheet refuses dungeon %d (requires %d)",
                        context.dungeon, binding.dungeon));
  }
  if (binding.decoded_8bpp.size() != kWallPrivateDecoded8bppBytes) {
    return absl::FailedPreconditionError(
        "Private-sheet binding has no decoded 8bpp pixels");
  }

  result.applied = true;
  result.reason =
      "applied room54 profile4 private stream for BG 0x08 / sprite "
      "0x36 without mutating global sheet identities";
  result.sheet_overrides_8bpp.emplace(result.bg_sheet, binding.decoded_8bpp);
  result.sheet_overrides_8bpp.emplace(result.sprite_sheet,
                                      binding.decoded_8bpp);
  return result;
}

absl::StatusOr<WallPrivateSheetRecipeReport> DryRunWallPrivateSheetRecipe(
    const WallPrivateSheetBinding& binding, const Json& object_plan) {
  WallPrivateSheetRecipeReport report;
  if (!object_plan.is_object()) {
    return absl::InvalidArgumentError("Object plan must be a JSON object");
  }
  const std::string plan_text = object_plan.dump();
  report.plan_sha256 = core::ComputeSourceArtifactSha256(plan_text);

  if (!object_plan.contains("room") ||
      object_plan["room"].get<int>() != static_cast<int>(binding.room)) {
    report.reason = "object plan room does not match private-sheet room 0x54";
    return report;
  }
  if (!object_plan.contains("edits") || !object_plan["edits"].is_array()) {
    report.reason = "object plan missing edits[]";
    return report;
  }
  if (binding.objects.empty()) {
    report.reason =
        "private-sheet binding has no object asset pins for recipe dry-run";
    return report;
  }

  std::vector<bool> matched(binding.objects.size(), false);
  for (const auto& edit : object_plan["edits"]) {
    if (!edit.is_object() || !edit.contains("object"))
      continue;
    const auto& object = edit["object"];
    if (!object.contains("id") || !object.contains("x") ||
        !object.contains("y") || !object.contains("size")) {
      continue;
    }
    const int id = object["id"].get<int>();
    const int x = object["x"].get<int>();
    const int y = object["y"].get<int>();
    const int size = object["size"].get<int>();
    for (size_t i = 0; i < binding.objects.size(); ++i) {
      const auto& expected = binding.objects[i];
      if (expected.id == id && expected.x == x && expected.y == y &&
          expected.size == size) {
        matched[i] = true;
        report.matched_assets.push_back(expected.asset_file);
        break;
      }
    }
    ++report.edit_count;
  }

  for (size_t i = 0; i < matched.size(); ++i) {
    if (!matched[i]) {
      report.reason = absl::StrFormat(
          "object plan missing expected furniture %s at (%d,%d) subtype 0x%02X",
          binding.objects[i].asset_file, binding.objects[i].x,
          binding.objects[i].y, binding.objects[i].size);
      return report;
    }
  }

  report.accepted = true;
  report.reason =
      "recipe dry-run accepted: room54 object plan matches private-sheet "
      "furniture pins; no ROM write";
  return report;
}

absl::StatusOr<WallPrivateSheetResolution> ResolveCompiledWallPrivateSheet(
    const Rom& rom, int room, int active_profile) {
  WallPrivateSheetResolution result;
  result.bg_sheet = kWallPrivateBgRequest;
  result.sprite_sheet = kWallPrivateSpriteAbsolute;
  if (room < 0 || room >= 0x128) {
    return absl::OutOfRangeError("Compiled WSP1 room is out of range");
  }
  if (active_profile <= 0) {
    result.applied = false;
    result.reason = "active wall profile is 0; private sheet stays off";
    return result;
  }

  const auto& bytes = rom.vector();
  auto pc = [](uint32_t a) -> size_t {
    return ((a >> 16) & 0x7F) * 0x8000 + (a & 0x7FFF);
  };
  auto fail = []() {
    return absl::FailedPreconditionError(
        "Unsupported or malformed compiled WSP1 private-sheet metadata");
  };

  // Locate WSP1 inside the existing selector span after WSR1.
  constexpr uint32_t kSpanBegin = 0x44E000;
  constexpr uint32_t kSpanEnd = 0x450000;
  std::optional<size_t> wsp1_pc;
  for (uint32_t addr = kSpanBegin; addr + 8 <= kSpanEnd; ++addr) {
    const size_t at = pc(addr);
    if (at + 8 > bytes.size())
      break;
    if (std::string(bytes.begin() + at, bytes.begin() + at + 4) == "WSP1") {
      wsp1_pc = at;
      break;
    }
  }
  if (!wsp1_pc.has_value()) {
    result.applied = false;
    result.reason = "compiled ROM has no WSP1 metadata";
    return result;
  }

  const size_t d = *wsp1_pc;
  if (bytes[d + 4] != 1 || bytes[d + 5] != 12)
    return fail();
  const uint16_t count =
      static_cast<uint16_t>(bytes[d + 6] | (bytes[d + 7] << 8));
  if (count == 0 || count > 64)
    return fail();
  const size_t table_bytes = static_cast<size_t>(count) * 12;
  if (d + 8 + table_bytes > bytes.size())
    return fail();

  struct MatchedRecord {
    uint8_t kind = 0;
    uint8_t request = 0;
    uint8_t slot = 0;
    uint8_t half = 0;
    uint32_t source = 0;
  };

  std::set<std::tuple<uint16_t, uint8_t, uint8_t, uint8_t>> seen;
  std::vector<MatchedRecord> matched;
  for (uint16_t i = 0; i < count; ++i) {
    const size_t r = d + 8 + static_cast<size_t>(i) * 12;
    const uint16_t rec_room =
        static_cast<uint16_t>(bytes[r] | (bytes[r + 1] << 8));
    const uint8_t kind = bytes[r + 2];
    const uint8_t request = bytes[r + 3];
    const uint8_t slot = bytes[r + 4];
    const uint8_t half = bytes[r + 5];
    const uint32_t source = bytes[r + 6] | (bytes[r + 7] << 8) |
                            (static_cast<uint32_t>(bytes[r + 8]) << 16);
    const uint16_t length =
        static_cast<uint16_t>(bytes[r + 9] | (bytes[r + 10] << 8));
    const uint8_t required_profile = bytes[r + 11];
    const auto identity = std::make_tuple(rec_room, kind, slot, request);
    if (!seen.insert(identity).second)
      return fail();
    if (kind > 1 || half > 1 || length != kWallPrivateDecoded3bppBytes)
      return fail();
    // Validated slots: 3 (shared BG08/sprite36 or BG08 alone) and 6
    // (independent BG stream such as room64's 0x6F).
    if (slot != kWallPrivateSlot && slot != 6)
      return fail();
    if (kind == 0) {
      if (slot == kWallPrivateSlot) {
        if (request != kWallPrivateBgRequest || half != 1)
          return fail();
      } else if (request == 0) {
        return fail();
      }
    } else {
      // Sprite streams remain slot-3 / relative-0x36 / low-half only.
      if (slot != kWallPrivateSlot || request != kWallPrivateSpriteRelative ||
          half != 0)
        return fail();
    }
    // Private kit streams live in kit storage after Glacia ($458000..).
    if (source < 0x458000 || source >= 0x45F000)
      return fail();
    if (rec_room != static_cast<uint16_t>(room))
      continue;
    if (required_profile != static_cast<uint8_t>(active_profile))
      continue;
    matched.push_back(MatchedRecord{kind, request, slot, half, source});
  }

  if (matched.empty()) {
    result.applied = false;
    result.reason =
        absl::StrFormat("no WSP1 record for room 0x%03X with active profile %d",
                        room, active_profile);
    return result;
  }

  // Room54-style pair: sprite + BG08/slot3 must share one source. Independent
  // BG slot6 streams may use different sources and never synthesize sprite A9.
  std::optional<uint32_t> paired_source;
  bool saw_bg08 = false;
  bool saw_sprite = false;
  for (const auto& rec : matched) {
    if (rec.kind == 0 && rec.slot == kWallPrivateSlot) {
      saw_bg08 = true;
      if (!paired_source.has_value())
        paired_source = rec.source;
      else if (*paired_source != rec.source)
        return fail();
    } else if (rec.kind == 1) {
      saw_sprite = true;
      if (!paired_source.has_value())
        paired_source = rec.source;
      else if (*paired_source != rec.source)
        return fail();
    }
  }
  if (saw_sprite && !saw_bg08)
    return fail();

  std::map<uint32_t, std::vector<uint8_t>> decoded_by_source;
  auto DecodeSource =
      [&](uint32_t source) -> absl::StatusOr<const std::vector<uint8_t>*> {
    if (auto it = decoded_by_source.find(source); it != decoded_by_source.end())
      return &it->second;
    const size_t source_pc = pc(source);
    ASSIGN_OR_RETURN(auto decoded, gfx::lc_lz2::DecompressExact(
                                       bytes.data(), bytes.size(), source_pc,
                                       kWallPrivateDecoded3bppBytes * 4,
                                       /*big_endian_copy=*/false));
    if (decoded.data.size() != kWallPrivateDecoded3bppBytes) {
      return absl::DataLossError(
          absl::StrFormat("WSP1 private stream decoded to %zu bytes; need %zu",
                          decoded.data.size(), kWallPrivateDecoded3bppBytes));
    }
    auto pixels = gfx::SnesTo8bppSheet(
        std::span<const uint8_t>(decoded.data.data(), decoded.data.size()), 3);
    if (pixels.size() != kWallPrivateDecoded8bppBytes) {
      return absl::DataLossError(
          "WSP1 private stream did not expand to one 8bpp sheet");
    }
    const std::string digest = core::ComputeSourceArtifactSha256(
        std::string(reinterpret_cast<const char*>(decoded.data.data()),
                    decoded.data.size()));
    if (decoded_by_source.empty()) {
      result.decoded_sha256 = digest;
    } else if (result.decoded_sha256 != digest) {
      // Multiple distinct streams: leave a blank digest rather than implying
      // one shared identity across independent BG sheets.
      result.decoded_sha256.clear();
    }
    auto [inserted, _] = decoded_by_source.emplace(source, std::move(pixels));
    return &inserted->second;
  };

  bool applied_sprite = false;
  bool applied_bg = false;
  for (const auto& rec : matched) {
    ASSIGN_OR_RETURN(const std::vector<uint8_t>* pixels,
                     DecodeSource(rec.source));
    if (rec.kind == 0) {
      const uint16_t sheet = rec.request;
      auto existing = result.sheet_overrides_8bpp.find(sheet);
      if (existing != result.sheet_overrides_8bpp.end() &&
          existing->second != *pixels)
        return fail();
      result.sheet_overrides_8bpp[sheet] = *pixels;
      if (!applied_bg || sheet == kWallPrivateBgRequest)
        result.bg_sheet = static_cast<uint8_t>(sheet);
      applied_bg = true;
    } else {
      const uint16_t sheet =
          static_cast<uint16_t>(rec.request + 0x73);  // absolute sprite
      auto existing = result.sheet_overrides_8bpp.find(sheet);
      if (existing != result.sheet_overrides_8bpp.end() &&
          existing->second != *pixels)
        return fail();
      result.sheet_overrides_8bpp[sheet] = *pixels;
      result.sprite_sheet = static_cast<uint8_t>(sheet);
      applied_sprite = true;
    }
  }

  // BG-only rooms (room64) must not invent a sprite override from the BG
  // stream. Sprite sheet field stays informational only when paired.
  if (!applied_sprite) {
    result.sprite_sheet = 0;
  }
  (void)applied_bg;

  result.applied = true;
  result.reason =
      "applied compiled WSP1 private stream for matching room/profile";
  return result;
}

}  // namespace yaze::zelda3
