#include "app/editor/message/message_preview.h"

namespace yaze {
namespace editor {

void MessagePreview::DrawTileToPreview(int x, int y, int srcx, int srcy,
                                       int pal, int sizex, int sizey) {
  const int num_x_tiles = 16;
  const int img_width = 512;  // (imgwidth/2)
  int draw_id = srcx + (srcy * 32);
  for (int yl = 0; yl < sizey * 8; yl++) {
    for (int xl = 0; xl < 4; xl++) {
      int mx = xl;
      int my = yl;

      // Formula information to get tile index position in the array.
      int tx = ((draw_id / num_x_tiles) * img_width) + ((draw_id & 0xF) << 2);
      uint8_t pixel = font_gfx16_data_2_[tx + (yl * 64) + xl];

      // nx,ny = object position, xx,yy = tile position, xl,yl = pixel
      // position
      int index = x + (y * 172) + (mx * 2) + (my * 172);
      if ((pixel & 0x0F) != 0) {
        current_preview_data_[index + 1] = (uint8_t)((pixel & 0x0F) + (0 * 4));
      }

      if (((pixel >> 4) & 0x0F) != 0) {
        current_preview_data_[index + 0] =
            (uint8_t)(((pixel >> 4) & 0x0F) + (0 * 4));
      }
    }
  }
}

void MessagePreview::DrawStringToPreview(std::string str) {
  for (const auto c : str) {
    DrawCharacterToPreview(c);
  }
}

void MessagePreview::DrawCharacterToPreview(char c) {
  DrawCharacterToPreview(FindMatchingCharacter(c));
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
      DrawStringToPreview("(NAME)");
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