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
#include "app/gui/style.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

constexpr int kGfxFont = 0x70000;  // 2bpp format
constexpr int kCharactersWidth = 0x74ADF;
constexpr int kNumMessages = 396;
constexpr int kCurrentMessageWidth = 172;
constexpr int kCurrentMessageHeight = 4096;
constexpr int kFontGfxMessageSize = 128;
constexpr int kFontGfxMessageDepth = 64;
constexpr int kFontGfx16Size = 172 * 4096;

constexpr uint8_t kWidthArraySize = 100;
constexpr uint8_t kBlockTerminator = 0x80;
constexpr uint8_t kMessageBankChangeId = 0x80;

class MessageEditor : public Editor {
 public:
  explicit MessageEditor(Rom* rom = nullptr) : rom_(rom) {
    type_ = EditorType::kMessage;
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;

  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;
  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Find() override;
  absl::Status Save() override;
  void Delete();
  void SelectAll();

  void DrawMessageList();
  void DrawCurrentMessage();
  void DrawTextCommands();
  void DrawSpecialCharacters();
  void DrawExpandedMessageSettings();
  void DrawDictionary();

  void DrawTileToPreview(int x, int y, int srcx, int srcy, int pal,
                         int sizex = 1, int sizey = 1);
  void DrawCharacterToPreview(char c);
  void DrawCharacterToPreview(const std::vector<uint8_t>& text);

  void DrawStringToPreview(std::string str);
  void DrawMessagePreview();
  std::string DisplayTextOverflowError(int pos, bool bank);

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
  Rom* rom_;
  bool skip_next = false;
  bool data_loaded_ = false;
  bool case_sensitive_ = false;
  bool match_whole_word_ = false;
  bool export_expanded_messages_ = false;

  int text_line_ = 0;
  int text_position_ = 0;
  int shown_lines_ = 0;

  std::string expanded_message_address_ = "";
  std::string search_text_ = "";

  std::array<uint8_t, kWidthArraySize> width_array = {0};
  std::vector<uint8_t> font_gfx16_data_;
  std::array<uint8_t, 0x4000> raw_font_gfx_data_;
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
                                         ImVec2(172 * 2, 4096)};

  gui::TextBox message_text_box_;

  absl::Status status_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_EDITOR_H
