#ifndef YAZE_ZELDA3_DUNGEON_ROOM_CENSUS_VANILLA_FINGERPRINTS_H_
#define YAZE_ZELDA3_DUNGEON_ROOM_CENSUS_VANILLA_FINGERPRINTS_H_

#include <cstdint>
#include <string>
#include <vector>

namespace yaze::zelda3 {

// Built-in fingerprints of the vanilla US 1.0 rooms: per room, the sorted
// 16-bit HashRoomCensusObject() values of its room-stream objects. The hashes
// are one-way; they let the census tell vanilla leftovers from authored rooms
// without a vanilla ROM and without shipping room data.
//
// Regenerate with:
//   z3ed dungeon-room-census --rom <US 1.0 ROM> \
//     --emit-vanilla-fingerprints src/zelda3/dungeon/room_census_vanilla_fingerprints_data.inc
bool HasBuiltinVanillaRoomFingerprints();
std::vector<uint16_t> BuiltinVanillaRoomFingerprint(int room_id);

// Source text for room_census_vanilla_fingerprints_data.inc.
std::string FormatVanillaFingerprintTable(
    const std::vector<std::vector<uint16_t>>& fingerprints,
    const std::string& source_note);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_CENSUS_VANILLA_FINGERPRINTS_H_
