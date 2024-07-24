#include "message_editor.h"

#include <absl/status/status.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>

#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/core/common.h"
#include "app/editor/utils/editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::Begin;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::End;
using ImGui::EndChild;
using ImGui::EndTable;
using ImGui::InputText;
using ImGui::SameLine;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;
using ImGui::TextWrapped;
using ImGui::TreeNode;

static ParsedElement FindMatchingElement(string str) {
  std::smatch match;
  for (auto& textElement : TextCommands) {
    match = textElement.MatchMe(str);
    if (match.size() > 0) {
      if (textElement.HasArgument) {
        return ParsedElement(textElement,
                             std::stoi(match[1].str(), nullptr, 16));
      } else {
        return ParsedElement(textElement, 0);
      }
    }
  }

  match = DictionaryElement.MatchMe(str);
  if (match.size() > 0) {
    return ParsedElement(DictionaryElement,
                         DICTOFF + std::stoi(match[1].str(), nullptr, 16));
  }
  return ParsedElement();
}

static string ReplaceAllDictionaryWords(string str) {
  string temp = str;
  for (const auto& entry : AllDictionaries) {
    if (absl::StrContains(temp, entry.Contents)) {
      temp = absl::StrReplaceAll(temp, {{entry.Contents, entry.Contents}});
    }
  }

  return temp;
}

static std::vector<uint8_t> ParseMessageToData(string str) {
  std::vector<uint8_t> bytes;
  string tempString = str;
  int pos = 0;

  while (pos < tempString.size()) {
    // Get next text fragment.
    if (tempString[pos] == '[') {
      int next = tempString.find(']', pos);
      if (next == -1) {
        break;
      }

      ParsedElement parsedElement =
          FindMatchingElement(tempString.substr(pos, next - pos + 1));
      if (!parsedElement.Active) {
        break;  // TODO: handle badness.
        // } else if (parsedElement.Parent == DictionaryElement) {
        //   bytes.push_back(parsedElement.Value);
      } else {
        bytes.push_back(parsedElement.Parent.ID);

        if (parsedElement.Parent.HasArgument) {
          bytes.push_back(parsedElement.Value);
        }
      }

      pos = next + 1;
      continue;
    } else {
      uint8_t bb = MessageEditor::FindMatchingCharacter(tempString[pos++]);

      if (bb != 0xFF) {
        // TODO: handle badness.
        bytes.push_back(bb);
      }
    }
  }

  return bytes;
}

absl::Status MessageEditor::Update() {
  if (rom()->is_loaded() && !data_loaded_) {
    RETURN_IF_ERROR(rom()->LoadFontGraphicsData())
    RETURN_IF_ERROR(Initialize());
    data_loaded_ = true;
  }

  if (BeginTable("##MessageEditor", 2,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
    TableSetupColumn("##ListOfMessages");
    TableSetupColumn("##MessageEditorColumn2");

    TableNextColumn();
    DrawMessageList();

    TableNextColumn();
    DrawCurrentMessage();

    EndTable();
  }

  return absl::OkStatus();
}

void MessageEditor::DrawMessageList() {
  for (const auto& message : ListOfTexts) {
    TextWrapped("%s", absl::StrCat("Message ", message.RawString).c_str());
    SameLine();
    if (Button(absl::StrCat("Message ", message.ID).c_str())) {
      SelectMessageID(message.ID);
    }
  }
}

void MessageEditor::DrawCurrentMessage() {
  TextWrapped("%s", absl::StrCat("Message ", CurrentMessage.RawString).c_str());
  SameLine();
  if (Button(absl::StrCat("Message ", CurrentMessage.ID).c_str())) {
    SelectMessageID(CurrentMessage.ID);
  }
  Separator();
  ImGui::BeginChild("MessageEditorCanvas", ImVec2(0, 0), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  font_gfx_canvas_.DrawBackground();
  font_gfx_canvas_.DrawContextMenu();
  font_gfx_canvas_.DrawBitmap(font_gfx_bitmap_, 0, 0);
  font_gfx_canvas_.DrawGrid();
  font_gfx_canvas_.DrawOverlay();
  EndChild();
  Separator();
  ImGui::BeginChild("CurrentGfxFont", ImVec2(0, 0), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  current_font_gfx16_canvas_.DrawBackground();
  current_font_gfx16_canvas_.DrawContextMenu();
  current_font_gfx16_canvas_.DrawBitmap(current_font_gfx16_bitmap_, 0, 0);
  current_font_gfx16_canvas_.DrawGrid();
  current_font_gfx16_canvas_.DrawOverlay();
  EndChild();
  Separator();
}

absl::Status MessageEditor::Initialize() {
  for (int i = 0; i < 100; i++) {
    widthArray[i] = rom()->data()[kCharactersWidth + i];
  }

  previewColors.AddColor(0x7FFF);  // White
  previewColors.AddColor(0x7C00);  // Red
  previewColors.AddColor(0x03E0);  // Green
  previewColors.AddColor(0x001F);  // Blue

  fontgfx16Ptr = rom()->font_gfx_data();

  // 4bpp
  font_gfx_bitmap_.Create(128, 128, 64, gfx::kFormat4bppIndexed, fontgfx16Ptr);
  RETURN_IF_ERROR(rom()->CreateAndRenderBitmap(128, 128, 64, fontgfx16Ptr,
                                               font_gfx_bitmap_, previewColors))

  currentfontgfx16Ptr.reserve(172 * 4096);
  for (int i = 0; i < 172 * 4096; i++) {
    currentfontgfx16Ptr.push_back(0);
  }

  // 8bpp
  current_font_gfx16_bitmap_.Create(172, 4096, 172, gfx::kFormat8bppIndexed,
                                    currentfontgfx16Ptr);
  RETURN_IF_ERROR(
      rom()->CreateAndRenderBitmap(172, 4096, 172, currentfontgfx16Ptr,
                                   current_font_gfx16_bitmap_, previewColors))

  // auto previewColors = new Color[]{
  //     Color.DimGray,
  //     Color.DarkBlue,
  //     Color.White,
  //     Color.DarkOrange,
  // };

  gfx::SnesPalette color_palette = font_gfx_bitmap_.palette();
  for (int i = 0; i < previewColors.size(); i++) {
    *color_palette.mutable_color(i) = previewColors[i];
  }

  *font_gfx_bitmap_.mutable_palette() = color_palette;

  BuildDictionaryEntries();
  ReadAllTextData();

  // foreach (MessageData messageData in
  // ListOfTexts) {
  //   DisplayedMessages.push_back(messageData);
  // }

  // textListbox.BeginUpdate();
  // textListbox.DataSource =
  // DisplayedMessages;
  // textListbox.EndUpdate();

  // textListbox.DisplayMember = "Text";
  // pictureBox2.Refresh();

  // SelectedTileID.Text =
  // selected_tile.ToString("X2");
  // SelectedTileASCII.Text =
  // ParseTextDataByte((uint8_t)selected_tile);

  // CreateFontGfxData(rom()->data());
  return absl::OkStatus();
}

void MessageEditor::BuildDictionaryEntries() {
  for (int i = 0; i < 97; i++) {
    std::vector<uint8_t> bytes;
    std::stringstream stringBuilder;

    int address = core::SnesToPc(
        0x0E0000 + (rom()->data()[kPointersDictionaries + (i * 2) + 1] << 8) +
        rom()->data()[kPointersDictionaries + (i * 2)]);

    int temppush_backress = core::SnesToPc(
        0x0E0000 +
        (rom()->data()[kPointersDictionaries + ((i + 1) * 2) + 1] << 8) +
        rom()->data()[kPointersDictionaries + ((i + 1) * 2)]);

    while (address < temppush_backress) {
      uint8_t uint8_tDictionary = rom()->data()[address++];
      bytes.push_back(uint8_tDictionary);
      stringBuilder << ParseTextDataByte(uint8_tDictionary);
    }

    // AllDictionaries[i] = DictionaryEntry{(uint8_t)i, stringBuilder.str()};
    AllDictionaries.push_back(DictionaryEntry{(uint8_t)i, stringBuilder.str()});
  }

  // AllDictionaries.OrderByDescending(dictionary = > dictionary.Length);
  AllDictionaries[0].Length = 0;
}

void MessageEditor::ReadAllTextData() {
  int messageID = 0;
  uint8_t value;
  int pos = kTextData;
  std::vector<uint8_t> temp_bytes_raw;
  std::vector<uint8_t> temp_bytes_parsed;

  std::string current_message_raw;
  std::string current_message_parsed;
  TextElement textElement;

  while (true) {
    value = rom()->data()[pos++];

    if (value == MESSAGETERMINATOR) {
      auto message =
          MessageData(messageID++, pos, current_message_raw, temp_bytes_raw,
                      current_message_parsed, temp_bytes_parsed);

      ListOfTexts.push_back(message);

      temp_bytes_raw.clear();
      temp_bytes_parsed.clear();
      current_message_raw.clear();
      current_message_parsed.clear();

      continue;
    } else if (value == 0xFF) {
      break;
    }

    temp_bytes_raw.push_back(value);

    // Check for command.
    textElement = FindMatchingCommand(value);

    if (!textElement.Empty()) {
      temp_bytes_parsed.push_back(value);
      if (textElement.HasArgument) {
        value = rom()->data()[pos++];
        temp_bytes_raw.push_back(value);
        temp_bytes_parsed.push_back(value);
      }

      current_message_raw.append(textElement.GetParameterizedToken(value));
      current_message_parsed.append(textElement.GetParameterizedToken(value));

      if (textElement.Token == BANKToken) {
        pos = kTextData2;
      }

      continue;
    }

    // Check for special characters.
    textElement = FindMatchingSpecial(value);

    if (!textElement.Empty()) {
      current_message_raw.append(textElement.GetParameterizedToken());
      current_message_parsed.append(textElement.GetParameterizedToken());
      temp_bytes_parsed.push_back(value);
      continue;
    }

    // Check for dictionary.
    int dictionary = FindDictionaryEntry(value);

    if (dictionary >= 0) {
      current_message_raw.append("[");
      current_message_raw.append(DICTIONARYTOKEN);
      current_message_raw.append(":");
      current_message_raw.append(core::UppercaseHexWord(dictionary));
      current_message_raw.append("]");

      int address = core::Get24LocalFromPC(
          rom()->data(), kPointersDictionaries + (dictionary * 2));
      int addressEnd = core::Get24LocalFromPC(
          rom()->data(), kPointersDictionaries + ((dictionary + 1) * 2));

      for (int i = address; i < addressEnd; i++) {
        temp_bytes_parsed.push_back(rom()->data()[i]);
        current_message_parsed.append(ParseTextDataByte(rom()->data()[i]));
      }

      continue;
    }

    // Everything else.
    if (CharEncoder.contains(value)) {
      std::string str = "";
      str.push_back(CharEncoder.at(value));
      current_message_raw.append(str);
      current_message_parsed.append(str);
      temp_bytes_parsed.push_back(value);
    }
  }

  // 00074703
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
    RETURN_IF_ERROR(rom()->Write(kCharactersWidth + i, widthArray[i]));
  }

  int pos = kTextData;
  bool inSecondBank = false;

  for (const auto& message : ListOfTexts) {
    for (const auto value : message.Data) {
      RETURN_IF_ERROR(rom()->Write(pos, value));

      // TODO: 0x80 somehow means the end of the first block. Need to ask Zarby
      // for clarification as to why this is the case. Check for the end of the
      // first block.
      if (value == 0x80) {
        // Make sure we didn't go over the space available in the first block.
        // 0x7FFF available.
        if ((!inSecondBank & pos) > kTextDataEnd) {
          DisplayTextOverflowError(pos, true);
          // *rom()->data() = backup.data();
          // rom()->data() = (uint8_t[])backup.Clone();
          return absl::InternalError(
              "Too much text data in the first block to save.");
        }

        // Switch to the second block.
        pos = kTextData2 - 1;
        inSecondBank = true;
      }

      pos++;
    }

    RETURN_IF_ERROR(
        rom()->Write(pos++, MESSAGETERMINATOR));  // , true, "Terminator text"
  }

  // Verify that we didn't go over the space available for the second block.
  // 0x14BF available.
  if ((inSecondBank & pos) > kTextData2End) {
    DisplayTextOverflowError(pos, false);
    // rom()->data() = backup;
    // return true;
    return absl::InternalError(
        "Too much text data in the second block to save.");
  }

  RETURN_IF_ERROR(rom()->Write(pos, 0xFF));  // , true, "End of text"

  return absl::OkStatus();
}

TextElement MessageEditor::FindMatchingCommand(uint8_t b) {
  TextElement empty_element;
  for (const auto text_element : TextCommands) {
    if (text_element.ID == b) {
      return text_element;
    }
  }

  return empty_element;
}

TextElement MessageEditor::FindMatchingSpecial(uint8_t value) {
  TextElement empty_element;
  for (const auto text_element : SpecialChars) {
    if (text_element.ID == value) {
      return text_element;
    }
  }

  return empty_element;
}

MessageEditor::DictionaryEntry MessageEditor::GetDictionaryFromID(
    uint8_t value) {
  // return AllDictionaries.First(dictionary = > dictionary.ID == value);
  return AllDictionaries[0];
}

uint8_t MessageEditor::FindDictionaryEntry(uint8_t value) {
  if (value < DICTOFF || value == 0xFF) {
    return -1;
  }

  return value - DICTOFF;
}

uint8_t MessageEditor::FindMatchingCharacter(char value) {
  for (const auto [key, char_value] : CharEncoder) {
    if (value == char_value) {
      return key;
    }
  }
  return 0xFF;
}

bool MessageEditor::SelectMessageID(int id) {
  // if (id < textListbox.Items.size()) {
  //   textListbox.SelectedIndex = id;
  //   return true;
  // }
  return false;
}

string MessageEditor::ParseTextDataByte(uint8_t value) {
  if (CharEncoder.contains(value)) {
    char c = CharEncoder.at(value);
    string str = "";
    str.push_back(c);
    return str;
  }

  // Check for command.
  TextElement textElement = FindMatchingCommand(value);
  if (!textElement.Empty()) {
    return textElement.GenericToken;
  }

  // Check for special characters.
  textElement = FindMatchingSpecial(value);
  if (!textElement.Empty()) {
    return textElement.GenericToken;
  }

  // Check for dictionary.
  int dictionary = FindDictionaryEntry(value);
  if (dictionary >= 0) {
    return absl::StrFormat("[%s:%X]", DICTIONARYTOKEN, dictionary);
  }

  return "";
}

void MessageEditor::DrawStringToPreview(string str) {
  for (const auto c : str) {
    DrawCharacterToPreview(c);
  }
}

void MessageEditor::DrawCharacterToPreview(char c) {
  DrawCharacterToPreview(FindMatchingCharacter(c));
}

void MessageEditor::DrawCharacterToPreview(std::vector<uint8_t> text) {
  for (const auto value : text) {
    if (skip_next) {
      skip_next = false;
      continue;
    }

    if (value < 100) {
      int srcy = value / 16;
      int srcx = value - (value & (~0xF));

      if (text_pos >= 170) {
        text_pos = 0;
        text_line++;
      }

      DrawTileToPreview(text_pos, text_line * 16, srcx, srcy, 0, false, false,
                        1, 2);
      text_pos += widthArray[value];
    } else if (value == 0x74) {
      text_pos = 0;
      text_line = 0;
    } else if (value == 0x73) {
      text_pos = 0;
      text_line += 1;
    } else if (value == 0x75) {
      text_pos = 0;
      text_line = 1;
    } else if (value == 0x76) {
      text_pos = 0;
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
      // DictionaryEntry dictionaryEntry =
      //     GetDictionaryFromID((uint8_t)(value - DICTOFF));
      // auto dictionaryEntry = GetDictionaryFromID(value - DICTOFF);
      // if (dictionaryEntry != null) {
      //   DrawCharacterToPreview(dictionaryEntry.Data);
      // }
    }
  }
}

void MessageEditor::DrawMessagePreview()  // From Parsing.
{
  // defaultColor = 6;
  text_line = 0;

  for (int i = 0; i < (172 * 4096); i++) {
    currentfontgfx16Ptr[i] = 0;
  }

  text_pos = 0;
  DrawCharacterToPreview(CurrentMessage.Data);

  shown_lines = 0;
}

void MessageEditor::DrawTileToPreview(int x, int y, int srcx, int srcy, int pal,
                                      bool mirror_x, bool mirror_y, int sizex,
                                      int sizey) {
  int drawid = srcx + (srcy * 32);
  for (int yl = 0; yl < sizey * 8; yl++) {
    for (int xl = 0; xl < 4; xl++) {
      int mx = xl;
      int my = yl;

      // Formula information to get tile index position in the array.
      // ((ID / nbrofXtiles) * (imgwidth/2) + (ID - ((ID/16)*16) ))
      int tx = ((drawid / 16) * 512) + ((drawid - ((drawid / 16) * 16)) * 4);
      uint8_t pixel = fontgfx16Ptr[tx + (yl * 64) + xl];

      // nx,ny = object position, xx,yy = tile position, xl,yl = pixel
      // position
      int index = x + (y * 172) + (mx * 2) + (my * 172);
      if ((pixel & 0x0F) != 0) {
        currentfontgfx16Ptr[index + 1] = (uint8_t)((pixel & 0x0F) + (0 * 4));
      }

      if (((pixel >> 4) & 0x0F) != 0) {
        currentfontgfx16Ptr[index + 0] =
            (uint8_t)(((pixel >> 4) & 0x0F) + (0 * 4));
      }
    }
  }
}

void MessageEditor::DisplayTextOverflowError(int pos, bool bank) {
  int space = bank ? kTextDataEnd - kTextData : kTextData2End - kTextData2;
  string bankSTR = bank ? "1st" : "2nd";
  string posSTR = bank ? absl::StrFormat("%X4", pos & 0xFFFF)
                       : absl::StrFormat("%X4", (pos - kTextData2) & 0xFFFF);
  std::string message = absl::StrFormat(
      "There is too much text data in the %s block to save.\n"
      "Available: %X4 | Used: %s",
      bankSTR, space, posSTR);
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
