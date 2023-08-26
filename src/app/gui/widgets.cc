#include "widgets.h"

#include <ImGuiColorTextEdit/TextEditor.h>
#include <imgui_memory_editor.h>

#include "absl/status/status.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gui {
namespace widgets {

// ============================================================================
// 65816 LanguageDefinition
// ============================================================================

static const char *const kKeywords[] = {
    "ADC", "AND", "ASL", "BCC", "BCS", "BEQ",   "BIT",   "BMI",       "BNE",
    "BPL", "BRA", "BRL", "BVC", "BVS", "CLC",   "CLD",   "CLI",       "CLV",
    "CMP", "CPX", "CPY", "DEC", "DEX", "DEY",   "EOR",   "INC",       "INX",
    "INY", "JMP", "JSR", "JSL", "LDA", "LDX",   "LDY",   "LSR",       "MVN",
    "NOP", "ORA", "PEA", "PER", "PHA", "PHB",   "PHD",   "PHP",       "PHX",
    "PHY", "PLA", "PLB", "PLD", "PLP", "PLX",   "PLY",   "REP",       "ROL",
    "ROR", "RTI", "RTL", "RTS", "SBC", "SEC",   "SEI",   "SEP",       "STA",
    "STP", "STX", "STY", "STZ", "TAX", "TAY",   "TCD",   "TCS",       "TDC",
    "TRB", "TSB", "TSC", "TSX", "TXA", "TXS",   "TXY",   "TYA",       "TYX",
    "WAI", "WDM", "XBA", "XCE", "ORG", "LOROM", "HIROM", "NAMESPACE", "DB"};

static const char *const kIdentifiers[] = {
    "abort",   "abs",     "acos",    "asin",     "atan",    "atexit",
    "atof",    "atoi",    "atol",    "ceil",     "clock",   "cosh",
    "ctime",   "div",     "exit",    "fabs",     "floor",   "fmod",
    "getchar", "getenv",  "isalnum", "isalpha",  "isdigit", "isgraph",
    "ispunct", "isspace", "isupper", "kbhit",    "log10",   "log2",
    "log",     "memcmp",  "modf",    "pow",      "putchar", "putenv",
    "puts",    "rand",    "remove",  "rename",   "sinh",    "sqrt",
    "srand",   "strcat",  "strcmp",  "strerror", "time",    "tolower",
    "toupper"};

TextEditor::LanguageDefinition GetAssemblyLanguageDef() {
  TextEditor::LanguageDefinition language_65816;
  for (auto &k : kKeywords) language_65816.mKeywords.emplace(k);

  for (auto &k : kIdentifiers) {
    TextEditor::Identifier id;
    id.mDeclaration = "Built-in function";
    language_65816.mIdentifiers.insert(std::make_pair(std::string(k), id));
  }

  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[ \\t]*#[ \\t]*[a-zA-Z_]+", TextEditor::PaletteIndex::Preprocessor));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\'\\\\?[^\\']\\'", TextEditor::PaletteIndex::CharLiteral));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",
          TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?[0-9]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "0[0-7]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?",
          TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/"
          "\\;\\,\\.]",
          TextEditor::PaletteIndex::Punctuation));

  language_65816.mCommentStart = "/*";
  language_65816.mCommentEnd = "*/";
  language_65816.mSingleLineComment = ";";

  language_65816.mCaseSensitive = false;
  language_65816.mAutoIndentation = true;

  language_65816.mName = "65816";

  return language_65816;
}

}  // namespace widgets
}  // namespace gui
}  // namespace app
}  // namespace yaze
