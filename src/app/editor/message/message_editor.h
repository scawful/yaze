#ifndef YAZE_APP_EDITOR_MESSAGE_EDITOR_H
#define YAZE_APP_EDITOR_MESSAGE_EDITOR_H

#include <absl/status/status.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>

#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/editor/utils/editor.h"
#include "app/gfx/bitmap.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

using std::string;

// TEXT EDITOR RELATED CONSTANTS
const int kGfxFont = 0x70000;  // 2bpp format
const int kTextData = 0xE0000;
const int kTextDataEnd = 0xE7FFF;
const int kTextData2 = 0x75F40;
const int kTextData2End = 0x773FF;
const int kPointersDictionaries = 0x74703;
const int kCharactersWidth = 0x74ADF;

const string DICTIONARYTOKEN = "D";
const uint8_t DICTOFF = 0x88;
const uint8_t MESSAGETERMINATOR = 0x7F;
const string BANKToken = "BANK";
const uint8_t BANKID = 0x80;

static int defaultColor = 6;

static std::vector<uint8_t> ParseMessageToData(string str);
static string AddNewLinesToCommands(string str);
static string ReplaceAllDictionaryWords(string str);
const std::string CHEESE = "\uBEBE";  // Inserted into commands to protect
                                      // them from dictionary replacements.

struct MessageData {
  int ID;
  int Address;
  std::string RawString;
  std::string ContentsParsed;
  std::vector<uint8_t> Data;
  std::vector<uint8_t> DataParsed;

  MessageData() = default;
  MessageData(int id, int address, const std::string& rawString,
              const std::vector<uint8_t>& rawData,
              const std::string& parsedString,
              const std::vector<uint8_t>& parsedData)
      : ID(id),
        Address(address),
        RawString(rawString),
        Data(rawData),
        DataParsed(parsedData),
        ContentsParsed(parsedString) {}

  // Copy constructor
  MessageData(const MessageData& other) {
    ID = other.ID;
    Address = other.Address;
    RawString = other.RawString;
    Data = other.Data;
    DataParsed = other.DataParsed;
    ContentsParsed = other.ContentsParsed;
  }

  void SetMessage(std::string messageString) {
    ContentsParsed = messageString;
    RawString = OptimizeMessageForDictionary(messageString);
    RecalculateData();
  }

  std::string ToString() {
    return absl::StrFormat("%0X - %s", ID, ContentsParsed);
  }

  std::string GetReadableDumpedContents() {
    std::stringstream stringBuilder;
    for (const auto& b : Data) {
      stringBuilder << absl::StrFormat("%0X ", b);
    }
    stringBuilder << absl::StrFormat("%00X", MESSAGETERMINATOR);

    return absl::StrFormat(
        "[[[[\r\nMessage "
        "%000X]]]]\r\n[Contents]\r\n%s\r\n\r\n[Data]\r\n%s"
        "\r\n\r\n\r\n\r\n",
        ID, AddNewLinesToCommands(ContentsParsed), stringBuilder.str());
  }

  std::string GetDumpedContents() {
    return absl::StrFormat("%000X : %s\r\n\r\n", ID, ContentsParsed);
  }

  std::string OptimizeMessageForDictionary(std::string messageString) {
    std::stringstream protons;
    bool command = false;
    for (const auto& c : messageString) {
      if (c == '[') {
        command = true;
      } else if (c == ']') {
        command = false;
      }

      protons << c;
      if (command) {
        protons << CHEESE;
      }
    }

    std::string protonsString = protons.str();
    std::string replacedString = ReplaceAllDictionaryWords(protonsString);
    std::string finalString =
        absl::StrReplaceAll(replacedString, {{CHEESE, ""}});

    return finalString;
  }

  void RecalculateData() {
    Data = ParseMessageToData(RawString);
    DataParsed = ParseMessageToData(ContentsParsed);
  }
};

struct TextElement {
  uint8_t ID;
  string Token;
  string GenericToken;
  string Pattern;
  string StrictPattern;
  string Description;
  bool HasArgument;

  TextElement() = default;
  TextElement(uint8_t id, string token, bool arg, string description) {
    ID = id;
    Token = token;
    auto format_string = arg ? "[%s:##]" : "[%s]";
    GenericToken = format_string + Token;
    HasArgument = arg;
    Description = description;
    Pattern =
        arg ? "\\[" + Token + ":?([0-9A-F]{1,2})\\]" : "\\[" + Token + "\\]";
    Pattern = absl::StrReplaceAll(Pattern, {{"[", "\\["}, {"]", "\\]"}});
    StrictPattern = absl::StrCat("^", Pattern, "$");
    StrictPattern = "^" + Pattern + "$";
  }

  string GetParameterizedToken(uint8_t value = 0) {
    if (HasArgument) {
      return absl::StrFormat("[%s:%02X]", Token, value);
    } else {
      return absl::StrFormat("[%s]", Token);
    }
  }

  string ToString() {
    return absl::StrFormat("%s %s", GenericToken, Description);
  }

  std::smatch MatchMe(std::string dfrag) const {
    std::regex pattern(StrictPattern);
    std::smatch match;
    std::regex_match(dfrag, match, pattern);
    return match;
  }

  bool Empty() { return ID == 0; }
};

struct ParsedElement {
  TextElement Parent;
  uint8_t Value;
  bool Active = false;

  ParsedElement() = default;
  ParsedElement(TextElement textElement, uint8_t value) {
    Parent = textElement;
    Value = value;
    Active = true;
  }
};

static ParsedElement FindMatchingElement(string str);

static const TextElement TextCommands[] = {
    TextElement(0x6B, "W", true, "Window border"),
    TextElement(0x6D, "P", true, "Window position"),
    TextElement(0x6E, "SPD", true, "Scroll speed"),
    TextElement(0x7A, "S", true, "Text draw speed"),
    TextElement(0x77, "C", true, "Text color"),
    TextElement(0x6A, "L", false, "Player name"),
    TextElement(0x74, "1", false, "Line 1"),
    TextElement(0x75, "2", false, "Line 2"),
    TextElement(0x76, "3", false, "Line 3"),
    TextElement(0x7E, "K", false, "Wait for key"),
    TextElement(0x73, "V", false, "Scroll text"),
    TextElement(0x78, "WT", true, "Delay X"),
    TextElement(0x6C, "N", true, "BCD number"),
    TextElement(0x79, "SFX", true, "Sound effect"),
    TextElement(0x71, "CH3", false, "Choose 3"),
    TextElement(0x72, "CH2", false, "Choose 2 high"),
    TextElement(0x6F, "CH2L", false, "Choose 2 low"),
    TextElement(0x68, "CH2I", false, "Choose 2 indented"),
    TextElement(0x69, "CHI", false, "Choose item"),
    TextElement(0x67, "IMG", false, "Next attract image"),
    TextElement(0x80, BANKToken, false, "Bank marker (automatic)"),
    TextElement(0x70, "NONO", false, "Crash"),
};

static std::vector<TextElement> SpecialChars = {
    TextElement(0x43, "...", false, "Ellipsis …"),
    TextElement(0x4D, "UP", false, "Arrow ↑"),
    TextElement(0x4E, "DOWN", false, "Arrow ↓"),
    TextElement(0x4F, "LEFT", false, "Arrow ←"),
    TextElement(0x50, "RIGHT", false, "Arrow →"),
    TextElement(0x5B, "A", false, "Button Ⓐ"),
    TextElement(0x5C, "B", false, "Button Ⓑ"),
    TextElement(0x5D, "X", false, "Button ⓧ"),
    TextElement(0x5E, "Y", false, "Button ⓨ"),
    TextElement(0x52, "HP1L", false, "1 HP left"),
    TextElement(0x53, "HP1R", false, "1 HP right"),
    TextElement(0x54, "HP2L", false, "2 HP left"),
    TextElement(0x55, "HP3L", false, "3 HP left"),
    TextElement(0x56, "HP3R", false, "3 HP right"),
    TextElement(0x57, "HP4L", false, "4 HP left"),
    TextElement(0x58, "HP4R", false, "4 HP right"),
    TextElement(0x47, "HY0", false, "Hieroglyph ☥"),
    TextElement(0x48, "HY1", false, "Hieroglyph 𓈗"),
    TextElement(0x49, "HY2", false, "Hieroglyph Ƨ"),
    TextElement(0x4A, "LFL", false, "Link face left"),
    TextElement(0x4B, "LFR", false, "Link face right"),
};

static const std::unordered_map<uint8_t, wchar_t> CharEncoder = {
    {0x00, 'A'},
    {0x01, 'B'},
    {0x02, 'C'},
    {0x03, 'D'},
    {0x04, 'E'},
    {0x05, 'F'},
    {0x06, 'G'},
    {0x07, 'H'},
    {0x08, 'I'},
    {0x09, 'J'},
    {0x0A, 'K'},
    {0x0B, 'L'},
    {0x0C, 'M'},
    {0x0D, 'N'},
    {0x0E, 'O'},
    {0x0F, 'P'},
    {0x10, 'Q'},
    {0x11, 'R'},
    {0x12, 'S'},
    {0x13, 'T'},
    {0x14, 'U'},
    {0x15, 'V'},
    {0x16, 'W'},
    {0x17, 'X'},
    {0x18, 'Y'},
    {0x19, 'Z'},
    {0x1A, 'a'},
    {0x1B, 'b'},
    {0x1C, 'c'},
    {0x1D, 'd'},
    {0x1E, 'e'},
    {0x1F, 'f'},
    {0x20, 'g'},
    {0x21, 'h'},
    {0x22, 'i'},
    {0x23, 'j'},
    {0x24, 'k'},
    {0x25, 'l'},
    {0x26, 'm'},
    {0x27, 'n'},
    {0x28, 'o'},
    {0x29, 'p'},
    {0x2A, 'q'},
    {0x2B, 'r'},
    {0x2C, 's'},
    {0x2D, 't'},
    {0x2E, 'u'},
    {0x2F, 'v'},
    {0x30, 'w'},
    {0x31, 'x'},
    {0x32, 'y'},
    {0x33, 'z'},
    {0x34, '0'},
    {0x35, '1'},
    {0x36, '2'},
    {0x37, '3'},
    {0x38, '4'},
    {0x39, '5'},
    {0x3A, '6'},
    {0x3B, '7'},
    {0x3C, '8'},
    {0x3D, '9'},
    {0x3E, '!'},
    {0x3F, '?'},
    {0x40, '-'},
    {0x41, '.'},
    {0x42, ','},
    {0x44, '>'},
    {0x45, '('},
    {0x46, ')'},
    {0x4C, '"'},
    {0x51, '\''},
    {0x59, ' '},
    {0x5A, '<'},
    // {0x5F, '¡'}, {0x60, '¡'}, {0x61, '¡'},  {0x62, ' '}, {0x63, ' '}, {0x64,
    // ' '},
    {0x65, ' '},
    {0x66, '_'},
};

static TextElement DictionaryElement =
    TextElement(0x80, DICTIONARYTOKEN, true, "Dictionary");

class MessageEditor : public Editor, public SharedRom {
 public:
  class TextMessageData {
   private:
    int ID;
    std::string Contents;
    std::string ContentsParsed;
    std::vector<uint8_t> Data;
    std::vector<uint8_t> DataParsed;

   public:
    TextMessageData(int i, std::string sraw, std::vector<uint8_t> draw,
                    std::string spar, std::vector<uint8_t> dpar)
        : ID(i),
          Data(draw),
          DataParsed(dpar),
          Contents(sraw),
          ContentsParsed(spar) {}

    void SetMessage(std::string s) {
      ContentsParsed = s;
      // Contents = OptimizeMessageForDictionary(s);
      RecalculateData();
    }

    void RecalculateData() {
      Data = ParseMessageToData(Contents);
      DataParsed = ParseMessageToData(ContentsParsed);
    }

    std::string ToString() {
      return absl::StrFormat("%000X - %s", ID, ContentsParsed);
    }

    std::string GetReadableDumpedContents() {
      std::stringstream d;
      for (const auto& b : Data) {
        d << absl::StrFormat("%00X ", b);
      }

      return absl::StrFormat(
          "[[[[\nMessage %000X]]]]\n[Contents]\n%s\n\n[Data]\n%s\n\n\n\n", ID,
          AddNewLinesToCommands(ContentsParsed), d.str());
    }

    std::string GetDumpedContents() {
      return absl::StrFormat("%000X : %s\n\n", ID, ContentsParsed);
    }
  };

  struct DictionaryEntry {
    uint8_t ID;
    std::string Contents;
    std::vector<uint8_t> Data;
    int Length;
    std::string Token;

    DictionaryEntry() = default;
    DictionaryEntry(uint8_t i, std::string s)
        : Contents(s), ID(i), Length(s.length()) {
      Token = absl::StrFormat("[%s:%00X]", DICTIONARYTOKEN, ID);
      Data = ParseMessageToData(Contents);
    }

    bool ContainedInString(std::string s) {
      return s.find(Contents) != std::string::npos;
    }

    std::string ReplaceInstancesOfIn(std::string s) {
      std::string replacedString = s;
      size_t pos = replacedString.find(Contents);
      while (pos != std::string::npos) {
        replacedString.replace(pos, Contents.length(), Token);
        pos = replacedString.find(Contents, pos + Token.length());
      }
      return replacedString;
    }
  };

  MessageEditor() { type_ = EditorType::kMessage; }

  absl::Status Update() override;
  void DrawMessageList();
  void DrawCurrentMessage();
  void DrawTextCommands();

  absl::Status Initialize();
  void ReadAllTextData();
  void BuildDictionaryEntries();

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

  TextElement FindMatchingCommand(uint8_t byte);
  TextElement FindMatchingSpecial(uint8_t value);
  string ParseTextDataByte(uint8_t value);
  DictionaryEntry GetDictionaryFromID(uint8_t value);

  static uint8_t FindDictionaryEntry(uint8_t value);
  static uint8_t FindMatchingCharacter(char value);
  void DrawTileToPreview(int x, int y, int srcx, int srcy, int pal,
                         bool mirror_x = false, bool mirror_y = false,
                         int sizex = 1, int sizey = 1);
  void DrawCharacterToPreview(char c);
  void DrawCharacterToPreview(std::vector<uint8_t> text);

  void DrawStringToPreview(string str);
  void DrawMessagePreview();
  std::string DisplayTextOverflowError(int pos, bool bank);

  static const std::vector<DictionaryEntry> AllDicts;

  uint8_t width_array[100];
  string romname = "";

  int text_line = 0;
  int text_pos = 0;
  int shown_lines = 0;
  int selected_tile = 0;

  bool skip_next = false;
  bool from_form = false;

  std::vector<MessageData> ListOfTexts;
  std::vector<MessageData> DisplayedMessages;
  std::vector<std::string> ParsedMessages;

  MessageData CurrentMessage;

 private:
  static const TextElement DictionaryElement;

  bool data_loaded_ = false;
  int current_message_id_ = 0;

  std::string search_text_ = "";

  gui::Canvas font_gfx_canvas_{"##FontGfxCanvas", ImVec2(128, 128)};
  gui::Canvas current_font_gfx16_canvas_{"##CurrentMessageGfx",
                                         ImVec2(128, 512)};

  gfx::Bitmap font_gfx_bitmap_;
  gfx::Bitmap current_font_gfx16_bitmap_;

  Bytes fontgfx16Ptr;
  Bytes currentfontgfx16Ptr;

  gfx::SnesPalette previewColors;

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
  };

  TextBox message_text_box_;
};

static std::vector<MessageEditor::DictionaryEntry> AllDictionaries;

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_EDITOR_H