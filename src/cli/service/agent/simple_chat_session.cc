#include "cli/service/agent/simple_chat_session.h"

#include <fstream>
#include <iostream>
#include <iomanip>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "cli/util/terminal_colors.h"

namespace yaze {
namespace cli {
namespace agent {

SimpleChatSession::SimpleChatSession() = default;

void SimpleChatSession::SetRomContext(Rom* rom) {
  agent_service_.SetRomContext(rom);
}

void SimpleChatSession::PrintTable(const ChatMessage::TableData& table) {
  if (table.headers.empty()) return;
  
  // Calculate column widths
  std::vector<size_t> col_widths(table.headers.size(), 0);
  for (size_t i = 0; i < table.headers.size(); ++i) {
    col_widths[i] = table.headers[i].length();
  }
  
  for (const auto& row : table.rows) {
    for (size_t i = 0; i < std::min(row.size(), col_widths.size()); ++i) {
      col_widths[i] = std::max(col_widths[i], row[i].length());
    }
  }
  
  // Print header
  std::cout << "  ";
  for (size_t i = 0; i < table.headers.size(); ++i) {
    std::cout << std::left << std::setw(col_widths[i] + 2) << table.headers[i];
  }
  std::cout << "\n  ";
  for (size_t i = 0; i < table.headers.size(); ++i) {
    std::cout << std::string(col_widths[i] + 2, '-');
  }
  std::cout << "\n";
  
  // Print rows
  for (const auto& row : table.rows) {
    std::cout << "  ";
    for (size_t i = 0; i < std::min(row.size(), table.headers.size()); ++i) {
      std::cout << std::left << std::setw(col_widths[i] + 2) << row[i];
    }
    std::cout << "\n";
  }
}

void SimpleChatSession::PrintMessage(const ChatMessage& msg, bool show_timestamp) {
  const char* sender = (msg.sender == ChatMessage::Sender::kUser) ? "You" : "Agent";
  
  if (show_timestamp) {
    std::string timestamp = absl::FormatTime("%H:%M:%S", msg.timestamp, absl::LocalTimeZone());
    std::cout << "[" << timestamp << "] ";
  }
  
  std::cout << sender << ": ";
  
  if (msg.table_data.has_value()) {
    std::cout << "\n";
    PrintTable(msg.table_data.value());
  } else if (msg.json_pretty.has_value()) {
    std::cout << "\n" << msg.json_pretty.value() << "\n";
  } else {
    std::cout << msg.message << "\n";
  }
}

absl::Status SimpleChatSession::SendAndWaitForResponse(
    const std::string& message, std::string* response_out) {
  auto result = agent_service_.SendMessage(message);
  if (!result.ok()) {
    return result.status();
  }
  
  const auto& response_msg = result.value();
  
  if (response_out != nullptr) {
    *response_out = response_msg.message;
  }
  
  return absl::OkStatus();
}

absl::Status SimpleChatSession::RunInteractive() {
  // Check if stdin is a TTY (interactive) or a pipe/file
  bool is_interactive = isatty(fileno(stdin));
  
  if (is_interactive) {
    std::cout << "Z3ED Agent Chat (Simple Mode)\n";
    std::cout << "Type 'quit' or 'exit' to end the session.\n";
    std::cout << "Type 'reset' to clear conversation history.\n";
    std::cout << "----------------------------------------\n\n";
  }
  
  std::string input;
  while (true) {
    if (is_interactive) {
      std::cout << "You: ";
      std::cout.flush();  // Ensure prompt is displayed before reading
    }
    
    if (!std::getline(std::cin, input)) {
      // EOF reached (piped input exhausted or Ctrl+D)
      if (is_interactive) {
        std::cout << "\n";
      }
      break;
    }
    
    if (input.empty()) continue;
    if (input == "quit" || input == "exit") break;
    
    if (input == "reset") {
      Reset();
      std::cout << "Conversation history cleared.\n\n";
      continue;
    }
    
    auto result = agent_service_.SendMessage(input);
    if (!result.ok()) {
      std::cerr << "Error: " << result.status().message() << "\n\n";
      continue;
    }
    
    PrintMessage(result.value(), false);
    std::cout << "\n";
  }
  
  return absl::OkStatus();
}

absl::Status SimpleChatSession::RunBatch(const std::string& input_file) {
  std::ifstream file(input_file);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Could not open file: %s", input_file));
  }
  
  std::cout << "Running batch session from: " << input_file << "\n";
  std::cout << "----------------------------------------\n\n";
  
  std::string line;
  int line_num = 0;
  while (std::getline(file, line)) {
    ++line_num;
    
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') continue;
    
    std::cout << "Input [" << line_num << "]: " << line << "\n";
    
    auto result = agent_service_.SendMessage(line);
    if (!result.ok()) {
      std::cerr << "Error: " << result.status().message() << "\n\n";
      continue;
    }
    
    PrintMessage(result.value(), false);
    std::cout << "\n";
  }
  
  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
