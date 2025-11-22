#ifndef YAZE_APP_ZELDA3_DUNGEON_OBJECT_REGISTRY_H
#define YAZE_APP_ZELDA3_DUNGEON_OBJECT_REGISTRY_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace yaze {
namespace zelda3 {

struct DungeonObjectInfo {
  int16_t id = 0;
  std::string name;
  bool is_custom = false;
};

/**
 * @brief Minimal registry for dungeon objects (vanilla or custom).
 *
 * This powers previews and can be extended to load custom object definitions
 * from disassembly artifacts (e.g., assets/asm/usdasm outputs).
 */
class DungeonObjectRegistry {
 public:
  void RegisterObject(const DungeonObjectInfo& info);
  void RegisterVanillaRange(int16_t start_id, int16_t end_id);
  void RegisterCustomObject(int16_t id, const std::string& name);

  const DungeonObjectInfo* Get(int16_t id) const;

 private:
  std::unordered_map<int16_t, DungeonObjectInfo> registry_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_OBJECT_REGISTRY_H
