#include "cli/service/agent/simple_chat_session.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#include <conio.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/time/time.h"
#include "cli/util/terminal_colors.h"

namespace yaze {
namespace cli {
namespace agent {

SimpleChatSession::SimpleChatSession() = default;

void SimpleChatSession::SetRomContext(Rom* rom) {
  agent_service_.SetRomContext(rom);
}

namespace {

std::string EscapeJsonString(absl::string_view input) {
  std::string output;
  output.reserve(input.size());
  for (char ch : input) {
    switch (ch) {
      case '\\':
        output.append("\\\\");
        break;
      case '"':
        output.append("\\\"");
        break;
      case '\b':
        output.append("\\b");
        break;
      case '\f':
        output.append("\\f");
        break;
      case '\n':
        output.append("\\n");
        break;
      case '\r':
        output.append("\\r");
        break;
      case '\t':
        output.append("\\t");
        break;
      default: {
        unsigned char code = static_cast<unsigned char>(ch);
        if (code < 0x20) {
          absl::StrAppendFormat(&output, "\\u%04x",
                                static_cast<unsigned int>(code));
        } else {
          output.push_back(ch);
        }
        break;
      }
    }
  }
  return output;
}

std::string QuoteJson(absl::string_view value) {
  return absl::StrCat("\"", EscapeJsonString(value), "\"");
}

std::string TableToJson(const ChatMessage::TableData& table) {
  std::vector<std::string> header_entries;
  header_entries.reserve(table.headers.size());
  for (const auto& header : table.headers) {
    header_entries.push_back(QuoteJson(header));
  }

  std::vector<std::string> row_entries;
  row_entries.reserve(table.rows.size());
  for (const auto& row : table.rows) {
    std::vector<std::string> cell_entries;
    cell_entries.reserve(row.size());
    for (const auto& cell : row) {
      cell_entries.push_back(QuoteJson(cell));
    }
    row_entries.push_back(
        absl::StrCat("[", absl::StrJoin(cell_entries, ","), "]"));
  }

  return absl::StrCat("{\"headers\":[", absl::StrJoin(header_entries, ","),
                       "],\"rows\":[", absl::StrJoin(row_entries, ","),
                       "]}");
}

std::string MetricsToJson(const ChatMessage::SessionMetrics& metrics) {
  return absl::StrCat(
      "{\"turn_index\":", metrics.turn_index, ","
      "\"total_user_messages\":", metrics.total_user_messages, ","
      "\"total_agent_messages\":", metrics.total_agent_messages, ","
      "\"total_tool_calls\":", metrics.total_tool_calls, ","
      "\"total_commands\":", metrics.total_commands, ","
      "\"total_proposals\":", metrics.total_proposals, ","
      "\"total_elapsed_seconds\":", metrics.total_elapsed_seconds, ","
      "\"average_latency_seconds\":", metrics.average_latency_seconds, "}");
}

std::string MessageToJson(const ChatMessage& msg, bool show_timestamp) {
  std::string json = "{";
  absl::StrAppend(&json, "\"sender\":\"",
                  msg.sender == ChatMessage::Sender::kUser ? "user"
                                                         : "agent",
                  "\"");

  absl::StrAppend(&json, ",\"message\":", QuoteJson(msg.message));

  if (msg.json_pretty.has_value()) {
    absl::StrAppend(&json, ",\"structured\":",
                    QuoteJson(msg.json_pretty.value()));
  }

  if (msg.table_data.has_value()) {
    absl::StrAppend(&json, ",\"table\":", TableToJson(*msg.table_data));
  }

  if (msg.metrics.has_value()) {
    absl::StrAppend(&json, ",\"metrics\":",
                    MetricsToJson(*msg.metrics));
  }

  if (show_timestamp) {
    std::string timestamp =
        absl::FormatTime("%Y-%m-%dT%H:%M:%S%z", msg.timestamp,
                          absl::LocalTimeZone());
    absl::StrAppend(&json, ",\"timestamp\":", QuoteJson(timestamp));
  }

  absl::StrAppend(&json, "}");
  return json;
}

void PrintMarkdownTable(const ChatMessage::TableData& table) {
  if (table.headers.empty()) {
    return;
  }

  std::cout << "\n|";
  for (const auto& header : table.headers) {
    std::cout << " " << header << " |";
  }
  std::cout << "\n|";
  for (size_t i = 0; i < table.headers.size(); ++i) {
    std::cout << " --- |";
  }
  std::cout << "\n";

  for (const auto& row : table.rows) {
    std::cout << "|";
    for (size_t i = 0; i < table.headers.size(); ++i) {
      if (i < row.size()) {
        std::cout << " " << row[i];
      }
      std::cout << " |";
    }
    std::cout << "\n";
  }
}

void PrintMarkdownMetrics(const ChatMessage::SessionMetrics& metrics) {
  std::cout << "\n> _Turn " << metrics.turn_index
            << ": users=" << metrics.total_user_messages
            << ", agents=" << metrics.total_agent_messages
            << ", tool-calls=" << metrics.total_tool_calls
            << ", commands=" << metrics.total_commands
            << ", proposals=" << metrics.total_proposals
            << ", elapsed="
            << absl::StrFormat("%.2fs avg %.2fs",
                                metrics.total_elapsed_seconds,
                                metrics.average_latency_seconds)
            << "_\n";
}

std::string SessionMetricsToJson(const ChatMessage::SessionMetrics& metrics) {
  return MetricsToJson(metrics);
}

}  // namespace

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

void SimpleChatSession::PrintMessage(const ChatMessage& msg,
                                     bool show_timestamp) {
  switch (config_.output_format) {
    case AgentOutputFormat::kFriendly: {
      const char* sender =
          (msg.sender == ChatMessage::Sender::kUser) ? "You" : "Agent";

      if (show_timestamp) {
        std::string timestamp = absl::FormatTime(
            "%H:%M:%S", msg.timestamp, absl::LocalTimeZone());
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

      if (msg.metrics.has_value()) {
        const auto& metrics = msg.metrics.value();
        std::cout << "  📊 Turn " << metrics.turn_index
                  << " summary — users: " << metrics.total_user_messages
                  << ", agents: " << metrics.total_agent_messages
                  << ", tools: " << metrics.total_tool_calls
                  << ", commands: " << metrics.total_commands
                  << ", proposals: " << metrics.total_proposals
                  << ", elapsed: "
                  << absl::StrFormat("%.2fs avg %.2fs",
                                      metrics.total_elapsed_seconds,
                                      metrics.average_latency_seconds)
                  << "\n";
      }
      break;
    }
    case AgentOutputFormat::kCompact: {
      if (msg.json_pretty.has_value()) {
        std::cout << msg.json_pretty.value() << "\n";
      } else if (msg.table_data.has_value()) {
        PrintTable(msg.table_data.value());
      } else {
        std::cout << msg.message << "\n";
      }
      break;
    }
    case AgentOutputFormat::kMarkdown: {
      std::cout << (msg.sender == ChatMessage::Sender::kUser ? "**You:** "
                                                             : "**Agent:** ");
      if (msg.table_data.has_value()) {
        PrintMarkdownTable(msg.table_data.value());
      } else if (msg.json_pretty.has_value()) {
        std::cout << "\n```json\n" << msg.json_pretty.value()
                  << "\n```\n";
      } else {
        std::cout << msg.message << "\n";
      }

      if (msg.metrics.has_value()) {
        PrintMarkdownMetrics(*msg.metrics);
      }
      break;
    }
    case AgentOutputFormat::kJson: {
      std::cout << MessageToJson(msg, show_timestamp) << std::endl;
      break;
    }
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
  
  if (is_interactive && config_.output_format == AgentOutputFormat::kFriendly) {
    std::cout << "Z3ED Agent Chat (Simple Mode)\n";
    if (config_.enable_vim_mode) {
      std::cout << "Vim mode enabled! Use hjkl to move, i for insert, ESC for normal mode.\n";
    }
    std::cout << "Type 'quit' or 'exit' to end the session.\n";
    std::cout << "Type 'reset' to clear conversation history.\n";
    std::cout << "----------------------------------------\n\n";
  }
  
  std::string input;
  while (true) {
    // Read input with or without vim mode
    if (config_.enable_vim_mode && is_interactive) {
      input = ReadLineWithVim();
      if (input.empty()) {
        // EOF reached
        if (config_.output_format != AgentOutputFormat::kJson) {
          std::cout << "\n";
        }
        break;
      }
    } else {
      if (is_interactive && config_.output_format != AgentOutputFormat::kJson) {
        std::cout << "You: ";
        std::cout.flush();  // Ensure prompt is displayed before reading
      }
      
      if (!std::getline(std::cin, input)) {
        // EOF reached (piped input exhausted or Ctrl+D)
        if (is_interactive && config_.output_format != AgentOutputFormat::kJson) {
          std::cout << "\n";
        }
        break;
      }
    }
    
    if (input.empty()) continue;
    if (input == "quit" || input == "exit") break;
    
    if (input == "reset") {
      Reset();
      if (config_.output_format == AgentOutputFormat::kJson) {
        std::cout << "{\"event\":\"history_cleared\"}" << std::endl;
      } else if (config_.output_format == AgentOutputFormat::kMarkdown) {
        std::cout << "> Conversation history cleared.\n\n";
      } else {
        std::cout << "Conversation history cleared.\n\n";
      }
      continue;
    }
    
    auto result = agent_service_.SendMessage(input);
    if (!result.ok()) {
      if (config_.output_format == AgentOutputFormat::kJson) {
        std::cout << absl::StrCat(
                         "{\"event\":\"error\",\"message\":",
                         QuoteJson(result.status().message()), "}")
                  << std::endl;
      } else if (config_.output_format == AgentOutputFormat::kMarkdown) {
        std::cout << "> **Error:** " << result.status().message() << "\n\n";
      } else if (config_.output_format == AgentOutputFormat::kCompact) {
        std::cout << "error: " << result.status().message() << "\n";
      } else {
        std::cerr << "Error: " << result.status().message() << "\n\n";
      }
      continue;
    }
    
    PrintMessage(result.value(), false);
    if (config_.output_format != AgentOutputFormat::kJson) {
      std::cout << "\n";
    }
  }

  const auto metrics = agent_service_.GetMetrics();
  if (config_.output_format == AgentOutputFormat::kJson) {
    std::cout << absl::StrCat("{\"event\":\"session_summary\",\"metrics\":",
                               SessionMetricsToJson(metrics), "}")
              << std::endl;
  } else if (config_.output_format == AgentOutputFormat::kMarkdown) {
    std::cout << "\n> **Session totals**  ";
    std::cout << "turns=" << metrics.turn_index << ", users="
              << metrics.total_user_messages << ", agents="
              << metrics.total_agent_messages << ", tools="
              << metrics.total_tool_calls << ", commands="
              << metrics.total_commands << ", proposals="
              << metrics.total_proposals << ", elapsed="
              << absl::StrFormat("%.2fs avg %.2fs",
                                  metrics.total_elapsed_seconds,
                                  metrics.average_latency_seconds)
              << "\n\n";
  } else {
    std::cout << "Session totals — turns: " << metrics.turn_index
              << ", user messages: " << metrics.total_user_messages
              << ", agent messages: " << metrics.total_agent_messages
              << ", tool calls: " << metrics.total_tool_calls
              << ", commands: " << metrics.total_commands
              << ", proposals: " << metrics.total_proposals
              << ", elapsed: "
              << absl::StrFormat("%.2fs avg %.2fs\n\n",
                                 metrics.total_elapsed_seconds,
                                 metrics.average_latency_seconds);
  }
  
  return absl::OkStatus();
}

absl::Status SimpleChatSession::RunBatch(const std::string& input_file) {
  std::ifstream file(input_file);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Could not open file: %s", input_file));
  }
  
  if (config_.output_format == AgentOutputFormat::kFriendly) {
    std::cout << "Running batch session from: " << input_file << "\n";
    std::cout << "----------------------------------------\n\n";
  } else if (config_.output_format == AgentOutputFormat::kMarkdown) {
    std::cout << "### Batch session: " << input_file << "\n\n";
  }
  
  std::string line;
  int line_num = 0;
  while (std::getline(file, line)) {
    ++line_num;
    
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') continue;
    
    if (config_.output_format == AgentOutputFormat::kFriendly) {
      std::cout << "Input [" << line_num << "]: " << line << "\n";
    } else if (config_.output_format == AgentOutputFormat::kMarkdown) {
      std::cout << "- **Input " << line_num << "**: " << line << "\n";
    } else if (config_.output_format == AgentOutputFormat::kJson) {
      std::cout << absl::StrCat(
                       "{\"event\":\"batch_input\",\"index\":",
                       line_num, ",\"prompt\":", QuoteJson(line), "}")
                << std::endl;
    }
    
    auto result = agent_service_.SendMessage(line);
    if (!result.ok()) {
      if (config_.output_format == AgentOutputFormat::kJson) {
        std::cout << absl::StrCat(
                         "{\"event\":\"error\",\"index\":", line_num,
                         ",\"message\":",
                         QuoteJson(result.status().message()), "}")
                  << std::endl;
      } else if (config_.output_format == AgentOutputFormat::kMarkdown) {
        std::cout << "  - ⚠️ " << result.status().message() << "\n";
      } else if (config_.output_format == AgentOutputFormat::kCompact) {
        std::cout << "error@" << line_num << ": "
                  << result.status().message() << "\n";
      } else {
        std::cerr << "Error: " << result.status().message() << "\n\n";
      }
      continue;
    }
    
    PrintMessage(result.value(), false);
    if (config_.output_format != AgentOutputFormat::kJson) {
      std::cout << "\n";
    }
  }

  const auto metrics = agent_service_.GetMetrics();
  if (config_.output_format == AgentOutputFormat::kJson) {
    std::cout << absl::StrCat("{\"event\":\"session_summary\",\"metrics\":",
                               SessionMetricsToJson(metrics), "}")
              << std::endl;
  } else if (config_.output_format == AgentOutputFormat::kMarkdown) {
    std::cout << "\n> **Batch totals**  turns=" << metrics.turn_index
              << ", users=" << metrics.total_user_messages << ", agents="
              << metrics.total_agent_messages << ", tools="
              << metrics.total_tool_calls << ", commands="
              << metrics.total_commands << ", proposals="
              << metrics.total_proposals << ", elapsed="
              << absl::StrFormat("%.2fs avg %.2fs",
                                  metrics.total_elapsed_seconds,
                                  metrics.average_latency_seconds)
              << "\n\n";
  } else {
    std::cout << "Batch session totals — turns: " << metrics.turn_index
              << ", user messages: " << metrics.total_user_messages
              << ", agent messages: " << metrics.total_agent_messages
              << ", tool calls: " << metrics.total_tool_calls
              << ", commands: " << metrics.total_commands
              << ", proposals: " << metrics.total_proposals
              << ", elapsed: "
              << absl::StrFormat("%.2fs avg %.2fs\n\n",
                                 metrics.total_elapsed_seconds,
                                 metrics.average_latency_seconds);
  }
  
  return absl::OkStatus();
}

std::string SimpleChatSession::ReadLineWithVim() {
  if (!vim_mode_) {
    vim_mode_ = std::make_unique<VimMode>();
    vim_mode_->SetAutoCompleteCallback(
        [this](const std::string& partial) {
          return GetAutocompleteOptions(partial);
        });
  }
  
  vim_mode_->Reset();
  
  // Show initial prompt
  std::cout << "You [" << vim_mode_->GetModeString() << "]: " << std::flush;
  
  while (true) {
    int ch;
#ifdef _WIN32
    ch = _getch();
#else
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
      ch = static_cast<int>(c);
    } else {
      break;  // EOF
    }
#endif
    
    if (vim_mode_->ProcessKey(ch)) {
      // Line complete
      std::string line = vim_mode_->GetLine();
      vim_mode_->AddToHistory(line);
      std::cout << "\n";
      return line;
    }
  }
  
  return "";  // EOF
}

std::vector<std::string> SimpleChatSession::GetAutocompleteOptions(
    const std::string& partial) {
  // Simple autocomplete with common commands
  std::vector<std::string> all_commands = {
      "/help", "/exit", "/quit", "/reset", "/history",
      "list rooms", "list sprites", "list palettes",
      "show room ", "describe ", "analyze "
  };
  
  std::vector<std::string> matches;
  for (const auto& cmd : all_commands) {
    if (cmd.find(partial) == 0) {
      matches.push_back(cmd);
    }
  }
  
  return matches;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
