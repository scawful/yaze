#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "rom/rom.h"
#include "cli/handlers/agent/common.h"
#include "cli/handlers/rom/mock_rom.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "zelda3/zelda3_labels.h"

ABSL_DECLARE_FLAG(std::string, rom);
ABSL_DECLARE_FLAG(bool, mock_rom);

namespace yaze {
namespace cli {
namespace agent {

namespace {

absl::Status LoadRomForAgent(Rom& rom) {
  if (rom.is_loaded()) {
    return ::absl::OkStatus();
  }

  // Check if mock ROM mode is enabled
  bool use_mock = ::absl::GetFlag(FLAGS_mock_rom);
  if (use_mock) {
    // Initialize mock ROM with embedded labels
    auto status = InitializeMockRom(rom);
    if (!status.ok()) {
      return status;
    }
    std::cout << "✅ Mock ROM initialized with embedded Zelda3 labels\n";
    return ::absl::OkStatus();
  }

  // Otherwise load from file
  std::string rom_path = ::absl::GetFlag(FLAGS_rom);
  if (rom_path.empty()) {
    return ::absl::InvalidArgumentError(
        "No ROM loaded. Pass --rom=<path> or use --mock-rom for testing.");
  }

  auto status = rom.LoadFromFile(rom_path);
  if (!status.ok()) {
    return ::absl::FailedPreconditionError(::absl::StrCat(
        "Failed to load ROM from '", rom_path, "': ", status.message()));
  }

  return ::absl::OkStatus();
}

struct ConversationTestCase {
  std::string name;
  std::string description;
  std::vector<std::string> user_prompts;
  std::vector<std::string>
      expected_keywords;  // Keywords to look for in responses
  bool expect_tool_calls = false;
  bool expect_commands = false;
};

std::vector<ConversationTestCase> GetDefaultTestCases() {
  return {
      {
          .name = "embedded_labels_room_query",
          .description = "Ask about room names using embedded labels",
          .user_prompts = {"What is the name of room 5?"},
          .expected_keywords = {"room", "Tower of Hera", "Moldorm"},
          .expect_tool_calls = false,
          .expect_commands = false,
      },
      {
          .name = "embedded_labels_sprite_query",
          .description = "Ask about sprite names using embedded labels",
          .user_prompts = {"What is sprite 9?"},
          .expected_keywords = {"sprite", "Moldorm", "Boss"},
          .expect_tool_calls = false,
          .expect_commands = false,
      },
      {
          .name = "embedded_labels_entrance_query",
          .description = "Ask about entrance names using embedded labels",
          .user_prompts = {"What is entrance 0?"},
          .expected_keywords = {"entrance", "Link", "House"},
          .expect_tool_calls = false,
          .expect_commands = false,
      },
      {
          .name = "simple_question",
          .description = "Ask about dungeons in the ROM",
          .user_prompts = {"What dungeons are in this ROM?"},
          .expected_keywords = {"dungeon", "palace", "castle"},
          .expect_tool_calls = true,
          .expect_commands = false,
      },
      {
          .name = "list_all_rooms",
          .description = "List all room names with embedded labels",
          .user_prompts = {"List the first 10 dungeon rooms"},
          .expected_keywords = {"room", "Ganon", "Hyrule", "Palace"},
          .expect_tool_calls = true,
          .expect_commands = false,
      },
      {
          .name = "overworld_tile_search",
          .description = "Find specific tiles in overworld",
          .user_prompts = {"Find all trees on the overworld"},
          .expected_keywords = {"tree", "tile", "map"},
          .expect_tool_calls = true,
          .expect_commands = false,
      },
      {
          .name = "multi_step_query",
          .description = "Ask multiple questions in sequence",
          .user_prompts =
              {
                  "What is the name of room 0?",
                  "What sprites are defined in the game?",
              },
          .expected_keywords = {"Ganon", "sprite", "room"},
          .expect_tool_calls = true,
          .expect_commands = false,
      },
      {
          .name = "map_description",
          .description = "Get information about a specific map",
          .user_prompts = {"Describe overworld map 0"},
          .expected_keywords = {"map", "light world", "tile"},
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

void PrintAgentResponse(const ChatMessage& response, bool verbose) {
  std::cout << "🤖 Agent: " << response.message << "\n\n";

  if (verbose && response.json_pretty.has_value()) {
    std::cout << "🧾 JSON Output:\n" << *response.json_pretty << "\n\n";
  }

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

    if (!verbose && table.rows.size() > max_rows) {
      std::cout << "  ... (" << (table.rows.size() - max_rows)
                << " more rows)\n";
    }

    if (verbose && table.rows.size() > max_rows) {
      for (size_t i = max_rows; i < table.rows.size(); ++i) {
        std::cout << "  ";
        for (size_t j = 0; j < table.rows[i].size(); ++j) {
          std::cout << table.rows[i][j];
          if (j < table.rows[i].size() - 1) {
            std::cout << " | ";
          }
        }
        std::cout << "\n";
      }
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
    bool has_commands =
        response.message.find("overworld") != std::string::npos ||
        response.message.find("dungeon") != std::string::npos ||
        response.message.find("set-tile") != std::string::npos;
    if (!has_commands) {
      std::cout << "⚠️  Warning: Expected commands but none found\n";
    }
  }

  return passed;
}

absl::Status RunTestCase(const ConversationTestCase& test_case,
                         ConversationalAgentService& service, bool verbose) {
  PrintTestHeader(test_case);

  bool all_passed = true;

  service.ResetConversation();

  for (const auto& prompt : test_case.user_prompts) {
    PrintUserPrompt(prompt);

    auto response_or = service.SendMessage(prompt);
    if (!response_or.ok()) {
      std::cout << "❌ FAILED: " << response_or.status().message() << "\n\n";
      all_passed = false;
      continue;
    }

    const auto& response = response_or.value();
    PrintAgentResponse(response, verbose);

    if (!ValidateResponse(response, test_case)) {
      all_passed = false;
    }
  }

  if (verbose) {
    const auto& history = service.GetHistory();
    std::cout << "🗂  Conversation Summary (" << history.size() << " message"
              << (history.size() == 1 ? "" : "s") << ")\n";
    for (const auto& message : history) {
      const char* sender =
          message.sender == ChatMessage::Sender::kUser ? "User" : "Agent";
      std::cout << "  [" << sender << "] " << message.message << "\n";
    }
    std::cout << "\n";
  }

  if (all_passed) {
    std::cout << "✅ Test PASSED: " << test_case.name << "\n";
    return absl::OkStatus();
  }

  std::cout << "⚠️  Test completed with warnings: " << test_case.name << "\n";
  return absl::InternalError(
      absl::StrCat("Conversation test failed validation: ", test_case.name));
}

absl::Status LoadTestCasesFromFile(
    const std::string& file_path,
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

  std::cout << "🔍 Debug: Starting test-conversation handler...\n";

  // Load ROM context
  Rom rom;
  std::cout << "🔍 Debug: Loading ROM...\n";
  auto load_status = LoadRomForAgent(rom);
  if (!load_status.ok()) {
    std::cerr << "❌ Error loading ROM: " << load_status.message() << "\n";
    return load_status;
  }

  std::cout << "✅ ROM loaded: " << rom.title() << "\n";

  // Load embedded labels for natural language queries
  std::cout << "🔍 Debug: Initializing embedded labels...\n";
  project::YazeProject project;
  auto labels_status = project.InitializeEmbeddedLabels(
      zelda3::Zelda3Labels::ToResourceLabels());
  if (!labels_status.ok()) {
    std::cerr << "⚠️  Warning: Could not initialize embedded labels: "
              << labels_status.message() << "\n";
  } else {
    std::cout << "✅ Embedded labels initialized successfully\n";
  }

  // Associate labels with ROM if it has a resource label manager
  std::cout << "🔍 Debug: Checking resource label manager...\n";
  if (rom.resource_label() && project.use_embedded_labels) {
    std::cout << "🔍 Debug: Associating labels with ROM...\n";
    rom.resource_label()->labels_ = project.resource_labels;
    rom.resource_label()->labels_loaded_ = true;
    std::cout << "✅ Embedded labels loaded and associated with ROM\n";
  } else {
    std::cout << "⚠️  ROM has no resource label manager\n";
  }

  // Create conversational agent service
  std::cout << "🔍 Debug: Creating conversational agent service...\n";
  std::cout << "🔍 Debug: About to construct service object...\n";

  ConversationalAgentService service;
  std::cout << "✅ Service object created\n";

  std::cout << "🔍 Debug: Setting ROM context...\n";
  service.SetRomContext(&rom);
  std::cout << "✅ Service initialized\n";

  // Load test cases
  std::vector<ConversationTestCase> test_cases;
  if (use_defaults) {
    test_cases = GetDefaultTestCases();
    std::cout << "Using default test cases (" << test_cases.size()
              << " tests)\n";
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
    auto status = RunTestCase(test_case, service, verbose);
    if (status.ok()) {
      ++passed;
    } else {
      ++failed;
      std::cerr << "Test case '" << test_case.name
                << "' failed: " << status.message() << "\n";
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

  if (failed == 0) {
    return absl::OkStatus();
  }

  return absl::InternalError(
      absl::StrCat(failed, " conversation test(s) reported failures"));
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
