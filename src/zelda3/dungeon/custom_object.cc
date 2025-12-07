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
  LOG_INFO("CustomObjectManager", "Initialized with custom objects folder: %s", base_path_.c_str());
}

absl::StatusOr<std::shared_ptr<CustomObject>> CustomObjectManager::LoadObject(const std::string& filename) {
  if (cache_.contains(filename)) {
    return cache_[filename];
  }

  // base_path_ should be the full path to the custom objects folder (e.g., Dungeons/Objects/Data)
  std::filesystem::path full_path = std::filesystem::path(base_path_) / filename;
  
  std::ifstream file(full_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return absl::NotFoundError("Could not open file: " + full_path.string());
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    return absl::InternalError("Failed to read file: " + full_path.string());
  }

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
  size_t offset = 0;
  
  while (offset + 1 < data.size()) {
    // Read Header (Little Endian)
    uint16_t header = data[offset] | (data[offset + 1] << 8);
    offset += 2;

    if (header == 0) break; // End of list

    uint8_t count = header & 0x1F;
    uint8_t stride = (header >> 8) & 0xFF; // Usually 0x80

    CustomObject::TileRow row;
    row.count = count;
    row.stride = stride;

    // Read `count` tiles
    for (int i = 0; i < count; ++i) {
      if (offset + 1 >= data.size()) {
        return absl::OutOfRangeError("Unexpected end of file while reading tiles");
      }

      uint16_t word = data[offset] | (data[offset + 1] << 8);
      offset += 2;

      // Decode tile attributes: vhopppcc cccccccc
      // c: tile ID (10 bits)
      // p: palette (3 bits)
      // o: priority (1 bit)
      // h: h-flip (1 bit)
      // v: v-flip (1 bit)
      
      gfx::TileInfo tile;
      tile.id_ = word & 0x01FF; // 9-bit tile ID? Or 10-bit? snes_tile.h says 10-bit ID is bits 0-9.
      // Standard SNES format: vhopppcc cccccccc
      // c = tile ID (10 bits)
      // p = palette (3 bits)
      // o = priority (1 bit)
      // h = h flip (1 bit)
      // v = v flip (1 bit)
      
      // But here `word` is raw tile info.
      // Re-reading snes_tile.h TileInfo constructor:
      // id_ = (uint16_t)(((b2 & 0x03) << 8) | b1);
      // So if 'word' is the 16-bit value:
      tile.id_ = word & 0x03FF;
      tile.palette_ = (word >> 10) & 0x07;
      tile.over_ = (word & 0x2000) != 0;
      tile.horizontal_mirror_ = (word & 0x4000) != 0;
      tile.vertical_mirror_ = (word & 0x8000) != 0;
      
      row.tiles.push_back(tile);
    }
    obj.rows.push_back(row);
  }

  // Calculate generic bounding box
  // This is a rough estimation assuming Stride 0x80 = 1 line down (16 pixels? 8 pixels?)
  // Stride 0x80 bytes = 128 bytes. 
  // SNES tile 4bpp = 32 bytes provided we are drawing to VRAM?
  // No, `CustomObjectHandler` writes to RAM tilemap buffer which is usually 2 bytes per tile.
  // So 0x80 bytes = 64 tiles.
  // If tilemap width is 64 tiles (64 * 8 = 512 pixels), then 0x80 is one full row of tiles.
  // So Stride 0x80 = y + 8 pixels.
  // Actually, let's assume standard VRAM tilemap buffer layout.
  
  if (!obj.rows.empty()) {
    obj.width = 16; // Minimum
    obj.height = obj.rows.size() * 8; // Maybe?
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
