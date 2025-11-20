#include "cli/service/agent/vim_mode.h"

#include <algorithm>
#include <cctype>
#include <iostream>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace yaze {
namespace cli {
namespace agent {

namespace {

// Key codes for special keys
constexpr int KEY_ESC = 27;
constexpr int KEY_ENTER = 10;
constexpr int KEY_BACKSPACE = 127;
constexpr int KEY_CTRL_P = 16;
constexpr int KEY_CTRL_N = 14;
constexpr int KEY_TAB = 9;

// Terminal control sequences
const char* CLEAR_LINE = "\033[2K\r";
const char* MOVE_CURSOR_HOME = "\r";
const char* SAVE_CURSOR = "\033[s";
const char* RESTORE_CURSOR = "\033[u";

#ifndef _WIN32
// Set terminal to raw mode (character-by-character input)
void SetRawMode(bool enable) {
  static struct termios orig_termios;
  static bool has_orig = false;

  if (enable) {
    if (!has_orig) {
      tcgetattr(STDIN_FILENO, &orig_termios);
      has_orig = true;
    }

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);  // Disable echo and canonical mode
    raw.c_cc[VMIN] = 1;               // Read at least 1 character
    raw.c_cc[VTIME] = 0;              // No timeout
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  } else {
    if (has_orig) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    }
  }
}
#endif

}  // namespace

VimMode::VimMode() {
#ifndef _WIN32
  SetRawMode(true);
#endif
}

std::string VimMode::GetModeString() const {
  switch (mode_) {
    case VimModeType::NORMAL:
      return "NORMAL";
    case VimModeType::INSERT:
      return "INSERT";
    case VimModeType::VISUAL:
      return "VISUAL";
    case VimModeType::COMMAND_LINE:
      return "COMMAND";
  }
  return "UNKNOWN";
}

void VimMode::Reset() {
  current_line_.clear();
  cursor_pos_ = 0;
  line_complete_ = false;
  redo_stack_.clear();
}

void VimMode::AddToHistory(const std::string& line) {
  if (!line.empty()) {
    history_.push_back(line);
    history_index_ = -1;  // Reset to no history selection
  }
}

bool VimMode::ProcessKey(int ch) {
  line_complete_ = false;

  switch (mode_) {
    case VimModeType::NORMAL:
      HandleNormalMode(ch);
      break;
    case VimModeType::INSERT:
      HandleInsertMode(ch);
      break;
    case VimModeType::VISUAL:
      // Visual mode not yet implemented
      SwitchMode(VimModeType::NORMAL);
      break;
    case VimModeType::COMMAND_LINE:
      // Command line mode not yet implemented
      if (ch == KEY_ESC) {
        SwitchMode(VimModeType::NORMAL);
      }
      break;
  }

  return line_complete_;
}

void VimMode::SwitchMode(VimModeType new_mode) {
  if (new_mode != mode_) {
    mode_ = new_mode;
    Render();  // Redraw with new mode indicator
  }
}

void VimMode::HandleNormalMode(int ch) {
  switch (ch) {
    // Enter insert mode
    case 'i':
      SwitchMode(VimModeType::INSERT);
      break;
    case 'a':
      MoveRight();
      SwitchMode(VimModeType::INSERT);
      break;
    case 'o':
      // Insert new line below (just append and go to insert mode)
      MoveToLineEnd();
      SwitchMode(VimModeType::INSERT);
      break;
    case 'O':
      // Insert new line above (go to beginning and insert mode)
      MoveToLineStart();
      SwitchMode(VimModeType::INSERT);
      break;

    // Movement
    case 'h':
      MoveLeft();
      break;
    case 'l':
      MoveRight();
      break;
    case 'w':
      MoveWordForward();
      break;
    case 'b':
      MoveWordBackward();
      break;
    case '0':
      MoveToLineStart();
      break;
    case '$':
      MoveToLineEnd();
      break;

    // Editing
    case 'x':
      DeleteChar();
      break;
    case 'd':
      // Simple implementation: dd deletes line
      {
        int next_ch;
#ifdef _WIN32
        next_ch = _getch();
#else
        read(STDIN_FILENO, &next_ch, 1);
#endif
        if (next_ch == 'd') {
          DeleteLine();
        }
      }
      break;
    case 'y':
      // yy yanks line
      {
        int next_ch;
#ifdef _WIN32
        next_ch = _getch();
#else
        read(STDIN_FILENO, &next_ch, 1);
#endif
        if (next_ch == 'y') {
          YankLine();
        }
      }
      break;
    case 'p':
      PasteAfter();
      break;
    case 'P':
      PasteBefore();
      break;
    case 'u':
      Undo();
      break;
    case KEY_CTRL_P:
    case 'k':
      HistoryPrev();
      break;
    case KEY_CTRL_N:
    case 'j':
      HistoryNext();
      break;

    // Accept line (Enter in normal mode)
    case KEY_ENTER:
      line_complete_ = true;
      break;

    // Command mode
    case ':':
      SwitchMode(VimModeType::COMMAND_LINE);
      break;
  }

  Render();
}

void VimMode::HandleInsertMode(int ch) {
  switch (ch) {
    case KEY_ESC:
      SwitchMode(VimModeType::NORMAL);
      if (cursor_pos_ > 0) {
        cursor_pos_--;  // Vim moves cursor left on ESC
      }
      break;
    case KEY_ENTER:
      line_complete_ = true;
      break;
    case KEY_BACKSPACE:
    case 8:  // Ctrl+H
      Backspace();
      break;
    case KEY_TAB:
      Complete();
      break;
    case KEY_CTRL_P:
      HistoryPrev();
      break;
    case KEY_CTRL_N:
      HistoryNext();
      break;
    default:
      if (ch >= 32 && ch < 127) {  // Printable ASCII
        InsertChar(static_cast<char>(ch));
      }
      break;
  }

  if (!line_complete_) {
    Render();
  }
}

// Movement implementations
void VimMode::MoveLeft() {
  if (cursor_pos_ > 0) {
    cursor_pos_--;
  }
}

void VimMode::MoveRight() {
  if (cursor_pos_ < static_cast<int>(current_line_.length())) {
    cursor_pos_++;
  }
}

void VimMode::MoveWordForward() {
  while (cursor_pos_ < static_cast<int>(current_line_.length()) &&
         !std::isspace(current_line_[cursor_pos_])) {
    cursor_pos_++;
  }
  while (cursor_pos_ < static_cast<int>(current_line_.length()) &&
         std::isspace(current_line_[cursor_pos_])) {
    cursor_pos_++;
  }
}

void VimMode::MoveWordBackward() {
  if (cursor_pos_ > 0) cursor_pos_--;
  while (cursor_pos_ > 0 && std::isspace(current_line_[cursor_pos_])) {
    cursor_pos_--;
  }
  while (cursor_pos_ > 0 && !std::isspace(current_line_[cursor_pos_ - 1])) {
    cursor_pos_--;
  }
}

void VimMode::MoveToLineStart() { cursor_pos_ = 0; }

void VimMode::MoveToLineEnd() { cursor_pos_ = current_line_.length(); }

// Editing implementations
void VimMode::DeleteChar() {
  if (cursor_pos_ < static_cast<int>(current_line_.length())) {
    undo_stack_.push_back(current_line_);
    current_line_.erase(cursor_pos_, 1);
  }
}

void VimMode::DeleteLine() {
  undo_stack_.push_back(current_line_);
  yank_buffer_ = current_line_;
  current_line_.clear();
  cursor_pos_ = 0;
}

void VimMode::YankLine() { yank_buffer_ = current_line_; }

void VimMode::PasteBefore() {
  if (!yank_buffer_.empty()) {
    undo_stack_.push_back(current_line_);
    current_line_.insert(cursor_pos_, yank_buffer_);
  }
}

void VimMode::PasteAfter() {
  if (!yank_buffer_.empty()) {
    undo_stack_.push_back(current_line_);
    if (cursor_pos_ < static_cast<int>(current_line_.length())) {
      cursor_pos_++;
    }
    current_line_.insert(cursor_pos_, yank_buffer_);
    cursor_pos_ += yank_buffer_.length() - 1;
  }
}

void VimMode::Undo() {
  if (!undo_stack_.empty()) {
    redo_stack_.push_back(current_line_);
    current_line_ = undo_stack_.back();
    undo_stack_.pop_back();
    cursor_pos_ =
        std::min(cursor_pos_, static_cast<int>(current_line_.length()));
  }
}

void VimMode::Redo() {
  if (!redo_stack_.empty()) {
    undo_stack_.push_back(current_line_);
    current_line_ = redo_stack_.back();
    redo_stack_.pop_back();
    cursor_pos_ =
        std::min(cursor_pos_, static_cast<int>(current_line_.length()));
  }
}

void VimMode::InsertChar(char c) {
  undo_stack_.push_back(current_line_);
  current_line_.insert(cursor_pos_, 1, c);
  cursor_pos_++;
}

void VimMode::Backspace() {
  if (cursor_pos_ > 0) {
    undo_stack_.push_back(current_line_);
    current_line_.erase(cursor_pos_ - 1, 1);
    cursor_pos_--;
  }
}

void VimMode::Delete() { DeleteChar(); }

void VimMode::Complete() {
  if (autocomplete_callback_) {
    autocomplete_options_ = autocomplete_callback_(current_line_);
    if (!autocomplete_options_.empty()) {
      // Simple implementation: insert first suggestion
      std::string completion = autocomplete_options_[0];
      undo_stack_.push_back(current_line_);
      current_line_ = completion;
      cursor_pos_ = completion.length();
    }
  }
}

// History navigation
void VimMode::HistoryPrev() {
  if (history_.empty()) return;

  if (history_index_ == -1) {
    history_index_ = history_.size() - 1;
  } else if (history_index_ > 0) {
    history_index_--;
  }

  current_line_ = history_[history_index_];
  cursor_pos_ = current_line_.length();
}

void VimMode::HistoryNext() {
  if (history_.empty() || history_index_ == -1) return;

  if (history_index_ < static_cast<int>(history_.size()) - 1) {
    history_index_++;
    current_line_ = history_[history_index_];
  } else {
    history_index_ = -1;
    current_line_.clear();
  }

  cursor_pos_ = current_line_.length();
}

void VimMode::Render() const {
  // Clear line and redraw
  std::cout << CLEAR_LINE;

  // Show mode indicator
  if (mode_ == VimModeType::INSERT) {
    std::cout << "-- INSERT -- ";
  } else if (mode_ == VimModeType::NORMAL) {
    std::cout << "-- NORMAL -- ";
  } else if (mode_ == VimModeType::COMMAND_LINE) {
    std::cout << ":";
  }

  // Show prompt and line
  std::cout << current_line_;

  // Move cursor to correct position
  int display_offset =
      (mode_ == VimModeType::INSERT ? 13 : 13);  // Length of "-- INSERT -- "
  std::cout << "\r";
  for (int i = 0; i < display_offset + cursor_pos_; ++i) {
    std::cout << "\033[C";  // Move cursor right
  }

  std::cout.flush();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
