#include "cli/handlers/agent/commands.h"

#include <iostream>
#include <algorithm>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "app/rom.h"
#include "app/editor/message/message_data.h"

namespace yaze {
namespace cli {
namespace agent {

absl::Status HandleDialogueListCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Parse arguments
  std::string format = "json";
  int limit = 50;  // Default limit
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    } else if (token == "--limit") {
      if (i + 1 < arg_vec.size()) {
        absl::SimpleAtoi(arg_vec[++i], &limit);
      }
    } else if (absl::StartsWith(token, "--limit=")) {
      absl::SimpleAtoi(token.substr(8), &limit);
    }
  }

  // Read all dialogue messages from ROM using ReadAllTextData
  constexpr int kTextData1 = 0xE0000;  // Bank $0E in ALTTP
  auto messages = editor::ReadAllTextData(rom_context->mutable_data(), kTextData1);
  
  // Limit the results
  int actual_limit = std::min(limit, static_cast<int>(messages.size()));
  
  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"dialogue_messages\": [\n";
    for (int i = 0; i < actual_limit; ++i) {
      const auto& msg = messages[i];
      // Create a preview (first 50 chars)
      std::string preview = msg.ContentsParsed;
      if (preview.length() > 50) {
        preview = preview.substr(0, 47) + "...";
      }
      // Replace newlines with spaces for preview
      for (char& c : preview) {
        if (c == '\n') c = ' ';
      }
      
      std::cout << "    {\n";
      std::cout << "      \"id\": \"0x" << std::hex << std::uppercase << msg.ID << std::dec << "\",\n";
      std::cout << "      \"decimal_id\": " << msg.ID << ",\n";
      std::cout << "      \"preview\": \"" << preview << "\"\n";
      std::cout << "    }";
      if (i < actual_limit - 1) {
        std::cout << ",";
      }
      std::cout << "\n";
    }
    std::cout << "  ],\n";
    std::cout << "  \"total\": " << messages.size() << ",\n";
    std::cout << "  \"showing\": " << actual_limit << ",\n";
    std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
    std::cout << "}\n";
  } else {
    // Table format
    std::cout << "Dialogue Messages (showing " << actual_limit << " of " << messages.size() << "):\n";
    std::cout << "----------------------------------------\n";
    for (int i = 0; i < actual_limit; ++i) {
      const auto& msg = messages[i];
      std::string preview = msg.ContentsParsed;
      if (preview.length() > 40) {
        preview = preview.substr(0, 37) + "...";
      }
      for (char& c : preview) {
        if (c == '\n') c = ' ';
      }
      std::cout << absl::StrFormat("0x%03X (%3d) | %s\n", msg.ID, msg.ID, preview);
    }
    std::cout << "----------------------------------------\n";
    std::cout << "Total: " << messages.size() << " messages\n";
  }

  return absl::OkStatus();
}

absl::Status HandleDialogueReadCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Parse arguments
  int message_id = -1;
  std::string format = "json";
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--id" || token == "--message") {
      if (i + 1 < arg_vec.size()) {
        const std::string& id_str = arg_vec[++i];
        if (absl::StartsWith(id_str, "0x") || absl::StartsWith(id_str, "0X")) {
          message_id = std::stoi(id_str, nullptr, 16);
        } else {
          absl::SimpleAtoi(id_str, &message_id);
        }
      }
    } else if (absl::StartsWith(token, "--id=") || absl::StartsWith(token, "--message=")) {
      std::string id_str = token.substr(token.find('=') + 1);
      if (absl::StartsWith(id_str, "0x") || absl::StartsWith(id_str, "0X")) {
        message_id = std::stoi(id_str, nullptr, 16);
      } else {
        absl::SimpleAtoi(id_str, &message_id);
      }
    } else if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  if (message_id < 0) {
    return absl::InvalidArgumentError(
        "Usage: dialogue-read --id <message_id> [--format json|text]");
  }

  // Read all dialogue messages from ROM
  constexpr int kTextData1 = 0xE0000;
  auto messages = editor::ReadAllTextData(rom_context->mutable_data(), kTextData1);
  
  // Find the specific message
  std::string dialogue_text;
  bool found = false;
  for (const auto& msg : messages) {
    if (msg.ID == message_id) {
      dialogue_text = msg.ContentsParsed;
      found = true;
      break;
    }
  }
  
  if (!found) {
    return absl::NotFoundError(
        absl::StrFormat("Message ID 0x%X not found in ROM", message_id));
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"message_id\": \"0x" << std::hex << std::uppercase 
              << message_id << std::dec << "\",\n";
    std::cout << "  \"decimal_id\": " << message_id << ",\n";
    std::cout << "  \"text\": \"" << dialogue_text << "\",\n";
    std::cout << "  \"length\": " << dialogue_text.length() << ",\n";
    std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
    std::cout << "}\n";
  } else {
    std::cout << "Message ID: 0x" << std::hex << std::uppercase 
              << message_id << std::dec << " (" << message_id << ")\n";
    std::cout << "Text: " << dialogue_text << "\n";
  }

  return absl::OkStatus();
}

absl::Status HandleDialogueSearchCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Parse arguments
  std::string query;
  std::string format = "json";
  int limit = 20;
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--query" || token == "--search") {
      if (i + 1 < arg_vec.size()) {
        query = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--query=")) {
      query = token.substr(8);
    } else if (absl::StartsWith(token, "--search=")) {
      query = token.substr(9);
    } else if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    } else if (token == "--limit") {
      if (i + 1 < arg_vec.size()) {
        absl::SimpleAtoi(arg_vec[++i], &limit);
      }
    } else if (absl::StartsWith(token, "--limit=")) {
      absl::SimpleAtoi(token.substr(8), &limit);
    }
  }

  if (query.empty()) {
    return absl::InvalidArgumentError(
        "Usage: dialogue-search --query <search_text> [--format json|text] [--limit N]");
  }

  // Read all dialogue messages from ROM and search
  constexpr int kTextData1 = 0xE0000;
  auto messages = editor::ReadAllTextData(rom_context->mutable_data(), kTextData1);
  
  // Search for messages containing the query string (case-insensitive)
  std::vector<std::pair<int, std::string>> results;
  std::string query_lower = query;
  std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
  
  for (const auto& msg : messages) {
    std::string msg_lower = msg.ContentsParsed;
    std::transform(msg_lower.begin(), msg_lower.end(), msg_lower.begin(), ::tolower);
    
    if (msg_lower.find(query_lower) != std::string::npos) {
      // Create preview with matched text
      std::string preview = msg.ContentsParsed;
      if (preview.length() > 60) {
        preview = preview.substr(0, 57) + "...";
      }
      for (char& c : preview) {
        if (c == '\n') c = ' ';
      }
      results.push_back({msg.ID, preview});
      
      if (results.size() >= static_cast<size_t>(limit)) {
        break;
      }
    }
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"query\": \"" << query << "\",\n";
    std::cout << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
      const auto& [id, text] = results[i];
      std::cout << "    {\n";
      std::cout << "      \"id\": \"0x" << std::hex << std::uppercase 
                << id << std::dec << "\",\n";
      std::cout << "      \"decimal_id\": " << id << ",\n";
      std::cout << "      \"text\": \"" << text << "\"\n";
      std::cout << "    }";
      if (i < results.size() - 1) {
        std::cout << ",";
      }
      std::cout << "\n";
    }
    std::cout << "  ],\n";
    std::cout << "  \"total_found\": " << results.size() << ",\n";
    std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
    std::cout << "}\n";
  } else {
    std::cout << "Search results for: \"" << query << "\"\n";
    std::cout << "----------------------------------------\n";
    for (const auto& [id, text] : results) {
      std::cout << absl::StrFormat("0x%03X (%3d): %s\n", id, id, text);
    }
    std::cout << "----------------------------------------\n";
    std::cout << "Found: " << results.size() << " matches\n";
  }

  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze

