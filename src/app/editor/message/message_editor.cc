#include "message_editor.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "app/core/platform/renderer.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze {
namespace editor {

using core::Renderer;

using ImGui::BeginChild;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndChild;
using ImGui::EndTable;
using ImGui::InputText;
using ImGui::InputTextMultiline;
using ImGui::SameLine;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableSetupColumn;
using ImGui::Text;
using ImGui::TextWrapped;

constexpr ImGuiTableFlags kMessageTableFlags = ImGuiTableFlags_Hideable |
                                               ImGuiTableFlags_Borders |
                                               ImGuiTableFlags_Resizable;

constexpr ImGuiTableFlags kDictTableFlags =
    ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

absl::Status MessageEditor::Initialize() {
  for (int i = 0; i < kWidthArraySize; i++) {
    width_array[i] = rom()->data()[kCharactersWidth + i];
  }

  all_dictionaries_ = BuildDictionaryEntries(rom());
  ReadAllTextDataV2();

  font_preview_colors_.AddColor(0x7FFF);  // White
  font_preview_colors_.AddColor(0x7C00);  // Red
  font_preview_colors_.AddColor(0x03E0);  // Green
  font_preview_colors_.AddColor(0x001F);  // Blue

  std::vector<uint8_t> data(0x4000, 0);
  for (int i = 0; i < 0x4000; i++) {
    data[i] = rom()->data()[kGfxFont + i];
  }
  font_gfx16_data_ = gfx::SnesTo8bppSheet(data, /*bpp=*/2, /*num_sheets=*/2);

  // 4bpp
  RETURN_IF_ERROR(Renderer::GetInstance().CreateAndRenderBitmap(
      kFontGfxMessageSize, kFontGfxMessageSize, kFontGfxMessageDepth,
      font_gfx16_data_, font_gfx_bitmap_, font_preview_colors_))

  current_font_gfx16_data_.reserve(kCurrentMessageWidth *
                                   kCurrentMessageHeight);
  for (int i = 0; i < kCurrentMessageWidth * kCurrentMessageHeight; i++) {
    current_font_gfx16_data_.push_back(0);
  }

  // 8bpp
  RETURN_IF_ERROR(Renderer::GetInstance().CreateAndRenderBitmap(
      kCurrentMessageWidth, kCurrentMessageHeight, 64, current_font_gfx16_data_,
      current_font_gfx16_bitmap_, font_preview_colors_))

  gfx::SnesPalette color_palette = font_gfx_bitmap_.palette();
  for (int i = 0; i < font_preview_colors_.size(); i++) {
    *color_palette.mutable_color(i) = font_preview_colors_[i];
  }

  *font_gfx_bitmap_.mutable_palette() = color_palette;

  for (const auto& each_message : list_of_texts_) {
    std::cout << "Message #" << each_message.ID << " at address "
              << core::HexLong(each_message.Address) << std::endl;
    std::cout << "  " << each_message.RawString << std::endl;

    // Each string has a [:XX] char encoded
    // The corresponding character is found in CharEncoder unordered_map
    std::string parsed_message = "";
    for (const auto& byte : each_message.Data) {
      // Find the char byte in the CharEncoder map
      if (CharEncoder.contains(byte)) {
        parsed_message.push_back(CharEncoder.at(byte));
      } else {
        // If the byte is not found in the CharEncoder map, it is a command
        // or a dictionary entry
        if (byte >= DICTOFF && byte < (DICTOFF + 97)) {
          // Dictionary entry
          auto dictionaryEntry = GetDictionaryFromID(byte - DICTOFF);
          parsed_message.append(dictionaryEntry.Contents);
        } else {
          // Command
          TextElement textElement = FindMatchingCommand(byte);
          if (!textElement.Empty()) {
            // If the element is line 2, 3 or V we add a newline
            if (textElement.ID == kScrollVertical || textElement.ID == kLine2 ||
                textElement.ID == kLine3)
              parsed_message.append("\n");

            parsed_message.append(textElement.GenericToken);
          }
        }
      }
    }
    std::cout << "  > " << parsed_message << std::endl;
    parsed_messages_.push_back(parsed_message);
  }

  DrawMessagePreview();

  return absl::OkStatus();
}

absl::Status MessageEditor::Update() {
  if (rom()->is_loaded() && !data_loaded_) {
    RETURN_IF_ERROR(Initialize());
    current_message_ = list_of_texts_[1];
    data_loaded_ = true;
  }

  if (BeginTable("##MessageEditor", 4, kDictTableFlags)) {
    TableSetupColumn("List");
    TableSetupColumn("Contents");
    TableSetupColumn("Commands");
    TableSetupColumn("Dictionary");

    TableHeadersRow();

    TableNextColumn();
    DrawMessageList();

    TableNextColumn();
    DrawCurrentMessage();

    TableNextColumn();
    DrawTextCommands();

    TableNextColumn();
    DrawDictionary();

    EndTable();
  }

  return absl::OkStatus();
}

void MessageEditor::DrawMessageList() {
  if (BeginChild("##MessagesList", ImVec2(0, 0), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    if (BeginTable("##MessagesTable", 3, kMessageTableFlags)) {
      TableSetupColumn("ID");
      TableSetupColumn("Contents");
      TableSetupColumn("Data");

      TableHeadersRow();

      for (const auto& message : list_of_texts_) {
        TableNextColumn();
        if (Button(core::HexWord(message.ID).c_str())) {
          current_message_ = message;
          DrawMessagePreview();
        }
        TableNextColumn();
        TextWrapped("%s", parsed_messages_[message.ID].c_str());
        TableNextColumn();
        TextWrapped(
            "%s",
            core::HexLong(list_of_texts_[message.ID].Address).c_str());
      }

      EndTable();
    }
    EndChild();
  }
}

void MessageEditor::DrawCurrentMessage() {
  Button(absl::StrCat("Message ", current_message_.ID).c_str());
  if (InputTextMultiline("##MessageEditor",
                         &parsed_messages_[current_message_.ID],
                         ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    current_message_.Data = ParseMessageToData(message_text_box_.text);
    DrawMessagePreview();
  }
  Separator();

  Text("Font Graphics");
  gui::BeginPadding(1);
  BeginChild("MessageEditorCanvas", ImVec2(0, 130));
  font_gfx_canvas_.DrawBackground();
  font_gfx_canvas_.DrawContextMenu();
  font_gfx_canvas_.DrawBitmap(font_gfx_bitmap_, 0, 0);
  font_gfx_canvas_.DrawGrid();
  font_gfx_canvas_.DrawOverlay();
  EndChild();
  gui::EndPadding();
  Separator();

  Text("Message Preview");
  if (Button("Refresh Bitmap")) {
    Renderer::GetInstance().UpdateBitmap(&current_font_gfx16_bitmap_);
  }
  gui::BeginPadding(1);
  BeginChild("CurrentGfxFont", ImVec2(0, 0), true,
             ImGuiWindowFlags_AlwaysVerticalScrollbar);
  current_font_gfx16_canvas_.DrawBackground();
  gui::EndPadding();
  current_font_gfx16_canvas_.DrawContextMenu();
  current_font_gfx16_canvas_.DrawBitmap(current_font_gfx16_bitmap_, 0, 0);
  current_font_gfx16_canvas_.DrawGrid();
  current_font_gfx16_canvas_.DrawOverlay();
  EndChild();
}

void MessageEditor::DrawTextCommands() {
  if (BeginChild("##TextCommands", ImVec2(0, 0), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    for (const auto& text_element : TextCommands) {
      if (Button(text_element.GenericToken.c_str())) {
      }
      SameLine();
      TextWrapped("%s", text_element.Description.c_str());
      Separator();
    }
    EndChild();
  }
}

void MessageEditor::DrawDictionary() {
  if (ImGui::BeginChild("##DictionaryChild", ImVec2(0, 0), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    if (BeginTable("##Dictionary", 2, kDictTableFlags)) {
      TableSetupColumn("ID");
      TableSetupColumn("Contents");

      for (const auto& dictionary : all_dictionaries_) {
        TableNextColumn();
        Text("%s", core::HexWord(dictionary.ID).c_str());
        TableNextColumn();
        Text("%s", dictionary.Contents.c_str());
      }
      EndTable();
    }

    EndChild();
  }
}

// TODO: Fix the command parsing.
void MessageEditor::ReadAllTextDataV2() {
  // Read all text data from the ROM.
  int pos = kTextData;
  int message_id = 0;

  std::vector<uint8_t> raw_message;
  std::vector<uint8_t> parsed_message;

  std::string current_raw_message;
  std::string current_parsed_message;

  uint8_t current_byte = 0;
  while (current_byte != 0xFF) {
    current_byte = rom()->data()[pos++];
    if (current_byte == kMessageTerminator) {
      auto message =
          MessageData(message_id++, pos, current_raw_message, raw_message,
                      current_parsed_message, parsed_message);

      list_of_texts_.push_back(message);

      raw_message.clear();
      parsed_message.clear();
      current_raw_message.clear();
      current_parsed_message.clear();

      continue;
    }

    raw_message.push_back(current_byte);

    // Check for command.
    TextElement text_element = FindMatchingCommand(current_byte);
    if (!text_element.Empty()) {
      parsed_message.push_back(current_byte);
      if (text_element.HasArgument) {
        current_byte = rom()->data()[pos++];
        raw_message.push_back(current_byte);
        parsed_message.push_back(current_byte);
      }

      current_raw_message.append(
          text_element.GetParameterizedToken(current_byte));
      current_parsed_message.append(
          text_element.GetParameterizedToken(current_byte));

      if (text_element.Token == BANKToken) {
        pos = kTextData2;
      }

      continue;
    }

    // Check for special characters.
    text_element = FindMatchingSpecial(current_byte);
    if (!text_element.Empty()) {
      current_raw_message.append(text_element.GetParameterizedToken());
      current_parsed_message.append(text_element.GetParameterizedToken());
      parsed_message.push_back(current_byte);
      continue;
    }

    // Check for dictionary.
    int dictionary = FindDictionaryEntry(current_byte);
    if (dictionary >= 0) {
      current_raw_message.append("[");
      current_raw_message.append(DICTIONARYTOKEN);
      current_raw_message.append(":");
      current_raw_message.append(core::HexWord(dictionary));
      current_raw_message.append("]");

      uint32_t address = core::Get24LocalFromPC(
          rom()->mutable_data(), kPointersDictionaries + (dictionary * 2));
      uint32_t address_end = core::Get24LocalFromPC(
          rom()->mutable_data(),
          kPointersDictionaries + ((dictionary + 1) * 2));

      for (uint32_t i = address; i < address_end; i++) {
        parsed_message.push_back(rom()->data()[i]);
        current_parsed_message.append(ParseTextDataByte(rom()->data()[i]));
      }

      continue;
    }

    // Everything else.
    if (CharEncoder.contains(current_byte)) {
      std::string str = "";
      str.push_back(CharEncoder.at(current_byte));
      current_raw_message.append(str);
      current_parsed_message.append(str);
      parsed_message.push_back(current_byte);
    }
  }
}

void MessageEditor::ReadAllTextData() {
  int pos = kTextData;
  int message_id = 0;
  uint8_t current_byte;
  std::vector<uint8_t> temp_bytes_raw;
  std::vector<uint8_t> temp_bytes_parsed;

  std::string current_message_raw;
  std::string current_message_parsed;
  TextElement text_element;

  while (true) {
    current_byte = rom()->data()[pos++];

    if (current_byte == kMessageTerminator) {
      auto message =
          MessageData(message_id++, pos, current_message_raw, temp_bytes_raw,
                      current_message_parsed, temp_bytes_parsed);

      list_of_texts_.push_back(message);

      temp_bytes_raw.clear();
      temp_bytes_parsed.clear();
      current_message_raw.clear();
      current_message_parsed.clear();

      continue;
    } else if (current_byte == 0xFF) {
      break;
    }

    temp_bytes_raw.push_back(current_byte);

    // Check for command.
    text_element = FindMatchingCommand(current_byte);

    if (!text_element.Empty()) {
      temp_bytes_parsed.push_back(current_byte);
      if (text_element.HasArgument) {
        current_byte = rom()->data()[pos++];
        temp_bytes_raw.push_back(current_byte);
        temp_bytes_parsed.push_back(current_byte);
      }

      current_message_raw.append(
          text_element.GetParameterizedToken(current_byte));
      current_message_parsed.append(
          text_element.GetParameterizedToken(current_byte));

      if (text_element.Token == BANKToken) {
        pos = kTextData2;
      }

      continue;
    }

    // Check for special characters.
    text_element = FindMatchingSpecial(current_byte);
    if (!text_element.Empty()) {
      current_message_raw.append(text_element.GetParameterizedToken());
      current_message_parsed.append(text_element.GetParameterizedToken());
      temp_bytes_parsed.push_back(current_byte);
      continue;
    }

    // Check for dictionary.
    int dictionary = FindDictionaryEntry(current_byte);

    if (dictionary >= 0) {
      current_message_raw.append("[");
      current_message_raw.append(DICTIONARYTOKEN);
      current_message_raw.append(":");
      current_message_raw.append(core::HexWord(dictionary));
      current_message_raw.append("]");

      uint32_t address = core::Get24LocalFromPC(
          rom()->mutable_data(), kPointersDictionaries + (dictionary * 2));
      uint32_t address_end = core::Get24LocalFromPC(
          rom()->mutable_data(), kPointersDictionaries + ((dictionary + 1) * 2));

      for (uint32_t i = address; i < address_end; i++) {
        temp_bytes_parsed.push_back(rom()->data()[i]);
        current_message_parsed.append(ParseTextDataByte(rom()->data()[i]));
      }

      continue;
    }

    // Everything else.
    if (CharEncoder.contains(current_byte)) {
      std::string str = "";
      str.push_back(CharEncoder.at(current_byte));
      current_message_raw.append(str);
      current_message_parsed.append(str);
      temp_bytes_parsed.push_back(current_byte);
    }
  }
}

std::string ReplaceAllDictionaryWords(std::string str,
                                      std::vector<DictionaryEntry> dictionary) {
  std::string temp = str;
  for (const auto& entry : dictionary) {
    if (absl::StrContains(temp, entry.Contents)) {
      temp = absl::StrReplaceAll(temp, {{entry.Contents, entry.Contents}});
    }
  }
  return temp;
}

DictionaryEntry MessageEditor::GetDictionaryFromID(uint8_t value) {
  if (value < 0 || value >= all_dictionaries_.size()) {
    return DictionaryEntry();
  }
  return all_dictionaries_[value];
}

void MessageEditor::DrawTileToPreview(int x, int y, int srcx, int srcy, int pal,
                                      int sizex, int sizey) {
  const int num_x_tiles = 16;
  const int img_width = 512;  // (imgwidth/2)
  int draw_id = srcx + (srcy * 32);
  for (int yl = 0; yl < sizey * 8; yl++) {
    for (int xl = 0; xl < 4; xl++) {
      int mx = xl;
      int my = yl;

      // Formula information to get tile index position in the array.
      // ((ID / nbrofXtiles) * (imgwidth/2) + (ID - ((ID/16)*16) ))
      int tx = ((draw_id / num_x_tiles) * img_width) +
               ((draw_id - ((draw_id / 16) * 16)) * 4);
      uint8_t pixel = font_gfx16_data_[tx + (yl * 64) + xl];

      // nx,ny = object position, xx,yy = tile position, xl,yl = pixel
      // position
      int index = x + (y * 172) + (mx * 2) + (my * 172);
      if ((pixel & 0x0F) != 0) {
        current_font_gfx16_data_[index + 1] =
            (uint8_t)((pixel & 0x0F) + (0 * 4));
      }

      if (((pixel >> 4) & 0x0F) != 0) {
        current_font_gfx16_data_[index + 0] =
            (uint8_t)(((pixel >> 4) & 0x0F) + (0 * 4));
      }
    }
  }
}

void MessageEditor::DrawStringToPreview(std::string str) {
  for (const auto c : str) {
    DrawCharacterToPreview(c);
  }
}

void MessageEditor::DrawCharacterToPreview(char c) {
  DrawCharacterToPreview(FindMatchingCharacter(c));
}

void MessageEditor::DrawCharacterToPreview(const std::vector<uint8_t>& text) {
  for (const uint8_t& value : text) {
    if (skip_next) {
      skip_next = false;
      continue;
    }

    if (value < 100) {
      int srcy = value / 16;
      int srcx = value - (value & (~0xF));

      if (text_position_ >= 170) {
        text_position_ = 0;
        text_line_++;
      }

      DrawTileToPreview(text_position_, text_line_ * 16, srcx, srcy, 0, 1, 2);
      text_position_ += width_array[value];
    } else if (value == kLine1) {
      text_position_ = 0;
      text_line_ = 0;
    } else if (value == kScrollVertical) {
      text_position_ = 0;
      text_line_ += 1;
    } else if (value == kLine2) {
      text_position_ = 0;
      text_line_ = 1;
    } else if (value == kLine3) {
      text_position_ = 0;
      text_line_ = 2;
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
      auto dictionaryEntry = GetDictionaryFromID(value - DICTOFF);
      DrawCharacterToPreview(dictionaryEntry.Data);
    }
  }
}

void MessageEditor::DrawMessagePreview() {
  // From Parsing.
  text_line_ = 0;
  for (int i = 0; i < (172 * 4096); i++) {
    current_font_gfx16_data_[i] = 0;
  }
  text_position_ = 0;
  DrawCharacterToPreview(current_message_.Data);
  shown_lines_ = 0;
}

absl::Status MessageEditor::Cut() {
  // Ensure that text is currently selected in the text box.
  if (!message_text_box_.text.empty()) {
    // Cut the selected text in the control and paste it into the Clipboard.
    message_text_box_.Cut();
  }
  return absl::OkStatus();
}

absl::Status MessageEditor::Paste() {
  // Determine if there is any text in the Clipboard to paste into the
  if (ImGui::GetClipboardText() != nullptr) {
    // Paste the text from the Clipboard into the text box.
    message_text_box_.Paste();
  }
  return absl::OkStatus();
}

absl::Status MessageEditor::Copy() {
  // Ensure that text is selected in the text box.
  if (message_text_box_.selection_length > 0) {
    // Copy the selected text to the Clipboard.
    message_text_box_.Copy();
  }
  return absl::OkStatus();
}

absl::Status MessageEditor::Undo() {
  // Determine if last operation can be undone in text box.
  if (message_text_box_.can_undo) {
    // Undo the last operation.
    message_text_box_.Undo();

    // clear the undo buffer to prevent last action from being redone.
    message_text_box_.clearUndo();
  }
  return absl::OkStatus();
}

absl::Status MessageEditor::Save() {
  std::vector<uint8_t> backup = rom()->vector();

  for (int i = 0; i < 100; i++) {
    RETURN_IF_ERROR(rom()->Write(kCharactersWidth + i, width_array[i]));
  }

  int pos = kTextData;
  bool in_second_bank = false;

  for (const auto& message : list_of_texts_) {
    for (const auto value : message.Data) {
      RETURN_IF_ERROR(rom()->Write(pos, value));

      if (value == kBlockTerminator) {
        // Make sure we didn't go over the space available in the first block.
        // 0x7FFF available.
        if ((!in_second_bank & pos) > kTextDataEnd) {
          return absl::InternalError(DisplayTextOverflowError(pos, true));
        }

        // Switch to the second block.
        pos = kTextData2 - 1;
        in_second_bank = true;
      }

      pos++;
    }

    RETURN_IF_ERROR(
        rom()->Write(pos++, kMessageTerminator));  // , true, "Terminator text"
  }

  // Verify that we didn't go over the space available for the second block.
  // 0x14BF available.
  if ((in_second_bank & pos) > kTextData2End) {
    // rom()->data() = backup;
    return absl::InternalError(DisplayTextOverflowError(pos, false));
  }

  RETURN_IF_ERROR(rom()->Write(pos, 0xFF));  // , true, "End of text"

  return absl::OkStatus();
}

std::string MessageEditor::DisplayTextOverflowError(int pos, bool bank) {
  int space = bank ? kTextDataEnd - kTextData : kTextData2End - kTextData2;
  std::string bankSTR = bank ? "1st" : "2nd";
  std::string posSTR =
      bank ? absl::StrFormat("%X4", pos & 0xFFFF)
           : absl::StrFormat("%X4", (pos - kTextData2) & 0xFFFF);
  std::string message = absl::StrFormat(
      "There is too much text data in the %s block to save.\n"
      "Available: %X4 | Used: %s",
      bankSTR, space, posSTR);
  return message;
}

void MessageEditor::Delete() {
  // Determine if any text is selected in the TextBox control.
  if (message_text_box_.selection_length == 0) {
    // clear all of the text in the textbox.
    message_text_box_.clear();
  }
}

void MessageEditor::SelectAll() {
  // Determine if any text is selected in the TextBox control.
  if (message_text_box_.selection_length == 0) {
    // Select all text in the text box.
    message_text_box_.SelectAll();

    // Move the cursor to the text box.
    message_text_box_.Focus();
  }
}

}  // namespace editor
}  // namespace yaze
