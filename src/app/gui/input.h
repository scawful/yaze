#ifndef YAZE_APP_CORE_INPUT_H
#define YAZE_APP_CORE_INPUT_H

#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/strings/string_view.h"

namespace yaze {
namespace app {
namespace gui {

constexpr ImVec2 kDefaultModalSize = ImVec2(200, 0);
constexpr ImVec2 kZeroPos = ImVec2(0, 0);

IMGUI_API bool InputHexWithScrollwheel(const char* label, uint32_t* data,
                                       uint32_t step = 0x01,
                                       float input_width = 50.f);

IMGUI_API bool InputHex(const char* label, uint64_t* data);
IMGUI_API bool InputHex(const char* label, int* data, int num_digits = 4,
                        float input_width = 50.f);
IMGUI_API bool InputHexShort(const char* label, uint32_t* data);
IMGUI_API bool InputHexWord(const char* label, uint16_t* data,
                            float input_width = 50.f, bool no_step = false);
IMGUI_API bool InputHexWord(const char* label, int16_t* data,
                            float input_width = 50.f, bool no_step = false);
IMGUI_API bool InputHexByte(const char* label, uint8_t* data,
                            float input_width = 50.f, bool no_step = false);

IMGUI_API bool InputHexByte(const char* label, uint8_t* data, uint8_t max_value,
                            float input_width = 50.f, bool no_step = false);

IMGUI_API bool ListBox(const char* label, int* current_item,
                       const std::vector<std::string>& items,
                       int height_in_items = -1);

using ItemLabelFlags = enum ItemLabelFlag {
  Left = 1u << 0u,
  Right = 1u << 1u,
  Default = Left,
};

IMGUI_API void ItemLabel(absl::string_view title, ItemLabelFlags flags);

IMGUI_API ImGuiID GetID(const std::string& id);

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif