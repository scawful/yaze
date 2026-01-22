#include "app/editor/message/message_preview.h"

namespace yaze {
namespace editor {

void MessagePreview::DrawTileToPreview(int x, int y, int srcx, int srcy,
                                       int pal, int sizex, int sizey) {
  const auto& font_data =
      !font_gfx16_data_.empty() ? font_gfx16_data_ : font_gfx16_data_2_;
  if (font_data.empty()) {
    return;
  }
  const size_t font_size = font_data.size();
  const size_t preview_size = current_preview_data_.size();
  const int sheet_width = 128;
  if (sheet_width <= 0) {
    return;
  }
  const int sheet_height = static_cast<int>(font_size / sheet_width);
  if (sheet_height <= 0) {
    return;
  }

  const int tile_size = 8;
  const int palette_offset = pal * 4;
  const int base_tile_y = srcy * sizey;

  for (int tile_y = 0; tile_y < sizey; ++tile_y) {
    for (int tile_x = 0; tile_x < sizex; ++tile_x) {
      const int tile_px = (srcx + tile_x) * tile_size;
      const int tile_py = (base_tile_y + tile_y) * tile_size;

      for (int py = 0; py < tile_size; ++py) {
        const int src_y = tile_py + py;
        if (src_y < 0 || src_y >= sheet_height) {
          continue;
        }

        for (int px = 0; px < tile_size; ++px) {
          const int src_x = tile_px + px;
          if (src_x < 0 || src_x >= sheet_width) {
            continue;
          }

          const size_t src_index =
              static_cast<size_t>(src_x + (src_y * sheet_width));
          if (src_index >= font_size) {
            continue;
          }

          const uint8_t pixel = font_data[src_index];
          if ((pixel & 0x0F) == 0) {
            continue;
          }

          const int dst_x = x + tile_x * tile_size + px;
          const int dst_y = y + tile_y * tile_size + py;
          const int dst_index = dst_x + (dst_y * kCurrentMessageWidth);
          if (dst_index < 0 ||
              static_cast<size_t>(dst_index) >= preview_size) {
            continue;
          }

          current_preview_data_[dst_index] =
              static_cast<uint8_t>(pixel + palette_offset);
        }
      }
    }
  }
}

void MessagePreview::DrawStringToPreview(const std::string& str) {
  for (const auto& c : str) {
    DrawCharacterToPreview(c);
  }
}

void MessagePreview::DrawCharacterToPreview(char c) {
  std::vector<uint8_t> text;
  text.push_back(FindMatchingCharacter(c));
  DrawCharacterToPreview(text);
}

void MessagePreview::DrawCharacterToPreview(const std::vector<uint8_t>& text) {
  for (const uint8_t& value : text) {
    if (skip_next) {
      skip_next = false;
      continue;
    }

    if (value < 100) {
      int srcy = value >> 4;
      int srcx = value & 0xF;

      if (text_position >= 170) {
        text_position = 0;
        text_line++;
      }

      DrawTileToPreview(text_position, text_line * 16, srcx, srcy, 0, 1, 2);
      text_position += width_array[value];
    } else if (value == kLine1) {
      text_position = 0;
      text_line = 0;
    } else if (value == kScrollVertical) {
      text_position = 0;
      text_line += 1;
    } else if (value == kLine2) {
      text_position = 0;
      text_line = 1;
    } else if (value == kLine3) {
      text_position = 0;
      text_line = 2;
    } else if (value == 0x6B || value == 0x6D || value == 0x6E ||
               value == 0x77 || value == 0x78 || value == 0x79 ||
               value == 0x7A) {
      skip_next = true;

      continue;
    } else if (value == 0x6C)  // BCD numbers.
    {
      DrawCharacterToPreview('0');
      skip_next = true;

      continue;
    } else if (value == 0x6A) {
      // Includes parentheses to be longer, since player names can be up to 6
      // characters.
      const std::string name = "(NAME)";
      DrawStringToPreview(name);
    } else if (value >= DICTOFF && value < (DICTOFF + 97)) {
      int pos = value - DICTOFF;
      if (pos < 0 || pos >= all_dictionaries_.size()) {
        // Invalid dictionary entry.
        std::cerr << "Invalid dictionary entry: " << pos << std::endl;
        continue;
      }
      auto dictionary_entry = FindRealDictionaryEntry(value, all_dictionaries_);
      DrawCharacterToPreview(dictionary_entry.Data);
    }
  }
}

void MessagePreview::DrawMessagePreview(const MessageData& message) {
  // From Parsing.
  text_line = 0;
  std::fill(current_preview_data_.begin(), current_preview_data_.end(), 0);
  text_position = 0;
  DrawCharacterToPreview(message.Data);
  shown_lines = 0;
}

}  // namespace editor
}  // namespace yaze
