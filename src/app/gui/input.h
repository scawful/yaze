#ifndef YAZE_APP_CORE_INPUT_H
#define YAZE_APP_CORE_INPUT_H

#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>

#include "absl/strings/string_view.h"

namespace yaze {
namespace app {
namespace gui {

constexpr ImVec2 kDefaultModalSize = ImVec2(200, 0);
constexpr ImVec2 kZeroPos = ImVec2(0, 0);

IMGUI_API bool InputHex(const char* label, uint64_t* data);
IMGUI_API bool InputHexShort(const char* label, uint32_t* data);
IMGUI_API bool InputHexWord(const char* label, uint16_t* data,
                            float input_width = 50.f);
IMGUI_API bool InputHexByte(const char* label, uint8_t* data,
                            float input_width = 50.f);

using ItemLabelFlags = enum ItemLabelFlag {
  Left = 1u << 0u,
  Right = 1u << 1u,
  Default = Left,
};

IMGUI_API void ItemLabel(absl::string_view title, ItemLabelFlags flags);

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif