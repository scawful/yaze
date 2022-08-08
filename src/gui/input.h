#ifndef YAZE_APP_CORE_INPUT_H
#define YAZE_APP_CORE_INPUT_H

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <cstddef>
#include <cstdint>

namespace yaze {
namespace gui {

IMGUI_API bool InputHex(const char* label, int* data);
IMGUI_API bool InputHexShort(const char* label, int* data);

}  // namespace gui
}  // namespace yaze

#endif