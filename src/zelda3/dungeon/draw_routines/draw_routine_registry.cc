#include "draw_routine_registry.h"

#include "core/features.h"
#include "zelda3/dungeon/draw_routines/corner_routines.h"
#include "zelda3/dungeon/draw_routines/diagonal_routines.h"
#include "zelda3/dungeon/draw_routines/downwards_routines.h"
#include "zelda3/dungeon/draw_routines/rightwards_routines.h"
#include "zelda3/dungeon/draw_routines/special_routines.h"

namespace yaze {
namespace zelda3 {

DrawRoutineRegistry& DrawRoutineRegistry::Get() {
  static DrawRoutineRegistry instance;
  if (!instance.initialized_) {
    instance.Initialize();
  }
  return instance;
}

void DrawRoutineRegistry::Initialize() {
  if (initialized_) return;
  BuildRegistry();
  initialized_ = true;
}

void DrawRoutineRegistry::BuildRegistry() {
  routines_.clear();
  routine_map_.clear();

  // Register routines from all modules
  draw_routines::RegisterRightwardsRoutines(routines_);
  draw_routines::RegisterDownwardsRoutines(routines_);
  draw_routines::RegisterDiagonalRoutines(routines_);
  draw_routines::RegisterCornerRoutines(routines_);
  draw_routines::RegisterSpecialRoutines(routines_);

  // Build lookup map
  for (auto& info : routines_) {
    routine_map_[info.id] = &info;
  }

  BuildObjectMapping();
}

const DrawRoutineInfo* DrawRoutineRegistry::GetRoutineInfo(int routine_id) const {
  auto it = routine_map_.find(routine_id);
  if (it == routine_map_.end()) {
    return nullptr;
  }
  return it->second;
}

bool DrawRoutineRegistry::RoutineDrawsToBothBGs(int routine_id) const {
  const DrawRoutineInfo* info = GetRoutineInfo(routine_id);
  return info != nullptr && info->draws_to_both_bgs;
}

bool DrawRoutineRegistry::GetRoutineDimensions(int routine_id, 
                                                int* base_width, 
                                                int* base_height) const {
  const DrawRoutineInfo* info = GetRoutineInfo(routine_id);
  if (info == nullptr) {
    return false;
  }
  if (base_width) *base_width = info->base_width;
  if (base_height) *base_height = info->base_height;
  return true;
}

int DrawRoutineRegistry::GetRoutineIdForObject(int16_t object_id) const {
  auto it = object_to_routine_map_.find(object_id);
  if (it != object_to_routine_map_.end()) {
    return it->second;
  }
  return -1;
}

void DrawRoutineRegistry::BuildObjectMapping() {
  object_to_routine_map_.clear();

  // Subtype 1 Object Mappings (0x00-0xFF)
  // Based on bank_01.asm routine table at $018200
  object_to_routine_map_[0x00] = 0;
  for (int id = 0x01; id <= 0x02; id++) {
    object_to_routine_map_[id] = 1;
  }
  for (int id = 0x03; id <= 0x04; id++) {
    object_to_routine_map_[id] = 2;
  }
  for (int id = 0x05; id <= 0x06; id++) {
    object_to_routine_map_[id] = 3;
  }
  for (int id = 0x07; id <= 0x08; id++) {
    object_to_routine_map_[id] = 4;
  }
  object_to_routine_map_[0x09] = 5;
  for (int id = 0x0A; id <= 0x0B; id++) {
    object_to_routine_map_[id] = 6;
  }

  // Diagonal walls (0x0C-0x20)
  for (int id : {0x0C, 0x0D, 0x10, 0x11, 0x14}) {
    object_to_routine_map_[id] = 5;
  }
  for (int id : {0x0E, 0x0F, 0x12, 0x13}) {
    object_to_routine_map_[id] = 6;
  }
  for (int id : {0x15, 0x18, 0x19, 0x1C, 0x1D, 0x20}) {
    object_to_routine_map_[id] = 17;
  }
  for (int id : {0x16, 0x17, 0x1A, 0x1B, 0x1E, 0x1F}) {
    object_to_routine_map_[id] = 18;
  }

  // Edge and Corner Objects (0x21-0x30)
  object_to_routine_map_[0x21] = 20;
  object_to_routine_map_[0x22] = 21;
  for (int id = 0x23; id <= 0x2E; id++) {
    object_to_routine_map_[id] = 22;
  }
  object_to_routine_map_[0x2F] = 23;
  object_to_routine_map_[0x30] = 24;

  // Custom Objects (0x31-0x32)
  if (core::FeatureFlags::get().kEnableCustomObjects) {
    object_to_routine_map_[0x31] = DrawRoutineIds::kCustomObject;
    object_to_routine_map_[0x32] = DrawRoutineIds::kCustomObject;
  } else {
    object_to_routine_map_[0x31] = DrawRoutineIds::kNothing;
    object_to_routine_map_[0x32] = DrawRoutineIds::kNothing;
  }
  object_to_routine_map_[0x33] = 16;
  object_to_routine_map_[0x34] = 25;
  object_to_routine_map_[0x35] = 26;
  object_to_routine_map_[0x36] = 27;
  object_to_routine_map_[0x37] = 27;
  object_to_routine_map_[0x38] = 28;
  object_to_routine_map_[0x39] = 29;
  object_to_routine_map_[0x3A] = 30;
  object_to_routine_map_[0x3B] = 30;
  object_to_routine_map_[0x3C] = 31;
  object_to_routine_map_[0x3D] = 29;
  object_to_routine_map_[0x3E] = 32;

  for (int id = 0x3F; id <= 0x46; id++) {
    object_to_routine_map_[id] = 22;
  }
  object_to_routine_map_[0x47] = 111;
  object_to_routine_map_[0x48] = 112;
  object_to_routine_map_[0x49] = 40;
  object_to_routine_map_[0x4A] = 40;
  object_to_routine_map_[0x4B] = 32;
  object_to_routine_map_[0x4C] = 52;
  object_to_routine_map_[0x4D] = 53;
  object_to_routine_map_[0x4E] = 53;
  object_to_routine_map_[0x4F] = 53;

  object_to_routine_map_[0x50] = 51;
  object_to_routine_map_[0x51] = 42;
  object_to_routine_map_[0x52] = 42;
  object_to_routine_map_[0x53] = 4;
  object_to_routine_map_[0x54] = 38;
  object_to_routine_map_[0x55] = 41;
  object_to_routine_map_[0x56] = 41;
  object_to_routine_map_[0x57] = 38;
  object_to_routine_map_[0x58] = 38;
  object_to_routine_map_[0x59] = 38;
  object_to_routine_map_[0x5A] = 38;
  object_to_routine_map_[0x5B] = 42;
  object_to_routine_map_[0x5C] = 42;
  object_to_routine_map_[0x5D] = 54;
  object_to_routine_map_[0x5E] = 55;
  object_to_routine_map_[0x5F] = DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus23;

  // Vertical (0x60-0x6F)
  object_to_routine_map_[0x60] = 7;
  for (int id = 0x61; id <= 0x62; id++) {
    object_to_routine_map_[id] = 8;
  }
  for (int id = 0x63; id <= 0x64; id++) {
    object_to_routine_map_[id] = 9;
  }
  for (int id = 0x65; id <= 0x66; id++) {
    object_to_routine_map_[id] = 10;
  }
  for (int id = 0x67; id <= 0x68; id++) {
    object_to_routine_map_[id] = 11;
  }
  object_to_routine_map_[0x69] = 12;
  for (int id = 0x6A; id <= 0x6B; id++) {
    object_to_routine_map_[id] = 13;
  }
  object_to_routine_map_[0x6C] = 14;
  object_to_routine_map_[0x6D] = 15;
  object_to_routine_map_[0x6E] = 38;
  object_to_routine_map_[0x6F] = 38;

  // 0x70-0x7F
  object_to_routine_map_[0x70] = 43;
  object_to_routine_map_[0x71] = 44;
  object_to_routine_map_[0x72] = 38;
  object_to_routine_map_[0x73] = 45;
  object_to_routine_map_[0x74] = 45;
  object_to_routine_map_[0x75] = 46;
  object_to_routine_map_[0x76] = 47;
  object_to_routine_map_[0x77] = 47;
  object_to_routine_map_[0x78] = 48;
  object_to_routine_map_[0x79] = 13;
  object_to_routine_map_[0x7A] = 13;
  object_to_routine_map_[0x7B] = 48;
  object_to_routine_map_[0x7C] = 49;
  object_to_routine_map_[0x7D] = 11;
  object_to_routine_map_[0x7E] = 38;
  object_to_routine_map_[0x7F] = 50;

  // 0x80-0x8F
  object_to_routine_map_[0x80] = 50;
  object_to_routine_map_[0x81] = 65;
  object_to_routine_map_[0x82] = 65;
  object_to_routine_map_[0x83] = 65;
  object_to_routine_map_[0x84] = 65;
  object_to_routine_map_[0x85] = 68;
  object_to_routine_map_[0x86] = 68;
  object_to_routine_map_[0x87] = 46;
  object_to_routine_map_[0x88] = 66;
  object_to_routine_map_[0x89] = 67;
  object_to_routine_map_[0x8A] = DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23;
  object_to_routine_map_[0x8B] = DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23;
  object_to_routine_map_[0x8C] = DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23;
  object_to_routine_map_[0x8D] = 13;
  object_to_routine_map_[0x8E] = 13;
  object_to_routine_map_[0x8F] = 69;

  // 0x90-0x9F
  object_to_routine_map_[0x90] = 8;
  object_to_routine_map_[0x91] = 8;
  object_to_routine_map_[0x92] = 7;
  object_to_routine_map_[0x93] = 7;
  object_to_routine_map_[0x94] = 43;
  object_to_routine_map_[0x95] = 70;
  object_to_routine_map_[0x96] = 71;
  for (int id = 0x97; id <= 0x9F; id++) {
    object_to_routine_map_[id] = 38;
  }

  // 0xA0-0xAF (diagonal ceilings and big hole)
  object_to_routine_map_[0xA0] = 75;
  object_to_routine_map_[0xA5] = 75;
  object_to_routine_map_[0xA9] = 75;
  object_to_routine_map_[0xA1] = 76;
  object_to_routine_map_[0xA6] = 76;
  object_to_routine_map_[0xAA] = 76;
  object_to_routine_map_[0xA2] = 77;
  object_to_routine_map_[0xA7] = 77;
  object_to_routine_map_[0xAB] = 77;
  object_to_routine_map_[0xA3] = 78;
  object_to_routine_map_[0xA8] = 78;
  object_to_routine_map_[0xAC] = 78;
  object_to_routine_map_[0xA4] = 61;
  object_to_routine_map_[0xAD] = 38;
  object_to_routine_map_[0xAE] = 38;
  object_to_routine_map_[0xAF] = 38;

  // 0xB0-0xBF
  object_to_routine_map_[0xB0] = 72;
  object_to_routine_map_[0xB1] = 72;
  object_to_routine_map_[0xB2] = 16;
  object_to_routine_map_[0xB3] = 22;
  object_to_routine_map_[0xB4] = 22;
  object_to_routine_map_[0xB5] = 8;
  object_to_routine_map_[0xB6] = 1;
  object_to_routine_map_[0xB7] = 1;
  object_to_routine_map_[0xB8] = 0;
  object_to_routine_map_[0xB9] = 0;
  object_to_routine_map_[0xBA] = 16;
  object_to_routine_map_[0xBB] = 55;
  object_to_routine_map_[0xBC] = 73;
  object_to_routine_map_[0xBD] = 74;
  object_to_routine_map_[0xBE] = 38;
  object_to_routine_map_[0xBF] = 38;

  // 0xC0-0xCF (SuperSquare)
  object_to_routine_map_[0xC0] = 56;
  object_to_routine_map_[0xC1] = 79;
  object_to_routine_map_[0xC2] = 56;
  object_to_routine_map_[0xC3] = 57;
  object_to_routine_map_[0xC4] = 59;
  for (int id = 0xC5; id <= 0xCA; id++) {
    object_to_routine_map_[id] = 58;
  }
  object_to_routine_map_[0xCB] = 38;
  object_to_routine_map_[0xCC] = 38;
  object_to_routine_map_[0xCD] = 80;
  object_to_routine_map_[0xCE] = 81;
  object_to_routine_map_[0xCF] = 38;

  // 0xD0-0xDF
  object_to_routine_map_[0xD0] = 38;
  object_to_routine_map_[0xD1] = 58;
  object_to_routine_map_[0xD2] = 58;
  object_to_routine_map_[0xD3] = 38;
  object_to_routine_map_[0xD4] = 38;
  object_to_routine_map_[0xD5] = 38;
  object_to_routine_map_[0xD6] = 38;
  object_to_routine_map_[0xD7] = 57;
  object_to_routine_map_[0xD8] = 64;
  object_to_routine_map_[0xD9] = 58;
  object_to_routine_map_[0xDA] = 64;
  object_to_routine_map_[0xDB] = 60;
  object_to_routine_map_[0xDC] = 82;
  object_to_routine_map_[0xDD] = 63;
  object_to_routine_map_[0xDE] = 62;
  object_to_routine_map_[0xDF] = 58;

  // 0xE0-0xEF
  for (int id = 0xE0; id <= 0xE8; id++) {
    object_to_routine_map_[id] = 58;
  }
  for (int id = 0xE9; id <= 0xEF; id++) {
    object_to_routine_map_[id] = 38;
  }

  // 0xF0-0xFD
  for (int id = 0xF0; id <= 0xF7; id++) {
    object_to_routine_map_[id] = 38;
  }
  for (int id = 0xF9; id <= 0xFD; id++) {
    object_to_routine_map_[id] = 39;
  }

  // Subtype 2 Object Mappings (0x100-0x13F)
  for (int id = 0x100; id <= 0x107; id++) {
    object_to_routine_map_[id] = 16;
  }
  for (int id = 0x108; id <= 0x10F; id++) {
    object_to_routine_map_[id] = 35;
  }
  for (int id = 0x110; id <= 0x113; id++) {
    object_to_routine_map_[id] = 36;
  }
  for (int id = 0x114; id <= 0x117; id++) {
    object_to_routine_map_[id] = 37;
  }
  for (int id = 0x118; id <= 0x11B; id++) {
    object_to_routine_map_[id] = 4;
  }
  object_to_routine_map_[0x11C] = 16;
  object_to_routine_map_[0x11D] = 28;
  object_to_routine_map_[0x11E] = 4;
  object_to_routine_map_[0x11F] = 25;
  object_to_routine_map_[0x120] = 25;
  object_to_routine_map_[0x121] = 28;
  object_to_routine_map_[0x122] = 98;
  object_to_routine_map_[0x123] = 30;
  object_to_routine_map_[0x124] = 16;
  object_to_routine_map_[0x125] = 16;
  object_to_routine_map_[0x126] = 28;
  object_to_routine_map_[0x127] = 4;
  object_to_routine_map_[0x128] = 98;
  object_to_routine_map_[0x129] = 16;
  object_to_routine_map_[0x12A] = 4;
  object_to_routine_map_[0x12B] = 4;
  object_to_routine_map_[0x12C] = 99;
  object_to_routine_map_[0x12D] = 83;
  object_to_routine_map_[0x12E] = 84;
  object_to_routine_map_[0x12F] = 85;
  for (int id = 0x130; id <= 0x133; id++) {
    object_to_routine_map_[id] = 86;
  }
  object_to_routine_map_[0x134] = 4;
  object_to_routine_map_[0x135] = 16;
  object_to_routine_map_[0x136] = 16;
  object_to_routine_map_[0x137] = 16;
  object_to_routine_map_[0x138] = 88;
  object_to_routine_map_[0x139] = 89;
  object_to_routine_map_[0x13A] = 90;
  object_to_routine_map_[0x13B] = 91;
  object_to_routine_map_[0x13C] = 16;
  object_to_routine_map_[0x13D] = 30;
  object_to_routine_map_[0x13E] = 100;
  object_to_routine_map_[0x13F] = 16;

  // Subtype 3 Object Mappings (0xF80-0xFFF)
  object_to_routine_map_[0xF80] = 94;
  object_to_routine_map_[0xF81] = 95;
  object_to_routine_map_[0xF82] = 96;
  for (int id = 0xF83; id <= 0xF89; id++) {
    object_to_routine_map_[id] = 33;
  }
  for (int id = 0xF8A; id <= 0xF8C; id++) {
    object_to_routine_map_[id] = 33;
  }
  object_to_routine_map_[0xF8D] = 97;
  object_to_routine_map_[0xF8E] = 33;
  object_to_routine_map_[0xF8F] = 33;
  object_to_routine_map_[0xF90] = 110;
  object_to_routine_map_[0xF91] = 110;
  object_to_routine_map_[0xF92] = 115;
  object_to_routine_map_[0xF93] = 110;
  object_to_routine_map_[0xF94] = 30;
  object_to_routine_map_[0xF95] = 106;
  object_to_routine_map_[0xF96] = 25;
  object_to_routine_map_[0xF97] = 97;
  object_to_routine_map_[0xF98] = 92;
  object_to_routine_map_[0xF99] = 39;
  object_to_routine_map_[0xF9A] = 39;
  for (int id = 0xF9B; id <= 0xF9D; id++) {
    object_to_routine_map_[id] = 86;
  }
  for (int id = 0xF9E; id <= 0xFA1; id++) {
    object_to_routine_map_[id] = 87;
  }
  for (int id = 0xFA2; id <= 0xFA5; id++) {
    object_to_routine_map_[id] = 110;
  }
  for (int id = 0xFA6; id <= 0xFA9; id++) {
    object_to_routine_map_[id] = 87;
  }
  object_to_routine_map_[0xFAA] = 16;
  object_to_routine_map_[0xFAB] = 110;
  object_to_routine_map_[0xFAC] = 110;
  object_to_routine_map_[0xFAD] = 16;
  object_to_routine_map_[0xFAE] = 16;
  object_to_routine_map_[0xFAF] = 110;
  object_to_routine_map_[0xFB0] = 110;
  object_to_routine_map_[0xFB1] = 114;
  object_to_routine_map_[0xFB2] = 114;
  object_to_routine_map_[0xFB3] = 86;
  for (int id = 0xFB4; id <= 0xFB9; id++) {
    object_to_routine_map_[id] = 16;
  }
  object_to_routine_map_[0xFBA] = 102;
  object_to_routine_map_[0xFBB] = 102;
  object_to_routine_map_[0xFBC] = 103;
  object_to_routine_map_[0xFBD] = 103;
  for (int id = 0xFBE; id <= 0xFC6; id++) {
    object_to_routine_map_[id] = 110;
  }
  object_to_routine_map_[0xFC7] = 93;
  object_to_routine_map_[0xFC8] = 16;
  object_to_routine_map_[0xFC9] = 110;
  object_to_routine_map_[0xFCA] = 110;
  object_to_routine_map_[0xFCB] = 16;
  object_to_routine_map_[0xFCC] = 16;
  object_to_routine_map_[0xFCD] = 100;
  object_to_routine_map_[0xFCE] = 30;
  for (int id = 0xFCF; id <= 0xFD3; id++) {
    object_to_routine_map_[id] = 110;
  }
  object_to_routine_map_[0xFD4] = 16;
  object_to_routine_map_[0xFD5] = 101;
  for (int id = 0xFD6; id <= 0xFDA; id++) {
    object_to_routine_map_[id] = 110;
  }
  object_to_routine_map_[0xFDB] = 101;
  object_to_routine_map_[0xFDC] = 103;
  object_to_routine_map_[0xFDD] = 100;
  object_to_routine_map_[0xFDE] = 110;
  object_to_routine_map_[0xFDF] = 110;
  object_to_routine_map_[0xFE0] = 108;
  object_to_routine_map_[0xFE1] = 108;
  object_to_routine_map_[0xFE2] = 16;
  for (int id = 0xFE3; id <= 0xFE5; id++) {
    object_to_routine_map_[id] = 110;
  }
  object_to_routine_map_[0xFE6] = 116;
  object_to_routine_map_[0xFE7] = 30;
  object_to_routine_map_[0xFE8] = 30;
  object_to_routine_map_[0xFE9] = 107;
  object_to_routine_map_[0xFEA] = 107;
  object_to_routine_map_[0xFEB] = 113;
  object_to_routine_map_[0xFEC] = 114;
  object_to_routine_map_[0xFED] = 114;
  object_to_routine_map_[0xFEE] = 107;
  object_to_routine_map_[0xFEF] = 107;
  object_to_routine_map_[0xFF0] = 104;
  object_to_routine_map_[0xFF1] = 105;
  object_to_routine_map_[0xFF2] = 106;
  object_to_routine_map_[0xFF3] = 38;
  object_to_routine_map_[0xFF4] = 16;
  object_to_routine_map_[0xFF5] = 110;
  object_to_routine_map_[0xFF6] = 16;
  object_to_routine_map_[0xFF7] = 16;
  object_to_routine_map_[0xFF8] = 109;
  object_to_routine_map_[0xFF9] = 30;
  object_to_routine_map_[0xFFA] = 16;
  object_to_routine_map_[0xFFB] = 106;
  for (int id = 0xFFC; id <= 0xFFE; id++) {
    object_to_routine_map_[id] = 110;
  }
  object_to_routine_map_[0xFFF] = 38;
}

}  // namespace zelda3
}  // namespace yaze

