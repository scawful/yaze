#ifndef YAZE_CLI_SERVICE_AGENT_VIM_MODE_H_
#define YAZE_CLI_SERVICE_AGENT_VIM_MODE_H_

#include <string>
#include <vector>
#include <functional>

namespace yaze {
namespace cli {
namespace agent {

/**
 * @enum VimModeType
 * @brief Vim editing modes
 */
enum class VimModeType {
  NORMAL,      // Command mode (ESC)
  INSERT,      // Insert mode (i, a, o, etc.)
  VISUAL,      // Visual selection mode (v)
  COMMAND_LINE // Ex command mode (:)
};

/**
 * @class VimMode
 * @brief Vim-style line editing for z3ed CLI chat
 * 
 * Provides vim keybindings for enhanced terminal UX:
 * - Normal mode: hjkl navigation, dd, yy, p, u
 * - Insert mode: Regular text input
 * - Command history: Ctrl+P, Ctrl+N or j/k in normal mode
 * - Tab completion for commands
 * - Syntax highlighting for code blocks
 */
class VimMode {
 public:
  VimMode();
  
  /**
   * @brief Process a key press
   * @param ch Character input
   * @return True if the line is complete (Enter pressed in insert mode)
   */
  bool ProcessKey(int ch);
  
  /**
   * @brief Get the current line being edited
   */
  std::string GetLine() const { return current_line_; }
  
  /**
   * @brief Get the current mode
   */
  VimModeType GetMode() const { return mode_; }
  
  /**
   * @brief Get mode string for display
   */
  std::string GetModeString() const;
  
  /**
   * @brief Get cursor position
   */
  int GetCursorPos() const { return cursor_pos_; }
  
  /**
   * @brief Reset for new line
   */
  void Reset();
  
  /**
   * @brief Add line to history
   */
  void AddToHistory(const std::string& line);
  
  /**
   * @brief Set autocomplete callback
   */
  void SetAutoCompleteCallback(std::function<std::vector<std::string>(const std::string&)> callback) {
    autocomplete_callback_ = callback;
  }
  
  /**
   * @brief Set command suggestion callback
   */
  void SetCommandSuggestionCallback(std::function<std::string(const std::string&)> callback) {
    command_suggestion_callback_ = callback;
  }
  
  /**
   * @brief Render the current line with syntax highlighting
   */
  void Render() const;
  
 private:
  // Mode handling
  void SwitchMode(VimModeType new_mode);
  
  // Normal mode commands
  void HandleNormalMode(int ch);
  void MoveLeft();
  void MoveRight();
  void MoveWordForward();
  void MoveWordBackward();
  void MoveToLineStart();
  void MoveToLineEnd();
  void DeleteChar();
  void DeleteLine();
  void YankLine();
  void PasteBefore();
  void PasteAfter();
  void Undo();
  void Redo();
  
  // Insert mode commands
  void HandleInsertMode(int ch);
  void InsertChar(char c);
  void Backspace();
  void Delete();
  void Complete();
  
  // History navigation
  void HistoryPrev();
  void HistoryNext();
  
  // Visual feedback
  void ShowSuggestion() const;
  
  VimModeType mode_ = VimModeType::INSERT;
  std::string current_line_;
  int cursor_pos_ = 0;
  
  // History
  std::vector<std::string> history_;
  int history_index_ = -1;
  
  // Undo/redo
  std::vector<std::string> undo_stack_;
  std::vector<std::string> redo_stack_;
  
  // Yank buffer
  std::string yank_buffer_;
  
  // Autocomplete
  std::function<std::vector<std::string>(const std::string&)> autocomplete_callback_;
  std::function<std::string(const std::string&)> command_suggestion_callback_;
  std::vector<std::string> autocomplete_options_;
  int autocomplete_index_ = 0;
  
  // State
  bool line_complete_ = false;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AGENT_VIM_MODE_H_
