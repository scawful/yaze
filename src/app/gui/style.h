#ifndef YAZE_APP_CORE_STYLE_H
#define YAZE_APP_CORE_STYLE_H

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "app/gfx/bitmap.h"
#include "app/gui/color.h"
#include "app/gui/modules/text_editor.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

struct Theme {
  std::string name;

  Color menu_bar_bg;
  Color title_bar_bg;

  Color header;
  Color header_hovered;
  Color header_active;

  Color title_bg_active;
  Color title_bg_collapsed;

  Color tab;
  Color tab_hovered;
  Color tab_active;

  Color button;
  Color button_hovered;
  Color button_active;

  Color clickable_text;
  Color clickable_text_hovered;
};

absl::StatusOr<Theme> LoadTheme(const std::string &filename);
absl::Status SaveTheme(const Theme &theme);
void ApplyTheme(const Theme &theme);

void ColorsYaze();

TextEditor::LanguageDefinition GetAssemblyLanguageDef();

void DrawBitmapViewer(const std::vector<gfx::Bitmap> &bitmaps, float scale,
                      int &current_bitmap);

void BeginWindowWithDisplaySettings(const char *id, bool *active,
                                    const ImVec2 &size = ImVec2(0, 0),
                                    ImGuiWindowFlags flags = 0);

void EndWindowWithDisplaySettings();

void BeginPadding(int i);
void EndPadding();

void BeginNoPadding();
void EndNoPadding();

void BeginChildWithScrollbar(const char *str_id);

void BeginChildBothScrollbars(int id);

void DrawDisplaySettings(ImGuiStyle *ref = nullptr);

void TextWithSeparators(const absl::string_view &text);

void DrawFontManager();

static const char *ExampleNames[] = {
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
      ImGuiMultiSelectIO *ms_io =
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
}  // namespace yaze

#endif
