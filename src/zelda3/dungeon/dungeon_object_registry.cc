#include "zelda3/dungeon/dungeon_object_registry.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace zelda3 {

void DungeonObjectRegistry::RegisterObject(const DungeonObjectInfo& info) {
  registry_[info.id] = info;
}

void DungeonObjectRegistry::RegisterVanillaRange(int16_t start_id,
                                                 int16_t end_id) {
  for (int16_t id = start_id; id <= end_id; ++id) {
    if (registry_.find(id) == registry_.end()) {
      registry_[id] =
          DungeonObjectInfo{.id = id,
                            .name = absl::StrFormat("Object 0x%03X", id),
                            .is_custom = false};
    }
  }
}

void DungeonObjectRegistry::RegisterCustomObject(int16_t id,
                                                 const std::string& name) {
  registry_[id] = DungeonObjectInfo{.id = id, .name = name, .is_custom = true};
}

const DungeonObjectInfo* DungeonObjectRegistry::Get(int16_t id) const {
  auto it = registry_.find(id);
  if (it == registry_.end()) {
    return nullptr;
  }
  return &it->second;
}

}  // namespace zelda3
}  // namespace yaze
