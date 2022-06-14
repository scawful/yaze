#include "input.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace yaze {
namespace Gui {

const int kStepOneHex = 0x01;
const int kStepFastHex = 0x0F;

bool InputHex(const char* label, int* data) {
  return ImGui::InputScalar(label, ImGuiDataType_U64, data, &kStepOneHex,
                            &kStepFastHex, "%06X",
                            ImGuiInputTextFlags_CharsHexadecimal);
}

bool InputHexShort(const char* label, int* data) {
  return ImGui::InputScalar(label, ImGuiDataType_U32, data, &kStepOneHex,
                            &kStepFastHex, "%06X",
                            ImGuiInputTextFlags_CharsHexadecimal);
}

}  // namespace Gui
}  // namespace yaze
