#ifndef YAZE_APP_CORE_INPUT_H
#define YAZE_APP_CORE_INPUT_H

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <cstddef>
#include <cstdint>

#include "absl/strings/string_view.h"

namespace yaze {
namespace gui {

IMGUI_API bool InputHex(const char* label, int* data);
IMGUI_API bool InputHexShort(const char* label, int* data);

using ItemLabelFlags = enum ItemLabelFlag {
  Left = 1u << 0u,
  Right = 1u << 1u,
  Default = Left,
};

IMGUI_API void ItemLabel(absl::string_view title, ItemLabelFlags flags);

}  // namespace gui
}  // namespace yaze

#endif