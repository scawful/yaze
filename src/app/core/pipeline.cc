#include "pipeline.h"

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
                        std::function<void()> callback) {
  if (button_text.has_value() && ImGui::Button(button_text->data())) {
    ImGuiFileDialog::Instance()->OpenDialog(display_key.data(), "Choose File",
                                            file_extensions.data(), ".");
  }

  if (ImGuiFileDialog::Instance()->Display(display_key.data())) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      callback();
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

}  // namespace core
}  // namespace app
}  // namespace yaze