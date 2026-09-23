#include "app/editor/dungeon/dungeon_room_edit.h"

#include "absl/strings/str_format.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

namespace {

bool HasValidMetadataSlot(const RoomMetadataEdit& edit) {
  const bool staircase = edit.field == RoomMetadataField::kStaircaseRoom ||
                         edit.field == RoomMetadataField::kStaircasePlane;
  return staircase ? edit.index >= 0 && edit.index < 4 : edit.index == 0;
}

int MetadataMaximum(RoomMetadataField field) {
  switch (field) {
    case RoomMetadataField::kLayout:
    case RoomMetadataField::kEffect:
      return 0x07;
    case RoomMetadataField::kBlockset:
      return 0x51;
    case RoomMetadataField::kFloor1:
    case RoomMetadataField::kFloor2:
      return 0x0F;
    case RoomMetadataField::kPalette:
      return 0x47;
    case RoomMetadataField::kSpriteset:
      return zelda3::kMaxDungeonSpriteset;
    case RoomMetadataField::kMessage:
      return 0x0FFF;
    case RoomMetadataField::kBg2:
      return 0x08;
    case RoomMetadataField::kCollision:
      return 0x04;
    case RoomMetadataField::kTag1:
    case RoomMetadataField::kTag2:
      return 0x3F;
    case RoomMetadataField::kHolewarp:
    case RoomMetadataField::kStaircaseRoom:
      return 0xFF;
    case RoomMetadataField::kStaircasePlane:
      return 0x03;
  }
  return -1;
}

}  // namespace

absl::Status ValidateRoomMetadataEdit(const RoomMetadataEdit& edit) {
  if (!HasValidMetadataSlot(edit)) {
    return absl::InvalidArgumentError("Invalid room metadata field slot");
  }
  const int maximum = MetadataMaximum(edit.field);
  if (maximum < 0) {
    return absl::InvalidArgumentError("Unknown room metadata field");
  }
  if (edit.value < 0 || edit.value > maximum) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Room metadata value %d is outside the supported range 0..%d",
        edit.value, maximum));
  }
  return absl::OkStatus();
}

absl::Status ApplyRoomMetadataEdit(zelda3::Room& room,
                                   const RoomMetadataEdit& edit) {
  // Keep the value predicate visible at the mutation boundary. The status
  // helper supplies error details; it is not the proof for narrowing casts.
  if (!HasValidMetadataSlot(edit) || edit.value < 0 ||
      edit.value > MetadataMaximum(edit.field)) {
    return ValidateRoomMetadataEdit(edit);
  }

  const uint8_t value = static_cast<uint8_t>(edit.value);
  switch (edit.field) {
    case RoomMetadataField::kLayout:
      room.SetLayoutId(value);
      break;
    case RoomMetadataField::kBlockset:
      room.SetBlockset(value);
      break;
    case RoomMetadataField::kFloor1:
      room.set_floor1(value);
      break;
    case RoomMetadataField::kFloor2:
      room.set_floor2(value);
      break;
    case RoomMetadataField::kPalette:
      room.SetPalette(value);
      break;
    case RoomMetadataField::kSpriteset:
      room.SetSpriteset(value);
      break;
    case RoomMetadataField::kMessage:
      room.SetMessageId(static_cast<uint16_t>(edit.value));
      break;
    case RoomMetadataField::kBg2:
      room.SetBg2(static_cast<background2>(value));
      break;
    case RoomMetadataField::kEffect:
      room.SetEffect(static_cast<zelda3::EffectKey>(value));
      break;
    case RoomMetadataField::kCollision:
      room.SetCollision(static_cast<zelda3::CollisionKey>(value));
      break;
    case RoomMetadataField::kTag1:
      room.SetTag1(static_cast<zelda3::TagKey>(value));
      break;
    case RoomMetadataField::kTag2:
      room.SetTag2(static_cast<zelda3::TagKey>(value));
      break;
    case RoomMetadataField::kHolewarp:
      room.SetHolewarp(value);
      break;
    case RoomMetadataField::kStaircaseRoom:
      room.SetStaircaseRoom(edit.index, value);
      break;
    case RoomMetadataField::kStaircasePlane:
      room.SetStaircasePlane(edit.index, value);
      break;
  }
  return absl::OkStatus();
}

}  // namespace yaze::editor
