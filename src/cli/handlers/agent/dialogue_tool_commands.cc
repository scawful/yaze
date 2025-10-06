#include "cli/handlers/agent/commands.h"

#include <iostream>
#include <set>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "app/rom.h"

namespace yaze {
namespace cli {
namespace agent {

absl:Status HandleDialogueListCommand(
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

  // Get all dialogue IDs from ROM
  // This is a simplified implementation - real one would parse dialogue data
  std::vector<int> dialogue_ids;
  
  // ALTTP has dialogue messages from 0x00 to ~0x1FF
  for (int i = 0; i < std::min(limit, 512); ++i) {
    dialogue_ids.push_back(i);
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"dialogue_messages\": [\n";
    for (size_t i = 0; i < dialogue_ids.size(); ++i) {
      int id = dialogue_ids[i];
      std::cout << "    {\n";
      std::cout << "      \"id\": \"0x" << std::hex << std::uppercase << id << std::dec << "\",\n";
      std::cout << "      \"decimal_id\": " << id << ",\n";
      std::cout << "      \"preview\": \"Message " << id << "...\"\n";
      std::cout << "    }";
      if (i < dialogue_ids.size() - 1) {
        std::cout << ",";
      }
      std::cout << "\n";
    }
    std::cout << "  ],\n";
    std::cout << "  \"total\": " << dialogue_ids.size() << ",\n";
    std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
    std::cout << "}\n";
  } else {
    // Table format
    std::cout << "Dialogue Messages (showing " << dialogue_ids.size() << "):\n";
    std::cout << "----------------------------------------\n";
    for (int id : dialogue_ids) {
      std::cout << absl::StrFormat("0x%03X (%3d) | Message %d\n", id, id, id);
    }
    std::cout << "----------------------------------------\n";
    std::cout << "Total: " << dialogue_ids.size() << " messages\n";
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
        std::string id_str = arg_vec[++i];
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

  // Simplified dialogue text - real implementation would decode from ROM
  std::string dialogue_text = absl::StrFormat(
      "This is dialogue message %d. Real implementation would decode from ROM data.",
      message_id);

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

  // Simplified search - real implementation would search actual dialogue data
  std::vector<std::pair<int, std::string>> results;
  results.push_back({0x01, absl::StrFormat("Message 1 containing '%s'", query)});
  results.push_back({0x15, absl::StrFormat("Another message with '%s'", query)});
  results.push_back({0x42, absl::StrFormat("Found '%s' in message 66", query)});

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

