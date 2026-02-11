#include "dungeon_validator.h"

#include "absl/strings/str_format.h"
#include "zelda3/dungeon/object_layer_semantics.h"

namespace yaze {
namespace zelda3 {

ValidationResult DungeonValidator::ValidateRoom(const Room& room) {
  ValidationResult result;

  // Check object count
  size_t object_count = room.GetTileObjects().size();
  if (object_count > kMaxObjects) {
    result.warnings.push_back(absl::StrFormat(
        "High object count (%zu > %d). May cause lag or memory issues.",
        object_count, kMaxObjects));
  }

  // Check sprite count
  size_t sprite_count = room.GetSprites().size();
  if (sprite_count > kMaxTotalSprites) {
    result.warnings.push_back(
        absl::StrFormat("Too many sprites (%zu > %d). Game limit is strict.",
                        sprite_count, kMaxTotalSprites));
  }

  // Check chest count (approximate, based on object ID)
  int chest_count = 0;
  for (const auto& obj : room.GetTileObjects()) {
    // Check for Big Chest (0xE4?? No, let's trust standard ranges)
    // ZScream logic for chests:
    // 0xF9 = Small Key Chest
    // 0xFA = Big Key Chest
    // 0xFB = Map Chest
    // 0xFC = Compass Chest
    // 0xFD = Big Chest
    // But simple chest objects are also common.
    // Let's count objects in the 0xF9-0xFD range as chests for now.
    if (obj.id_ >= 0xF9 && obj.id_ <= 0xFD) {
      chest_count++;
    }
  }

  if (chest_count > kMaxChests) {
    result.errors.push_back(absl::StrFormat(
        "Too many chests (%d > %d). Item collection flags will conflict.",
        chest_count, kMaxChests));
    result.is_valid = false;
  }

  // Check bounds
  int bg3_count = 0;
  for (const auto& obj : room.GetTileObjects()) {
    const int layer = static_cast<int>(obj.GetLayerValue());
    if (layer < 0 || layer > 2) {
      result.errors.push_back(absl::StrFormat(
          "Object 0x%02X has invalid layer %d", obj.id_, layer));
      result.is_valid = false;
    } else if (layer == 2) {
      ++bg3_count;
    }

    const auto semantics = GetObjectLayerSemantics(obj);
    if (layer == 2 && semantics.draws_to_both_bgs) {
      result.errors.push_back(absl::StrFormat(
          "Object 0x%02X in BG3 is marked as Both-BGs (unsafe layer state)",
          obj.id_));
      result.is_valid = false;
    }

    if (obj.x_ < 0 || obj.x_ >= 64 || obj.y_ < 0 || obj.y_ >= 64) {
      result.errors.push_back(absl::StrFormat(
          "Object 0x%02X out of bounds at (%d, %d)", obj.id_, obj.x_, obj.y_));
      result.is_valid = false;
    }
  }

  if (bg3_count > kMaxBg3Objects) {
    result.errors.push_back(absl::StrFormat(
        "Too many BG3 objects (%d > %d). Reduce background-layer object count.",
        bg3_count, kMaxBg3Objects));
    result.is_valid = false;
  }

  return result;
}

}  // namespace zelda3
}  // namespace yaze
