#ifndef YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H
#define YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H

// ===========================================================================
// Message Data System for Zelda 3 (A Link to the Past)
// ===========================================================================
//
// This system handles the parsing, editing, and serialization of in-game text
// messages from The Legend of Zelda: A Link to the Past (SNES).
//
// ## Architecture Overview
//
// The message system consists of several key components:
//
// 1. **Character Encoding** (`CharEncoder`):
//    Maps byte values (0x00-0x66) to displayable characters (A-Z, a-z, 0-9,
//    punctuation). This is the basic text representation in the ROM.
//
// 2. **Text Commands** (`TextCommands`):
//    Special control codes (0x67-0x80) that control message display behavior:
//    - Window appearance (border, position)
//    - Text flow (line breaks, scrolling, delays)
//    - Interactive elements (choices, player name insertion)
//    - Some commands have arguments (e.g., [W:02] = window border type 2)
//
// 3. **Special Characters** (`SpecialChars`):
//    Extended character set (0x43-0x5E) for game-specific symbols:
//    - Directional arrows
//    - Button prompts (A, B, X, Y)
//    - HP indicators
//    - Hieroglyphs
//
// 4. **Dictionary System** (`DictionaryEntry`):
//    Compression system using byte values 0x88+ to reference common
//    words/phrases stored separately in ROM. This saves space by replacing
//    frequently-used text with single-byte references.
//
// 5. **Message Data** (`MessageData`):
//    Represents a single in-game message with both raw binary data and parsed
//    human-readable text. Each message is terminated by 0x7F in ROM.
//
// ## Data Flow
//
// ### Reading from ROM:
// ROM bytes → ReadAllTextData() → MessageData (raw) → ParseMessageData() →
// Human-readable string with [command] tokens
//
// ### Writing to ROM:
// User edits text → ParseMessageToData() → Binary bytes → ROM
//
// ### Dictionary Optimization:
// Text string → OptimizeMessageForDictionary() → Replace common phrases with
// [D:XX] tokens → Smaller binary representation
//
// ## ROM Memory Layout (SNES)
//
// - Text Data Block 1: 0xE0000 - 0xE7FFF (32KB)
// - Text Data Block 2: 0x75F40 - 0x773FF (5.3KB)
// - Dictionary Pointers: 0x74703
// - Character Widths: Table storing pixel widths for proportional font
// - Font Graphics: 0x70000+ (2bpp tile data)
//
// ## Message Format
//
// Messages are stored as byte sequences terminated by 0x7F:
// Example: [0x00, 0x01, 0x02, 0x7F] = "ABC"
// Example: [0x6A, 0x59, 0x2C, 0x61, 0x32, 0x28, 0x2B, 0x23, 0x7F]
//          = "[L] saved Hyrule" (0x6A = player name command)
//
// ## Token Syntax (Human-Readable Format)
//
// Commands:     [TOKEN:HEX] or [TOKEN]
//               Examples: [W:02] (window border), [K] (wait for key)
// Dictionary:   [D:HEX]
//               Examples: [D:00] (first dictionary entry)
// Special Chars:[TOKEN]
//               Examples: [A] (A button), [UP] (up arrow)
//
// ===========================================================================

#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "rom/rom.h"

namespace yaze {
namespace editor {

const std::string kBankToken = "BANK";
const std::string DICTIONARYTOKEN = "D";
constexpr uint8_t kMessageTerminator = 0x7F;  // Marks end of message in ROM
constexpr uint8_t DICTOFF = 0x88;  // Dictionary entries start at byte 0x88
constexpr uint8_t kWidthArraySize = 100;
constexpr uint8_t kBankSwitchCommand = 0x80;

// Character encoding table: Maps ROM byte values to displayable characters
// Used for both parsing ROM data into text and converting text back to bytes
static const std::unordered_map<uint8_t, wchar_t> CharEncoder = {
    {0x00, 'A'},  {0x01, 'B'},  {0x02, 'C'},  {0x03, 'D'},  {0x04, 'E'},
    {0x05, 'F'},  {0x06, 'G'},  {0x07, 'H'},  {0x08, 'I'},  {0x09, 'J'},
    {0x0A, 'K'},  {0x0B, 'L'},  {0x0C, 'M'},  {0x0D, 'N'},  {0x0E, 'O'},
    {0x0F, 'P'},  {0x10, 'Q'},  {0x11, 'R'},  {0x12, 'S'},  {0x13, 'T'},
    {0x14, 'U'},  {0x15, 'V'},  {0x16, 'W'},  {0x17, 'X'},  {0x18, 'Y'},
    {0x19, 'Z'},  {0x1A, 'a'},  {0x1B, 'b'},  {0x1C, 'c'},  {0x1D, 'd'},
    {0x1E, 'e'},  {0x1F, 'f'},  {0x20, 'g'},  {0x21, 'h'},  {0x22, 'i'},
    {0x23, 'j'},  {0x24, 'k'},  {0x25, 'l'},  {0x26, 'm'},  {0x27, 'n'},
    {0x28, 'o'},  {0x29, 'p'},  {0x2A, 'q'},  {0x2B, 'r'},  {0x2C, 's'},
    {0x2D, 't'},  {0x2E, 'u'},  {0x2F, 'v'},  {0x30, 'w'},  {0x31, 'x'},
    {0x32, 'y'},  {0x33, 'z'},  {0x34, '0'},  {0x35, '1'},  {0x36, '2'},
    {0x37, '3'},  {0x38, '4'},  {0x39, '5'},  {0x3A, '6'},  {0x3B, '7'},
    {0x3C, '8'},  {0x3D, '9'},  {0x3E, '!'},  {0x3F, '?'},  {0x40, '-'},
    {0x41, '.'},  {0x42, ','},  {0x44, '>'},  {0x45, '('},  {0x46, ')'},
    {0x4C, '"'},  {0x51, '\''}, {0x59, ' '},  {0x5A, '<'},  {0x5F, L'¡'},
    {0x60, L'¡'}, {0x61, L'¡'}, {0x62, L' '}, {0x63, L' '}, {0x64, L' '},
    {0x65, ' '},  {0x66, '_'},
};

// Finds the ROM byte value for a given character (reverse lookup in
// CharEncoder) Returns 0xFF if character is not found
uint8_t FindMatchingCharacter(char value);

// Checks if a byte value represents a dictionary entry
// Returns dictionary index (0-96) or -1 if not a dictionary entry
int8_t FindDictionaryEntry(uint8_t value);

// Converts a human-readable message string (with [command] tokens) into ROM
// bytes This is the inverse operation of ParseMessageData
std::vector<uint8_t> ParseMessageToData(std::string str);

// Result of parsing text into message bytes with diagnostics.
struct MessageParseResult {
  std::vector<uint8_t> bytes;
  std::vector<std::string> errors;
  std::vector<std::string> warnings;

  bool ok() const { return errors.empty(); }
};

// Converts text into message bytes and captures parse errors/warnings.
MessageParseResult ParseMessageToDataWithDiagnostics(std::string_view str);

// Message bank for bundle import/export.
enum class MessageBank {
  kVanilla,
  kExpanded,
};

std::string MessageBankToString(MessageBank bank);
absl::StatusOr<MessageBank> MessageBankFromString(std::string_view value);

// Represents a single dictionary entry (common word/phrase) used for text
// compression Dictionary entries are stored separately in ROM and referenced by
// bytes 0x88-0xE8 Example: Dictionary entry 0x00 might contain "the" and be
// referenced as [D:00]
struct DictionaryEntry {
  uint8_t ID = 0;             // Dictionary index (0-96)
  std::string Contents = "";  // The actual text this entry represents
  std::vector<uint8_t> Data;  // Binary representation of Contents
  int Length = 0;             // Character count
  std::string Token = "";     // Human-readable token like "[D:00]"

  DictionaryEntry() = default;
  DictionaryEntry(uint8_t i, std::string_view s)
      : ID(i), Contents(s), Length(s.length()) {
    Token = absl::StrFormat("[%s:%02X]", DICTIONARYTOKEN, ID);
    Data = ParseMessageToData(Contents);
  }

  // Checks if this dictionary entry's text appears in the given string
  bool ContainedInString(std::string_view s) const {
    // Convert to std::string to avoid Debian string_view bug with
    // absl::StrContains
    return absl::StrContains(std::string(s), Contents);
  }

  // Replaces all occurrences of this dictionary entry's text with its token
  // Example: "the cat" with dictionary[0]="the" becomes "[D:00] cat"
  std::string ReplaceInstancesOfIn(std::string_view s) const {
    auto replaced_string = std::string(s);
    size_t pos = replaced_string.find(Contents);
    while (pos != std::string::npos) {
      replaced_string.replace(pos, Contents.length(), Token);
      pos = replaced_string.find(Contents, pos + Token.length());
    }
    return replaced_string;
  }
};

constexpr int kTextData = 0xE0000;
constexpr int kTextDataEnd = 0xE7FFF;
constexpr int kNumDictionaryEntries = 0x61;
constexpr int kPointersDictionaries = 0x74703;
constexpr uint8_t kScrollVertical = 0x73;
constexpr uint8_t kLine1 = 0x74;
constexpr uint8_t kLine2 = 0x75;
constexpr uint8_t kLine3 = 0x76;

// Reads all dictionary entries from ROM and builds the dictionary table
std::vector<DictionaryEntry> BuildDictionaryEntries(Rom* rom);

// Replaces all dictionary words in a string with their [D:XX] tokens
// Used for text compression when saving messages back to ROM
std::string ReplaceAllDictionaryWords(
    std::string str, const std::vector<DictionaryEntry>& dictionary);

// Looks up a dictionary entry by its ROM byte value
DictionaryEntry FindRealDictionaryEntry(
    uint8_t value, const std::vector<DictionaryEntry>& dictionary);

// Special marker inserted into commands to protect them from dictionary
// replacements during optimization. Removed after dictionary replacement is
// complete.
const std::string CHEESE = "\uBEBE";

// Represents a complete in-game message with both raw and parsed
// representations Messages can exist in two forms:
// 1. Raw: Direct ROM bytes with dictionary references as [D:XX] tokens
// 2. Parsed: Fully expanded with dictionary words replaced by actual text
struct MessageData {
  int ID = 0;                  // Message index in the ROM
  int Address = 0;             // ROM address where this message is stored
  std::string RawString;       // Human-readable with [D:XX] dictionary tokens
  std::string ContentsParsed;  // Fully expanded human-readable text
  std::vector<uint8_t> Data;   // Raw ROM bytes (may contain dict references)
  std::vector<uint8_t> DataParsed;  // Expanded bytes (dict entries expanded)

  MessageData() = default;
  MessageData(int id, int address, const std::string& rawString,
              const std::vector<uint8_t>& rawData,
              const std::string& parsedString,
              const std::vector<uint8_t>& parsedData)
      : ID(id),
        Address(address),
        RawString(rawString),
        ContentsParsed(parsedString),
        Data(rawData),
        DataParsed(parsedData) {}

  // Copy constructor
  MessageData(const MessageData& other) {
    ID = other.ID;
    Address = other.Address;
    RawString = other.RawString;
    Data = other.Data;
    DataParsed = other.DataParsed;
    ContentsParsed = other.ContentsParsed;
  }

  // Optimizes a message by replacing common phrases with dictionary tokens
  // Inserts CHEESE markers inside commands to prevent dictionary replacement
  // from corrupting command syntax like [W:02]
  // Example: "Link saved the day" → "[D:00] saved [D:01] day"
  std::string OptimizeMessageForDictionary(
      std::string_view message_string,
      const std::vector<DictionaryEntry>& dictionary) {
    std::stringstream protons;
    bool command = false;
    // Insert CHEESE markers inside commands to protect them
    for (const auto& c : message_string) {
      if (c == '[') {
        command = true;
      } else if (c == ']') {
        command = false;
      }

      protons << c;
      if (command) {
        protons << CHEESE;  // Protect command contents from replacement
      }
    }

    std::string protons_string = protons.str();
    std::string replaced_string =
        ReplaceAllDictionaryWords(protons_string, dictionary);
    std::string final_string =
        absl::StrReplaceAll(replaced_string, {{CHEESE, ""}});

    return final_string;
  }

  // Updates this message with new text content
  // Automatically optimizes the message using dictionary compression
  void SetMessage(const std::string& message,
                  const std::vector<DictionaryEntry>& dictionary) {
    RawString = message;
    ContentsParsed = OptimizeMessageForDictionary(message, dictionary);
  }
};

// Message bundle entry for JSON import/export.
struct MessageBundleEntry {
  int id = 0;
  MessageBank bank = MessageBank::kVanilla;
  std::string raw;
  std::string parsed;
  std::string text;
};

constexpr int kMessageBundleVersion = 1;

// Represents a text command or special character definition
// Text commands control message display (line breaks, colors, choices, etc.)
// Special characters are game-specific symbols (arrows, buttons, HP hearts)
struct TextElement {
  uint8_t ID;                 // ROM byte value for this element
  std::string Token;          // Short token like "W" or "UP"
  std::string GenericToken;   // Display format like "[W:##]" or "[UP]"
  std::string Pattern;        // Regex pattern for parsing
  std::string StrictPattern;  // Strict regex pattern for exact matching
  std::string Description;    // Human-readable description
  bool HasArgument;           // True if command takes a parameter byte

  TextElement() = default;
  TextElement(uint8_t id, const std::string& token, bool arg,
              const std::string& description) {
    ID = id;
    Token = token;
    if (arg) {
      GenericToken = absl::StrFormat("[%s:##]", Token);
    } else {
      GenericToken = absl::StrFormat("[%s]", Token);
    }
    HasArgument = arg;
    Description = description;
    if (arg) {
      Pattern = absl::StrFormat(
          "\\[%s(:[0-9A-F]{1,2})?\\]",
          absl::StrReplaceAll(Token, {{"[", "\\["}, {"]", "\\]"}}));
    } else {
      Pattern = absl::StrFormat(
          "\\[%s\\]", absl::StrReplaceAll(Token, {{"[", "\\["}, {"]", "\\]"}}));
    }
    StrictPattern = absl::StrFormat("^%s$", Pattern);
  }

  std::string GetParamToken(uint8_t value = 0) const {
    if (HasArgument) {
      return absl::StrFormat("[%s:%02X]", Token, value);
    } else {
      return absl::StrFormat("[%s]", Token);
    }
  }

  std::smatch MatchMe(const std::string& dfrag) const {
    std::regex pattern(StrictPattern);
    std::smatch match;
    std::regex_match(dfrag, match, pattern);
    return match;
  }

  bool Empty() const { return ID == 0; }

  // Comparison operator
  bool operator==(const TextElement& other) const { return ID == other.ID; }
};

const static std::string kWindowBorder = "Window border";
const static std::string kWindowPosition = "Window position";
const static std::string kScrollSpeed = "Scroll speed";
const static std::string kTextDrawSpeed = "Text draw speed";
const static std::string kTextColor = "Text color";
const static std::string kPlayerName = "Player name";
const static std::string kLine1Str = "Line 1";
const static std::string kLine2Str = "Line 2";
const static std::string kLine3Str = "Line 3";
const static std::string kWaitForKey = "Wait for key";
const static std::string kScrollText = "Scroll text";
const static std::string kDelayX = "Delay X";
const static std::string kBCDNumber = "BCD number";
const static std::string kSoundEffect = "Sound effect";
const static std::string kChoose3 = "Choose 3";
const static std::string kChoose2High = "Choose 2 high";
const static std::string kChoose2Low = "Choose 2 low";
const static std::string kChoose2Indented = "Choose 2 indented";
const static std::string kChooseItem = "Choose item";
const static std::string kNextAttractImage = "Next attract image";
const static std::string kBankMarker = "Bank marker (automatic)";
const static std::string kCrash = "Crash";

static const std::vector<TextElement> TextCommands = {
    TextElement(0x6B, "W", true, kWindowBorder),
    TextElement(0x6D, "P", true, kWindowPosition),
    TextElement(0x6E, "SPD", true, kScrollSpeed),
    TextElement(0x7A, "S", true, kTextDrawSpeed),
    TextElement(0x77, "C", true, kTextColor),
    TextElement(0x6A, "L", false, kPlayerName),
    TextElement(0x74, "1", false, kLine1Str),
    TextElement(0x75, "2", false, kLine2Str),
    TextElement(0x76, "3", false, kLine3Str),
    TextElement(0x7E, "K", false, kWaitForKey),
    TextElement(0x73, "V", false, kScrollText),
    TextElement(0x78, "WT", true, kDelayX),
    TextElement(0x6C, "N", true, kBCDNumber),
    TextElement(0x79, "SFX", true, kSoundEffect),
    TextElement(0x71, "CH3", false, kChoose3),
    TextElement(0x72, "CH2", false, kChoose2High),
    TextElement(0x6F, "CH2L", false, kChoose2Low),
    TextElement(0x68, "CH2I", false, kChoose2Indented),
    TextElement(0x69, "CHI", false, kChooseItem),
    TextElement(0x67, "IMG", false, kNextAttractImage),
    TextElement(0x80, kBankToken, false, kBankMarker),
    TextElement(0x70, "NONO", false, kCrash),
};

// Finds the TextElement definition for a command byte value
// Returns nullopt if the byte is not a recognized command
std::optional<TextElement> FindMatchingCommand(uint8_t b);

// Special characters available in Zelda 3 messages
// These are symbols and game-specific icons that appear in text
static const std::vector<TextElement> SpecialChars = {
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

// Finds the TextElement definition for a special character byte
// Returns nullopt if the byte is not a recognized special character
std::optional<TextElement> FindMatchingSpecial(uint8_t b);

// Result of parsing a text token like "[W:02]"
// Contains both the command definition and its argument value
struct ParsedElement {
  TextElement Parent;   // The command or special character definition
  uint8_t Value;        // Argument value (if command has argument)
  bool Active = false;  // True if parsing was successful

  ParsedElement() = default;
  ParsedElement(const TextElement& textElement, uint8_t value)
      : Parent(textElement), Value(value), Active(true) {}
};

// Parses a token string like "[W:02]" and returns its ParsedElement
// Returns inactive ParsedElement if token is invalid
ParsedElement FindMatchingElement(const std::string& str);

// Converts a single ROM byte into its human-readable text representation
// Handles characters, commands, special chars, and dictionary references
std::string ParseTextDataByte(uint8_t value);

// Parses a single message from ROM data starting at current_pos
// Updates current_pos to point after the message terminator
// Returns error if message is malformed (e.g., missing terminator)
absl::StatusOr<MessageData> ParseSingleMessage(
    const std::vector<uint8_t>& rom_data, int* current_pos);

// Converts MessageData objects into human-readable strings with [command]
// tokens This is the main function for displaying messages in the editor
// Properly handles commands with arguments to avoid parsing errors
std::vector<std::string> ParseMessageData(
    std::vector<MessageData>& message_data,
    const std::vector<DictionaryEntry>& dictionary_entries);

constexpr int kTextData2 = 0x75F40;
constexpr int kTextData2End = 0x773FF;

// Reads all text data from the ROM and returns a vector of MessageData objects.
// When max_pos > 0, the parser stops if pos exceeds max_pos (safety bound).
std::vector<MessageData> ReadAllTextData(uint8_t* rom, int pos = kTextData,
                                         int max_pos = -1);

// Calls the file dialog and loads expanded messages from a BIN file.
absl::Status LoadExpandedMessages(std::string& expanded_message_path,
                                  std::vector<std::string>& parsed_messages,
                                  std::vector<MessageData>& expanded_messages,
                                  std::vector<DictionaryEntry>& dictionary);

// Serializes a vector of MessageData to a JSON object.
nlohmann::json SerializeMessagesToJson(const std::vector<MessageData>& messages);

// Exports messages to a JSON file at the specified path.
absl::Status ExportMessagesToJson(const std::string& path,
                                  const std::vector<MessageData>& messages);

// Serializes message bundles (vanilla + expanded) to JSON.
nlohmann::json SerializeMessageBundle(const std::vector<MessageData>& vanilla,
                                      const std::vector<MessageData>& expanded);

// Exports message bundle to JSON file.
absl::Status ExportMessageBundleToJson(
    const std::string& path, const std::vector<MessageData>& vanilla,
    const std::vector<MessageData>& expanded);

// Parses message bundle JSON into entries.
absl::StatusOr<std::vector<MessageBundleEntry>> ParseMessageBundleJson(
    const nlohmann::json& json);

// Loads message bundle entries from a JSON file.
absl::StatusOr<std::vector<MessageBundleEntry>> LoadMessageBundleFromJson(
    const std::string& path);

// ===========================================================================
// Line Width Validation
// ===========================================================================

constexpr int kMaxLineWidth = 32;  // Maximum visible characters per line

// Validates that no line in a message exceeds kMaxLineWidth visible characters.
// Splits on line break tokens: [1], [2], [3], [V], [K]
// Returns a vector of warning strings (empty if all lines are within bounds).
// Command tokens like [W:02], [SFX:2D] etc. are not counted as visible chars.
std::vector<std::string> ValidateMessageLineWidths(const std::string& message);

// ===========================================================================
// Org Format (.org) Import/Export
// ===========================================================================

// Parses an org-mode header line like "** 0F - Skeleton Guard"
// Returns {message_id, label} pair, or nullopt if not a valid header.
std::optional<std::pair<int, std::string>> ParseOrgHeader(
    const std::string& line);

// Parses the full content of a .org file into message entries.
// Returns a vector of {message_id, body_text} pairs.
std::vector<std::pair<int, std::string>> ParseOrgContent(
    const std::string& content);

// Exports messages to .org format string.
// messages: vector of {message_id, body_text} pairs
// labels: parallel vector of human-readable labels for each message
std::string ExportToOrgFormat(
    const std::vector<std::pair<int, std::string>>& messages,
    const std::vector<std::string>& labels);

// ===========================================================================
// Expanded Message Bank (Oracle of Secrets: $2F8000)
// ===========================================================================

// PC address of SNES $2F8000 (expanded message region start)
constexpr int kExpandedTextDataDefault = 0x178000;
// PC address of SNES $2FFFFF (expanded message region end)
constexpr int kExpandedTextDataEndDefault = 0x17FFFF;

int GetExpandedTextDataStart();
int GetExpandedTextDataEnd();

// Reads expanded messages from a ROM buffer at the given PC address.
// Messages are 0x7F-terminated, region is 0xFF-terminated.
// Uses the same parsing as ReadAllTextData but for the expanded bank.
std::vector<MessageData> ReadExpandedTextData(uint8_t* rom, int pos);

// Writes encoded messages to the expanded region of a ROM buffer.
// Each message text is encoded via ParseMessageToData, terminated with 0x7F.
// The region is terminated with 0xFF.
// Returns error if total size exceeds (end - start + 1).
absl::Status WriteExpandedTextData(uint8_t* rom, int start, int end,
                                   const std::vector<std::string>& messages);

// Writes all vanilla message data back to the ROM with bank switching.
absl::Status WriteAllTextData(Rom* rom,
                              const std::vector<MessageData>& messages);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MESSAGE_MESSAGE_DATA_H
