#ifndef YAZE_APP_CORE_STYLE_H
#define YAZE_APP_CORE_STYLE_H

#include <functional>
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

// Table canvas management helpers for GUI elements that need proper sizing
void BeginTableCanvas(const char* table_id, int columns, ImVec2 canvas_size = ImVec2(0, 0));
void EndTableCanvas();
void SetupCanvasTableColumn(const char* label, float width_ratio = 0.0f);
void BeginCanvasTableCell(ImVec2 min_size = ImVec2(0, 0));

void DrawDisplaySettings(ImGuiStyle *ref = nullptr);
void DrawDisplaySettingsForPopup(ImGuiStyle *ref = nullptr); // Popup-safe version

void TextWithSeparators(const absl::string_view &text);

void DrawFontManager();

struct TextBox {
  std::string text;
  std::string buffer;
  int cursor_pos = 0;
  int selection_start = 0;
  int selection_end = 0;
  int selection_length = 0;
  bool has_selection = false;
  bool has_focus = false;
  bool changed = false;
  bool can_undo = false;

  void Undo() {
    text = buffer;
    cursor_pos = selection_start;
    has_selection = false;
  }
  void clearUndo() { can_undo = false; }
  void Copy() { ImGui::SetClipboardText(text.c_str()); }
  void Cut() {
    Copy();
    text.erase(selection_start, selection_end - selection_start);
    cursor_pos = selection_start;
    has_selection = false;
    changed = true;
  }
  void Paste() {
    text.erase(selection_start, selection_end - selection_start);
    text.insert(selection_start, ImGui::GetClipboardText());
    std::string str = ImGui::GetClipboardText();
    cursor_pos = selection_start + str.size();
    has_selection = false;
    changed = true;
  }
  void clear() {
    text.clear();
    buffer.clear();
    cursor_pos = 0;
    selection_start = 0;
    selection_end = 0;
    selection_length = 0;
    has_selection = false;
    has_focus = false;
    changed = false;
    can_undo = false;
  }
  void SelectAll() {
    selection_start = 0;
    selection_end = text.size();
    selection_length = text.size();
    has_selection = true;
  }
  void Focus() { has_focus = true; }
};

// Generic multi-select component that can be used with different types of data
template <typename T>
class MultiSelect {
 public:
  // Callback function type for rendering an item
  using ItemRenderer =
      std::function<void(int index, const T &item, bool is_selected)>;

  // Constructor with optional title and default flags
  MultiSelect(
      const char *title = "Selection",
      ImGuiMultiSelectFlags flags = ImGuiMultiSelectFlags_ClearOnEscape |
                                    ImGuiMultiSelectFlags_BoxSelect1d)
      : title_(title), flags_(flags), selection_() {}

  // Set the items to display
  void SetItems(const std::vector<T> &items) { items_ = items; }

  // Set the renderer function for items
  void SetItemRenderer(ItemRenderer renderer) { item_renderer_ = renderer; }

  // Set the height of the selection area (in font size units)
  void SetHeight(float height_in_font_units = 20.0f) {
    height_in_font_units_ = height_in_font_units;
  }

  // Set the child window flags
  void SetChildFlags(ImGuiChildFlags flags) { child_flags_ = flags; }

  // Update and render the multi-select component
  void Update() {
    ImGui::Text("%s: %d/%d", title_, selection_.Size, items_.size());

    if (ImGui::BeginChild(
            "##MultiSelectChild",
            ImVec2(-FLT_MIN, ImGui::GetFontSize() * height_in_font_units_),
            child_flags_)) {
      ImGuiMultiSelectIO *ms_io =
          ImGui::BeginMultiSelect(flags_, selection_.Size, items_.size());
      selection_.ApplyRequests(ms_io);

      ImGuiListClipper clipper;
      clipper.Begin(items_.size());
      if (ms_io->RangeSrcItem != -1)
        clipper.IncludeItemByIndex((int)ms_io->RangeSrcItem);

      while (clipper.Step()) {
        for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; n++) {
          bool item_is_selected = selection_.Contains((ImGuiID)n);
          ImGui::SetNextItemSelectionUserData(n);

          if (item_renderer_) {
            item_renderer_(n, items_[n], item_is_selected);
          } else {
            // Default rendering if no custom renderer is provided
            char label[64];
            snprintf(label, sizeof(label), "Item %d", n);
            ImGui::Selectable(label, item_is_selected);
          }
        }
      }

      ms_io = ImGui::EndMultiSelect();
      selection_.ApplyRequests(ms_io);
    }
    ImGui::EndChild();
  }

  // Get the selected indices
  std::vector<int> GetSelectedIndices() const {
    std::vector<int> indices;
    for (int i = 0; i < items_.size(); i++) {
      if (selection_.Contains((ImGuiID)i)) {
        indices.push_back(i);
      }
    }
    return indices;
  }

  // Clear the selection
  void ClearSelection() { selection_.Clear(); }

 private:
  const char *title_;
  ImGuiMultiSelectFlags flags_;
  ImGuiSelectionBasicStorage selection_;
  std::vector<T> items_;
  ItemRenderer item_renderer_;
  float height_in_font_units_ = 20.0f;
  ImGuiChildFlags child_flags_ =
      ImGuiChildFlags_FrameStyle | ImGuiChildFlags_ResizeY;
};

}  // namespace gui
}  // namespace yaze

#endif
