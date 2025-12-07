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
  int current_buffer_pos = 0; // Relative byte offset in tilemap buffer (stride 64 = 32 tiles)

  // Safety check for empty data
  if (data.empty()) {
    return obj;
  }

  while (cursor + 1 < data.size()) {
    // Read Header (little endian)
    uint16_t header = data[cursor] | (data[cursor + 1] << 8);
    cursor += 2;

    if (header == 0) break;

    int count = header & 0x001F;
    int jump_offset = (header >> 8) & 0xFF;

    // Line Loop
    for (int i = 0; i < count; ++i) {
      if (cursor + 1 >= data.size()) {
        LOG_WARN("CustomObjectManager", "Unexpected end of file parsing object");
        break;
      }

      uint16_t tile_data = data[cursor] | (data[cursor + 1] << 8);
      cursor += 2;

      // Calculate relative X/Y from current buffer position
      // Standard Screen Buffer Stride = 64 bytes (32 tiles)
      int rel_y = current_buffer_pos / 64; 
      int rel_x = (current_buffer_pos % 64) / 2; // 2 bytes per tile

      obj.tiles.push_back({rel_x, rel_y, tile_data});

      current_buffer_pos += 2; // Advance 1 tile in buffer
    }

    // Advance buffer position for next segment
    current_buffer_pos += jump_offset;
  }

  return obj;
}

absl::StatusOr<std::shared_ptr<CustomObject>> CustomObjectManager::GetObjectInternal(int object_id, int subtype) {
  const std::vector<std::string>* list = nullptr;
  if (object_id == 0x31) {
    list = &kSubtype1Filenames;
  } else if (object_id == 0x32) {
    list = &kSubtype2Filenames;
  } else {
    return absl::InvalidArgumentError("Invalid object ID for CustomObjectManager");
  }

  if (subtype < 0 || subtype >= static_cast<int>(list->size())) {
    return absl::OutOfRangeError("Subtype index out of range");
  }

  return LoadObject((*list)[subtype]);
}

int CustomObjectManager::GetSubtypeCount(int object_id) const {
  if (object_id == 0x31) return kSubtype1Filenames.size();
  if (object_id == 0x32) return kSubtype2Filenames.size();
  return 0;
}

void CustomObjectManager::ReloadAll() {
  cache_.clear();
}

} // namespace zelda3
} // namespace yaze
