#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, rom, "", "Path to the ROM file");
ABSL_FLAG(bool, mock_rom, false,
          "Use mock ROM mode for testing without requiring an actual ROM file. "
          "Loads all Zelda3 embedded labels but no actual ROM data.");
ABSL_FLAG(bool, quiet, false, "Suppress non-essential output");

// AI Service Configuration Flags
ABSL_FLAG(std::string, ai_provider, "auto",
          "AI provider to use: 'auto' (try gemini→ollama→mock), 'gemini', "
          "'ollama', or 'mock'");
ABSL_FLAG(std::string, ai_model, "",
          "AI model to use (provider-specific, e.g., 'llama3' for Ollama, "
          "'gemini-1.5-flash' for Gemini)");
ABSL_FLAG(std::string, gemini_api_key, "",
          "Gemini API key (can also use GEMINI_API_KEY environment variable)");
ABSL_FLAG(std::string, anthropic_api_key, "",
          "Anthropic API key (can also use ANTHROPIC_API_KEY environment variable)");
ABSL_FLAG(std::string, ollama_host, "http://localhost:11434",
          "Ollama server host URL");
ABSL_FLAG(std::string, prompt_version, "default",
          "Prompt version to use: 'default' or 'v2'");
ABSL_FLAG(bool, use_function_calling, false,
          "Enable native Gemini function calling (incompatible with JSON "
          "output mode)");

// --- Agent Control Flags ---
ABSL_FLAG(bool, agent_control, false,
          "Enable the gRPC server to allow the agent to control the emulator.");

ABSL_FLAG(std::string, gui_server_address, "localhost:50051",
          "Address of the YAZE GUI gRPC server");
