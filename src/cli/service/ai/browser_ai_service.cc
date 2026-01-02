#ifdef __EMSCRIPTEN__

#include "cli/service/ai/browser_ai_service.h"

#include <emscripten.h>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "rom/rom.h"  // Full definition needed for Rom member access

namespace yaze {
namespace cli {

namespace {

// Helper function to escape JSON strings
std::string EscapeJson(const std::string& str) {
  std::stringstream ss;
  for (char c : str) {
    switch (c) {
      case '"':
        ss << "\\\"";
        break;
      case '\\':
        ss << "\\\\";
        break;
      case '\b':
        ss << "\\b";
        break;
      case '\f':
        ss << "\\f";
        break;
      case '\n':
        ss << "\\n";
        break;
      case '\r':
        ss << "\\r";
        break;
      case '\t':
        ss << "\\t";
        break;
      default:
        if (c < 0x20) {
          ss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
             << static_cast<int>(c);
        } else {
          ss << c;
        }
        break;
    }
  }
  return ss.str();
}

// Helper to convert chat history to Gemini format
std::string ConvertHistoryToGeminiFormat(
    const std::vector<agent::ChatMessage>& history) {
  nlohmann::json contents = nlohmann::json::array();

  for (const auto& msg : history) {
    nlohmann::json part;
    part["text"] = msg.message;

    nlohmann::json content;
    content["parts"] = nlohmann::json::array({part});
    content["role"] =
        (msg.sender == agent::ChatMessage::Sender::kUser) ? "user" : "model";

    contents.push_back(content);
  }

  return contents.dump();
}

}  // namespace

BrowserAIService::BrowserAIService(
    const BrowserAIConfig& config,
    std::unique_ptr<net::IHttpClient> http_client)
    : config_(config), http_client_(std::move(http_client)) {
  // Normalize provider name
  config_.provider = absl::AsciiStrToLower(config_.provider);
  if (config_.provider.empty()) {
    config_.provider = "gemini";
  }
  // Set sensible defaults per provider
  if (config_.provider == "openai") {
    if (config_.model.empty())
      config_.model = "gpt-4o-mini";
    if (config_.api_base.empty())
      config_.api_base = kOpenAIApiBaseUrl;
  } else {
    if (config_.model.empty())
      config_.model = "gemini-2.5-flash";
  }

  if (!http_client_) {
    // This shouldn't happen in normal usage but handle gracefully
    LogDebug("Warning: No HTTP client provided to BrowserAIService");
  }

  // Set timeout on HTTP client
  if (http_client_) {
    http_client_->SetTimeout(config_.timeout_seconds);
  }

  LogDebug(absl::StrFormat("BrowserAIService initialized with model: %s",
                           config_.model));
}

void BrowserAIService::SetRomContext(Rom* rom) {
  std::lock_guard<std::mutex> lock(mutex_);
  rom_ = rom;
  if (rom_ && rom_->is_loaded()) {
    // Add ROM-specific context to system instruction
    config_.system_instruction = absl::StrFormat(
        "You are assisting with ROM hacking for The Legend of Zelda: A Link to "
        "the Past. "
        "The ROM file '%s' is currently loaded. %s",
        rom_->filename(), config_.system_instruction);
  }
}

absl::StatusOr<AgentResponse> BrowserAIService::GenerateResponse(
    const std::string& prompt) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!http_client_) {
    return absl::FailedPreconditionError("HTTP client not initialized");
  }

  if (config_.api_key.empty()) {
    return absl::InvalidArgumentError(
        "API key not set. Please provide an API key.");
  }

  LogDebug(absl::StrFormat("Generating response for prompt: %s", prompt));

  // Build API URL
  std::string url = BuildApiUrl("generateContent");

  // Build request body
  std::string request_body;
  if (config_.provider == "openai") {
    url = config_.api_base.empty() ? kOpenAIApiBaseUrl : config_.api_base;
    url += "/chat/completions";
    request_body = BuildOpenAIRequestBody(prompt, nullptr);
  } else {
    request_body = BuildRequestBody(prompt);
  }

  // Set headers
  net::Headers headers;
  headers["Content-Type"] = "application/json";
  if (config_.provider == "openai") {
    headers["Authorization"] = "Bearer " + config_.api_key;
  }

  // Make API request
  auto response_or = http_client_->Post(url, request_body, headers);
  if (!response_or.ok()) {
    return absl::InternalError(absl::StrFormat("Failed to make API request: %s",
                                               response_or.status().message()));
  }

  const auto& response = response_or.value();

  // Check HTTP status
  if (!response.IsSuccess()) {
    if (response.IsClientError()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("API request failed with status %d: %s",
                          response.status_code, response.body));
    } else {
      return absl::InternalError(absl::StrFormat(
          "API server error %d: %s", response.status_code, response.body));
    }
  }

  // Parse response
  if (config_.provider == "openai") {
    return ParseOpenAIResponse(response.body);
  }
  return ParseGeminiResponse(response.body);
}

absl::StatusOr<AgentResponse> BrowserAIService::GenerateResponse(
    const std::vector<agent::ChatMessage>& history) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!http_client_) {
    return absl::FailedPreconditionError("HTTP client not initialized");
  }

  if (config_.api_key.empty()) {
    return absl::InvalidArgumentError(
        "API key not set. Please provide an API key.");
  }

  if (history.empty()) {
    return absl::InvalidArgumentError("Chat history cannot be empty");
  }

  LogDebug(
      absl::StrFormat("Generating response from %zu messages", history.size()));

  // Build API URL
  std::string url = BuildApiUrl("generateContent");

  std::string request_body;
  if (config_.provider == "openai") {
    url = config_.api_base.empty() ? kOpenAIApiBaseUrl : config_.api_base;
    url += "/chat/completions";
    request_body = BuildOpenAIRequestBody("", &history);
  } else {
    // Convert history to Gemini format and build request
    nlohmann::json request;
    request["contents"] =
        nlohmann::json::parse(ConvertHistoryToGeminiFormat(history));

    // Add generation config
    request["generationConfig"]["temperature"] = config_.temperature;
    request["generationConfig"]["maxOutputTokens"] = config_.max_output_tokens;

    // Add system instruction if provided
    if (!config_.system_instruction.empty()) {
      request["systemInstruction"]["parts"][0]["text"] =
          config_.system_instruction;
    }

    request_body = request.dump();
  }

  // Set headers
  net::Headers headers;
  headers["Content-Type"] = "application/json";
  if (config_.provider == "openai") {
    headers["Authorization"] = "Bearer " + config_.api_key;
  }

  // Make API request
  auto response_or = http_client_->Post(url, request_body, headers);
  if (!response_or.ok()) {
    return absl::InternalError(absl::StrFormat("Failed to make API request: %s",
                                               response_or.status().message()));
  }

  const auto& response = response_or.value();

  // Check HTTP status
  if (!response.IsSuccess()) {
    return absl::InternalError(
        absl::StrFormat("API request failed with status %d: %s",
                        response.status_code, response.body));
  }

  // Parse response
  if (config_.provider == "openai") {
    return ParseOpenAIResponse(response.body);
  }
  return ParseGeminiResponse(response.body);
}

absl::StatusOr<std::vector<ModelInfo>> BrowserAIService::ListAvailableModels() {
  std::lock_guard<std::mutex> lock(mutex_);
  // For browser context, return curated lists for configured provider
  std::vector<ModelInfo> models;

  const std::string provider =
      config_.provider.empty() ? "gemini" : config_.provider;

  if (provider == "openai") {
    models.push_back({.name = "gpt-4o-mini",
                      .display_name = "GPT-4o Mini",
                      .provider = "openai",
                      .description = "Fast/cheap OpenAI model",
                      .family = "gpt-4o",
                      .is_local = false});
    models.push_back({.name = "gpt-4o",
                      .display_name = "GPT-4o",
                      .provider = "openai",
                      .description = "Balanced OpenAI flagship model",
                      .family = "gpt-4o",
                      .is_local = false});
    models.push_back({.name = "gpt-4.1-mini",
                      .display_name = "GPT-4.1 Mini",
                      .provider = "openai",
                      .description = "Lightweight 4.1 variant",
                      .family = "gpt-4.1",
                      .is_local = false});
  } else {
    models.push_back(
        {.name = "gemini-2.5-flash",
         .display_name = "Gemini 2.0 Flash (Experimental)",
         .provider = "gemini",
         .description = "Fastest Gemini model with experimental features",
         .family = "gemini",
         .is_local = false});

    models.push_back({.name = "gemini-1.5-flash",
                      .display_name = "Gemini 1.5 Flash",
                      .provider = "gemini",
                      .description = "Fast and efficient for most tasks",
                      .family = "gemini",
                      .is_local = false});

    models.push_back({.name = "gemini-1.5-flash-8b",
                      .display_name = "Gemini 1.5 Flash 8B",
                      .provider = "gemini",
                      .description = "Smaller, faster variant of Flash",
                      .family = "gemini",
                      .parameter_size = "8B",
                      .is_local = false});

    models.push_back({.name = "gemini-1.5-pro",
                      .display_name = "Gemini 1.5 Pro",
                      .provider = "gemini",
                      .description = "Most capable model for complex tasks",
                      .family = "gemini",
                      .is_local = false});
  }

  return models;
}

absl::StatusOr<AgentResponse> BrowserAIService::AnalyzeImage(
    const std::string& image_data, const std::string& prompt) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!http_client_) {
    return absl::FailedPreconditionError("HTTP client not initialized");
  }

  if (config_.provider == "openai") {
    return absl::UnimplementedError(
        "Image analysis not yet supported for OpenAI in WASM build");
  }

  if (config_.api_key.empty()) {
    return absl::InvalidArgumentError(
        "API key not set. Please provide a Gemini API key.");
  }

  LogDebug(absl::StrFormat("Analyzing image with prompt: %s", prompt));

  // Build API URL
  std::string url = BuildApiUrl("generateContent");

  // Determine MIME type from image data prefix if present
  std::string mime_type = "image/png";  // Default
  if (image_data.find("data:image/jpeg") == 0 ||
      image_data.find("data:image/jpg") == 0) {
    mime_type = "image/jpeg";
  }

  // Strip data URL prefix if present
  std::string clean_image_data = image_data;
  size_t comma_pos = image_data.find(',');
  if (comma_pos != std::string::npos && image_data.find("data:") == 0) {
    clean_image_data = image_data.substr(comma_pos + 1);
  }

  // Build multimodal request
  std::string request_body =
      BuildMultimodalRequestBody(prompt, clean_image_data, mime_type);

  // Set headers
  net::Headers headers;
  headers["Content-Type"] = "application/json";

  // Make API request
  auto response_or = http_client_->Post(url, request_body, headers);
  if (!response_or.ok()) {
    return absl::InternalError(absl::StrFormat("Failed to make API request: %s",
                                               response_or.status().message()));
  }

  const auto& response = response_or.value();

  // Check HTTP status
  if (!response.IsSuccess()) {
    return absl::InternalError(
        absl::StrFormat("API request failed with status %d: %s",
                        response.status_code, response.body));
  }

  // Parse response
  return ParseGeminiResponse(response.body);
}

absl::Status BrowserAIService::CheckAvailability() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!http_client_) {
    return absl::FailedPreconditionError("HTTP client not initialized");
  }

  if (config_.api_key.empty()) {
    return absl::InvalidArgumentError("API key not set");
  }

  net::Headers headers;
  std::string url;

  if (config_.provider == "openai") {
    url = config_.api_base.empty() ? kOpenAIApiBaseUrl : config_.api_base;
    if (!url.empty() && url.back() == '/')
      url.pop_back();
    url += "/models";
    headers["Authorization"] = "Bearer " + config_.api_key;
  } else {
    url = absl::StrFormat("%s%s?key=%s", kGeminiApiBaseUrl, config_.model,
                          config_.api_key);
  }

  auto response_or = http_client_->Get(url, headers);

  if (!response_or.ok()) {
    return absl::UnavailableError(
        absl::StrFormat("Cannot reach %s API: %s", config_.provider,
                        response_or.status().message()));
  }

  const auto& response = response_or.value();
  if (!response.IsSuccess()) {
    if (response.status_code == 401 || response.status_code == 403) {
      return absl::PermissionDeniedError("Invalid API key");
    }
    return absl::UnavailableError(absl::StrFormat(
        "%s API returned error %d", config_.provider, response.status_code));
  }

  return absl::OkStatus();
}

void BrowserAIService::UpdateApiKey(const std::string& api_key) {
  std::lock_guard<std::mutex> lock(mutex_);
  config_.api_key = api_key;

  // Store in sessionStorage for this session
  // Note: This is handled by the secure storage module
  LogDebug("API key updated");
}

std::string BrowserAIService::BuildApiUrl(const std::string& endpoint) const {
  if (config_.provider == "openai") {
    std::string base =
        config_.api_base.empty() ? kOpenAIApiBaseUrl : config_.api_base;
    if (!base.empty() && base.back() == '/') {
      base.pop_back();
    }
    return absl::StrFormat("%s/%s", base, endpoint);
  }

  return absl::StrFormat("%s%s:%s?key=%s", kGeminiApiBaseUrl, config_.model,
                         endpoint, config_.api_key);
}

std::string BrowserAIService::BuildRequestBody(const std::string& prompt,
                                               bool include_system) const {
  nlohmann::json request;

  // Build contents array with user prompt
  nlohmann::json user_part;
  user_part["text"] = prompt;

  nlohmann::json user_content;
  user_content["parts"] = nlohmann::json::array({user_part});
  user_content["role"] = "user";

  request["contents"] = nlohmann::json::array({user_content});

  // Add generation config
  request["generationConfig"]["temperature"] = config_.temperature;
  request["generationConfig"]["maxOutputTokens"] = config_.max_output_tokens;

  // Add system instruction if provided and requested
  if (include_system && !config_.system_instruction.empty()) {
    nlohmann::json system_part;
    system_part["text"] = config_.system_instruction;
    request["systemInstruction"]["parts"] =
        nlohmann::json::array({system_part});
  }

  return request.dump();
}

std::string BrowserAIService::BuildMultimodalRequestBody(
    const std::string& prompt, const std::string& image_data,
    const std::string& mime_type) const {
  nlohmann::json request;

  // Build parts array with text and image
  nlohmann::json text_part;
  text_part["text"] = prompt;

  nlohmann::json image_part;
  image_part["inline_data"]["mime_type"] = mime_type;
  image_part["inline_data"]["data"] = image_data;

  nlohmann::json content;
  content["parts"] = nlohmann::json::array({text_part, image_part});
  content["role"] = "user";

  request["contents"] = nlohmann::json::array({content});

  // Add generation config
  request["generationConfig"]["temperature"] = config_.temperature;
  request["generationConfig"]["maxOutputTokens"] = config_.max_output_tokens;

  // Add system instruction if provided
  if (!config_.system_instruction.empty()) {
    nlohmann::json system_part;
    system_part["text"] = config_.system_instruction;
    request["systemInstruction"]["parts"] =
        nlohmann::json::array({system_part});
  }

  return request.dump();
}

std::string BrowserAIService::BuildOpenAIRequestBody(
    const std::string& prompt,
    const std::vector<agent::ChatMessage>* history) const {
  nlohmann::json request;
  request["model"] = config_.model.empty() ? "gpt-4o-mini" : config_.model;

  nlohmann::json messages = nlohmann::json::array();
  if (!config_.system_instruction.empty()) {
    messages.push_back(
        {{"role", "system"}, {"content", config_.system_instruction}});
  }

  if (history && !history->empty()) {
    for (const auto& msg : *history) {
      messages.push_back(
          {{"role", msg.sender == agent::ChatMessage::Sender::kUser
                        ? "user"
                        : "assistant"},
           {"content", msg.message}});
    }
  } else if (!prompt.empty()) {
    messages.push_back({{"role", "user"}, {"content", prompt}});
  }

  request["messages"] = messages;
  request["temperature"] = config_.temperature;
  request["max_tokens"] = config_.max_output_tokens;

  return request.dump();
}

absl::StatusOr<AgentResponse> BrowserAIService::ParseGeminiResponse(
    const std::string& response_body) const {
  try {
    nlohmann::json json = nlohmann::json::parse(response_body);

    // Check for API errors
    auto error_status = CheckForApiError(json);
    if (!error_status.ok()) {
      return error_status;
    }

    // Extract text from candidates
    std::string text_content = ExtractTextFromCandidates(json);

    if (text_content.empty()) {
      return absl::InternalError("Empty response from Gemini API");
    }

    // Build agent response
    AgentResponse response;
    response.text_response = text_content;
    response.provider = "gemini";
    response.model = config_.model;

    // Add any safety ratings or filters as warnings
    if (json.contains("promptFeedback") &&
        json["promptFeedback"].contains("safetyRatings")) {
      for (const auto& rating : json["promptFeedback"]["safetyRatings"]) {
        if (rating.contains("probability") &&
            rating["probability"] != "NEGLIGIBLE" &&
            rating["probability"] != "LOW") {
          response.warnings.push_back(absl::StrFormat(
              "Content flagged: %s (%s)", rating.value("category", "unknown"),
              rating.value("probability", "unknown")));
        }
      }
    }

    LogDebug(absl::StrFormat("Successfully parsed response with %zu characters",
                             text_content.length()));

    return response;

  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse Gemini response: %s", e.what()));
  }
}

absl::StatusOr<AgentResponse> BrowserAIService::ParseOpenAIResponse(
    const std::string& response_body) const {
  try {
    nlohmann::json json = nlohmann::json::parse(response_body);

    if (json.contains("error")) {
      const auto& err = json["error"];
      std::string message = err.value("message", "Unknown error");
      int code = err.value("code", 0);
      if (code == 401 || code == 403)
        return absl::UnauthenticatedError(message);
      if (code == 429)
        return absl::ResourceExhaustedError(message);
      return absl::InternalError(message);
    }

    if (!json.contains("choices") || !json["choices"].is_array() ||
        json["choices"].empty()) {
      return absl::InternalError("Empty response from OpenAI API");
    }

    const auto& choice = json["choices"][0];
    if (!choice.contains("message") || !choice["message"].contains("content")) {
      return absl::InternalError("Malformed OpenAI response");
    }

    std::string text = choice["message"]["content"].get<std::string>();
    if (text.empty()) {
      return absl::InternalError("OpenAI returned empty content");
    }

    AgentResponse response;
    response.text_response = text;
    response.provider = "openai";
    response.model = config_.model;
    return response;
  } catch (const nlohmann::json::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse OpenAI response: %s", e.what()));
  }
}

std::string BrowserAIService::ExtractTextFromCandidates(
    const nlohmann::json& json) const {
  if (!json.contains("candidates") || !json["candidates"].is_array() ||
      json["candidates"].empty()) {
    return "";
  }

  const auto& candidate = json["candidates"][0];

  if (!candidate.contains("content") ||
      !candidate["content"].contains("parts") ||
      !candidate["content"]["parts"].is_array() ||
      candidate["content"]["parts"].empty()) {
    return "";
  }

  std::string result;
  for (const auto& part : candidate["content"]["parts"]) {
    if (part.contains("text")) {
      result += part["text"].get<std::string>();
    }
  }

  return result;
}

absl::Status BrowserAIService::CheckForApiError(
    const nlohmann::json& json) const {
  if (json.contains("error")) {
    const auto& error = json["error"];
    int code = error.value("code", 0);
    std::string message = error.value("message", "Unknown error");
    std::string status = error.value("status", "");

    // Map common error codes to appropriate status codes
    if (code == 400 || status == "INVALID_ARGUMENT") {
      return absl::InvalidArgumentError(message);
    } else if (code == 401 || status == "UNAUTHENTICATED") {
      return absl::UnauthenticatedError(message);
    } else if (code == 403 || status == "PERMISSION_DENIED") {
      return absl::PermissionDeniedError(message);
    } else if (code == 429 || status == "RESOURCE_EXHAUSTED") {
      return absl::ResourceExhaustedError(message);
    } else if (code == 503 || status == "UNAVAILABLE") {
      return absl::UnavailableError(message);
    } else {
      return absl::InternalError(message);
    }
  }

  return absl::OkStatus();
}

void BrowserAIService::LogDebug(const std::string& message) const {
  if (config_.verbose) {
    // Use console.log for browser debugging
    EM_ASM({ console.log('[BrowserAIService] ' + UTF8ToString($0)); },
           message.c_str());
  }
}

}  // namespace cli
}  // namespace yaze

#endif  // __EMSCRIPTEN__
