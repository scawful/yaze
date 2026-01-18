#include "message_editor.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/system/panel_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style.h"
#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "util/log.h"

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
  // Register panels with PanelManager (dependency injection)
  if (!dependencies_.panel_manager)
    return;

  auto* panel_manager = dependencies_.panel_manager;
  const size_t session_id = dependencies_.session_id;

  // Register EditorPanel implementations (they provide both metadata and drawing)
  panel_manager->RegisterEditorPanel(
      std::make_unique<MessageListPanel>([this]() { DrawMessageList(); }));
  panel_manager->RegisterEditorPanel(
      std::make_unique<MessageEditorPanel>([this]() { DrawCurrentMessage(); }));
  panel_manager->RegisterEditorPanel(std::make_unique<FontAtlasPanel>([this]() {
    DrawFontAtlas();
    DrawExpandedMessageSettings();
  }));
  panel_manager->RegisterEditorPanel(
      std::make_unique<DictionaryPanel>([this]() {
        DrawTextCommands();
        DrawSpecialCharacters();
        DrawDictionary();
      }));

  // Show message list by default
  panel_manager->ShowPanel(session_id, "message.message_list");

  for (int i = 0; i < kWidthArraySize; i++) {
    message_preview_.width_array[i] = rom()->data()[kCharactersWidth + i];
  }

  message_preview_.all_dictionaries_ = BuildDictionaryEntries(rom());
  list_of_texts_ = ReadAllTextData(rom()->mutable_data());
  LOG_INFO("MessageEditor", "Loaded %zu messages from ROM",
           list_of_texts_.size());

  if (game_data()) {
    font_preview_colors_ = game_data()->palette_groups.hud.palette(0);
  }

  for (int i = 0; i < 0x4000; i++) {
    raw_font_gfx_data_[i] = rom()->data()[kGfxFont + i];
  }
  message_preview_.font_gfx16_data_ =
      gfx::SnesTo8bppSheet(raw_font_gfx_data_, /*bpp=*/2, /*num_sheets=*/2);

  // Create bitmap for font graphics
  font_gfx_bitmap_.Create(kFontGfxMessageSize, kFontGfxMessageSize,
                          kFontGfxMessageDepth,
                          message_preview_.font_gfx16_data_);
  font_gfx_bitmap_.SetPalette(font_preview_colors_);

  // Queue texture creation - will be processed in render loop
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &font_gfx_bitmap_);

  LOG_INFO("MessageEditor", "Font bitmap created and texture queued");
  *current_font_gfx16_bitmap_.mutable_palette() = font_preview_colors_;

  auto load_font = zelda3::LoadFontGraphics(*rom());
  if (load_font.ok()) {
    message_preview_.font_gfx16_data_2_ = load_font.value().vector();
  }
  parsed_messages_ =
      ParseMessageData(list_of_texts_, message_preview_.all_dictionaries_);
  expanded_message_base_id_ = static_cast<int>(list_of_texts_.size());

  if (!list_of_texts_.empty()) {
    // Default to message 1 if available, otherwise 0
    size_t default_idx = list_of_texts_.size() > 1 ? 1 : 0;
    current_message_ = list_of_texts_[default_idx];
    current_message_index_ = current_message_.ID;
    current_message_is_expanded_ = false;
    message_text_box_.text = parsed_messages_[current_message_.ID];
    DrawMessagePreview();
  } else {
    LOG_ERROR("MessageEditor", "No messages found in ROM!");
  }
}

absl::Status MessageEditor::Load() {
  gfx::ScopedTimer timer("MessageEditor::Load");
  return absl::OkStatus();
}

absl::Status MessageEditor::Update() {
  // Panel drawing is handled centrally by PanelManager::DrawAllVisiblePanels()
  // via the EditorPanel implementations registered in Initialize().
  // No local drawing needed here.
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
          current_message_index_ = message.ID;
          current_message_is_expanded_ = false;
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
      const int expanded_base = expanded_message_base_id_;
      for (const auto& expanded_message : expanded_messages_) {
        const int display_id = expanded_base + expanded_message.ID;
        const char* display_text = "Missing text";
        if (display_id >= 0 &&
            display_id < static_cast<int>(parsed_messages_.size())) {
          display_text = parsed_messages_[display_id].c_str();
        }
        TableNextColumn();
        PushID(display_id);
        if (Button(util::HexWord(display_id).c_str())) {
          current_message_ = expanded_message;
          current_message_index_ = expanded_message.ID;
          current_message_is_expanded_ = true;
          message_text_box_.text = display_text;
          DrawMessagePreview();
        }
        PopID();
        TableNextColumn();
        TextWrapped("%s", display_text);
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
    UpdateCurrentMessageFromText(message_text_box_.text);
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

  if (ImGui::Button("Load Expanded Message")) {
    std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty()) {
      expanded_message_path_ = path;
      expanded_message_base_id_ = static_cast<int>(list_of_texts_.size());
      if (parsed_messages_.size() >
          static_cast<size_t>(expanded_message_base_id_)) {
        parsed_messages_.resize(expanded_message_base_id_);
      }
      expanded_messages_.clear();
      if (!LoadExpandedMessages(expanded_message_path_, parsed_messages_,
                                expanded_messages_,
                                message_preview_.all_dictionaries_)
               .ok()) {
        if (auto* popup_manager = dependencies_.popup_manager) {
          popup_manager->Show("Error");
        }
      }
    }
  }

  if (expanded_messages_.size() > 0) {
    ImGui::Text("Expanded Path: %s", expanded_message_path_.c_str());
    ImGui::Text("Expanded Messages: %lu", expanded_messages_.size());
    if (ImGui::Button("Add New Message")) {
      MessageData new_message;
      new_message.ID = expanded_messages_.back().ID + 1;
      new_message.Address = expanded_messages_.back().Address +
                            expanded_messages_.back().Data.size();
      expanded_messages_.push_back(new_message);
      const int display_id = expanded_message_base_id_ + new_message.ID;
      if (display_id >= 0 &&
          static_cast<size_t>(display_id) >= parsed_messages_.size()) {
        parsed_messages_.resize(display_id + 1);
      }
    }

    if (ImGui::Button("Save Expanded Messages")) {
      if (expanded_message_path_.empty()) {
        expanded_message_path_ = util::FileDialogWrapper::ShowSaveFileDialog();
      }
      if (!expanded_message_path_.empty()) {
        PRINT_IF_ERROR(SaveExpandedMessages());
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save As...")) {
      std::string path = util::FileDialogWrapper::ShowSaveFileDialog();
      if (!path.empty()) {
        expanded_message_path_ = path;
        PRINT_IF_ERROR(SaveExpandedMessages());
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Export to JSON")) {
      std::string path = util::FileDialogWrapper::ShowSaveFileDialog();
      if (!path.empty()) {
        PRINT_IF_ERROR(ExportMessagesToJson(path, expanded_messages_));
      }
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
      UpdateCurrentMessageFromText(message_text_box_.text);
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
      UpdateCurrentMessageFromText(message_text_box_.text);
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

void MessageEditor::UpdateCurrentMessageFromText(const std::string& text) {
  std::string raw_text = text;
  raw_text.erase(std::remove(raw_text.begin(), raw_text.end(), '\n'),
                 raw_text.end());

  current_message_.RawString = raw_text;
  current_message_.ContentsParsed = text;
  current_message_.Data = ParseMessageToData(raw_text);

  int parsed_index = current_message_index_;
  if (current_message_is_expanded_) {
    parsed_index = expanded_message_base_id_ + current_message_index_;
  }

  if (parsed_index >= 0) {
    if (static_cast<size_t>(parsed_index) >= parsed_messages_.size()) {
      parsed_messages_.resize(parsed_index + 1);
    }
    parsed_messages_[parsed_index] = text;
  }

  if (current_message_is_expanded_) {
    if (current_message_index_ >= 0 &&
        current_message_index_ <
            static_cast<int>(expanded_messages_.size())) {
      expanded_messages_[current_message_index_] = current_message_;
    }
  } else {
    if (current_message_index_ >= 0 &&
        current_message_index_ < static_cast<int>(list_of_texts_.size())) {
      list_of_texts_[current_message_index_] = current_message_;
    }
  }

  DrawMessagePreview();
}

void MessageEditor::DrawMessagePreview() {
  // Render the message to the preview bitmap
  message_preview_.DrawMessagePreview(current_message_);

  // Validate preview data before updating
  if (message_preview_.current_preview_data_.empty()) {
    LOG_WARN("MessageEditor", "Preview data is empty, skipping bitmap update");
    return;
  }

  if (current_font_gfx16_bitmap_.is_active()) {
    // CRITICAL: Use set_data() to properly update both data_ AND surface_
    // mutable_data() returns a reference but doesn't update the surface!
    current_font_gfx16_bitmap_.set_data(message_preview_.current_preview_data_);

    // Validate surface was updated
    if (!current_font_gfx16_bitmap_.surface()) {
      LOG_ERROR("MessageEditor", "Bitmap surface is null after set_data()");
      return;
    }

    // Queue texture update so changes are visible immediately
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &current_font_gfx16_bitmap_);

    LOG_DEBUG(
        "MessageEditor",
        "Updated message preview bitmap (size: %zu) and queued texture update",
        message_preview_.current_preview_data_.size());
  } else {
    // Create bitmap and queue texture creation with 8-bit indexed depth
    current_font_gfx16_bitmap_.Create(kCurrentMessageWidth,
                                      kCurrentMessageHeight, 8,
                                      message_preview_.current_preview_data_);
    current_font_gfx16_bitmap_.SetPalette(font_preview_colors_);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &current_font_gfx16_bitmap_);

    LOG_INFO("MessageEditor",
             "Created message preview bitmap (%dx%d) with 8-bit depth and "
             "queued texture creation",
             kCurrentMessageWidth, kCurrentMessageHeight);
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
  if (expanded_message_path_.empty()) {
    return absl::InvalidArgumentError(
        "No path specified for expanded messages");
  }

  // Ensure the ROM object is loaded/initialized if needed, or just use it as a buffer
  // The original code used expanded_message_bin_ which wasn't clearly initialized in this scope
  // except potentially in LoadExpandedMessages via a static local?
  // Wait, LoadExpandedMessages used a static local Rom.
  // We need to ensure expanded_message_bin_ member is populated or we load it.

  if (!expanded_message_bin_.is_loaded()) {
    // Try to load from the path if it exists, otherwise create new?
    // For now, let's assume we are overwriting or updating.
    // If we are just writing raw data, maybe we don't need a full ROM load if we just write bytes?
    // But SaveToFile expects a loaded ROM structure.
    // Let's try to load it first.
    auto status = expanded_message_bin_.LoadFromFile(expanded_message_path_);
    if (!status.ok()) {
      // If file doesn't exist, maybe we should create a buffer?
      // For now, let's propagate error if we can't load it to update it.
      // Or if it's a new file, we might need to handle that.
      // Let's assume for this task we are updating an existing BIN or creating one.
      // If creating, we might need to initialize expanded_message_bin_ with enough size.
      // Let's just try to load, and if it fails (e.g. new file), initialize empty.
      expanded_message_bin_.Expand(0x200000);  // Default 2MB? Or just enough?
    }
  }

  for (const auto& expanded_message : expanded_messages_) {
    // Ensure vector is large enough
    if (expanded_message.Address + expanded_message.Data.size() >
        expanded_message_bin_.size()) {
      expanded_message_bin_.Expand(expanded_message.Address +
                                   expanded_message.Data.size() + 0x1000);
    }
    std::copy(expanded_message.Data.begin(), expanded_message.Data.end(),
              expanded_message_bin_.mutable_data() + expanded_message.Address);
  }

  // Write terminator
  if (!expanded_messages_.empty()) {
    size_t end_pos = expanded_messages_.back().Address +
                     expanded_messages_.back().Data.size();
    if (end_pos < expanded_message_bin_.size()) {
      expanded_message_bin_.WriteByte(end_pos, 0xFF);
    }
  }

  expanded_message_bin_.set_filename(expanded_message_path_);
  RETURN_IF_ERROR(expanded_message_bin_.SaveToFile(
      Rom::SaveSettings{.backup = true, .save_new = false}));
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
