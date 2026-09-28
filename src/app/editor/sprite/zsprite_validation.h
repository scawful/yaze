#ifndef YAZE_APP_EDITOR_SPRITE_ZSPRITE_VALIDATION_H_
#define YAZE_APP_EDITOR_SPRITE_ZSPRITE_VALIDATION_H_

#include <cstdint>
#include <string_view>

#include "absl/status/status.h"

namespace yaze::editor::zsprite {
// Check the same byte snapshot subsequently decoded by ZSprite::Load. Counts
// and .NET string lengths are bounded before allocating any frame or routine.
inline absl::Status ValidateZsmBytes(std::string_view data) {
  size_t cursor = 0;
  auto bytes = [&](size_t count) {
    if (count > data.size() - cursor)
      return false;
    cursor += count;
    return true;
  };
  auto count32 = [&](uint32_t limit, uint32_t& value) {
    if (!bytes(4))
      return false;
    value = 0;
    for (int i = 0; i < 4; ++i)
      value |= uint32_t(static_cast<uint8_t>(data[cursor - 4 + i])) << (8 * i);
    return value <= limit;
  };
  auto boolean = [&] {
    if (!bytes(1))
      return false;
    return static_cast<uint8_t>(data[cursor - 1]) <= 1;
  };
  auto string = [&] {
    uint32_t length = 0;
    for (int i = 0; i < 5; ++i) {
      if (!bytes(1))
        return false;
      const auto value = static_cast<uint8_t>(data[cursor - 1]);
      if (i == 4 && (value & 0xf0))
        return false;
      length |= uint32_t(value & 127) << (7 * i);
      if (!(value & 128))
        return length <= 1024 * 1024 && bytes(length);
    }
    return false;
  };
  auto invalid = [] {
    return absl::DataLossError("Malformed or oversized ZSM asset");
  };
  if (data.size() > 8 * 1024 * 1024)
    return invalid();
  uint32_t count = 0;
  if (!count32(256, count))
    return invalid();
  for (uint32_t i = 0; i < count; ++i)
    if (!string() || !bytes(3))
      return invalid();
  if (!count32(256, count))
    return invalid();
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t tiles = 0;
    if (!count32(128, tiles))
      return invalid();
    for (uint32_t j = 0; j < tiles; ++j)
      if (!bytes(3) || !boolean() || !boolean() || !bytes(1) || !boolean() ||
          !bytes(3))
        return invalid();
  }
  for (int i = 0; i < 20; ++i)
    if (!boolean())
      return invalid();
  if (!bytes(6))
    return invalid();
  if (cursor < data.size()) {
    if (!string() || !count32(1024, count))
      return invalid();
    for (uint32_t i = 0; i < count; ++i)
      if (!string() || !string())
        return invalid();
  }
  if (cursor < data.size() && !string())
    return invalid();
  return cursor == data.size() ? absl::OkStatus() : invalid();
}
}  // namespace yaze::editor::zsprite
#endif
