#ifndef YAZE_SRC_CLI_SERVICE_AI_CLI_HARNESS_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AI_CLI_HARNESS_SERVICE_H_

#include <atomic>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/provider_ids.h"

#ifdef YAZE_AI_RUNTIME_AVAILABLE
#include "cli/service/ai/prompt_builder.h"
#endif

namespace yaze::cli {

struct CliHarnessProfile {
  std::string provider_id;
  std::string display_name;
  std::string binary_name;
  std::string default_model;
  std::string read_only_summary;
  bool supports_live_model_list = false;
};

struct CliHarnessConfig {
  std::string provider = kProviderGeminiCli;
  std::string model;
  std::string working_directory;
  std::string binary_override;
  std::string system_instruction;
  int timeout_seconds = 300;
  bool verbose = false;
  std::atomic<bool>* cancel_requested = nullptr;
};

// Normalizes harness aliases (e.g. "local-gemini" -> "gemini-cli",
// "antigravity" -> "agy", "cursor" -> "cursor-agent").
std::string NormalizeCliHarnessProviderId(absl::string_view provider);

// Looks up profile metadata for a CLI harness provider.
absl::StatusOr<CliHarnessProfile> GetCliHarnessProfile(
    absl::string_view provider);

// Builds the argv vector (without argv[0] binary) for a CLI harness call.
// Always enables plan / read-only mode and never passes skip-permissions or yolo flags.
absl::StatusOr<std::vector<std::string>> BuildCliHarnessArgv(
    absl::string_view provider, const std::string& prompt,
    const std::string& model, int timeout_seconds = 300);

// Parses JSON / JSONL CLI output into AgentResponse (with raw-text fallback
// when JSON parsing fails on a non-empty stdout payload).
absl::StatusOr<AgentResponse> ParseCliHarnessOutput(
    absl::string_view provider, absl::string_view model,
    absl::string_view stdout_text, absl::string_view stderr_text = "");

// Parses stdout from `agy models` (<id>\t<display_name>).
std::vector<ModelInfo> ParseAgyModelsOutput(absl::string_view output);

// Parses stdout from `cursor-agent --list-models` (<id> - <display_name>).
std::vector<ModelInfo> ParseCursorAgentModelsOutput(absl::string_view output);

// Returns current curated fallback models for a CLI harness provider.
std::vector<ModelInfo> GetFallbackCliModels(absl::string_view provider);

class CliHarnessService : public AIService {
 public:
  explicit CliHarnessService(CliHarnessConfig config);

  void SetRomContext(Rom* rom) override;
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override;
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) override;
  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() override;
  std::string GetProviderName() const override;

  absl::Status CheckAvailability();
  const CliHarnessConfig& config() const { return config_; }

 private:
  absl::StatusOr<AgentResponse> ExecuteHarness(const std::string& prompt);

  CliHarnessConfig config_;
#ifdef YAZE_AI_RUNTIME_AVAILABLE
  PromptBuilder prompt_builder_;
#endif
};

}  // namespace yaze::cli

#endif  // YAZE_SRC_CLI_SERVICE_AI_CLI_HARNESS_SERVICE_H_
