#ifndef YAZE_SRC_CLI_UTIL_TERMINAL_COLORS_H_
#define YAZE_SRC_CLI_UTIL_TERMINAL_COLORS_H_

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace yaze {
namespace cli {
namespace util {

// ANSI color codes
namespace colors {
constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kDim = "\033[2m";

// Regular colors
constexpr const char* kBlack = "\033[30m";
constexpr const char* kRed = "\033[31m";
constexpr const char* kGreen = "\033[32m";
constexpr const char* kYellow = "\033[33m";
constexpr const char* kBlue = "\033[34m";
constexpr const char* kMagenta = "\033[35m";
constexpr const char* kCyan = "\033[36m";
constexpr const char* kWhite = "\033[37m";

// Bright colors
constexpr const char* kBrightBlack = "\033[90m";
constexpr const char* kBrightRed = "\033[91m";
constexpr const char* kBrightGreen = "\033[92m";
constexpr const char* kBrightYellow = "\033[93m";
constexpr const char* kBrightBlue = "\033[94m";
constexpr const char* kBrightMagenta = "\033[95m";
constexpr const char* kBrightCyan = "\033[96m";
constexpr const char* kBrightWhite = "\033[97m";

// Background colors
constexpr const char* kBgBlack = "\033[40m";
constexpr const char* kBgRed = "\033[41m";
constexpr const char* kBgGreen = "\033[42m";
constexpr const char* kBgYellow = "\033[43m";
constexpr const char* kBgBlue = "\033[44m";
constexpr const char* kBgMagenta = "\033[45m";
constexpr const char* kBgCyan = "\033[46m";
constexpr const char* kBgWhite = "\033[47m";
}  // namespace colors

// Icon set
namespace icons {
constexpr const char* kSuccess = "✓";
constexpr const char* kError = "✗";
constexpr const char* kWarning = "⚠";
constexpr const char* kInfo = "ℹ";
constexpr const char* kSpinner = "◐◓◑◒";
constexpr const char* kRobot = "🤖";
constexpr const char* kTool = "🔧";
constexpr const char* kThinking = "💭";
constexpr const char* kArrow = "→";
}  // namespace icons

// Simple loading indicator
class LoadingIndicator {
 public:
  LoadingIndicator(const std::string& message, bool show = true)
      : message_(message), show_(show), running_(false) {}

  ~LoadingIndicator() { Stop(); }

  void Start() {
    if (!show_ || running_)
      return;
    running_ = true;

    thread_ = std::thread([this]() {
      const char* spinner[] = {"⠋", "⠙", "⠹", "⠸", "⠼",
                               "⠴", "⠦", "⠧", "⠇", "⠏"};
      int idx = 0;

      while (running_) {
        std::cout << "\r" << colors::kCyan << spinner[idx] << " "
                  << colors::kBold << message_ << colors::kReset << std::flush;
        idx = (idx + 1) % 10;
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
      }

      // Clear the line
      std::cout << "\r" << std::string(message_.length() + 10, ' ') << "\r"
                << std::flush;
    });
  }

  void Stop() {
    if (running_) {
      running_ = false;
      if (thread_.joinable()) {
        thread_.join();
      }
    }
  }

  void UpdateMessage(const std::string& message) { message_ = message; }

 private:
  std::string message_;
  bool show_;
  bool running_;
  std::thread thread_;
};

// Utility functions for colored output
inline void PrintSuccess(const std::string& message) {
  std::cout << colors::kGreen << icons::kSuccess << " " << message
            << colors::kReset << std::endl;
}

inline void PrintError(const std::string& message) {
  std::cerr << colors::kRed << icons::kError << " " << message << colors::kReset
            << std::endl;
}

inline void PrintWarning(const std::string& message) {
  std::cerr << colors::kYellow << icons::kWarning << " " << message
            << colors::kReset << std::endl;
}

inline void PrintInfo(const std::string& message) {
  std::cout << colors::kBlue << icons::kInfo << " " << message << colors::kReset
            << std::endl;
}

inline void PrintToolCall(const std::string& tool_name,
                          const std::string& details = "") {
  std::cout << colors::kMagenta << icons::kTool << " " << colors::kBold
            << "Calling tool: " << colors::kReset << colors::kCyan << tool_name
            << colors::kReset;
  if (!details.empty()) {
    std::cout << colors::kDim << " (" << details << ")" << colors::kReset;
  }
  std::cout << std::endl;
}

inline void PrintThinking(const std::string& message = "Processing...") {
  std::cout << colors::kYellow << icons::kThinking << " " << colors::kDim
            << message << colors::kReset << std::endl;
}

inline void PrintSeparator() {
  std::cout << colors::kDim << "─────────────────────────────────────────"
            << colors::kReset << std::endl;
}

}  // namespace util
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_UTIL_TERMINAL_COLORS_H_
