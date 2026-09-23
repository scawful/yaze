#ifndef YAZE_ZELDA3_DUNGEON_MINECART_OBJECT_SEMANTICS_H_
#define YAZE_ZELDA3_DUNGEON_MINECART_OBJECT_SEMANTICS_H_

namespace yaze::zelda3 {

// Oracle overloads canonical dungeon object 0x31's size nibble as a custom
// graphics subtype. Slots 13 and 15 are decorations, not minecart rails.
inline bool IsMinecartTrackGraphicsSubtype(int object_id, int subtype) {
  if (object_id != 0x31) {
    return true;
  }
  return subtype >= 0 && (subtype <= 12 || subtype == 14);
}

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_MINECART_OBJECT_SEMANTICS_H_
