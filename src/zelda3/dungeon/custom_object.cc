#include "zelda3/dungeon/custom_object.h"

#include <fstream>
#include <filesystem>
#include <iostream>

#include "util/log.h" 

namespace yaze {
namespace zelda3 {

const std::vector<std::string> CustomObjectManager::kSubtype1Filenames = {
    "track_LR.bin",                // 00
    "track_UD.bin",                // 01
    "track_corner_TL.bin",         // 02
    "track_corner_TR.bin",         // 03
    "track_corner_BL.bin",         // 04
    "track_corner_BR.bin",         // 05
    "track_floor_UD.bin",          // 06
    "track_floor_LR.bin",          // 07
    "track_floor_corner_TL.bin",   // 08
    "track_floor_corner_TR.bin",   // 09
    "track_floor_corner_BL.bin",   // 10
    "track_floor_corner_BR.bin",   // 11
    "track_floor_any.bin",         // 12
    "wall_sword_house.bin",        // 13
    "track_any.bin",               // 14
    "small_statue.bin",            // 15
};

const std::vector<std::string> CustomObjectManager::kSubtype2Filenames = {
    "furnace.bin",    // 00
    "firewood.bin",   // 01
    "ice_chair.bin",  // 02
};

CustomObjectManager& CustomObjectManager::Get() {
  static CustomObjectManager instance;
  return instance;
}

void CustomObjectManager::Initialize(const std::string& custom_objects_folder) {
  base_path_ = custom_objects_folder;
  cache_.clear();
}

void CustomObjectManager::SetObjectFileMap(
    const std::unordered_map<int, std::vector<std::string>>& map) {
  custom_file_map_ = map;
  cache_.clear();
}

void CustomObjectManager::ClearObjectFileMap() {
  custom_file_map_.clear();
  cache_.clear();
}

const std::vector<std::string>* CustomObjectManager::ResolveFileList(
    int object_id) const {
  auto custom_it = custom_file_map_.find(object_id);
  if (custom_it != custom_file_map_.end()) {
    return &custom_it->second;
  }
  if (object_id == 0x31) {
    return &kSubtype1Filenames;
  }
  if (object_id == 0x32) {
    return &kSubtype2Filenames;
  }
  return nullptr;
}

absl::StatusOr<std::shared_ptr<CustomObject>> CustomObjectManager::LoadObject(
    const std::string& filename) {
  if (cache_.contains(filename)) {
    return cache_[filename];
  }

  // base_path_ should be the full path to the custom objects folder (e.g., Dungeons/Objects/Data)
  std::filesystem::path full_path = std::filesystem::path(base_path_) / filename;
  
  std::ifstream file(full_path, std::ios::binary);
  if (!file) {
    LOG_ERROR("CustomObjectManager", "Failed to open file: %s", full_path.c_str());
    return absl::NotFoundError("Could not open file: " + full_path.string());
  }

  // Read entire file into buffer
  std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

  auto object_or_error = ParseBinaryData(buffer);
  if (!object_or_error.ok()) {
    return object_or_error.status();
  }

  auto object_ptr = std::make_shared<CustomObject>(std::move(object_or_error.value()));
  cache_[filename] = object_ptr;
  
  return object_ptr;
}

absl::StatusOr<CustomObject> CustomObjectManager::ParseBinaryData(const std::vector<uint8_t>& data) {
  CustomObject obj;
  size_t cursor = 0;
  int current_buffer_pos = 0;

  // Safety check for empty data
  if (data.empty()) {
    return obj;
  }

  // Dungeon room tilemap buffer stride = 128 bytes (64 tiles per row, 2 bytes per tile).
  // This matches the SNES dungeon room buffer layout where rooms can be up to 64 tiles wide.
  // The jump offset of 0x80 (128) advances by exactly 1 row.
  constexpr int kBufferStride = 128;

  while (cursor + 1 < data.size()) {
    // Read Header (little endian)
    uint16_t header = data[cursor] | (data[cursor + 1] << 8);
    cursor += 2;

    if (header == 0) break;

    int count = header & 0x001F;
    int jump_offset = (header >> 8) & 0xFF;

    // ASM behavior: PHY saves the row start position, tiles are drawn at
    // incrementing positions, then PLA restores the original position and
    // the jump offset is added to that original position (not the post-tile position).
    int row_start_pos = current_buffer_pos;

    // Line Loop
    for (int i = 0; i < count; ++i) {
      if (cursor + 1 >= data.size()) {
        LOG_WARN("CustomObjectManager", "Unexpected end of file parsing object");
        break;
      }

      uint16_t tile_data = data[cursor] | (data[cursor + 1] << 8);
      cursor += 2;

      // Calculate relative X/Y from current buffer position
      // Buffer stride = 128 bytes (64 tiles per row)
      int rel_y = current_buffer_pos / kBufferStride; 
      int rel_x = (current_buffer_pos % kBufferStride) / 2; // 2 bytes per tile

      obj.tiles.push_back({rel_x, rel_y, tile_data});

      current_buffer_pos += 2; // Advance 1 tile in buffer
    }

    // Advance buffer position for next segment from the ROW START, not current position.
    // This matches the ASM: PLA (restore original Y) then ADC jump_offset.
    current_buffer_pos = row_start_pos + jump_offset;
  }

  return obj;
}

absl::StatusOr<std::shared_ptr<CustomObject>> CustomObjectManager::GetObjectInternal(int object_id, int subtype) {
  const std::vector<std::string>* list = ResolveFileList(object_id);
  int index = subtype;

  if (!list && object_id >= 0x100 && object_id <= 0x103) {
    // Minecart Track Override for standard corners
    // 0x100 = TL -> index 2 (track_corner_TL.bin)
    // 0x101 = BL -> index 4 (track_corner_BL.bin)
    // 0x102 = TR -> index 3 (track_corner_TR.bin)
    // 0x103 = BR -> index 5 (track_corner_BR.bin)
    switch (object_id) {
      case 0x100: index = 2; break;
      case 0x101: index = 4; break;
      case 0x102: index = 3; break;
      case 0x103: index = 5; break;
    }
    list = ResolveFileList(0x31);
  }

  if (!list) {
    return absl::NotFoundError("Object ID not mapped to custom object");
  }

  if (index < 0 || index >= static_cast<int>(list->size())) {
    return absl::OutOfRangeError("Subtype index out of range");
  }

  return LoadObject((*list)[index]);
}

int CustomObjectManager::GetSubtypeCount(int object_id) const {
  if (const auto* list = ResolveFileList(object_id)) {
    return static_cast<int>(list->size());
  }
  if (object_id >= 0x100 && object_id <= 0x103) {
    if (const auto* list = ResolveFileList(0x31)) {
      return static_cast<int>(list->size());
    }
  }
  return 0;
}

void CustomObjectManager::AddObjectFile(int object_id,
                                        const std::string& filename) {
  // If no custom_file_map_ entry exists, seed from static defaults
  if (custom_file_map_.find(object_id) == custom_file_map_.end()) {
    const auto* defaults = (object_id == 0x31) ? &kSubtype1Filenames
                           : (object_id == 0x32) ? &kSubtype2Filenames
                                                 : nullptr;
    if (defaults) {
      custom_file_map_[object_id] = *defaults;
    }
  }
  custom_file_map_[object_id].push_back(filename);

  // Clear cache for the new file so it loads fresh
  cache_.erase(filename);
}

std::vector<std::string> CustomObjectManager::GetEffectiveFileList(
    int object_id) const {
  const auto* list = ResolveFileList(object_id);
  if (list) return *list;
  return {};
}

void CustomObjectManager::ReloadAll() {
  cache_.clear();
}

std::string CustomObjectManager::ResolveFilename(int object_id,
                                                  int subtype) const {
  const auto* list = ResolveFileList(object_id);
  if (!list && object_id >= 0x100 && object_id <= 0x103) {
    list = ResolveFileList(0x31);
  }
  if (list && subtype >= 0 && subtype < static_cast<int>(list->size())) {
    return (*list)[subtype];
  }
  return "";
}

} // namespace zelda3
} // namespace yaze
