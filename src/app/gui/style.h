#ifndef YAZE_APP_CORE_STYLE_H
#define YAZE_APP_CORE_STYLE_H

#include "ImGuiColorTextEdit/TextEditor.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include <functional>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"

namespace yaze {
namespace app {
namespace gui {

void BeginWindowWithDisplaySettings(const char* id, bool* active,
                                    const ImVec2& size = ImVec2(0, 0),
                                    ImGuiWindowFlags flags = 0);

void EndWindowWithDisplaySettings();

void BeginPadding(int i);
void EndPadding();

void BeginNoPadding();
void EndNoPadding();

void BeginChildWithScrollbar(const char* str_id);

void BeginChildBothScrollbars(int id);

void DrawDisplaySettings(ImGuiStyle* ref = nullptr);

void TextWithSeparators(const absl::string_view& text);

void ColorsYaze();

TextEditor::LanguageDefinition GetAssemblyLanguageDef();

class BitmapViewer {
 public:
  BitmapViewer() : current_bitmap_index_(0) {}

  void Display(const std::vector<gfx::Bitmap>& bitmaps, float scale = 1.0f) {
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
    ImVec2 size(current_bitmap.width() * scale,
                current_bitmap.height() * scale);
    // ImGui::Image(tex_id, size);

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

// ============================================================================

static const char* ExampleNames[] = {
    "Artichoke",      "Arugula",          "Asparagus",    "Avocado",
    "Bamboo Shoots",  "Bean Sprouts",     "Beans",        "Beet",
    "Belgian Endive", "Bell Pepper",      "Bitter Gourd", "Bok Choy",
    "Broccoli",       "Brussels Sprouts", "Burdock Root", "Cabbage",
    "Calabash",       "Capers",           "Carrot",       "Cassava",
    "Cauliflower",    "Celery",           "Celery Root",  "Celcuce",
    "Chayote",        "Chinese Broccoli", "Corn",         "Cucumber"};

struct MultiSelectWithClipper {
  const int ITEMS_COUNT = 10000;
  void Update() {
    // Use default selection.Adapter: Pass index to
    // SetNextItemSelectionUserData(), store index in Selection
    static ImGuiSelectionBasicStorage selection;

    ImGui::Text("Selection: %d/%d", selection.Size, ITEMS_COUNT);
    if (ImGui::BeginChild(
            "##Basket", ImVec2(-FLT_MIN, ImGui::GetFontSize() * 20),
            ImGuiChildFlags_FrameStyle | ImGuiChildFlags_ResizeY)) {
      ImGuiMultiSelectFlags flags = ImGuiMultiSelectFlags_ClearOnEscape |
                                    ImGuiMultiSelectFlags_BoxSelect1d;
      ImGuiMultiSelectIO* ms_io =
          ImGui::BeginMultiSelect(flags, selection.Size, ITEMS_COUNT);
      selection.ApplyRequests(ms_io);

      ImGuiListClipper clipper;
      clipper.Begin(ITEMS_COUNT);
      if (ms_io->RangeSrcItem != -1)
        clipper.IncludeItemByIndex(
            (int)ms_io->RangeSrcItem);  // Ensure RangeSrc item is not clipped.
      while (clipper.Step()) {
        for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; n++) {
          char label[64];
          // sprintf(label, "Object %05d: %s", n,
          //         ExampleNames[n % IM_ARRAYSIZE(ExampleNames)]);
          bool item_is_selected = selection.Contains((ImGuiID)n);
          ImGui::SetNextItemSelectionUserData(n);
          ImGui::Selectable(label, item_is_selected);
        }
      }

      ms_io = ImGui::EndMultiSelect();
      selection.ApplyRequests(ms_io);
    }
    ImGui::EndChild();
    ImGui::TreePop();
  }
};

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif
