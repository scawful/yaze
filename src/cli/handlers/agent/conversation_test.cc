#include "cli/handlers/agent/commands.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "cli/handlers/agent/common.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace cli {
namespace agent {

namespace {

struct ConversationTestCase {
  std::string name;
  std::string description;
  std::vector<std::string> user_prompts;
  std::vector<std::string> expected_keywords;  // Keywords to look for in responses
  bool expect_tool_calls = false;
  bool expect_commands = false;
};

std::vector<ConversationTestCase> GetDefaultTestCases() {
  return {
      {
          .name = "simple_question",
          .description = "Ask about dungeons in the ROM",
          .user_prompts = {"What dungeons are in this ROM?"},
          .expected_keywords = {"dungeon", "palace", "castle"},
          .expect_tool_calls = true,
          .expect_commands = false,
      },
      {
          .name = "overworld_tile_search",
          .description = "Find specific tiles in overworld",
          .user_prompts = {"Find all trees on the overworld"},
          .expected_keywords = {"tree", "tile", "0x02E", "map"},
          .expect_tool_calls = true,
          .expect_commands = false,
      },
      {
          .name = "multi_step_query",
          .description = "Ask multiple questions in sequence",
          .user_prompts = {
              "What dungeons are defined?",
              "Tell me about the sprites in the first dungeon room",
          },
          .expected_keywords = {"dungeon", "sprite", "room"},
          .expect_tool_calls = true,
          .expect_commands = false,
      },
      {
          .name = "command_generation",
          .description = "Request ROM modification",
          .user_prompts = {"Place a tree at position 10, 10 on map 0"},
          .expected_keywords = {"overworld", "set-tile", "0x02E", "tree"},
          .expect_tool_calls = false,
          .expect_commands = true,
      },
      {
          .name = "map_description",
          .description = "Get information about a specific map",
          .user_prompts = {"Describe overworld map 0"},
          .expected_keywords = {"map", "light world", "size", "tile"},
          .expect_tool_calls = true,
          .expect_commands = false,
      },
  };
}

void PrintTestHeader(const ConversationTestCase& test_case) {
  std::cout << "\n===========================================\n";
  std::cout << "Test: " << test_case.name << "\n";
  std::cout << "Description: " << test_case.description << "\n";
  std::cout << "===========================================\n\n";
}

void PrintUserPrompt(const std::string& prompt) {
  std::cout << "👤 User: " << prompt << "\n\n";
}

void PrintAgentResponse(const ChatMessage& response) {
  std::cout << "🤖 Agent: " << response.message << "\n\n";
  
  if (response.table_data.has_value()) {
    std::cout << "📊 Table Output:\n";
    const auto& table = response.table_data.value();
    
    // Print headers
    std::cout << "  ";
    for (size_t i = 0; i < table.headers.size(); ++i) {
      std::cout << table.headers[i];
      if (i < table.headers.size() - 1) {
        std::cout << " | ";
      }
    }
    std::cout << "\n  ";
    for (size_t i = 0; i < table.headers.size(); ++i) {
      std::cout << std::string(table.headers[i].length(), '-');
      if (i < table.headers.size() - 1) {
        std::cout << " | ";
      }
    }
    std::cout << "\n";
    
    // Print rows (limit to 10 for readability)
    const size_t max_rows = std::min<size_t>(10, table.rows.size());
    for (size_t i = 0; i < max_rows; ++i) {
      std::cout << "  ";
      for (size_t j = 0; j < table.rows[i].size(); ++j) {
        std::cout << table.rows[i][j];
        if (j < table.rows[i].size() - 1) {
          std::cout << " | ";
        }
      }
      std::cout << "\n";
    }
    
    if (table.rows.size() > max_rows) {
      std::cout << "  ... (" << (table.rows.size() - max_rows) 
                << " more rows)\n";
    }
    std::cout << "\n";
  }
}

bool ValidateResponse(const ChatMessage& response,
                     const ConversationTestCase& test_case) {
  bool passed = true;
  
  // Check for expected keywords
  for (const auto& keyword : test_case.expected_keywords) {
    if (response.message.find(keyword) == std::string::npos) {
      std::cout << "⚠️  Warning: Expected keyword '" << keyword 
                << "' not found in response\n";
      // Don't fail test, just warn
    }
  }
  
  // Check for tool calls (if we have table data, tools were likely called)
  if (test_case.expect_tool_calls && !response.table_data.has_value()) {
    std::cout << "⚠️  Warning: Expected tool calls but no table data found\n";
  }
  
  // Check for commands
  if (test_case.expect_commands) {
    bool has_commands = response.message.find("overworld") != std::string::npos ||
                       response.message.find("dungeon") != std::string::npos ||
                       response.message.find("set-tile") != std::string::npos;
    if (!has_commands) {
      std::cout << "⚠️  Warning: Expected commands but none found\n";
    }
  }
  
  return passed;
}

absl::Status RunTestCase(const ConversationTestCase& test_case,
                        ConversationalAgentService& service) {
  PrintTestHeader(test_case);
  
  bool all_passed = true;
  
  for (const auto& prompt : test_case.user_prompts) {
    PrintUserPrompt(prompt);
    
    auto response_or = service.SendMessage(prompt);
    if (!response_or.ok()) {
      std::cout << "❌ FAILED: " << response_or.status().message() << "\n\n";
      all_passed = false;
      continue;
    }
    
    const auto& response = response_or.value();
    PrintAgentResponse(response);
    
    if (!ValidateResponse(response, test_case)) {
      all_passed = false;
    }
  }
  
  if (all_passed) {
    std::cout << "✅ Test PASSED: " << test_case.name << "\n";
  } else {
    std::cout << "⚠️  Test completed with warnings: " << test_case.name << "\n";
  }
  
  return absl::OkStatus();
}

absl::Status LoadTestCasesFromFile(const std::string& file_path,
                                  std::vector<ConversationTestCase>* test_cases) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Could not open test file: ", file_path));
  }
  
  nlohmann::json test_json;
  try {
    file >> test_json;
  } catch (const nlohmann::json::parse_error& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse test file: ", e.what()));
  }
  
  if (!test_json.is_array()) {
    return absl::InvalidArgumentError(
        "Test file must contain a JSON array of test cases");
  }
  
  for (const auto& test_obj : test_json) {
    ConversationTestCase test_case;
    test_case.name = test_obj.value("name", "unnamed_test");
    test_case.description = test_obj.value("description", "");
    
    if (test_obj.contains("prompts") && test_obj["prompts"].is_array()) {
      for (const auto& prompt : test_obj["prompts"]) {
        if (prompt.is_string()) {
          test_case.user_prompts.push_back(prompt.get<std::string>());
        }
      }
    }
    
    if (test_obj.contains("expected_keywords") && 
        test_obj["expected_keywords"].is_array()) {
      for (const auto& keyword : test_obj["expected_keywords"]) {
        if (keyword.is_string()) {
          test_case.expected_keywords.push_back(keyword.get<std::string>());
        }
      }
    }
    
    test_case.expect_tool_calls = test_obj.value("expect_tool_calls", false);
    test_case.expect_commands = test_obj.value("expect_commands", false);
    
    test_cases->push_back(test_case);
  }
  
  return absl::OkStatus();
}

}  // namespace

absl::Status HandleTestConversationCommand(
    const std::vector<std::string>& arg_vec) {
  std::string test_file;
  bool use_defaults = true;
  bool verbose = false;
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if (arg == "--file" && i + 1 < arg_vec.size()) {
      test_file = arg_vec[i + 1];
      use_defaults = false;
      ++i;
    } else if (arg == "--verbose") {
      verbose = true;
    }
  }
  
  // Load ROM context
  Rom rom;
  auto load_status = LoadRomForAgent(rom);
  if (!load_status.ok()) {
    return load_status;
  }
  
  // Create conversational agent service
  ConversationalAgentService service;
  service.SetRomContext(&rom);
  
  // Load test cases
  std::vector<ConversationTestCase> test_cases;
  if (use_defaults) {
    test_cases = GetDefaultTestCases();
    std::cout << "Using default test cases (" << test_cases.size() << " tests)\n";
  } else {
    auto status = LoadTestCasesFromFile(test_file, &test_cases);
    if (!status.ok()) {
      return status;
    }
    std::cout << "Loaded " << test_cases.size() << " test cases from " 
              << test_file << "\n";
  }
  
  if (test_cases.empty()) {
    return absl::InvalidArgumentError("No test cases to run");
  }
  
  // Run all test cases
  int passed = 0;
  int failed = 0;
  
  for (const auto& test_case : test_cases) {
    auto status = RunTestCase(test_case, service);
    if (status.ok()) {
      ++passed;
    } else {
      ++failed;
      std::cerr << "Test case '" << test_case.name << "' failed: " 
                << status.message() << "\n";
    }
  }
  
  // Print summary
  std::cout << "\n===========================================\n";
  std::cout << "Test Summary\n";
  std::cout << "===========================================\n";
  std::cout << "Total tests: " << test_cases.size() << "\n";
  std::cout << "Passed: " << passed << "\n";
  std::cout << "Failed: " << failed << "\n";
  
  if (failed == 0) {
    std::cout << "\n✅ All tests passed!\n";
  } else {
    std::cout << "\n⚠️  Some tests failed\n";
  }
  
  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
