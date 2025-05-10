#ifndef YAZEE_MESSAGE_PREVIEW_H_
#define YAZEE_MESSAGE_PREVIEW_H_

#include <string>
#include <vector>

#include "app/editor/message/message_data.h"

namespace yaze {
namespace editor {

constexpr int kCurrentMessageWidth = 172;
constexpr int kCurrentMessageHeight = 4096;

struct MessagePreview {
  MessagePreview() {
    current_preview_data_.resize(kCurrentMessageWidth * kCurrentMessageHeight);
    std::fill(current_preview_data_.begin(), current_preview_data_.end(), 0);
  }
  void DrawTileToPreview(int x, int y, int srcx, int srcy, int pal,
                         int sizex = 1, int sizey = 1);

  void DrawStringToPreview(const std::string& str);
  void DrawCharacterToPreview(char c);
  void DrawCharacterToPreview(const std::vector<uint8_t>& text);

  void DrawMessagePreview(const MessageData& message);

  bool skip_next = false;
  int text_line = 0;
  int text_position = 0;
  int shown_lines = 0;

  std::array<uint8_t, kWidthArraySize> width_array = {0};
  std::vector<uint8_t> font_gfx16_data_;
  std::vector<uint8_t> font_gfx16_data_2_;
  std::vector<uint8_t> current_preview_data_;
  std::vector<DictionaryEntry> all_dictionaries_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZEE_MESSAGE_PREVIEW_H_
