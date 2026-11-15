#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_WALL_PRIVATE_SHEET_H_
#define YAZE_ZELDA3_DUNGEON_DUNGEON_WALL_PRIVATE_SHEET_H_

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace yaze::zelda3 {

// Source-mode mirror of Oracle WSP1 private-stream admission for room 54.
// Logical BG request 0x08 and sprite-relative 0x36 stay as caller IDs; only the
// decoded private pixels replace those sheet lookups when the full context
// matches. Wrong context refuses instead of falling back to global 08/36 art.
inline constexpr int kWallPrivateArmoryRoom = 0x54;
inline constexpr int kWallPrivateArmoryProfile = 4;
inline constexpr int kWallPrivateArmoryBlockset = 0x19;
inline constexpr int kWallPrivateArmoryDungeon = 10;
inline constexpr uint8_t kWallPrivateBgRequest = 0x08;
inline constexpr uint8_t kWallPrivateSpriteRelative = 0x36;
inline constexpr uint8_t kWallPrivateSpriteAbsolute =
    static_cast<uint8_t>(kWallPrivateSpriteRelative + 0x73);
inline constexpr uint8_t kWallPrivateSlot = 3;
inline constexpr size_t kWallPrivateDecoded3bppBytes = 0x600;
inline constexpr size_t kWallPrivateDecoded8bppBytes = 4096;

struct WallPrivateSheetContext {
  int room = -1;
  int profile = -1;
  int blockset = -1;
  int dungeon = -1;
  bool indoor = true;
};

struct WallPrivateSheetObjectExpectation {
  int id = 0;
  int x = 0;
  int y = 0;
  int size = 0;  // subtype for type-2 objects
  std::string asset_file;
  std::string asset_sha256;
};

struct WallPrivateSheetBinding {
  std::string kit_id;
  std::string profile_id;
  std::string sheet_path;
  std::string kit_path;
  std::string decoded_sha256;
  uint8_t bg_request = kWallPrivateBgRequest;
  uint8_t sprite_relative = kWallPrivateSpriteRelative;
  uint8_t slot = kWallPrivateSlot;
  uint8_t required_profile = kWallPrivateArmoryProfile;
  uint8_t required_blockset = kWallPrivateArmoryBlockset;
  uint16_t room = kWallPrivateArmoryRoom;
  int dungeon = kWallPrivateArmoryDungeon;
  std::vector<uint8_t> decoded_3bpp;
  std::vector<uint8_t> decoded_8bpp;
  std::vector<WallPrivateSheetObjectExpectation> objects;
};

struct WallPrivateSheetResolution {
  bool applied = false;
  std::string reason;
  std::map<uint16_t, std::vector<uint8_t>> sheet_overrides_8bpp;
  std::string decoded_sha256;
  uint8_t bg_sheet = kWallPrivateBgRequest;
  uint8_t sprite_sheet = kWallPrivateSpriteAbsolute;
};

struct WallPrivateSheetRecipeReport {
  bool accepted = false;
  std::string reason;
  std::string plan_sha256;
  int edit_count = 0;
  std::vector<std::string> matched_assets;
};

// Loads prison_armory.kit.json (private_pointer sheet) plus the exact 0x600
// decoded 3bpp bytes. Hashes must match the kit's decoded_sha256.
absl::StatusOr<WallPrivateSheetBinding> LoadWallPrivateSheetBinding(
    const std::filesystem::path& kit_json,
    const std::filesystem::path& sheet_3bpp);

// Optional object asset pins from BINDING_CONTRACT*.json bindings[].
absl::Status AttachWallPrivateSheetObjectBindings(
    WallPrivateSheetBinding* binding, const nlohmann::json& contract);

// Positive: room/profile/blockset/dungeon/indoor match binding.
// Negative: any mismatch refuses with applied=false (not Ok status) so callers
// can report control cases without treating refusal as a crash.
absl::StatusOr<WallPrivateSheetResolution> ResolveWallPrivateSheet(
    const WallPrivateSheetBinding& binding,
    const WallPrivateSheetContext& context);

// Dry-run recipe check for OBJECT_PLAN_54.json against the binding's expected
// furniture placements and asset pins. Never writes ROM/project.
absl::StatusOr<WallPrivateSheetRecipeReport> DryRunWallPrivateSheetRecipe(
    const WallPrivateSheetBinding& binding, const nlohmann::json& object_plan);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_WALL_PRIVATE_SHEET_H_
