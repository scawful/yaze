#include "cli/service/agent/enhanced_tui.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

// ANSI color codes
constexpr const char* RESET = "\033[0m";
constexpr const char* BOLD = "\033[1m";
constexpr const char* DIM = "\033[2m";
constexpr const char* ITALIC = "\033[3m";
constexpr const char* UNDERLINE = "\033[4m";

// Foreground colors
constexpr const char* FG_BLACK = "\033[30m";
constexpr const char* FG_RED = "\033[31m";
constexpr const char* FG_GREEN = "\033[32m";
constexpr const char* FG_YELLOW = "\033[33m";
constexpr const char* FG_BLUE = "\033[34m";
constexpr const char* FG_MAGENTA = "\033[35m";
constexpr const char* FG_CYAN = "\033[36m";
constexpr const char* FG_WHITE = "\033[37m";
constexpr const char* FG_BRIGHT_BLACK = "\033[90m";
constexpr const char* FG_BRIGHT_RED = "\033[91m";
constexpr const char* FG_BRIGHT_GREEN = "\033[92m";
constexpr const char* FG_BRIGHT_YELLOW = "\033[93m";
constexpr const char* FG_BRIGHT_BLUE = "\033[94m";
constexpr const char* FG_BRIGHT_MAGENTA = "\033[95m";
constexpr const char* FG_BRIGHT_CYAN = "\033[96m";
constexpr const char* FG_BRIGHT_WHITE = "\033[97m";

// Background colors
constexpr const char* BG_BLACK = "\033[40m";
constexpr const char* BG_RED = "\033[41m";
constexpr const char* BG_GREEN = "\033[42m";
constexpr const char* BG_YELLOW = "\033[43m";
constexpr const char* BG_BLUE = "\033[44m";
constexpr const char* BG_MAGENTA = "\033[45m";
constexpr const char* BG_CYAN = "\033[46m";
constexpr const char* BG_WHITE = "\033[47m";
constexpr const char* BG_BRIGHT_BLACK = "\033[100m";
constexpr const char* BG_BRIGHT_RED = "\033[101m";
constexpr const char* BG_BRIGHT_GREEN = "\033[102m";
constexpr const char* BG_BRIGHT_YELLOW = "\033[103m";
constexpr const char* BG_BRIGHT_BLUE = "\033[104m";
constexpr const char* BG_BRIGHT_MAGENTA = "\033[105m";
constexpr const char* BG_BRIGHT_CYAN = "\033[106m";
constexpr const char* BG_BRIGHT_WHITE = "\033[107m";

// Key codes
constexpr int KEY_ESC = 27;
constexpr int KEY_ENTER = 10;
constexpr int KEY_BACKSPACE = 127;
constexpr int KEY_TAB = 9;
constexpr int KEY_UP = 1000;
constexpr int KEY_DOWN = 1001;
constexpr int KEY_LEFT = 1002;
constexpr int KEY_RIGHT = 1003;
constexpr int KEY_HOME = 1004;
constexpr int KEY_END = 1005;
constexpr int KEY_DELETE = 1006;
constexpr int KEY_PAGE_UP = 1007;
constexpr int KEY_PAGE_DOWN = 1008;

// Terminal control sequences
constexpr const char* CLEAR_SCREEN = "\033[2J";
constexpr const char* CLEAR_LINE = "\033[2K";
constexpr const char* CURSOR_HOME = "\033[H";
constexpr const char* SAVE_CURSOR = "\033[s";
constexpr const char* RESTORE_CURSOR = "\033[u";
constexpr const char* HIDE_CURSOR = "\033[?25l";
constexpr const char* SHOW_CURSOR = "\033[?25h";

// Get terminal size
std::pair<int, int> GetTerminalSize() {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return {csbi.srWindow.Right - csbi.srWindow.Left + 1,
          csbi.srWindow.Bottom - csbi.srWindow.Top + 1};
#else
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return {w.ws_col, w.ws_row};
#endif
}

// Read a single character from stdin
int ReadChar() {
#ifdef _WIN32
  return _getch();
#else
  return getchar();
#endif
}

// Check if a key is a special key
bool IsSpecialKey(int key) {
  return key >= 1000;
}

}  // namespace

// Helper function to convert TUITheme to string
std::string TUIThemeToString(TUITheme theme) {
  switch (theme) {
    case TUITheme::kDefault:
      return "Default";
    case TUITheme::kDark:
      return "Dark";
    case TUITheme::kLight:
      return "Light";
    case TUITheme::kZelda:
      return "Zelda";
    case TUITheme::kCyberpunk:
      return "Cyberpunk";
    default:
      return "Unknown";
  }
}

EnhancedTUI::EnhancedTUI(const TUIConfig& config) : config_(config) {
  LoadTheme(config_.theme);
}

EnhancedTUI::~EnhancedTUI() {
  Shutdown();
}

absl::Status EnhancedTUI::Initialize() {
  if (terminal_initialized_) {
    return absl::OkStatus();
  }

  SetupTerminal();

  auto [width, height] = GetTerminalSize();
  terminal_width_ = width;
  terminal_height_ = height;

  CalculateLayout();

  terminal_initialized_ = true;

  return absl::OkStatus();
}

void EnhancedTUI::Shutdown() {
  if (!terminal_initialized_) {
    return;
  }

  RestoreTerminal();
  terminal_initialized_ = false;
}

void EnhancedTUI::SetRomContext(Rom* rom) {
  rom_context_ = rom;
}

absl::Status EnhancedTUI::Run() {
  RETURN_IF_ERROR(Initialize());

  ClearScreen();
  RefreshDisplay();

  while (true) {
    RETURN_IF_ERROR(HandleInput());
    RefreshDisplay();
  }

  return absl::OkStatus();
}

void EnhancedTUI::DisplayMessage(const std::string& message,
                                 const std::string& sender, bool is_error) {
  std::string timestamp = FormatTimestamp();
  std::string formatted_message;

  if (is_error) {
    formatted_message = absl::StrFormat("%s %sERROR%s %s: %s", timestamp,
                                        FG_RED, RESET, sender, message);
  } else {
    formatted_message =
        absl::StrFormat("%s %s: %s", timestamp, sender, message);
  }

  output_history_.push_back(formatted_message);

  // Keep history size manageable
  if (output_history_.size() > config_.max_output_lines) {
    output_history_.erase(output_history_.begin(),
                          output_history_.begin() + 100);
  }
}

void EnhancedTUI::DisplayToolOutput(const std::string& output,
                                    const std::string& tool_name) {
  std::string timestamp = FormatTimestamp();
  std::string formatted_output = absl::StrFormat(
      "%s %sTOOL%s %s:\n%s", timestamp, FG_CYAN, RESET, tool_name, output);

  output_history_.push_back(formatted_output);
}

void EnhancedTUI::DisplaySuggestions(
    const std::vector<std::string>& suggestions) {
  if (suggestions.empty()) {
    return;
  }

  std::string suggestion_text =
      absl::StrFormat("%sSuggestions:%s\n", FG_YELLOW, RESET);

  for (size_t i = 0; i < suggestions.size() && i < 5; ++i) {
    suggestion_text += absl::StrFormat("  %s%d.%s %s\n", FG_BRIGHT_BLACK, i + 1,
                                       RESET, suggestions[i]);
  }

  output_history_.push_back(suggestion_text);
}

void EnhancedTUI::UpdateStatusBar(const std::string& status) {
  // Status updates are handled in the status bar drawing
  // This method can be extended to store status history
}

void EnhancedTUI::ShowHelp(const std::string& command) {
  std::string help_text;

  if (commands_.find(command) != commands_.end()) {
    help_text = absl::StrFormat("%sHelp for '%s':%s\n%s\n", FG_GREEN, command,
                                RESET, command_descriptions_[command]);
  } else {
    help_text =
        absl::StrFormat("%sUnknown command: %s%s\n", FG_RED, command, RESET);
  }

  output_history_.push_back(help_text);
}

void EnhancedTUI::RegisterCommand(
    const std::string& name,
    std::function<absl::Status(const std::vector<std::string>&)> handler,
    const std::string& description) {
  commands_[name] = handler;
  command_descriptions_[name] = description;
}

void EnhancedTUI::SetConfig(const TUIConfig& config) {
  config_ = config;
  LoadTheme(config_.theme);
  CalculateLayout();
}

void EnhancedTUI::SetupTerminal() {
  std::cout << HIDE_CURSOR;
  std::cout.flush();
}

void EnhancedTUI::RestoreTerminal() {
  std::cout << SHOW_CURSOR;
  std::cout.flush();
}

void EnhancedTUI::ClearScreen() {
  std::cout << CLEAR_SCREEN << CURSOR_HOME;
  std::cout.flush();
}

void EnhancedTUI::RefreshDisplay() {
  std::cout << CURSOR_HOME;

  DrawHeader();
  DrawCommandPalette();
  DrawChatArea();
  DrawToolOutput();
  DrawStatusBar();
  DrawSidebar();

  std::cout.flush();
}

void EnhancedTUI::CalculateLayout() {
  layout_.header_height = 3;
  layout_.status_height = 2;
  layout_.sidebar_width = std::min(30, terminal_width_ / 4);
  layout_.help_width = std::min(40, terminal_width_ / 3);
  layout_.chat_height =
      terminal_height_ - layout_.header_height - layout_.status_height - 2;
  layout_.tool_height = std::min(8, terminal_height_ / 4);
}

void EnhancedTUI::DrawHeader() {
  TUIStyle style = GetStyle(TUIComponent::kHeader);
  std::string header_text = absl::StrFormat(
      "%sZ3ED Enhanced TUI v2.0%s | ROM: %s | Theme: %s",
      ApplyStyle("", style).c_str(), RESET, rom_context_ ? "Loaded" : "None",
      TUIThemeToString(config_.theme));

  std::cout << CURSOR_HOME;
  std::cout << header_text;

  // Fill remaining width with spaces
  int remaining = terminal_width_ - header_text.length();
  for (int i = 0; i < remaining; ++i) {
    std::cout << " ";
  }

  std::cout << "\n";
  std::cout << std::string(terminal_width_, '-') << "\n";
}

void EnhancedTUI::DrawCommandPalette() {
  if (!in_command_palette_) {
    return;
  }

  TUIStyle style = GetStyle(TUIComponent::kCommandPalette);
  std::cout << ApplyStyle("Command Palette: ", style);
  std::cout << palette_filter_;

  if (!palette_matches_.empty()) {
    std::cout << "\n";
    for (size_t i = 0; i < palette_matches_.size() && i < 5; ++i) {
      if (i == palette_selection_) {
        std::cout << ApplyStyle("> " + palette_matches_[i], style);
      } else {
        std::cout << "  " << palette_matches_[i];
      }
      std::cout << "\n";
    }
  }
}

void EnhancedTUI::DrawChatArea() {
  TUIStyle style = GetStyle(TUIComponent::kChatArea);

  // Draw chat history
  int start_line = layout_.header_height + 1;
  int end_line = start_line + layout_.chat_height;

  for (int line = start_line;
       line < end_line &&
       line - start_line < static_cast<int>(output_history_.size());
       ++line) {
    std::cout << "\033[" << line << ";1H";

    int history_index = output_history_.size() - (end_line - line);
    if (history_index >= 0 &&
        history_index < static_cast<int>(output_history_.size())) {
      std::string history_line = output_history_[history_index];
      std::string truncated =
          TruncateText(history_line, terminal_width_ - layout_.sidebar_width);
      std::cout << truncated;
    }
  }

  // Draw input prompt
  std::cout << "\033[" << end_line << ";1H";
  std::cout << ApplyStyle(config_.prompt_style, style);
  std::cout << current_input_;

  // Position cursor
  std::cout << "\033[" << end_line << ";"
            << (config_.prompt_style.length() + current_input_.length() + 1)
            << "H";
}

void EnhancedTUI::DrawToolOutput() {
  // Tool output is integrated into the chat area
  // This method can be extended for a dedicated tool output panel
}

void EnhancedTUI::DrawStatusBar() {
  TUIStyle style = GetStyle(TUIComponent::kStatusBar);

  std::cout << "\033[" << terminal_height_ - 1 << ";1H";

  std::string status_text =
      absl::StrFormat("Commands: %d | History: %d | Mode: %s",
                      static_cast<int>(commands_.size()),
                      static_cast<int>(command_history_.size()),
                      in_command_palette_ ? "Palette" : "Normal");

  std::cout << ApplyStyle(status_text, style);

  // Fill remaining width
  int remaining = terminal_width_ - status_text.length();
  for (int i = 0; i < remaining; ++i) {
    std::cout << " ";
  }
}

void EnhancedTUI::DrawSidebar() {
  TUIStyle style = GetStyle(TUIComponent::kSidebar);

  int sidebar_start = terminal_width_ - layout_.sidebar_width + 1;

  std::cout << "\033[" << layout_.header_height + 1 << ";" << sidebar_start
            << "H";
  std::cout << ApplyStyle("ROM Info", style) << "\n";

  if (rom_context_) {
    std::cout << "\033[" << layout_.header_height + 2 << ";" << sidebar_start
              << "H";
    std::cout << "Size: " << rom_context_->size() << " bytes\n";
    std::cout << "\033[" << layout_.header_height + 3 << ";" << sidebar_start
              << "H";
    std::cout << "Loaded: Yes\n";
  } else {
    std::cout << "\033[" << layout_.header_height + 2 << ";" << sidebar_start
              << "H";
    std::cout << "No ROM loaded\n";
  }

  // Draw shortcuts
  std::cout << "\033[" << layout_.header_height + 5 << ";" << sidebar_start
            << "H";
  std::cout << ApplyStyle("Shortcuts", style) << "\n";
  std::cout << "\033[" << layout_.header_height + 6 << ";" << sidebar_start
            << "H";
  std::cout << "Ctrl+P: Command Palette\n";
  std::cout << "\033[" << layout_.header_height + 7 << ";" << sidebar_start
            << "H";
  std::cout << "Tab: Autocomplete\n";
  std::cout << "\033[" << layout_.header_height + 8 << ";" << sidebar_start
            << "H";
  std::cout << "Esc: Exit Palette\n";
}

absl::Status EnhancedTUI::HandleInput() {
  int key = ReadChar();

  HandleKeyPress(key);

  return absl::OkStatus();
}

void EnhancedTUI::HandleKeyPress(int key) {
  if (in_command_palette_) {
    HandleCommandPaletteKey(key);
  } else {
    HandleNormalKey(key);
  }
}

void EnhancedTUI::HandleNormalKey(int key) {
  switch (key) {
    case KEY_ENTER:
      if (!current_input_.empty()) {
        ProcessCommand(current_input_);
        command_history_.push_back(current_input_);
        current_input_.clear();
      }
      break;

    case KEY_BACKSPACE:
      if (!current_input_.empty()) {
        current_input_.pop_back();
      }
      break;

    case KEY_TAB:
      if (config_.enable_autocomplete) {
        auto suggestions = GetCommandSuggestions(current_input_);
        if (!suggestions.empty()) {
          current_input_ = suggestions[0];
        }
      }
      break;

    case KEY_ESC:
      // Could be used for command palette
      break;

    default:
      if (key >= 32 && key <= 126) {  // Printable characters
        current_input_ += static_cast<char>(key);
      }
      break;
  }
}

void EnhancedTUI::HandleCommandPaletteKey(int key) {
  switch (key) {
    case KEY_ENTER:
      if (!palette_matches_.empty() &&
          palette_selection_ < static_cast<int>(palette_matches_.size())) {
        current_input_ = palette_matches_[palette_selection_];
        in_command_palette_ = false;
      }
      break;

    case KEY_ESC:
      in_command_palette_ = false;
      break;

    case KEY_UP:
      if (palette_selection_ > 0) {
        palette_selection_--;
      }
      break;

    case KEY_DOWN:
      if (palette_selection_ < static_cast<int>(palette_matches_.size()) - 1) {
        palette_selection_++;
      }
      break;

    case KEY_BACKSPACE:
      if (!palette_filter_.empty()) {
        palette_filter_.pop_back();
        UpdatePaletteMatches();
      }
      break;

    default:
      if (key >= 32 && key <= 126) {
        palette_filter_ += static_cast<char>(key);
        UpdatePaletteMatches();
      }
      break;
  }
}

void EnhancedTUI::UpdatePaletteMatches() {
  palette_matches_.clear();
  palette_selection_ = 0;

  for (const auto& [command, _] : commands_) {
    if (absl::AsciiStrToLower(command).find(
            absl::AsciiStrToLower(palette_filter_)) != std::string::npos) {
      palette_matches_.push_back(command);
    }
  }

  std::sort(palette_matches_.begin(), palette_matches_.end());
}

absl::Status EnhancedTUI::ProcessCommand(const std::string& input) {
  std::vector<std::string> parts =
      absl::StrSplit(input, ' ', absl::SkipEmpty());
  if (parts.empty()) {
    return absl::OkStatus();
  }

  std::string command = parts[0];
  std::vector<std::string> args(parts.begin() + 1, parts.end());

  if (commands_.find(command) != commands_.end()) {
    return commands_[command](args);
  } else {
    DisplayMessage("Unknown command: " + command, "System", true);
    return absl::NotFoundError("Unknown command: " + command);
  }
}

std::vector<std::string> EnhancedTUI::GetCommandSuggestions(
    const std::string& partial) {
  std::vector<std::string> suggestions;

  for (const auto& [command, _] : commands_) {
    if (absl::AsciiStrToLower(command).find(absl::AsciiStrToLower(partial)) ==
        0) {
      suggestions.push_back(command);
    }
  }

  std::sort(suggestions.begin(), suggestions.end());
  return suggestions;
}

TUIStyle EnhancedTUI::GetStyle(TUIComponent component) const {
  auto it = styles_.find(component);
  if (it != styles_.end()) {
    return it->second;
  }

  // Default style
  return TUIStyle{FG_WHITE, BG_BLACK, FG_CYAN, FG_BRIGHT_BLACK};
}

std::string EnhancedTUI::ApplyStyle(const std::string& text,
                                    const TUIStyle& style) const {
  std::string result;

  if (!style.foreground_color.empty()) {
    result += style.foreground_color;
  }

  if (!style.background_color.empty()) {
    result += style.background_color;
  }

  if (style.bold) {
    result += BOLD;
  }

  if (style.italic) {
    result += ITALIC;
  }

  if (style.underline) {
    result += UNDERLINE;
  }

  result += text;
  result += RESET;

  return result;
}

void EnhancedTUI::LoadTheme(TUITheme theme) {
  switch (theme) {
    case TUITheme::kDefault:
      styles_[TUIComponent::kHeader] = {FG_BRIGHT_WHITE, BG_BLUE, FG_CYAN,
                                        FG_BRIGHT_BLACK, true};
      styles_[TUIComponent::kChatArea] = {FG_WHITE, BG_BLACK, FG_GREEN,
                                          FG_BRIGHT_BLACK};
      styles_[TUIComponent::kStatusBar] = {FG_BRIGHT_BLACK, BG_BLACK, FG_CYAN,
                                           FG_BRIGHT_BLACK};
      styles_[TUIComponent::kSidebar] = {FG_BRIGHT_WHITE, BG_BLACK, FG_YELLOW,
                                         FG_BRIGHT_BLACK};
      break;

    case TUITheme::kDark:
      styles_[TUIComponent::kHeader] = {FG_BRIGHT_WHITE, BG_BLACK, FG_CYAN,
                                        FG_BRIGHT_BLACK, true};
      styles_[TUIComponent::kChatArea] = {FG_BRIGHT_WHITE, BG_BLACK, FG_GREEN,
                                          FG_BRIGHT_BLACK};
      styles_[TUIComponent::kStatusBar] = {FG_BRIGHT_BLACK, BG_BLACK, FG_CYAN,
                                           FG_BRIGHT_BLACK};
      styles_[TUIComponent::kSidebar] = {FG_BRIGHT_WHITE, BG_BRIGHT_BLACK,
                                         FG_YELLOW, FG_BRIGHT_BLACK};
      break;

    case TUITheme::kZelda:
      styles_[TUIComponent::kHeader] = {FG_BRIGHT_GREEN, BG_BLACK, FG_YELLOW,
                                        FG_GREEN, true};
      styles_[TUIComponent::kChatArea] = {FG_WHITE, BG_BLACK, FG_GREEN,
                                          FG_BRIGHT_BLACK};
      styles_[TUIComponent::kStatusBar] = {FG_GREEN, BG_BLACK, FG_YELLOW,
                                           FG_BRIGHT_BLACK};
      styles_[TUIComponent::kSidebar] = {FG_BRIGHT_GREEN, BG_BLACK, FG_YELLOW,
                                         FG_BRIGHT_BLACK};
      break;

    default:
      LoadTheme(TUITheme::kDefault);
      break;
  }
}

std::string EnhancedTUI::FormatTimestamp() const {
  if (!config_.show_timestamps) {
    return "";
  }

  auto now = std::time(nullptr);
  auto tm = *std::localtime(&now);

  std::ostringstream oss;
  oss << std::put_time(&tm, "%H:%M:%S");
  return "[" + oss.str() + "]";
}

std::string EnhancedTUI::TruncateText(const std::string& text,
                                      int max_width) const {
  if (static_cast<int>(text.length()) <= max_width) {
    return text;
  }

  return text.substr(0, max_width - 3) + "...";
}

std::vector<std::string> EnhancedTUI::WrapText(const std::string& text,
                                               int width) const {
  std::vector<std::string> lines;
  std::vector<std::string> words = absl::StrSplit(text, ' ', absl::SkipEmpty());

  std::string current_line;
  for (const auto& word : words) {
    if (static_cast<int>(current_line.length() + word.length() + 1) <= width) {
      if (!current_line.empty()) {
        current_line += " ";
      }
      current_line += word;
    } else {
      if (!current_line.empty()) {
        lines.push_back(current_line);
        current_line = word;
      } else {
        lines.push_back(word);
      }
    }
  }

  if (!current_line.empty()) {
    lines.push_back(current_line);
  }

  return lines;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
