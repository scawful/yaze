#ifndef YAZE_APP_CORE_PIPELINE_H
#define YAZE_APP_CORE_PIPELINE_H

#include <ImGuiFileDialog/ImGuiFileDialog.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <imgui_memory_editor.h>

#include <functional>
#include <optional>

#include "absl/strings/string_view.h"

namespace yaze {
namespace app {
namespace core {

void FileDialogPipeline(absl::string_view display_key,
                        absl::string_view file_extensions,
                        std::optional<absl::string_view> button_text,
                        std::function<void()> callback);

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif