#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, rom, "", "Path to the ROM file");

// AI Service Configuration Flags
ABSL_FLAG(std::string, ai_provider, "auto",
          "AI provider to use: 'auto' (try gemini→ollama→mock), 'gemini', 'ollama', or 'mock'");
ABSL_FLAG(std::string, ai_model, "",
          "AI model to use (provider-specific, e.g., 'llama3' for Ollama, "
          "'gemini-1.5-flash' for Gemini)");
ABSL_FLAG(std::string, gemini_api_key, "",
          "Gemini API key (can also use GEMINI_API_KEY environment variable)");
ABSL_FLAG(std::string, ollama_host, "http://localhost:11434",
          "Ollama server host URL");
ABSL_FLAG(std::string, prompt_version, "default",
          "Prompt version to use: 'default' or 'v2'");
ABSL_FLAG(bool, use_function_calling, false,
          "Enable native Gemini function calling (incompatible with JSON output mode)");
