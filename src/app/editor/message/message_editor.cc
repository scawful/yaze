#include "message_editor.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/core/platform/renderer.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/style.h"
#include "app/rom.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui_memory_editor.h"
#include "util/hex.h"

namespace yaze {
namespace editor {

using core::Renderer;

using ImGui::BeginChild;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndChild;
using ImGui::EndTable;
using ImGui::InputTextMultiline;
using ImGui::PopID;
using ImGui::PushID;
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

void MessageEditor::Initialize() {
  for (int i = 0; i < kWidthArraySize; i++) {
    width_array[i] = rom()->data()[kCharactersWidth + i];
  }

  all_dictionaries_ = BuildDictionaryEntries(rom());
  ReadAllTextData(rom(), list_of_texts_);

  font_preview_colors_.AddColor(gfx::SnesColor(0x7FFF));  // White
  font_preview_colors_.AddColor(gfx::SnesColor(0x7C00));  // Red
  font_preview_colors_.AddColor(gfx::SnesColor(0x03E0));  // Green
  font_preview_colors_.AddColor(gfx::SnesColor(0x001F));  // Blue

  for (int i = 0; i < 0x4000; i++) {
    raw_font_gfx_data_[i] = rom()->data()[kGfxFont + i];
  }
  font_gfx16_data_ =
      gfx::SnesTo8bppSheet(raw_font_gfx_data_, /*bpp=*/2, /*num_sheets=*/2);

  // 4bpp
  Renderer::Get().CreateAndRenderBitmap(
      kFontGfxMessageSize, kFontGfxMessageSize, kFontGfxMessageDepth,
      font_gfx16_data_, font_gfx_bitmap_, font_preview_colors_);

  current_font_gfx16_data_.reserve(kCurrentMessageWidth *
                                   kCurrentMessageHeight);
  std::fill(current_font_gfx16_data_.begin(), current_font_gfx16_data_.end(),
            0);

  // 8bpp
  Renderer::Get().CreateAndRenderBitmap(
      kCurrentMessageWidth, kCurrentMessageHeight, 172,
      current_font_gfx16_data_, current_font_gfx16_bitmap_,
      font_preview_colors_);

  *font_gfx_bitmap_.mutable_palette() = font_preview_colors_;
  *current_font_gfx16_bitmap_.mutable_palette() = font_preview_colors_;

  parsed_messages_ = ParseMessageData(list_of_texts_, all_dictionaries_);
  DrawMessagePreview();
}

absl::Status MessageEditor::Load() { return absl::OkStatus(); }

absl::Status MessageEditor::Update() {
  if (rom()->is_loaded() && !data_loaded_) {
    Initialize();
    current_message_ = list_of_texts_[1];
    message_text_box_.text = parsed_messages_[current_message_.ID];
    data_loaded_ = true;
  }

  if (BeginTable("##MessageEditor", 4, kMessageTableFlags)) {
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
    DrawSpecialCharacters();

    TableNextColumn();
    DrawExpandedMessageSettings();
    DrawDictionary();

    EndTable();
  }
  CLEAR_AND_RETURN_STATUS(status_);
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
        PushID(message.ID);
        if (Button(util::HexWord(message.ID).c_str())) {
          current_message_ = message;
          message_text_box_.text = parsed_messages_[message.ID];
          DrawMessagePreview();
        }
        PopID();
        TableNextColumn();
        TextWrapped("%s", parsed_messages_[message.ID].c_str());
        TableNextColumn();
        TextWrapped("%s",
                    util::HexLong(list_of_texts_[message.ID].Address).c_str());
      }

      EndTable();
    }
  }
  EndChild();
}

void MessageEditor::DrawCurrentMessage() {
  if (!rom()->is_loaded()) {
    return;
  }
  Button(absl::StrCat("Message ", current_message_.ID).c_str());
  if (InputTextMultiline("##MessageEditor", &message_text_box_.text,
                         ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    current_message_.Data = ParseMessageToData(message_text_box_.text);
    DrawMessagePreview();
  }
  Separator();

  static bool show_message_data = false;
  if (ImGui::Button("View Message Data")) {
    show_message_data = true;
  }
  if (show_message_data) {
    MemoryEditor mem_edit;
    mem_edit.DrawWindow("Message Data", current_message_.Data.data(),
                        current_message_.Data.size());
  }

  Text("Font Graphics");
  gui::BeginCanvas(font_gfx_canvas_, ImVec2(0, 130));
  font_gfx_canvas_.DrawBitmap(font_gfx_bitmap_, 0, 0);
  gui::EndCanvas(font_gfx_canvas_);
  Separator();

  ImGui::BeginChild("##MessagePreview", ImVec2(0, 0), true, 1);
  Text("Message Preview");
  if (Button("View Palette")) {
    ImGui::OpenPopup("Palette");
  }
  if (ImGui::BeginPopup("Palette")) {
    status_ = gui::DisplayPalette(font_preview_colors_, true);
    ImGui::EndPopup();
  }
  gui::BeginPadding(1);
  BeginChild("CurrentGfxFont", ImVec2(340, 0), true,
             ImGuiWindowFlags_AlwaysVerticalScrollbar);
  current_font_gfx16_canvas_.DrawBackground();
  gui::EndPadding();
  current_font_gfx16_canvas_.DrawContextMenu();

  // Handle mouse wheel scrolling
  if (ImGui::IsWindowHovered()) {
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel > 0 && shown_lines_ > 0) {
      shown_lines_--;
    } else if (wheel < 0 && shown_lines_ < text_line_ - 2) {
      shown_lines_++;
    }
  }

  // Draw only the visible portion of the text
  current_font_gfx16_canvas_.DrawBitmap(
      current_font_gfx16_bitmap_, ImVec2(0, 0),  // Destination position
      ImVec2(340,
             font_gfx_canvas_.canvas_size().y),  // Destination size
      ImVec2(0, shown_lines_ * 16),              // Source position
      ImVec2(170,
             font_gfx_canvas_.canvas_size().y / 2)  // Source size
  );

  current_font_gfx16_canvas_.DrawGrid();
  current_font_gfx16_canvas_.DrawOverlay();
  EndChild();
  ImGui::EndChild();
}

void MessageEditor::DrawTextCommands() {
  ImGui::BeginChild("##TextCommands",
                    ImVec2(0, ImGui::GetWindowContentRegionMax().y / 2), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  for (const auto& text_element : TextCommands) {
    if (Button(text_element.GenericToken.c_str())) {
      message_text_box_.text.append(text_element.GenericToken);
    }
    SameLine();
    TextWrapped("%s", text_element.Description.c_str());
    Separator();
  }
  EndChild();
}

void MessageEditor::DrawSpecialCharacters() {
  ImGui::BeginChild("##SpecialChars", ImVec2(0, 0), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  for (const auto& text_element : SpecialChars) {
    if (Button(text_element.GenericToken.c_str())) {
      message_text_box_.text.append(text_element.GenericToken);
    }
    SameLine();
    TextWrapped("%s", text_element.Description.c_str());
    Separator();
  }
  EndChild();
}

void MessageEditor::DrawExpandedMessageSettings() {
  ImGui::BeginChild("##ExpandedMessageSettings", ImVec2(0, 100), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  // Input for the address of the expanded messages
  ImGui::InputText("Address", &expanded_message_address_,
                   ImGuiInputTextFlags_CharsHexadecimal);

  if (ImGui::Button("Load Expanded Message")) {
    // Load the expanded message from the address.
    // TODO: Implement this.
  }
  EndChild();
}

void MessageEditor::DrawDictionary() {
  if (all_dictionaries_.empty()) {
    return;
  }
  if (ImGui::BeginChild("##DictionaryChild",
                        ImVec2(0, ImGui::GetWindowContentRegionMax().y / 2),
                        true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    if (BeginTable("##Dictionary", 2, kMessageTableFlags)) {
      TableSetupColumn("ID");
      TableSetupColumn("Contents");
      TableHeadersRow();
      for (const auto& dictionary : all_dictionaries_) {
        TableNextColumn();
        Text("%s", util::HexWord(dictionary.ID).c_str());
        TableNextColumn();
        Text("%s", dictionary.Contents.c_str());
      }
      EndTable();
    }
  }
  EndChild();
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
      int tx = ((draw_id / num_x_tiles) * img_width) + ((draw_id & 0xF) << 2);
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
      // int srcy = value / 16;
      int srcy = value >> 4;
      int srcx = value & 0xF;

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
      int pos = value - DICTOFF;
      if (pos < 0 || pos >= all_dictionaries_.size()) {
        // Invalid dictionary entry.
        std::cerr << "Invalid dictionary entry: " << pos << std::endl;
        continue;
      }
      auto dictionary_entry = all_dictionaries_[pos];
      DrawCharacterToPreview(dictionary_entry.Data);
    }
  }
}

void MessageEditor::DrawMessagePreview() {
  // From Parsing.
  text_line_ = 0;
  std::fill(current_font_gfx16_data_.begin(), current_font_gfx16_data_.end(),
            0);
  text_position_ = 0;
  DrawCharacterToPreview(current_message_.Data);
  shown_lines_ = 0;

  // Update the bitmap with the new data
  current_font_gfx16_bitmap_.mutable_data() = current_font_gfx16_data_;
  Renderer::Get().UpdateBitmap(&current_font_gfx16_bitmap_);
}

absl::Status MessageEditor::Save() {
  std::vector<uint8_t> backup = rom()->vector();

  for (int i = 0; i < kWidthArraySize; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(kCharactersWidth + i, width_array[i]));
  }

  int pos = kTextData;
  bool in_second_bank = false;

  for (const auto& message : list_of_texts_) {
    for (const auto value : message.Data) {
      RETURN_IF_ERROR(rom()->WriteByte(pos, value));

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

    RETURN_IF_ERROR(rom()->WriteByte(pos++, kMessageTerminator));
  }

  // Verify that we didn't go over the space available for the second block.
  // 0x14BF available.
  if ((in_second_bank & pos) > kTextData2End) {
    // TODO: Restore the backup.
    return absl::InternalError(DisplayTextOverflowError(pos, false));
  }

  RETURN_IF_ERROR(rom()->WriteByte(pos, 0xFF));

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

absl::Status MessageEditor::Redo() {
  // Implementation of redo functionality
  // This would require tracking a redo stack in the TextBox struct
  return absl::OkStatus();
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

absl::Status MessageEditor::Find() {
  if (ImGui::Begin("Find Text", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static char find_text[256] = "";
    ImGui::InputText("Search", find_text, IM_ARRAYSIZE(find_text));

    if (ImGui::Button("Find Next")) {
      search_text_ = find_text;
    }

    ImGui::SameLine();
    if (ImGui::Button("Find All")) {
      search_text_ = find_text;
    }

    ImGui::SameLine();
    if (ImGui::Button("Replace")) {
      // TODO: Implement replace functionality
    }

    ImGui::Checkbox("Case Sensitive", &case_sensitive_);
    ImGui::SameLine();
    ImGui::Checkbox("Match Whole Word", &match_whole_word_);
  }
  ImGui::End();

  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
