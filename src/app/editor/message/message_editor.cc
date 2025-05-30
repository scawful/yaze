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
#include "gui/input.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/hex.h"

namespace yaze {
namespace editor {

namespace {
std::string DisplayTextOverflowError(int pos, bool bank) {
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
}  // namespace

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
    message_preview_.width_array[i] = rom()->data()[kCharactersWidth + i];
  }

  message_preview_.all_dictionaries_ = BuildDictionaryEntries(rom());
  list_of_texts_ = ReadAllTextData(rom()->mutable_data());
  font_preview_colors_ = rom()->palette_group().hud.palette(0);

  for (int i = 0; i < 0x4000; i++) {
    raw_font_gfx_data_[i] = rom()->data()[kGfxFont + i];
  }
  message_preview_.font_gfx16_data_ =
      gfx::SnesTo8bppSheet(raw_font_gfx_data_, /*bpp=*/2, /*num_sheets=*/2);
  Renderer::Get().CreateAndRenderBitmap(
      kFontGfxMessageSize, kFontGfxMessageSize, kFontGfxMessageDepth,
      message_preview_.font_gfx16_data_, font_gfx_bitmap_,
      font_preview_colors_);
  *font_gfx_bitmap_.mutable_palette() = font_preview_colors_;
  *current_font_gfx16_bitmap_.mutable_palette() = font_preview_colors_;

  auto load_font = LoadFontGraphics(*rom());
  if (load_font.ok()) {
    message_preview_.font_gfx16_data_2_ = load_font.value().vector();
  }
  parsed_messages_ =
      ParseMessageData(list_of_texts_, message_preview_.all_dictionaries_);
  current_message_ = list_of_texts_[1];
  message_text_box_.text = parsed_messages_[current_message_.ID];
  DrawMessagePreview();
}

absl::Status MessageEditor::Load() { return absl::OkStatus(); }

absl::Status MessageEditor::Update() {
  if (BeginTable("##MessageEditor", 4, kMessageTableFlags)) {
    TableSetupColumn("List");
    TableSetupColumn("Contents");
    TableSetupColumn("Font Atlas");
    TableSetupColumn("Commands");
    TableHeadersRow();

    TableNextColumn();
    DrawMessageList();

    TableNextColumn();
    DrawCurrentMessage();

    TableNextColumn();
    DrawFontAtlas();
    DrawExpandedMessageSettings();

    TableNextColumn();
    DrawTextCommands();
    DrawSpecialCharacters();
    DrawDictionary();

    EndTable();
  }
  return absl::OkStatus();
}

void MessageEditor::DrawMessageList() {
  gui::BeginNoPadding();
  if (BeginChild("##MessagesList", ImVec2(0, 0), true,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    gui::EndNoPadding();
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
      for (const auto& expanded_message : expanded_messages_) {
        TableNextColumn();
        PushID(expanded_message.ID + 0x18D);
        if (Button(util::HexWord(expanded_message.ID + 0x18D).c_str())) {
          current_message_ = expanded_message;
          message_text_box_.text =
              parsed_messages_[expanded_message.ID + 0x18D];
          DrawMessagePreview();
        }
        PopID();
        TableNextColumn();
        TextWrapped("%s",
                    parsed_messages_[expanded_message.ID + 0x18C].c_str());
        TableNextColumn();
        TextWrapped("%s", util::HexLong(expanded_message.Address).c_str());
      }

      EndTable();
    }
  }
  EndChild();
}

void MessageEditor::DrawCurrentMessage() {
  Button(absl::StrCat("Message ", current_message_.ID).c_str());
  if (InputTextMultiline("##MessageEditor", &message_text_box_.text,
                         ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
    std::string temp = message_text_box_.text;
    // Strip newline characters.
    temp.erase(std::remove(temp.begin(), temp.end(), '\n'), temp.end());
    current_message_.Data = ParseMessageToData(temp);
    DrawMessagePreview();
  }
  Separator();
  gui::MemoryEditorPopup("Message Data", current_message_.Data);

  ImGui::BeginChild("##MessagePreview", ImVec2(0, 0), true, 1);
  Text("Message Preview");
  if (Button("View Palette")) {
    ImGui::OpenPopup("Palette");
  }
  if (ImGui::BeginPopup("Palette")) {
    gui::DisplayPalette(font_preview_colors_, true);
    ImGui::EndPopup();
  }
  gui::BeginPadding(1);
  BeginChild("CurrentGfxFont", ImVec2(348, 0), true,
             ImGuiWindowFlags_NoScrollWithMouse);
  current_font_gfx16_canvas_.DrawBackground();
  gui::EndPadding();
  current_font_gfx16_canvas_.DrawContextMenu();

  // Handle mouse wheel scrolling
  if (ImGui::IsWindowHovered()) {
    float wheel = ImGui::GetIO().MouseWheel;
    if (wheel > 0 && message_preview_.shown_lines > 0) {
      message_preview_.shown_lines--;
    } else if (wheel < 0 &&
               message_preview_.shown_lines < message_preview_.text_line - 2) {
      message_preview_.shown_lines++;
    }
  }

  // Draw only the visible portion of the text
  current_font_gfx16_canvas_.DrawBitmap(
      current_font_gfx16_bitmap_, ImVec2(0, 0),  // Destination position
      ImVec2(340,
             font_gfx_canvas_.canvas_size().y),      // Destination size
      ImVec2(0, message_preview_.shown_lines * 16),  // Source position
      ImVec2(170,
             font_gfx_canvas_.canvas_size().y / 2)  // Source size
  );

  current_font_gfx16_canvas_.DrawGrid();
  current_font_gfx16_canvas_.DrawOverlay();
  EndChild();
  ImGui::EndChild();
}

void MessageEditor::DrawFontAtlas() {
  gui::BeginCanvas(font_gfx_canvas_, ImVec2(256, 256));
  font_gfx_canvas_.DrawBitmap(font_gfx_bitmap_, 0, 0, 2.0f);
  font_gfx_canvas_.DrawTileSelector(16, 32);
  gui::EndCanvas(font_gfx_canvas_);
}

void MessageEditor::DrawExpandedMessageSettings() {
  ImGui::BeginChild("##ExpandedMessageSettings", ImVec2(0, 100), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::Text("Expanded Messages");
  static std::string expanded_message_path = "";
  if (ImGui::Button("Load Expanded Message")) {
    expanded_message_path = core::FileDialogWrapper::ShowOpenFileDialog();
    if (!expanded_message_path.empty()) {
      // Load the expanded message from the path.
      static Rom expanded_message_rom;
      if (!expanded_message_rom.LoadFromFile(expanded_message_path, false)
               .ok()) {
        context_->popup_manager->Show("Error");
      }
      expanded_messages_ =
          ReadAllTextData(expanded_message_rom.mutable_data(), 0);
      auto parsed_expanded_messages = ParseMessageData(
          expanded_messages_, message_preview_.all_dictionaries_);
      // Insert into parsed_messages
      for (const auto& expanded_message : expanded_messages_) {
        parsed_messages_.push_back(
            parsed_expanded_messages[expanded_message.ID]);
      }
    }
  }

  if (expanded_messages_.size() > 0) {
    ImGui::Text("Expanded Path: %s", expanded_message_path.c_str());
    ImGui::Text("Expanded Messages: %lu", expanded_messages_.size());
    if (ImGui::Button("Add New Message")) {
      MessageData new_message;
      new_message.ID = expanded_messages_.back().ID + 1;
      new_message.Address = expanded_messages_.back().Address +
                            expanded_messages_.back().Data.size();
      expanded_messages_.push_back(new_message);
    }
    if (ImGui::Button("Save Expanded Messages")) {
      PRINT_IF_ERROR(SaveExpandedMessages());
    }
  }

  EndChild();
}

void MessageEditor::DrawTextCommands() {
  ImGui::BeginChild("##TextCommands",
                    ImVec2(0, ImGui::GetContentRegionAvail().y / 2), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  static uint8_t command_parameter = 0;
  gui::InputHexByte("Command Parameter", &command_parameter);
  for (const auto& text_element : TextCommands) {
    if (Button(text_element.GenericToken.c_str())) {
      message_text_box_.text.append(
          text_element.GetParamToken(command_parameter));
    }
    SameLine();
    TextWrapped("%s", text_element.Description.c_str());
    Separator();
  }
  EndChild();
}

void MessageEditor::DrawSpecialCharacters() {
  ImGui::BeginChild("##SpecialChars",
                    ImVec2(0, ImGui::GetContentRegionAvail().y / 2), true,
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

void MessageEditor::DrawDictionary() {
  if (ImGui::BeginChild("##DictionaryChild",
                        ImVec2(0, ImGui::GetContentRegionAvail().y), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    if (BeginTable("##Dictionary", 2, kMessageTableFlags)) {
      TableSetupColumn("ID");
      TableSetupColumn("Contents");
      TableHeadersRow();
      for (const auto& dictionary : message_preview_.all_dictionaries_) {
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

void MessageEditor::DrawMessagePreview() {
  message_preview_.DrawMessagePreview(current_message_);
  if (current_font_gfx16_bitmap_.is_active()) {
    current_font_gfx16_bitmap_.mutable_data() =
        message_preview_.current_preview_data_;
    Renderer::Get().UpdateBitmap(&current_font_gfx16_bitmap_);
  } else {
    Renderer::Get().CreateAndRenderBitmap(
        kCurrentMessageWidth, kCurrentMessageHeight, 172,
        message_preview_.current_preview_data_, current_font_gfx16_bitmap_,
        font_preview_colors_);
  }
}

absl::Status MessageEditor::Save() {
  std::vector<uint8_t> backup = rom()->vector();

  for (int i = 0; i < kWidthArraySize; i++) {
    RETURN_IF_ERROR(rom()->WriteByte(kCharactersWidth + i,
                                     message_preview_.width_array[i]));
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
    std::copy(backup.begin(), backup.end(), rom()->mutable_data());
    return absl::InternalError(DisplayTextOverflowError(pos, false));
  }

  RETURN_IF_ERROR(rom()->WriteByte(pos, 0xFF));

  return absl::OkStatus();
}

absl::Status MessageEditor::SaveExpandedMessages() {
  for (const auto& expanded_message : expanded_messages_) {
    std::copy(expanded_message.Data.begin(), expanded_message.Data.end(),
              expanded_message_bin_.mutable_data() + expanded_message.Address);
  }
  RETURN_IF_ERROR(expanded_message_bin_.WriteByte(
      expanded_messages_.back().Address + expanded_messages_.back().Data.size(),
      0xFF));
  RETURN_IF_ERROR(expanded_message_bin_.SaveToFile(
      Rom::SaveSettings{.backup = true, .save_new = false, .z3_save = false}));
  return absl::OkStatus();
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
