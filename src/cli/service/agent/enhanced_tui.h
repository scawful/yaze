#ifndef YAZE_SRC_CLI_SERVICE_AGENT_ENHANCED_TUI_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_ENHANCED_TUI_H_

#include <string>
#include <vector>
#include <map>
#include <functional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {

class Rom;

namespace cli {
namespace agent {

/**
 * @enum TUITheme
 * @brief Visual themes for the enhanced TUI
 */
enum class TUITheme {
  kDefault,     // Default terminal colors
  kDark,        // Dark theme with bright accents
  kLight,       // Light theme with dark text
  kZelda,       // Zelda-themed colors (green/gold)
  kCyberpunk    // Cyberpunk theme (neon colors)
};

/**
 * @enum TUIComponent
 * @brief Different UI components in the enhanced TUI
 */
enum class TUIComponent {
  kHeader,           // Top header with title and status
  kCommandPalette,   // Command palette with fuzzy search
  kChatArea,         // Main chat conversation area
  kToolOutput,       // Tool execution results
  kStatusBar,        // Bottom status bar
  kSidebar,          // Sidebar with ROM info and shortcuts
  kHelpPanel,        // Context-sensitive help panel
  kHistoryPanel      // Command history and suggestions
};

/**
 * @struct TUIStyle
 * @brief Visual styling configuration for TUI components
 */
struct TUIStyle {
  std::string foreground_color;
  std::string background_color;
  std::string accent_color;
  std::string border_color;
  bool bold = false;
  bool italic = false;
  bool underline = false;
};

/**
 * @struct TUIConfig
 * @brief Configuration for the enhanced TUI
 */
struct TUIConfig {
  TUITheme theme = TUITheme::kDefault;
  bool enable_syntax_highlighting = true;
  bool enable_autocomplete = true;
  bool enable_fuzzy_search = true;
  bool enable_mouse_support = false;
  bool enable_transparency = false;
  int max_history_size = 1000;
  int max_output_lines = 10000;
  bool auto_scroll = true;
  bool show_timestamps = true;
  bool show_command_hints = true;
  bool enable_shortcuts = true;
  std::string prompt_style = ">>> ";
  std::string continuation_prompt = "... ";
};

/**
 * @class EnhancedTUI
 * @brief Enhanced Terminal User Interface for z3ed CLI
 * 
 * Provides a modern, feature-rich TUI with:
 * - Multi-panel layout with resizable components
 * - Syntax highlighting for code and JSON
 * - Fuzzy search and autocomplete
 * - Command palette with shortcuts
 * - Rich output formatting with colors and tables
 * - Mouse support (optional)
 * - Customizable themes
 * - Real-time command suggestions
 * - History navigation and search
 * - Tool output integration
 * - Context-sensitive help
 */
class EnhancedTUI {
 public:
  explicit EnhancedTUI(const TUIConfig& config = TUIConfig{});
  ~EnhancedTUI();

  // Initialize the TUI (setup terminal, colors, etc.)
  absl::Status Initialize();
  
  // Cleanup and restore terminal state
  void Shutdown();

  // Set ROM context for command execution
  void SetRomContext(Rom* rom);

  // Main event loop
  absl::Status Run();

  // Display a message in the chat area
  void DisplayMessage(const std::string& message, 
                     const std::string& sender = "User",
                     bool is_error = false);

  // Display tool output in the tool area
  void DisplayToolOutput(const std::string& output, 
                        const std::string& tool_name);

  // Display command suggestions
  void DisplaySuggestions(const std::vector<std::string>& suggestions);

  // Update status bar with current information
  void UpdateStatusBar(const std::string& status);

  // Show help panel for a specific command
  void ShowHelp(const std::string& command);

  // Register a command handler
  void RegisterCommand(const std::string& name,
                      std::function<absl::Status(const std::vector<std::string>&)> handler,
                      const std::string& description = "");

  // Get current configuration
  const TUIConfig& GetConfig() const { return config_; }

  // Update configuration
  void SetConfig(const TUIConfig& config);

 private:
  // Terminal control
  void SetupTerminal();
  void RestoreTerminal();
  void ClearScreen();
  void RefreshDisplay();

  // Layout management
  void CalculateLayout();
  void DrawHeader();
  void DrawCommandPalette();
  void DrawChatArea();
  void DrawToolOutput();
  void DrawStatusBar();
  void DrawSidebar();
  void DrawHelpPanel();

  // Input handling
  absl::Status HandleInput();
  void HandleKeyPress(int key);
  void HandleMouseEvent(int x, int y, int button);
  void HandleNormalKey(int key);
  void HandleCommandPaletteKey(int key);
  void UpdatePaletteMatches();

  // Command processing
  absl::Status ProcessCommand(const std::string& input);
  std::vector<std::string> GetCommandSuggestions(const std::string& partial);
  void ExecuteCommand(const std::string& command, const std::vector<std::string>& args);

  // Styling and theming
  TUIStyle GetStyle(TUIComponent component) const;
  std::string ApplyStyle(const std::string& text, const TUIStyle& style) const;
  void LoadTheme(TUITheme theme);

  // Utility functions
  std::string FormatTimestamp() const;
  std::string TruncateText(const std::string& text, int max_width) const;
  std::vector<std::string> WrapText(const std::string& text, int width) const;

  TUIConfig config_;
  Rom* rom_context_ = nullptr;
  
  // Terminal state
  int terminal_width_ = 80;
  int terminal_height_ = 24;
  bool terminal_initialized_ = false;
  
  // Layout state
  struct Layout {
    int header_height = 3;
    int status_height = 2;
    int sidebar_width = 30;
    int help_width = 40;
    int chat_height = 15;
    int tool_height = 8;
  } layout_;

  // UI state
  std::string current_input_;
  std::vector<std::string> command_history_;
  std::vector<std::string> output_history_;
  std::map<std::string, std::function<absl::Status(const std::vector<std::string>&)>> commands_;
  std::map<std::string, std::string> command_descriptions_;
  
  // Styling
  std::map<TUIComponent, TUIStyle> styles_;
  std::map<TUITheme, std::map<TUIComponent, TUIStyle>> themes_;

  // Input state
  int cursor_x_ = 0;
  int cursor_y_ = 0;
  bool in_command_palette_ = false;
  std::string palette_filter_;
  std::vector<std::string> palette_matches_;
  int palette_selection_ = 0;
};

/**
 * @class TUICommandHandler
 * @brief Base class for TUI-integrated command handlers
 * 
 * Extends CommandHandler with TUI-specific features:
 * - Rich output formatting
 * - Progress indicators
 * - Interactive prompts
 * - Real-time updates
 */
class TUICommandHandler : public resources::CommandHandler {
 public:
  explicit TUICommandHandler(EnhancedTUI* tui) : tui_(tui) {}

 protected:
  // Override to provide TUI-specific output
  virtual void DisplayProgress(const std::string& message) {
    if (tui_) {
      tui_->UpdateStatusBar(message);
    }
  }

  virtual void DisplayRichOutput(const std::string& output) {
    if (tui_) {
      tui_->DisplayToolOutput(output, GetCommandName());
    }
  }

  virtual absl::StatusOr<std::string> PromptUser(const std::string& /* prompt */) {
    // TODO: Implement interactive prompting in TUI
    return absl::UnimplementedError("Interactive prompting not yet implemented");
  }

  virtual std::string GetCommandName() const = 0;

 private:
  EnhancedTUI* tui_ = nullptr;
};

/**
 * @class TUIAutocomplete
 * @brief Advanced autocomplete system for the TUI
 */
class TUIAutocomplete {
 public:
  TUIAutocomplete();

  // Add a command to the autocomplete database
  void AddCommand(const std::string& command, const std::string& description);

  // Get completions for a partial input
  std::vector<std::string> GetCompletions(const std::string& partial);

  // Get fuzzy matches for a query
  std::vector<std::string> GetFuzzyMatches(const std::string& query);

  // Learn from user input patterns
  void LearnFromInput(const std::string& input);

 private:
  std::map<std::string, std::string> commands_;
  std::map<std::string, int> usage_count_;
  std::vector<std::string> recent_commands_;
};

// Helper function to convert TUITheme to string
std::string TUIThemeToString(TUITheme theme);

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_ENHANCED_TUI_H_
