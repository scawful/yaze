#ifndef YAZE_GUI_WIDGETS_H
#define YAZE_GUI_WIDGETS_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"

namespace yaze {
namespace app {
namespace gui {
namespace widgets {

TextEditor::LanguageDefinition GetAssemblyLanguageDef();

class BitmapViewer {
 public:
  BitmapViewer() : current_bitmap_index_(0) {}

  void Display(const std::vector<gfx::Bitmap>& bitmaps) {
    if (bitmaps.empty()) {
      ImGui::Text("No bitmaps available.");
      return;
    }

    // Display the current bitmap index and total count.
    ImGui::Text("Viewing Bitmap %d / %zu", current_bitmap_index_ + 1,
                bitmaps.size());

    // Buttons to navigate through bitmaps.
    if (ImGui::Button("<- Prev")) {
      if (current_bitmap_index_ > 0) {
        --current_bitmap_index_;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Next ->")) {
      if (current_bitmap_index_ < bitmaps.size() - 1) {
        ++current_bitmap_index_;
      }
    }

    // Display the current bitmap.
    const gfx::Bitmap& current_bitmap = bitmaps[current_bitmap_index_];
    // Assuming Bitmap has a function to get its texture ID, and width and
    // height.
    ImTextureID tex_id = current_bitmap.texture();
    ImVec2 size(current_bitmap.width(), current_bitmap.height());
    ImGui::Image(tex_id, size);

    // Scroll if the image is larger than the display area.
    if (ImGui::BeginChild("BitmapScrollArea", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      ImGui::Image(tex_id, size);
      ImGui::EndChild();
    }
  }

 private:
  int current_bitmap_index_;
};

}  // namespace widgets
}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif