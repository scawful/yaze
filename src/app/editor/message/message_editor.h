#ifndef YAZE_APP_EDITOR_MESSAGE_EDITOR_H
#define YAZE_APP_EDITOR_MESSAGE_EDITOR_H

#include <array>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/message/message_data.h"
#include "app/gfx/bitmap.h"
#include "app/gui/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

constexpr int kGfxFont = 0x70000;  // 2bpp format
constexpr int kTextData2 = 0x75F40;
constexpr int kTextData2End = 0x773FF;
constexpr int kCharactersWidth = 0x74ADF;
constexpr int kNumMessages = 396;
constexpr int kCurrentMessageWidth = 172;
constexpr int kCurrentMessageHeight = 4096;
constexpr int kFontGfxMessageSize = 128;
constexpr int kFontGfxMessageDepth = 8;
constexpr int kFontGfx16Size = 172 * 4096;

constexpr uint8_t kWidthArraySize = 100;
constexpr uint8_t kBlockTerminator = 0x80;
constexpr uint8_t kMessageBankChangeId = 0x80;

static TextElement DictionaryElement =
    TextElement(0x80, DICTIONARYTOKEN, true, "Dictionary");

class MessageEditor : public Editor, public SharedRom {
 public:
  MessageEditor() { type_ = EditorType::kMessage; }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  void DrawMessageList();
  void DrawCurrentMessage();
  void DrawTextCommands();
  void DrawDictionary();

  void ReadAllTextDataV2();
  [[deprecated]] void ReadAllTextData();

  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Undo() override;
  absl::Status Redo() override {
    return absl::UnimplementedError("Redo not implemented");
  }
  absl::Status Find() override {
    return absl::UnimplementedError("Find not implemented");
  }
  absl::Status Save();
  void Delete();
  void SelectAll();

  DictionaryEntry GetDictionaryFromID(uint8_t value);
  void DrawTileToPreview(int x, int y, int srcx, int srcy, int pal,
                         int sizex = 1, int sizey = 1);
  void DrawCharacterToPreview(char c);
  void DrawCharacterToPreview(const std::vector<uint8_t>& text);

  void DrawStringToPreview(std::string str);
  void DrawMessagePreview();
  std::string DisplayTextOverflowError(int pos, bool bank);

 private:
  bool skip_next = false;
  bool data_loaded_ = false;

  int text_line_ = 0;
  int text_position_ = 0;
  int shown_lines_ = 0;

  std::string search_text_ = "";

  std::array<uint8_t, kWidthArraySize> width_array = {0};
  std::vector<uint8_t> font_gfx16_data_;
  std::vector<uint8_t> current_font_gfx16_data_;
  std::vector<std::string> parsed_messages_;
  std::vector<MessageData> list_of_texts_;
  std::vector<DictionaryEntry> all_dictionaries_;

  MessageData current_message_;

  gfx::Bitmap font_gfx_bitmap_;
  gfx::Bitmap current_font_gfx16_bitmap_;
  gfx::SnesPalette font_preview_colors_;

  gui::Canvas font_gfx_canvas_{"##FontGfxCanvas", ImVec2(128, 128)};
  gui::Canvas current_font_gfx16_canvas_{"##CurrentMessageGfx",
                                         ImVec2(172, 4096)};

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

  TextBox message_text_box_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_EDITOR_H
