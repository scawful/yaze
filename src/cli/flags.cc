#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, rom, "", "Path to the ROM file");

// AI Service Configuration Flags
ABSL_FLAG(std::string, ai_provider, "mock",
          "AI provider to use: 'mock' (default), 'ollama', or 'gemini'");
ABSL_FLAG(std::string, ai_model, "",
          "AI model to use (provider-specific, e.g., 'llama3' for Ollama, "
          "'gemini-1.5-flash' for Gemini)");
ABSL_FLAG(std::string, gemini_api_key, "",
          "Gemini API key (can also use GEMINI_API_KEY environment variable)");
ABSL_FLAG(std::string, ollama_host, "http://localhost:11434",
          "Ollama server host URL");
