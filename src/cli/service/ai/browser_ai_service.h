#ifndef YAZE_SRC_CLI_SERVICE_AI_BROWSER_AI_SERVICE_H_
#define YAZE_SRC_CLI_SERVICE_AI_BROWSER_AI_SERVICE_H_

#ifdef __EMSCRIPTEN__

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/net/http_client.h"
#include "cli/service/ai/ai_service.h"
#include "cli/service/ai/common.h"
#include "nlohmann/json.hpp"

namespace yaze {

class Rom;

namespace cli {

namespace agent {
struct ChatMessage;
}

/**
 * @struct BrowserAIConfig
 * @brief Configuration for browser-based AI service
 */
struct BrowserAIConfig {
  // Provider selector: "gemini" (default) or "openai"
  std::string provider = "gemini";

  // API keys (provider-specific)
  std::string api_key;  // Gemini/OpenAI API key
  std::string model = "gemini-2.5-flash";  // Default to latest flash model

  // Optional custom endpoints (leave empty for defaults)
  std::string api_base;  // e.g., "https://api.openai.com/v1"

  float temperature = 0.7f;
  int max_output_tokens = 2048;
  std::string system_instruction;  // System prompt
  bool verbose = false;            // Enable debug logging
  int timeout_seconds = 30;        // Request timeout
};

/**
 * @class BrowserAIService
 * @brief Browser-based AI service implementation using Gemini API
 *
 * This class provides AI capabilities for the WASM web build using
 * the Gemini API through browser fetch. It implements the AIService
 * interface to provide consistent AI functionality across platforms.
 *
 * Features:
 * - Text generation using Gemini API
 * - Vision model support for image analysis
 * - Secure API key management via sessionStorage
 * - CORS-compliant HTTP requests
 * - Proper error handling and timeout management
 */
class BrowserAIService : public AIService {
 public:
  /**
   * @brief Constructor
   * @param config Browser AI configuration including API key and model settings
   * @param http_client HTTP client for making API requests (ownership transferred)
   */
  explicit BrowserAIService(const BrowserAIConfig& config,
                             std::unique_ptr<net::IHttpClient> http_client);

  /**
   * @brief Destructor
   */
  ~BrowserAIService() override = default;

  /**
   * @brief Set ROM context for prompt generation
   * @param rom Pointer to active ROM (not owned)
   */
  void SetRomContext(Rom* rom) override;

  /**
   * @brief Generate a response from a single prompt
   * @param prompt The user's prompt
   * @return AI response or error status
   */
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::string& prompt) override;

  /**
   * @brief Generate a response from conversation history
   * @param history Chat message history
   * @return AI response or error status
   */
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<agent::ChatMessage>& history) override;

  /**
   * @brief List available models for this service
   * @return List of available models for the active provider
   */
  absl::StatusOr<std::vector<ModelInfo>> ListAvailableModels() override;

  /**
   * @brief Get the provider name
   * @return provider name for the browser AI service
   */
  std::string GetProviderName() const override { return config_.provider; }

  /**
   * @brief Analyze an image with a text prompt (vision model)
   * @param image_data Base64 encoded image data
   * @param prompt Text prompt for image analysis
   * @return AI response or error status
   */
  absl::StatusOr<AgentResponse> AnalyzeImage(const std::string& image_data,
                                              const std::string& prompt);

  /**
   * @brief Check if the service is available
   * @return Status indicating availability
   */
  absl::Status CheckAvailability();

  /**
   * @brief Update API key (stores in sessionStorage)
   * @param api_key New API key
   */
  void UpdateApiKey(const std::string& api_key);

 private:
  bool RequiresApiKey() const;
  std::string GetOpenAIApiBase() const;

  /**
   * @brief Build the Gemini API URL for the configured model
   * @param endpoint API endpoint (e.g., "generateContent")
   * @return Complete API URL
   */
  std::string BuildApiUrl(const std::string& endpoint) const;

  /**
   * @brief Build request body for Gemini API
   * @param prompt User prompt
   * @param include_system Whether to include system instruction
   * @return JSON request body as string
   */
  std::string BuildRequestBody(const std::string& prompt,
                               bool include_system = true) const;

  /**
   * @brief Build multimodal request body (text + image)
   * @param prompt Text prompt
   * @param image_data Base64 encoded image
   * @param mime_type Image MIME type (e.g., "image/png")
   * @return JSON request body as string
   */
  std::string BuildMultimodalRequestBody(const std::string& prompt,
                                         const std::string& image_data,
                                         const std::string& mime_type) const;

  /**
   * @brief Build request body for OpenAI chat API
   */
  std::string BuildOpenAIRequestBody(
      const std::string& prompt,
      const std::vector<agent::ChatMessage>* history = nullptr) const;

  /**
   * @brief Parse Gemini API response
   * @param response_body JSON response from API
   * @return Parsed agent response or error
   */
  absl::StatusOr<AgentResponse> ParseGeminiResponse(
      const std::string& response_body) const;

  /**
   * @brief Parse OpenAI API response
   */
  absl::StatusOr<AgentResponse> ParseOpenAIResponse(
      const std::string& response_body) const;

  /**
   * @brief Extract text from Gemini response candidates
   * @param json Parsed JSON response
   * @return Extracted text content
   */
  std::string ExtractTextFromCandidates(const nlohmann::json& json) const;

  /**
   * @brief Check for API errors in response
   * @param json Parsed JSON response
   * @return Error status if present, OK otherwise
   */
  absl::Status CheckForApiError(const nlohmann::json& json) const;

  /**
   * @brief Log debug information if verbose mode is enabled
   * @param message Debug message
   */
  void LogDebug(const std::string& message) const;

  // Configuration
  BrowserAIConfig config_;

  // HTTP client for API requests
  std::unique_ptr<net::IHttpClient> http_client_;

  // ROM context (not owned)
  Rom* rom_ = nullptr;

  // Gemini API base URL
  static constexpr const char* kGeminiApiBaseUrl =
      "https://generativelanguage.googleapis.com/v1beta/models/";
  static constexpr const char* kOpenAIApiBaseUrl =
      "https://api.openai.com/v1";

  // Mutex for thread safety
  mutable std::mutex mutex_;
};

}  // namespace cli
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_SRC_CLI_SERVICE_AI_BROWSER_AI_SERVICE_H_
