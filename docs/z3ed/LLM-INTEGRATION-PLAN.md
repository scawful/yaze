# LLM Integration Plan for z3ed Agent System

**Status**: Implementation Ready | Priority: High  
**Created**: October 3, 2025  
**Estimated Time**: 12-15 hours

## Executive Summary

This document outlines the practical implementation plan for integrating LLM capabilities into the z3ed agent system. The infrastructure is **already in place** with the `AIService` interface, `MockAIService` for testing, and a partially implemented `GeminiAIService`. This plan focuses on making LLM integration production-ready with both local (Ollama) and remote (Gemini, Claude) options.

**Current State**:
- ✅ `AIService` interface defined (`src/cli/service/ai_service.h`)
- ✅ `MockAIService` operational (returns hardcoded test commands)
- ✅ `GeminiAIService` skeleton implemented (needs fixes + proper prompting)
- ✅ Agent workflow fully functional with proposal system
- ✅ Resource catalogue (command schemas) ready for LLM consumption
- ✅ GUI automation harness operational for verification

**What's Missing**:
- 🔧 Ollama integration for local LLM support
- 🔧 Improved Gemini prompting with resource catalogue
- 🔧 Claude API integration as alternative remote option
- 🔧 AI service selection mechanism (env vars + CLI flags)
- 🔧 Proper prompt engineering with system instructions
- 🔧 Error handling and retry logic for API failures
- 🔧 Token usage monitoring and cost tracking

---

## 1. Implementation Priorities

### Phase 1: Ollama Local Integration (4-6 hours) 🎯 START HERE

**Rationale**: Ollama provides the fastest path to a working LLM agent with no API keys, costs, or rate limits. Perfect for development and testing.

**Benefits**:
- **Privacy**: All processing happens locally
- **Zero Cost**: No API charges or token limits
- **Offline**: Works without internet connection
- **Fast Iteration**: No rate limits for testing
- **Model Flexibility**: Easily swap between codellama, llama3, qwen2.5-coder, etc.

#### 1.1. Create OllamaAIService Class

**File**: `src/cli/service/ollama_ai_service.h`

```cpp
#ifndef YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_
#define YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai_service.h"

namespace yaze {
namespace cli {

// Ollama configuration for local LLM inference
struct OllamaConfig {
  std::string base_url = "http://localhost:11434";  // Default Ollama endpoint
  std::string model = "qwen2.5-coder:7b";           // Recommended for code generation
  float temperature = 0.1;                          // Low temp for deterministic commands
  int max_tokens = 2048;                            // Sufficient for command lists
  std::string system_prompt;                        // Injected from resource catalogue
};

class OllamaAIService : public AIService {
 public:
  explicit OllamaAIService(const OllamaConfig& config);
  
  // Generate z3ed commands from natural language prompt
  absl::StatusOr<std::vector<std::string>> GetCommands(
      const std::string& prompt) override;
  
  // Health check: verify Ollama server is running and model is available
  absl::Status CheckAvailability();
  
  // List available models on Ollama server
  absl::StatusOr<std::vector<std::string>> ListAvailableModels();

 private:
  OllamaConfig config_;
  
  // Build system prompt from resource catalogue
  std::string BuildSystemPrompt();
  
  // Parse JSON response from Ollama API
  absl::StatusOr<std::string> ParseOllamaResponse(const std::string& json_response);
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_OLLAMA_AI_SERVICE_H_
```

**File**: `src/cli/service/ollama_ai_service.cc`

```cpp
#include "cli/service/ollama_ai_service.h"

#include <cstdlib>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

#ifdef YAZE_WITH_HTTPLIB
#include "incl/httplib.h"
#include "third_party/json/src/json.hpp"
#endif

namespace yaze {
namespace cli {

OllamaAIService::OllamaAIService(const OllamaConfig& config) : config_(config) {
  if (config_.system_prompt.empty()) {
    config_.system_prompt = BuildSystemPrompt();
  }
}

std::string OllamaAIService::BuildSystemPrompt() {
  // TODO: Read from docs/api/z3ed-resources.yaml
  return R"(You are an expert ROM hacking assistant for The Legend of Zelda: A Link to the Past.
Your role is to generate PRECISE z3ed CLI commands to fulfill user requests.

CRITICAL RULES:
1. Output ONLY a JSON array of command strings
2. Each command must follow exact z3ed syntax
3. Commands must be executable without modification
4. Use only commands from the available command set
5. Include all required arguments with proper flags

AVAILABLE COMMANDS:
- rom info --rom <path>
- rom validate --rom <path>
- palette export --group <group> --id <id> --to <file>
- palette import --group <group> --id <id> --from <file>
- palette set-color --file <file> --index <index> --color <hex_color>
- overworld get-tile --map <map_id> --x <x> --y <y>
- overworld set-tile --map <map_id> --x <x> --y <y> --tile <tile_id>
- dungeon export-room --room <room_id> --to <file>
- dungeon import-room --room <room_id> --from <file>

RESPONSE FORMAT:
["command1", "command2", "command3"]

EXAMPLE USER REQUEST: "Make all soldier armors red"
CORRECT RESPONSE:
["palette export --group sprites --id soldier --to /tmp/soldier.pal",
 "palette set-color --file /tmp/soldier.pal --index 5 --color FF0000",
 "palette import --group sprites --id soldier --from /tmp/soldier.pal"]

Begin your response now.)";
}

absl::Status OllamaAIService::CheckAvailability() {
#ifndef YAZE_WITH_HTTPLIB
  return absl::UnimplementedError(
      "Ollama service requires httplib. Build with vcpkg or system httplib.");
#else
  try {
    httplib::Client cli(config_.base_url);
    cli.set_connection_timeout(5);  // 5 second timeout
    
    auto res = cli.Get("/api/tags");
    if (!res) {
      return absl::UnavailableError(absl::StrFormat(
          "Cannot connect to Ollama server at %s. "
          "Make sure Ollama is installed and running (ollama serve).",
          config_.base_url));
    }
    
    if (res->status != 200) {
      return absl::InternalError(absl::StrFormat(
          "Ollama server error: HTTP %d", res->status));
    }
    
    // Check if requested model is available
    nlohmann::json models_json = nlohmann::json::parse(res->body);
    bool model_found = false;
    for (const auto& model : models_json["models"]) {
      if (model["name"].get<std::string>().find(config_.model) != std::string::npos) {
        model_found = true;
        break;
      }
    }
    
    if (!model_found) {
      return absl::NotFoundError(absl::StrFormat(
          "Model '%s' not found. Pull it with: ollama pull %s",
          config_.model, config_.model));
    }
    
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat("Ollama check failed: ", e.what()));
  }
#endif
}

absl::StatusOr<std::vector<std::string>> OllamaAIService::ListAvailableModels() {
#ifndef YAZE_WITH_HTTPLIB
  return absl::UnimplementedError("Requires httplib support");
#else
  httplib::Client cli(config_.base_url);
  auto res = cli.Get("/api/tags");
  
  if (!res || res->status != 200) {
    return absl::UnavailableError("Cannot list Ollama models");
  }
  
  nlohmann::json models_json = nlohmann::json::parse(res->body);
  std::vector<std::string> models;
  for (const auto& model : models_json["models"]) {
    models.push_back(model["name"].get<std::string>());
  }
  return models;
#endif
}

absl::StatusOr<std::vector<std::string>> OllamaAIService::GetCommands(
    const std::string& prompt) {
#ifndef YAZE_WITH_HTTPLIB
  return absl::UnimplementedError(
      "Ollama service requires httplib. Build with vcpkg or system httplib.");
#else
  
  // Build request payload
  nlohmann::json request_body = {
    {"model", config_.model},
    {"prompt", config_.system_prompt + "\n\nUSER REQUEST: " + prompt},
    {"stream", false},
    {"temperature", config_.temperature},
    {"max_tokens", config_.max_tokens},
    {"format", "json"}  // Force JSON output
  };
  
  httplib::Client cli(config_.base_url);
  cli.set_read_timeout(60);  // Longer timeout for inference
  
  auto res = cli.Post("/api/generate", request_body.dump(), "application/json");
  
  if (!res) {
    return absl::UnavailableError(
        "Failed to connect to Ollama. Is 'ollama serve' running?");
  }
  
  if (res->status != 200) {
    return absl::InternalError(absl::StrFormat(
        "Ollama API error: HTTP %d - %s", res->status, res->body));
  }
  
  // Parse response
  try {
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    std::string generated_text = response_json["response"].get<std::string>();
    
    // Parse the command array from generated text
    nlohmann::json commands_json = nlohmann::json::parse(generated_text);
    
    if (!commands_json.is_array()) {
      return absl::InvalidArgumentError(
          "LLM did not return a JSON array. Response: " + generated_text);
    }
    
    std::vector<std::string> commands;
    for (const auto& cmd : commands_json) {
      if (cmd.is_string()) {
        commands.push_back(cmd.get<std::string>());
      }
    }
    
    if (commands.empty()) {
      return absl::InvalidArgumentError(
          "LLM returned empty command list. Prompt may be unclear.");
    }
    
    return commands;
    
  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(absl::StrCat(
        "Failed to parse Ollama response: ", e.what(), "\nRaw: ", res->body));
  }
#endif
}

}  // namespace cli
}  // namespace yaze
```

#### 1.2. Add CMake Configuration

**File**: `CMakeLists.txt` (add to dependencies section)

```cmake
# Optional httplib for AI services (Ollama, Gemini)
option(YAZE_WITH_HTTPLIB "Enable HTTP client for AI services" ON)

if(YAZE_WITH_HTTPLIB)
  find_package(httplib CONFIG)
  if(httplib_FOUND)
    set(YAZE_WITH_HTTPLIB ON)
    add_compile_definitions(YAZE_WITH_HTTPLIB)
    message(STATUS "httplib found - AI services enabled")
  else()
    # Try to use bundled httplib from third_party
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/httplib")
      set(YAZE_WITH_HTTPLIB ON)
      add_compile_definitions(YAZE_WITH_HTTPLIB)
      message(STATUS "Using bundled httplib - AI services enabled")
    else()
      set(YAZE_WITH_HTTPLIB OFF)
      message(WARNING "httplib not found - AI services disabled")
    endif()
  endif()
endif()
```

#### 1.3. Wire into Agent Commands

**File**: `src/cli/handlers/agent/general_commands.cc`

Replace hardcoded `MockAIService` usage with service selection:

```cpp
#include "cli/service/ollama_ai_service.h"
#include "cli/service/gemini_ai_service.h"

// Helper: Select AI service based on environment
std::unique_ptr<AIService> CreateAIService() {
  // Priority: Ollama (local) > Gemini (remote) > Mock (testing)
  
  const char* ollama_env = std::getenv("YAZE_AI_PROVIDER");
  const char* gemini_key = std::getenv("GEMINI_API_KEY");
  
  // Explicit provider selection
  if (ollama_env && std::string(ollama_env) == "ollama") {
    OllamaConfig config;
    // Allow model override via env
    if (const char* model = std::getenv("OLLAMA_MODEL")) {
      config.model = model;
    }
    auto service = std::make_unique<OllamaAIService>(config);
    
    // Health check
    if (auto status = service->CheckAvailability(); !status.ok()) {
      std::cerr << "⚠️  Ollama unavailable: " << status.message() << std::endl;
      std::cerr << "   Falling back to MockAIService" << std::endl;
      return std::make_unique<MockAIService>();
    }
    
    std::cout << "🤖 Using Ollama AI with model: " << config.model << std::endl;
    return service;
  }
  
  // Gemini if API key provided
  if (gemini_key && std::strlen(gemini_key) > 0) {
    std::cout << "🤖 Using Gemini AI (remote)" << std::endl;
    return std::make_unique<GeminiAIService>(gemini_key);
  }
  
  // Default: Mock service for testing
  std::cout << "🤖 Using MockAIService (no LLM configured)" << std::endl;
  std::cout << "   Set YAZE_AI_PROVIDER=ollama or GEMINI_API_KEY to enable LLM" << std::endl;
  return std::make_unique<MockAIService>();
}

// Update HandleRunCommand:
absl::Status HandleRunCommand(const std::vector<std::string>& arg_vec) {
  // ... existing setup code ...
  
  auto ai_service = CreateAIService();  // ← Replace MockAIService instantiation
  auto commands_or = ai_service->GetCommands(prompt);
  
  // ... rest of execution logic ...
}
```

#### 1.4. Testing & Validation

**Prerequisites**:
```bash
# Install Ollama (macOS)
brew install ollama

# Start Ollama server
ollama serve &

# Pull recommended model
ollama pull qwen2.5-coder:7b

# Test connectivity
curl http://localhost:11434/api/tags
```

**End-to-End Test Script** (`scripts/test_ollama_integration.sh`):

```bash
#!/bin/bash
set -e

echo "🧪 Testing Ollama AI Integration"

# 1. Check Ollama availability
echo "Checking Ollama server..."
if ! curl -s http://localhost:11434/api/tags > /dev/null; then
  echo "❌ Ollama not running. Start with: ollama serve"
  exit 1
fi

# 2. Check model availability
echo "Checking qwen2.5-coder:7b model..."
if ! ollama list | grep -q "qwen2.5-coder:7b"; then
  echo "⚠️  Model not found. Pulling..."
  ollama pull qwen2.5-coder:7b
fi

# 3. Test agent run with simple prompt
echo "Testing agent run command..."
export YAZE_AI_PROVIDER=ollama
export OLLAMA_MODEL=qwen2.5-coder:7b

./build/bin/z3ed agent run \
  --prompt "Export the first overworld palette to /tmp/test.pal" \
  --rom zelda3.sfc \
  --sandbox

# 4. Verify proposal created
echo "Checking proposal registry..."
if ! ./build/bin/z3ed agent list | grep -q "pending"; then
  echo "❌ No pending proposal found"
  exit 1
fi

# 5. Review generated commands
echo "✅ Reviewing generated commands..."
./build/bin/z3ed agent diff --format yaml

echo "✅ Ollama integration test passed!"
```

---

### Phase 2: Improve Gemini Integration (2-3 hours)

The existing `GeminiAIService` needs fixes and better prompting:

#### 2.1. Fix GeminiAIService Implementation

**File**: `src/cli/service/gemini_ai_service.cc`

```cpp
absl::StatusOr<std::vector<std::string>> GeminiAIService::GetCommands(
    const std::string& prompt) {
#ifndef YAZE_WITH_HTTPLIB
  return absl::UnimplementedError(
      "Gemini AI service requires httplib. Build with vcpkg.");
#else
  if (api_key_.empty()) {
    return absl::FailedPreconditionError(
        "GEMINI_API_KEY not set. Get key from: https://makersuite.google.com/app/apikey");
  }

  // Build comprehensive system instruction
  std::string system_instruction = R"({
    "role": "system",
    "content": "You are an expert ROM hacking assistant for The Legend of Zelda: A Link to the Past. Generate ONLY a JSON array of z3ed CLI commands. Each command must be executable without modification. Available commands: rom info, rom validate, palette export/import/set-color, overworld get-tile/set-tile, dungeon export-room/import-room. Response format: [\"command1\", \"command2\"]"
  })";

  httplib::Client cli("https://generativelanguage.googleapis.com");
  cli.set_read_timeout(60);
  
  nlohmann::json request_body = {
    {"contents", {{
      {"role", "user"},
      {"parts", {{
        {"text", absl::StrFormat("System: %s\n\nUser: %s", 
                                 system_instruction, prompt)}
      }}}
    }}},
    {"generationConfig", {
      {"temperature", 0.1},          // Low temp for deterministic output
      {"maxOutputTokens", 2048},
      {"topP", 0.8},
      {"topK", 10}
    }},
    {"safetySettings", {
      {{"category", "HARM_CATEGORY_DANGEROUS_CONTENT"}, {"threshold", "BLOCK_NONE"}}
    }}
  };

  httplib::Headers headers = {
      {"Content-Type", "application/json"},
  };
  
  std::string endpoint = absl::StrFormat(
      "/v1beta/models/gemini-1.5-flash:generateContent?key=%s", api_key_);

  auto res = cli.Post(endpoint, headers, request_body.dump(), "application/json");

  if (!res) {
    return absl::UnavailableError(
        "Failed to connect to Gemini API. Check internet connection.");
  }

  if (res->status != 200) {
    return absl::InternalError(absl::StrFormat(
        "Gemini API error: HTTP %d - %s", res->status, res->body));
  }

  // Parse response
  try {
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    
    // Extract text from nested structure
    std::string text_content = 
        response_json["candidates"][0]["content"]["parts"][0]["text"]
        .get<std::string>();
    
    // Gemini may wrap JSON in markdown code blocks - strip them
    if (text_content.find("```json") != std::string::npos) {
      size_t start = text_content.find("[");
      size_t end = text_content.rfind("]");
      if (start != std::string::npos && end != std::string::npos) {
        text_content = text_content.substr(start, end - start + 1);
      }
    }
    
    nlohmann::json commands_array = nlohmann::json::parse(text_content);
    
    if (!commands_array.is_array()) {
      return absl::InvalidArgumentError(
          "Gemini did not return a JSON array. Response: " + text_content);
    }
    
    std::vector<std::string> commands;
    for (const auto& cmd : commands_array) {
      if (cmd.is_string()) {
        commands.push_back(cmd.get<std::string>());
      }
    }
    
    return commands;
    
  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(absl::StrCat(
        "Failed to parse Gemini response: ", e.what(), "\nRaw: ", res->body));
  }
#endif
}
```

---

### Phase 3: Add Claude Integration (2-3 hours)

Claude 3.5 Sonnet is excellent for code generation and has a generous free tier.

#### 3.1. Create ClaudeAIService

**File**: `src/cli/service/claude_ai_service.h`

```cpp
#ifndef YAZE_SRC_CLI_CLAUDE_AI_SERVICE_H_
#define YAZE_SRC_CLI_CLAUDE_AI_SERVICE_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai_service.h"

namespace yaze {
namespace cli {

class ClaudeAIService : public AIService {
 public:
  explicit ClaudeAIService(const std::string& api_key);
  
  absl::StatusOr<std::vector<std::string>> GetCommands(
      const std::string& prompt) override;

 private:
  std::string api_key_;
  std::string model_ = "claude-3-5-sonnet-20241022";  // Latest version
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_CLAUDE_AI_SERVICE_H_
```

**File**: `src/cli/service/claude_ai_service.cc`

```cpp
#include "cli/service/claude_ai_service.h"

#include "absl/strings/str_format.h"

#ifdef YAZE_WITH_HTTPLIB
#include "incl/httplib.h"
#include "third_party/json/src/json.hpp"
#endif

namespace yaze {
namespace cli {

ClaudeAIService::ClaudeAIService(const std::string& api_key) 
    : api_key_(api_key) {}

absl::StatusOr<std::vector<std::string>> ClaudeAIService::GetCommands(
    const std::string& prompt) {
#ifndef YAZE_WITH_HTTPLIB
  return absl::UnimplementedError("Claude service requires httplib");
#else
  if (api_key_.empty()) {
    return absl::FailedPreconditionError(
        "CLAUDE_API_KEY not set. Get key from: https://console.anthropic.com/");
  }

  httplib::Client cli("https://api.anthropic.com");
  cli.set_read_timeout(60);
  
  nlohmann::json request_body = {
    {"model", model_},
    {"max_tokens", 2048},
    {"temperature", 0.1},
    {"system", "You are an expert ROM hacking assistant. Generate ONLY a JSON array of z3ed commands. No explanations."},
    {"messages", {{
      {"role", "user"},
      {"content", prompt}
    }}}
  };

  httplib::Headers headers = {
    {"Content-Type", "application/json"},
    {"x-api-key", api_key_},
    {"anthropic-version", "2023-06-01"}
  };

  auto res = cli.Post("/v1/messages", headers, request_body.dump(), 
                      "application/json");

  if (!res) {
    return absl::UnavailableError("Failed to connect to Claude API");
  }

  if (res->status != 200) {
    return absl::InternalError(absl::StrFormat(
        "Claude API error: HTTP %d - %s", res->status, res->body));
  }

  try {
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    std::string text_content = 
        response_json["content"][0]["text"].get<std::string>();
    
    // Claude may wrap in markdown - strip if present
    if (text_content.find("```json") != std::string::npos) {
      size_t start = text_content.find("[");
      size_t end = text_content.rfind("]");
      if (start != std::string::npos && end != std::string::npos) {
        text_content = text_content.substr(start, end - start + 1);
      }
    }
    
    nlohmann::json commands_json = nlohmann::json::parse(text_content);
    
    std::vector<std::string> commands;
    for (const auto& cmd : commands_json) {
      if (cmd.is_string()) {
        commands.push_back(cmd.get<std::string>());
      }
    }
    
    return commands;
    
  } catch (const std::exception& e) {
    return absl::InternalError(absl::StrCat(
        "Failed to parse Claude response: ", e.what()));
  }
#endif
}

}  // namespace cli
}  // namespace yaze
```

---

### Phase 4: Enhanced Prompt Engineering (3-4 hours)

#### 4.1. Load Resource Catalogue into System Prompt

**File**: `src/cli/service/prompt_builder.h`

```cpp
#ifndef YAZE_SRC_CLI_PROMPT_BUILDER_H_
#define YAZE_SRC_CLI_PROMPT_BUILDER_H_

#include <string>
#include "absl/status/statusor.h"

namespace yaze {
namespace cli {

// Utility for building comprehensive LLM prompts from resource catalogue
class PromptBuilder {
 public:
  // Load command schemas from docs/api/z3ed-resources.yaml
  static absl::StatusOr<std::string> LoadResourceCatalogue();
  
  // Build system prompt with full command documentation
  static std::string BuildSystemPrompt();
  
  // Build few-shot examples for better LLM performance
  static std::string BuildFewShotExamples();
  
  // Inject ROM context (current ROM info, loaded editors, etc.)
  static std::string BuildContextPrompt();
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_PROMPT_BUILDER_H_
```

#### 4.2. Few-Shot Examples

Include proven examples in system prompt:

```cpp
std::string PromptBuilder::BuildFewShotExamples() {
  return R"(
EXAMPLE 1:
User: "Make soldier armor red"
Response: ["palette export --group sprites --id soldier --to /tmp/soldier.pal", 
           "palette set-color --file /tmp/soldier.pal --index 5 --color FF0000",
           "palette import --group sprites --id soldier --from /tmp/soldier.pal"]

EXAMPLE 2:
User: "Validate ROM integrity"
Response: ["rom validate --rom zelda3.sfc"]

EXAMPLE 3:
User: "Change tile at coordinates (10, 20) in Light World to grass"
Response: ["overworld set-tile --map 0 --x 10 --y 20 --tile 0x40"]
)";
}
```

---

## 2. Configuration & User Experience

### Environment Variables

```bash
# AI Provider Selection
export YAZE_AI_PROVIDER=ollama    # Options: ollama, gemini, claude, mock
export OLLAMA_MODEL=qwen2.5-coder:7b
export OLLAMA_URL=http://localhost:11434

# API Keys (remote providers)
export GEMINI_API_KEY=your_key_here
export CLAUDE_API_KEY=your_key_here

# Logging & Debugging
export YAZE_AI_DEBUG=1            # Log full prompts and responses
export YAZE_AI_CACHE_DIR=/tmp/yaze_ai_cache  # Cache LLM responses
```

### CLI Flags

Add new flags to `z3ed agent run`:

```bash
# Override provider for single command
z3ed agent run --prompt "..." --ai-provider ollama

# Override model
z3ed agent run --prompt "..." --ai-model "llama3:70b"

# Dry run: show generated commands without executing
z3ed agent run --prompt "..." --dry-run

# Interactive mode: confirm each command before execution
z3ed agent run --prompt "..." --interactive
```

---

## 3. Testing & Validation

### Unit Tests

**File**: `test/cli/ai_service_test.cc`

```cpp
#include "cli/service/ollama_ai_service.h"
#include "cli/service/gemini_ai_service.h"
#include "cli/service/claude_ai_service.h"
#include <gtest/gtest.h>

TEST(OllamaAIServiceTest, CheckAvailability) {
  OllamaConfig config;
  config.base_url = "http://localhost:11434";
  OllamaAIService service(config);
  
  // Should not crash, may return unavailable if Ollama not running
  auto status = service.CheckAvailability();
  EXPECT_TRUE(status.ok() || 
              absl::IsUnavailable(status) || 
              absl::IsNotFound(status));
}

TEST(OllamaAIServiceTest, GetCommands) {
  // Skip if Ollama not available
  OllamaConfig config;
  OllamaAIService service(config);
  if (!service.CheckAvailability().ok()) {
    GTEST_SKIP() << "Ollama not available";
  }
  
  auto result = service.GetCommands("Validate the ROM");
  ASSERT_TRUE(result.ok()) << result.status();
  
  auto commands = result.value();
  EXPECT_GT(commands.size(), 0);
  EXPECT_THAT(commands[0], testing::HasSubstr("rom validate"));
}
```

### Integration Tests

**File**: `scripts/test_ai_services.sh`

```bash
#!/bin/bash
set -e

echo "🧪 Testing AI Services Integration"

# Test 1: Ollama (if available)
if curl -s http://localhost:11434/api/tags > /dev/null 2>&1; then
  echo "✓ Ollama available - testing..."
  export YAZE_AI_PROVIDER=ollama
  ./build/bin/z3ed agent plan --prompt "Export first palette"
else
  echo "⊘ Ollama not running - skipping"
fi

# Test 2: Gemini (if key set)
if [ -n "$GEMINI_API_KEY" ]; then
  echo "✓ Gemini API key set - testing..."
  export YAZE_AI_PROVIDER=gemini
  ./build/bin/z3ed agent plan --prompt "Validate ROM"
else
  echo "⊘ GEMINI_API_KEY not set - skipping"
fi

# Test 3: Claude (if key set)
if [ -n "$CLAUDE_API_KEY" ]; then
  echo "✓ Claude API key set - testing..."
  export YAZE_AI_PROVIDER=claude
  ./build/bin/z3ed agent plan --prompt "Export dungeon room"
else
  echo "⊘ CLAUDE_API_KEY not set - skipping"
fi

echo "✅ All available AI services tested successfully"
```

---

## 4. Documentation Updates

### User Guide

**File**: `docs/z3ed/AI-SERVICE-SETUP.md`

```markdown
# Setting Up LLM Integration for z3ed

## Quick Start: Ollama (Recommended)

1. **Install Ollama**:
   ```bash
   # macOS
   brew install ollama
   
   # Linux
   curl -fsSL https://ollama.com/install.sh | sh
   ```

2. **Start Server**:
   ```bash
   ollama serve
   ```

3. **Pull Model**:
   ```bash
   ollama pull qwen2.5-coder:7b  # Recommended: fast + accurate
   ```

4. **Configure z3ed**:
   ```bash
   export YAZE_AI_PROVIDER=ollama
   ```

5. **Test**:
   ```bash
   z3ed agent run --prompt "Validate my ROM" --rom zelda3.sfc
   ```

## Alternative: Gemini API (Remote)

1. Get API key: https://makersuite.google.com/app/apikey
2. Configure:
   ```bash
   export GEMINI_API_KEY=your_key_here
   export YAZE_AI_PROVIDER=gemini
   ```
3. Run: `z3ed agent run --prompt "..."`

## Alternative: Claude API (Remote)

1. Get API key: https://console.anthropic.com/
2. Configure:
   ```bash
   export CLAUDE_API_KEY=your_key_here
   export YAZE_AI_PROVIDER=claude
   ```
3. Run: `z3ed agent run --prompt "..."`

## Troubleshooting

**Issue**: "Cannot connect to Ollama"  
**Solution**: Make sure `ollama serve` is running

**Issue**: "Model not found"  
**Solution**: Run `ollama pull <model_name>`

**Issue**: "LLM returned empty command list"  
**Solution**: Rephrase prompt to be more specific
```

---

## 5. Implementation Timeline

### Week 1 (October 7-11)
- **Day 1-2**: Implement `OllamaAIService` class
- **Day 3**: Wire into agent commands with service selection
- **Day 4**: Testing and validation
- **Day 5**: Documentation and examples

### Week 2 (October 14-18)
- **Day 1**: Fix and improve `GeminiAIService`
- **Day 2**: Implement `ClaudeAIService`
- **Day 3**: Enhanced prompt engineering with resource catalogue
- **Day 4**: Integration tests across all services
- **Day 5**: User guide and troubleshooting docs

---

## 6. Success Criteria

✅ **Phase 1 Complete When**:
- Ollama service connects and generates valid commands
- `z3ed agent run` works end-to-end with local LLM
- Health checks report clear error messages
- Test script passes on macOS with Ollama installed

✅ **Phase 2 Complete When**:
- Gemini API calls succeed with valid responses
- Markdown code block stripping works reliably
- Error messages are actionable (e.g., "API key invalid")

✅ **Phase 3 Complete When**:
- Claude service implemented with same interface
- All three services (Ollama, Gemini, Claude) work interchangeably
- Service selection mechanism is transparent to user

✅ **Phase 4 Complete When**:
- System prompts include full resource catalogue
- Few-shot examples improve command accuracy >90%
- LLM responses consistently match expected command format

---

## 7. Future Enhancements (Post-MVP)

- **Response Caching**: Cache LLM responses by prompt hash to reduce costs/latency
- **Token Usage Tracking**: Monitor and report token consumption per session
- **Model Comparison**: A/B test different models for accuracy/cost trade-offs
- **Fine-Tuning**: Fine-tune local models on z3ed command corpus
- **Multi-Turn Dialogue**: Support follow-up questions and clarifications
- **Agentic Loop**: LLM self-corrects based on execution results
- **GUI Integration**: In-app AI assistant panel in YAZE editor

---

## Appendix A: Recommended Models

| Model | Provider | Size | Speed | Accuracy | Use Case |
|-------|----------|------|-------|----------|----------|
| qwen2.5-coder:7b | Ollama | 7B | Fast | High | **Recommended**: Best balance |
| codellama:13b | Ollama | 13B | Medium | Higher | Complex tasks |
| llama3:70b | Ollama | 70B | Slow | Highest | Maximum accuracy |
| gemini-1.5-flash | Gemini | N/A | Fast | High | Remote option, low cost |
| claude-3.5-sonnet | Claude | N/A | Medium | Highest | Premium remote option |

## Appendix B: Example Prompts

**Simple**:
- "Validate the ROM"
- "Export the first palette"
- "Show ROM info"

**Moderate**:
- "Make soldier armor red"
- "Change tile at (10, 20) in Light World to grass"
- "Export dungeon room 5 to /tmp/room5.bin"

**Complex**:
- "Find all palettes using color #FF0000 and change to #00FF00"
- "Export all dungeon rooms, modify object 3 in each, then reimport"
- "Generate a comparison report between two ROM versions"

---

## Next Steps

**👉 START HERE**: Implement Phase 1 (Ollama Integration) by following section 1.1-1.4 above.

Once complete, update this document with:
- Actual time spent vs. estimates
- Issues encountered and solutions
- Model performance observations
- User feedback

**Questions? Blockers?** Open an issue or ping @scawful in Discord.
