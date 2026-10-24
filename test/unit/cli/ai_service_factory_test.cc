#include "cli/service/ai/service_factory.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#if defined(YAZE_AI_RUNTIME_AVAILABLE) && defined(YAZE_WITH_JSON)
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/ai/ai_config_utils.h"
#include "cli/service/ai/anthropic_ai_service.h"
#include "cli/service/ai/gemini_ai_service.h"
#include "cli/service/ai/openai_ai_service.h"
#include "nlohmann/json.hpp"
#endif
#ifdef YAZE_BUILD_AGENT_UI
#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_editor.h"
#endif
#include "rom/rom.h"

ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, anthropic_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, openai_base_url);
ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze::cli {

#if defined(YAZE_AI_RUNTIME_AVAILABLE) && defined(YAZE_WITH_JSON)
class GeminiAIServiceTestPeer {
 public:
  static absl::StatusOr<AgentResponse> Parse(const std::string& response,
                                             bool native_function_calling) {
    GeminiConfig config;
    config.system_instruction = "Offline captured-response parser test";
    config.use_function_calling = native_function_calling;
    GeminiAIService service(config);
    return service.ParseGeminiResponse(response);
  }
  static absl::StatusOr<std::string> Build(
      const std::vector<agent::ChatMessage>& history) {
    GeminiConfig config;
    config.system_instruction = "Offline request roundtrip";
    config.max_output_tokens = 8192;
    GeminiAIService service(config);
    return service.BuildRequestBody(history);
  }
};
class OpenAIAIServiceTestPeer {
 public:
  static absl::StatusOr<AgentResponse> Parse(const std::string& response,
                                             OpenAIConfig config = {}) {
    config.system_instruction = "Offline response roundtrip";
    OpenAIAIService service(config);
    return service.ParseOpenAIResponse(response);
  }
  static absl::StatusOr<std::string> Build(
      const std::vector<agent::ChatMessage>& history,
      OpenAIConfig config = {}) {
    config.system_instruction = "Offline request roundtrip";
    config.max_output_tokens = 8192;
    OpenAIAIService service(config);
    return service.BuildRequestBody(history);
  }
  static absl::StatusOr<std::vector<ModelInfo>> Models(
      const std::string& body, const std::string& base) {
    OpenAIConfig config;
    config.base_url = base;
    config.system_instruction = "Offline model discovery";
    return OpenAIAIService(config).ParseModelList(body);
  }
  static bool Responses(const std::string& base, const std::string& model) {
    OpenAIConfig config;
    config.system_instruction = "Offline route test";
    config.base_url = base;
    config.model = model;
    return OpenAIAIService(config).UsesResponsesApi();
  }
};
class AnthropicAIServiceTestPeer {
 public:
  static absl::StatusOr<AgentResponse> Parse(const std::string& response) {
    AnthropicConfig config;
    config.system_instruction = "Offline response roundtrip";
    return AnthropicAIService(config).ParseAnthropicResponse(response);
  }
  static absl::StatusOr<std::string> Build(
      const std::vector<agent::ChatMessage>& history) {
    AnthropicConfig config;
    config.system_instruction = "Offline request roundtrip";
    config.max_output_tokens = 8192;
    return AnthropicAIService(config).BuildRequestBody(history);
  }
};
#endif

namespace {

#if defined(YAZE_AI_RUNTIME_AVAILABLE) && defined(YAZE_WITH_JSON)
constexpr char kCapturedBasinSummary[] =
    "22 objects in dungeon room 0x25 (ID 37) were successfully moved two tile "
    "rows north. The changes were applied to the native editor session with "
    "ID \"0\", and an undo action was recorded. The ROM was not saved.";

std::string GeminiTextResponse(const std::string& text) {
  return nlohmann::json{
      {"candidates",
       {{{"content", {{"parts", {{{"text", text}}}}, {"role", "model"}}},
         {"finishReason", "STOP"}}}}}
      .dump();
}

TEST(GeminiResponseTest, NativeFunctionCallingAcceptsCapturedPlainTextSummary) {
  const auto response = GeminiAIServiceTestPeer::Parse(
      GeminiTextResponse(kCapturedBasinSummary), true);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_EQ(response->text_response, kCapturedBasinSummary);
  EXPECT_TRUE(response->commands.empty());
  EXPECT_TRUE(response->tool_calls.empty());
}

TEST(GeminiResponseTest, LegacyJsonModeRejectsPlainTextSummary) {
  const auto response = GeminiAIServiceTestPeer::Parse(
      GeminiTextResponse(kCapturedBasinSummary), false);
  ASSERT_FALSE(response.ok());
  EXPECT_THAT(response.status().message(),
              testing::HasSubstr("Expected at least one of"));
}

TEST(GeminiResponseTest, NativeFunctionCallKeepsArgumentsAlongsidePlainText) {
  const std::string batch =
      R"({"session_id":"0","expected_objects":[],"moves":[]})";
  const nlohmann::json function_part = {
      {"functionCall",
       {{"name", "dungeon-edit-apply"},
        {"args", {{"room", "0x25"}, {"batch", batch}}}}}};
  const nlohmann::json payload = {
      {"candidates",
       {{{"content",
          {{"parts", {function_part, {{"text", "Batch applied."}}}}}},
         {"finishReason", "STOP"}}}}};
  const auto response = GeminiAIServiceTestPeer::Parse(payload.dump(), true);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_EQ(response->text_response, "Batch applied.");
  ASSERT_EQ(response->tool_calls.size(), 1u);
  EXPECT_EQ(response->tool_calls[0].tool_name, "dungeon-edit-apply");
  EXPECT_EQ(response->tool_calls[0].args.at("room"), "0x25");
  EXPECT_EQ(response->tool_calls[0].args.at("batch"), batch);
  EXPECT_TRUE(response->commands.empty());
}

TEST(GeminiResponseTest, NativeFunctionCallingRejectsEmptyText) {
  const auto response =
      GeminiAIServiceTestPeer::Parse(GeminiTextResponse(" \n\t "), true);
  ASSERT_FALSE(response.ok());
  EXPECT_THAT(response.status().message(),
              testing::HasSubstr("Expected at least one of"));
}

std::vector<agent::ChatMessage> InitialNativeHistory() {
  return {{agent::ChatMessage::Sender::kUser,
           "Inspect 53 guards, move 22 basin objects north; do not save.",
           absl::UnixEpoch()}};
}

void AppendNativeExchange(std::vector<agent::ChatMessage>& history,
                          const AgentResponse& response,
                          const std::string& output, bool is_error = false) {
  ASSERT_TRUE(response.native_content);
  agent::ChatMessage assistant;
  assistant.sender = agent::ChatMessage::Sender::kAgent;
  assistant.timestamp = absl::UnixEpoch();
  assistant.is_internal = true;
  assistant.native_content = response.native_content;
  history.push_back(assistant);
  for (const auto& call : response.tool_calls) {
    agent::ChatMessage result;
    result.sender = agent::ChatMessage::Sender::kUser;
    result.timestamp = absl::UnixEpoch();
    result.is_internal = true;
    result.native_tool_result =
        NativeToolResult{response.native_content->provider,
                         response.native_content->model,
                         response.native_content->api,
                         call.id,
                         call.tool_name,
                         output,
                         is_error};
    history.push_back(result);
  }
}

TEST(NativeProviderProtocolTest, Gemini38InspectApplySummaryRoundTrip) {
  auto history = InitialNativeHistory();
  for (const auto& name : {"dungeon-edit-inspect", "dungeon-edit-apply"}) {
    const nlohmann::json content = {
        {"role", "model"},
        {"parts",
         {{{"thoughtSignature", "offline-captured-signature"},
           {"functionCall",
            {{"name", name}, {"id", name}, {"args", {{"room", "0x25"}}}}}}}}};
    const auto parsed = GeminiAIServiceTestPeer::Parse(
        nlohmann::json{
            {"candidates", {{{"content", content}, {"finishReason", "STOP"}}}}}
            .dump(),
        true);
    ASSERT_TRUE(parsed.ok()) << parsed.status();
    ASSERT_EQ(parsed->tool_calls.size(), 1u);
    EXPECT_EQ(parsed->tool_calls[0].id, name);
    const std::string receipt =
        name == std::string("dungeon-edit-inspect")
            ? R"({"room":37,"object_count":53,"rom_saved":false})"
            : R"({"room":37,"moved_objects":22,"undo_recorded":true,"rom_saved":false})";
    AppendNativeExchange(history, *parsed, receipt);
    const auto body = GeminiAIServiceTestPeer::Build(history);
    ASSERT_TRUE(body.ok()) << body.status();
    const auto request = nlohmann::json::parse(*body);
    const auto& contents = request["contents"];
    EXPECT_EQ(contents[contents.size() - 2], content);
    EXPECT_EQ(contents.back()["parts"][0]["functionResponse"]["name"], name);
    EXPECT_EQ(contents.back()["parts"][0]["functionResponse"]["id"], name);
    EXPECT_EQ(contents.back()["parts"][0]["functionResponse"]["response"],
              nlohmann::json::parse(receipt));
    EXPECT_EQ(request["generationConfig"]["thinkingConfig"]["thinkingLevel"],
              "LOW");
    EXPECT_EQ(request["generationConfig"]["maxOutputTokens"], 8192);
    EXPECT_FALSE(request["generationConfig"].contains("temperature"));
    ASSERT_TRUE(request.contains("tools"));
    EXPECT_FALSE(request["tools"].empty());
  }
  auto summary = GeminiAIServiceTestPeer::Parse(
      GeminiTextResponse(kCapturedBasinSummary), true);
  ASSERT_TRUE(summary.ok()) << summary.status();
  EXPECT_EQ(summary->text_response, kCapturedBasinSummary);
}

TEST(NativeProviderProtocolTest,
     OpenAi61ResponsesInspectApplySummaryRoundTrip) {
  auto history = InitialNativeHistory();
  for (const auto& name : {"dungeon-edit-inspect", "dungeon-edit-apply"}) {
    const nlohmann::json output = {
        {{"type", "reasoning"},
         {"id", "rs_offline"},
         {"summary", nlohmann::json::array()},
         {"encrypted_content", "offline-encrypted-state"}},
        {{"type", "function_call"},
         {"id", "fc_item_not_call_id"},
         {"call_id", name},
         {"name", name},
         {"arguments", R"({"room":"0x25"})"}}};
    auto parsed = OpenAIAIServiceTestPeer::Parse(
        nlohmann::json{{"output", output}, {"status", "completed"}}.dump());
    ASSERT_TRUE(parsed.ok()) << parsed.status();
    ASSERT_EQ(parsed->tool_calls.size(), 1u);
    EXPECT_EQ(parsed->tool_calls[0].id, name);
    AppendNativeExchange(history, *parsed, R"({"room":37,"rom_saved":false})");
    const auto body = OpenAIAIServiceTestPeer::Build(history);
    ASSERT_TRUE(body.ok()) << body.status();
    const auto request = nlohmann::json::parse(*body);
    EXPECT_EQ(request["model"], "gpt-6.1-sol");
    EXPECT_EQ(request["max_output_tokens"], 8192);
    EXPECT_EQ(request["reasoning"]["effort"], "low");
    EXPECT_EQ(request["store"], false);
    EXPECT_EQ(request["include"][0], "reasoning.encrypted_content");
    EXPECT_FALSE(request.contains("messages"));
    EXPECT_FALSE(request.contains("temperature"));
    EXPECT_FALSE(request.contains("max_tokens"));
    const auto& input = request["input"];
    EXPECT_EQ(input[input.size() - 3], output[0]);
    EXPECT_EQ(input[input.size() - 2], output[1]);
    EXPECT_EQ(input.back()["type"], "function_call_output");
    EXPECT_EQ(input.back()["call_id"], name);
    EXPECT_EQ(input.back()["output"], R"({"room":37,"rom_saved":false})");
    ASSERT_FALSE(request["tools"].empty());
    EXPECT_EQ(request["tools"][0]["type"], "function");
    EXPECT_TRUE(request["tools"][0].contains("name"));
    EXPECT_FALSE(request["tools"][0].contains("function"));
    EXPECT_EQ(request["tools"][0]["strict"], false);
  }
  auto summary = OpenAIAIServiceTestPeer::Parse(nlohmann::json{
      {"status", "completed"},
      {"output",
       {{{"type", "message"},
         {"role", "assistant"},
         {"content",
          {{{"type", "output_text"},
            {"text", kCapturedBasinSummary}}}}}}}}.dump());
  ASSERT_TRUE(summary.ok()) << summary.status();
  EXPECT_EQ(summary->text_response, kCapturedBasinSummary);
}

TEST(NativeProviderProtocolTest, Anthropic55InspectApplySummaryRoundTrip) {
  auto history = InitialNativeHistory();
  for (const auto& name : {"dungeon-edit-inspect", "dungeon-edit-apply"}) {
    const nlohmann::json content = {
        {{"type", "thinking"},
         {"thinking", ""},
         {"signature", "offline-thinking-signature"}},
        {{"type", "tool_use"},
         {"id", name},
         {"name", name},
         {"input", {{"room", "0x25"}}}}};
    auto parsed = AnthropicAIServiceTestPeer::Parse(
        nlohmann::json{{"content", content}, {"stop_reason", "tool_use"}}
            .dump());
    ASSERT_TRUE(parsed.ok()) << parsed.status();
    ASSERT_EQ(parsed->tool_calls.size(), 1u);
    EXPECT_EQ(parsed->tool_calls[0].id, name);
    AppendNativeExchange(history, *parsed, R"({"rom_saved":false})");
    const auto body = AnthropicAIServiceTestPeer::Build(history);
    ASSERT_TRUE(body.ok()) << body.status();
    const auto request = nlohmann::json::parse(*body);
    EXPECT_EQ(request["model"], "claude-sonnet-5-5");
    EXPECT_EQ(request["output_config"]["effort"], "medium");
    EXPECT_EQ(request["max_tokens"], 8192);
    EXPECT_FALSE(request.contains("temperature"));
    EXPECT_FALSE(request.contains("top_p"));
    EXPECT_FALSE(request.contains("top_k"));
    const auto& messages = request["messages"];
    EXPECT_EQ(messages[messages.size() - 2]["content"], content);
    EXPECT_EQ(messages.back()["content"][0]["type"], "tool_result");
    EXPECT_EQ(messages.back()["content"][0]["tool_use_id"], name);
    EXPECT_EQ(messages.back()["content"][0]["content"],
              R"({"rom_saved":false})");
    EXPECT_EQ(messages.back()["content"][0]["is_error"], false);
    ASSERT_FALSE(request["tools"].empty());
  }
  const auto summary = AnthropicAIServiceTestPeer::Parse(nlohmann::json{
      {"content", {{{"type", "text"}, {"text", kCapturedBasinSummary}}}},
      {"stop_reason", "end_turn"}}
                                                             .dump());
  ASSERT_TRUE(summary.ok()) << summary.status();
  EXPECT_EQ(summary->text_response, kCapturedBasinSummary);
}

TEST(NativeProviderProtocolTest,
     ResponsesRoutingRequiresExactOfficialHostAndKnownModel) {
  for (const auto& base :
       {"https://api.openai.com", "https://api.openai.com/v1/",
        "HTTPS://API.OPENAI.COM", "https://api.openai.com:443"}) {
    EXPECT_TRUE(OpenAIAIServiceTestPeer::Responses(base, "gpt-6.1-sol"))
        << base;
    EXPECT_FALSE(OpenAIAIServiceTestPeer::Responses(base, "gpt-4o-mini"))
        << base;
    EXPECT_FALSE(
        OpenAIAIServiceTestPeer::Responses(base, "explicit-custom-model"))
        << base;
  }
  for (const auto& base :
       {"http://localhost:1234/v1", "http://127.0.0.1:8000/afs/v1",
        "https://api.openai.com.evil.test", "https://proxy/api.openai.com"}) {
    EXPECT_FALSE(OpenAIAIServiceTestPeer::Responses(base, "gpt-6.1-sol"))
        << base;
  }
}

TEST(NativeProviderProtocolTest,
     CompatibleEndpointRetainsChatCompletionsToolRoundTrip) {
  OpenAIConfig config;
  config.base_url = "http://localhost:1234/v1/";
  config.model = "explicit-local-model";
  const nlohmann::json assistant = {
      {"role", "assistant"},
      {"content", nullptr},
      {"tool_calls",
       {{{"id", "local-inspect"},
         {"type", "function"},
         {"function",
          {{"name", "dungeon-edit-inspect"},
           {"arguments", R"({"room":"0x25"})"}}}}}}};
  const auto parsed = OpenAIAIServiceTestPeer::Parse(
      nlohmann::json{{"choices", {{{"message", assistant}}}}}.dump(), config);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  auto history = InitialNativeHistory();
  AppendNativeExchange(history, *parsed, "local receipt");
  auto body = OpenAIAIServiceTestPeer::Build(history, config);
  ASSERT_TRUE(body.ok()) << body.status();
  const auto request = nlohmann::json::parse(*body);
  EXPECT_TRUE(request.contains("messages"));
  EXPECT_FALSE(request.contains("input"));
  EXPECT_EQ(request["max_tokens"], 8192);
  EXPECT_TRUE(request.contains("temperature"));
  const auto& messages = request["messages"];
  EXPECT_EQ(messages[messages.size() - 2], assistant);
  EXPECT_EQ(messages.back()["role"], "tool");
  EXPECT_EQ(messages.back()["tool_call_id"], "local-inspect");
  EXPECT_EQ(messages.back()["content"], "local receipt");
}

TEST(NativeProviderProtocolTest, ParallelToolResultsAndErrorsStayGrouped) {
  auto history = InitialNativeHistory();
  const nlohmann::json content = {{{"type", "tool_use"},
                                   {"id", "first"},
                                   {"name", "dungeon-edit-inspect"},
                                   {"input", nlohmann::json::object()}},
                                  {{"type", "tool_use"},
                                   {"id", "second"},
                                   {"name", "dungeon-edit-inspect"},
                                   {"input", nlohmann::json::object()}}};
  const auto parsed = AnthropicAIServiceTestPeer::Parse(
      nlohmann::json{{"content", content}}.dump());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  AppendNativeExchange(history, *parsed, "guard mismatch", true);
  auto body = AnthropicAIServiceTestPeer::Build(history);
  ASSERT_TRUE(body.ok()) << body.status();
  const auto request = nlohmann::json::parse(*body);
  ASSERT_EQ(request["messages"].size(), 3u);
  const auto& results = request["messages"].back()["content"];
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0]["tool_use_id"], "first");
  EXPECT_EQ(results[1]["tool_use_id"], "second");
  EXPECT_EQ(results[0]["is_error"], true);
  EXPECT_EQ(results[1]["is_error"], true);
}

TEST(NativeProviderProtocolTest, ProviderSwitchSkipsForeignOpaqueState) {
  auto history = InitialNativeHistory();
  agent::ChatMessage foreign;
  foreign.sender = agent::ChatMessage::Sender::kAgent;
  foreign.timestamp = absl::UnixEpoch();
  foreign.is_internal = true;
  foreign.native_content = NativeAIContent{
      "gemini", "gemini-3.8-flash", "generateContent", "invalid-foreign-json"};
  history.push_back(foreign);
  agent::ChatMessage receipt;
  receipt.sender = agent::ChatMessage::Sender::kUser;
  receipt.timestamp = absl::UnixEpoch();
  receipt.is_internal = true;
  receipt.message = "Previous provider inspected the room.";
  receipt.native_tool_result =
      NativeToolResult{"gemini",     "gemini-3.8-flash",     "generateContent",
                       "foreign-id", "dungeon-edit-inspect", "old",
                       false};
  history.push_back(receipt);
  auto body = OpenAIAIServiceTestPeer::Build(history);
  ASSERT_TRUE(body.ok()) << body.status();
  EXPECT_EQ(body->find("invalid-foreign-json"), std::string::npos);
  EXPECT_EQ(body->find("foreign-id"), std::string::npos);
  EXPECT_EQ(body->find("function_call_output"), std::string::npos);
}

TEST(NativeProviderProtocolTest,
     ModelDiscoveryKeepsCurrentCloudModelsAndCustomNames) {
  const std::string body =
      R"({"data":[{"id":"gpt-6.1-sol"},{"id":"gpt-6-luna"},{"id":"gpt-audio"},{"id":"gpt-image-1"},{"id":"explicit-local-model"}]})";
  auto cloud = OpenAIAIServiceTestPeer::Models(body, "https://api.openai.com");
  ASSERT_TRUE(cloud.ok()) << cloud.status();
  ASSERT_EQ(cloud->size(), 2u);
  EXPECT_EQ((*cloud)[0].name, "gpt-6.1-sol");
  EXPECT_EQ((*cloud)[1].name, "gpt-6-luna");
  auto local = OpenAIAIServiceTestPeer::Models(body, "http://localhost:1234");
  ASSERT_TRUE(local.ok()) << local.status();
  EXPECT_EQ(local->size(), 5u);
  auto failed_local = OpenAIAIServiceTestPeer::Models(
      R"({"error":{"message":"unavailable"}})", "http://localhost:1234");
  EXPECT_FALSE(failed_local.ok());
  auto failed_cloud = OpenAIAIServiceTestPeer::Models(
      R"({"error":{"message":"unavailable"}})", "https://api.openai.com");
  ASSERT_TRUE(failed_cloud.ok());
  ASSERT_FALSE(failed_cloud->empty());
  EXPECT_EQ(failed_cloud->front().name, "gpt-6.1-sol");
}

TEST(NativeProviderProtocolTest, HistoryWindowKeepsWholeNativeToolTurn) {
  std::vector<agent::ChatMessage> history;
  for (int i = 0; i < 30; ++i) {
    history.push_back(
        {agent::ChatMessage::Sender::kUser, "old prompt", absl::UnixEpoch()});
    history.push_back(
        {agent::ChatMessage::Sender::kAgent, "old reply", absl::UnixEpoch()});
  }
  history.push_back(InitialNativeHistory().front());
  for (int i = 0; i < 8; ++i) {
    AgentResponse response;
    response.native_content = NativeAIContent{
        "openai", "gpt-6.1-sol", "responses",
        R"([{"type":"function_call","call_id":"same-pair","name":"dungeon-edit-inspect","arguments":"{}"}])"};
    response.tool_calls.push_back({"dungeon-edit-inspect", {}, "same-pair"});
    AppendNativeExchange(history, response, "receipt");
  }
  const auto body = OpenAIAIServiceTestPeer::Build(history);
  ASSERT_TRUE(body.ok()) << body.status();
  const auto request = nlohmann::json::parse(*body);
  const auto& input = request["input"];
  ASSERT_EQ(input.size(), 17u);
  EXPECT_EQ(input[0]["role"], "user");
  for (size_t i = 1; i < input.size(); i += 2) {
    EXPECT_EQ(input[i]["type"], "function_call");
    EXPECT_EQ(input[i + 1]["type"], "function_call_output");
    EXPECT_EQ(input[i]["call_id"], input[i + 1]["call_id"]);
  }
}

TEST(NativeProviderProtocolTest, NativeModeDoesNotExecuteLegacyTextToolCalls) {
  const std::string legacy =
      R"({"tool_calls":[{"tool_name":"dungeon-edit-inspect","args":{"room":"0x25"}}]})";
  EXPECT_FALSE(
      GeminiAIServiceTestPeer::Parse(GeminiTextResponse(legacy), true).ok());
  const nlohmann::json openai = {
      {"choices",
       {{{"message", {{"role", "assistant"}, {"content", legacy}}}}}}};
  EXPECT_FALSE(OpenAIAIServiceTestPeer::Parse(openai.dump()).ok());
  const auto gemini_legacy =
      GeminiAIServiceTestPeer::Parse(GeminiTextResponse(legacy), false);
  ASSERT_TRUE(gemini_legacy.ok());
  EXPECT_EQ(gemini_legacy->tool_calls.size(), 1u);
  EXPECT_FALSE(gemini_legacy->native_content);
  OpenAIConfig config;
  config.use_function_calling = false;
  const auto openai_legacy =
      OpenAIAIServiceTestPeer::Parse(openai.dump(), config);
  ASSERT_TRUE(openai_legacy.ok());
  EXPECT_EQ(openai_legacy->tool_calls.size(), 1u);
  EXPECT_FALSE(openai_legacy->native_content);
}

TEST(NativeProviderProtocolTest,
     MixedNativeAndLegacyTextCallsExecuteOnlyNativeCall) {
  const std::string legacy =
      R"({"text_response":"Inspecting room.","tool_calls":[{"tool_name":"dungeon-edit-inspect","args":{"room":"0x25"}}]})";
  auto payload = nlohmann::json::parse(
      R"({"choices":[{"message":{"role":"assistant","content":"","tool_calls":[{"id":"native-only","function":{"name":"dungeon-edit-inspect","arguments":"{\"room\":\"0x25\"}"}}]}}]})");
  payload["choices"][0]["message"]["content"] = legacy;
  const auto parsed = OpenAIAIServiceTestPeer::Parse(payload.dump());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  ASSERT_EQ(parsed->tool_calls.size(), 1u);
  EXPECT_EQ(parsed->tool_calls[0].id, "native-only");
}
TEST(NativeProviderProtocolTest,
     AnthropicDiscoveryRejectsShellControlCharactersOffline) {
  AnthropicConfig config;
  config.system_instruction = "Offline credential validation";
  config.api_key = "offline' ; forbidden-shell-suffix";
  const auto models = AnthropicAIService(config).ListAvailableModels();
  ASSERT_FALSE(models.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(models.status()));
  EXPECT_THAT(models.status().message(),
              testing::Not(testing::HasSubstr(config.api_key)));
}
#endif

Rom BuildTestRom(std::string_view filename, size_t size_bytes) {
  Rom rom;
  std::vector<uint8_t> data(size_bytes, 0);
  EXPECT_TRUE(rom.LoadFromData(data).ok());
  rom.set_filename(filename);
  return rom;
}

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const char* value) : name_(name) {
    const char* prev = std::getenv(name);
    if (prev) {
      had_prev_ = true;
      prev_value_ = prev;
    }
    Set(value);
  }

  ~ScopedEnvVar() {
    if (had_prev_) {
      Set(prev_value_.c_str());
    } else {
      Unset();
    }
  }

 private:
  void Set(const char* value) {
#ifdef _WIN32
    _putenv_s(name_.c_str(), value ? value : "");
#else
    if (value) {
      setenv(name_.c_str(), value, 1);
    } else {
      unsetenv(name_.c_str());
    }
#endif
  }

  void Unset() {
#ifdef _WIN32
    _putenv_s(name_.c_str(), "");
#else
    unsetenv(name_.c_str());
#endif
  }

  std::string name_;
  bool had_prev_ = false;
  std::string prev_value_;
};

class ScopedFlagState {
 public:
  ScopedFlagState()
      : ai_provider_(absl::GetFlag(FLAGS_ai_provider)),
        ai_model_(absl::GetFlag(FLAGS_ai_model)),
        gemini_api_key_(absl::GetFlag(FLAGS_gemini_api_key)),
        anthropic_api_key_(absl::GetFlag(FLAGS_anthropic_api_key)),
        ollama_host_(absl::GetFlag(FLAGS_ollama_host)),
        openai_base_url_(absl::GetFlag(FLAGS_openai_base_url)),
        rom_(absl::GetFlag(FLAGS_rom)) {}

  ~ScopedFlagState() {
    absl::SetFlag(&FLAGS_ai_provider, ai_provider_);
    absl::SetFlag(&FLAGS_ai_model, ai_model_);
    absl::SetFlag(&FLAGS_gemini_api_key, gemini_api_key_);
    absl::SetFlag(&FLAGS_anthropic_api_key, anthropic_api_key_);
    absl::SetFlag(&FLAGS_ollama_host, ollama_host_);
    absl::SetFlag(&FLAGS_openai_base_url, openai_base_url_);
    absl::SetFlag(&FLAGS_rom, rom_);
  }

 private:
  std::string ai_provider_;
  std::string ai_model_;
  std::string gemini_api_key_;
  std::string anthropic_api_key_;
  std::string ollama_host_;
  std::string openai_base_url_;
  std::string rom_;
};

TEST(AIServiceFactoryTest, BuildConfigReadsRomFlagIntoPathHint) {
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_rom, "roms/oos168.sfc");

  const AIServiceConfig config = BuildAIServiceConfigFromFlags();

  EXPECT_EQ(config.rom_path_hint, "roms/oos168.sfc");
}

TEST(AIServiceFactoryTest, DetectPromptProfileUsesOracleRomPathHint) {
  AIServiceConfig config;
  config.rom_path_hint = "roms/oos168.sfc";

  EXPECT_EQ(DetectPromptProfile(config), AgentPromptProfile::kOracleOfSecrets);
}

TEST(AIServiceFactoryTest, DetectPromptProfileUsesOracleRomContextFilename) {
  AIServiceConfig config;
  Rom rom = BuildTestRom("roms/oracle-dev.sfc", 0x400000);
  config.rom_context = &rom;

  EXPECT_EQ(DetectPromptProfile(config), AgentPromptProfile::kOracleOfSecrets);
}

#if defined(YAZE_BUILD_AGENT_UI) && defined(YAZE_AI_RUNTIME_AVAILABLE)
TEST(AIServiceFactoryTest, MaxOutputTokensFlowsFromEditorToChatProvider) {
  const auto app_data = std::filesystem::temp_directory_path() /
                        ("yaze-agent-token-limit-" +
                         std::to_string(absl::ToUnixNanos(absl::Now())));
  {
    ScopedEnvVar isolated_app_data("YAZE_APP_DATA_DIR",
                                   app_data.string().c_str());
    editor::AgentEditor agent_editor;
    editor::AgentEditor::AgentConfig config;
    config.provider = kProviderMock;
    config.max_output_tokens = 8192;
    agent_editor.ApplyConfig(config);

    const auto* chat = agent_editor.GetAgentChat();
    ASSERT_NE(chat, nullptr);
    const auto& applied =
        agent_editor.GetAgentChat()->GetAgentService()->provider_config();
    ASSERT_TRUE(applied.max_output_tokens.has_value());
    EXPECT_EQ(*applied.max_output_tokens, 8192);
    ASSERT_TRUE(applied.use_function_calling.has_value());
    EXPECT_TRUE(*applied.use_function_calling);
  }
  std::error_code ec;
  std::filesystem::remove_all(app_data, ec);
}
#endif

// These tests require AI runtime to be enabled (service_factory.cc vs stub)
#ifdef YAZE_AI_RUNTIME_AVAILABLE

TEST(AIServiceFactoryTest, OpenAIMissingKeyReturnsError) {
  AIServiceConfig config;
  config.provider = "openai";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
#ifdef YAZE_WITH_JSON
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("OpenAI API key"));
#else
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kInvalidArgument);
#endif
}

#ifdef YAZE_WITH_JSON
TEST(AIServiceFactoryTest, GeminiMissingKeyReturnsError) {
  AIServiceConfig config;
  config.provider = "gemini";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("Gemini API key"));
}
#else
TEST(AIServiceFactoryTest, GeminiUnavailableWithoutJsonSupport) {
  AIServiceConfig config;
  config.provider = "gemini";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("YAZE_WITH_JSON"));
}
#endif

#ifdef YAZE_WITH_JSON
TEST(AIServiceFactoryTest, AnthropicMissingKeyReturnsError) {
  AIServiceConfig config;
  config.provider = "anthropic";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("Anthropic API key"));
}

TEST(AIServiceFactoryTest, ClaudeAliasMapsToAnthropic) {
  AIServiceConfig config;
  config.provider = "claude";
  config.anthropic_api_key = "test-key";

  auto service_or = CreateAIServiceStrict(config);

  ASSERT_TRUE(service_or.ok());
  EXPECT_EQ(service_or.value()->GetProviderName(), "anthropic");
}
#else
TEST(AIServiceFactoryTest, AnthropicUnavailableWithoutJsonSupport) {
  AIServiceConfig config;
  config.provider = "anthropic";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("YAZE_WITH_JSON"));
}

TEST(AIServiceFactoryTest, ClaudeAliasRequiresJsonSupport) {
  AIServiceConfig config;
  config.provider = "claude";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("YAZE_WITH_JSON"));
}
#endif

#ifdef YAZE_WITH_JSON
TEST(AIServiceFactoryTest, OpenAIWithKeyCreatesService) {
  AIServiceConfig config;
  config.provider = "openai";
  config.openai_api_key = "test-key";

  auto service_or = CreateAIServiceStrict(config);

  ASSERT_TRUE(service_or.ok());
  EXPECT_EQ(service_or.value()->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, ChatGptAliasMapsToOpenAI) {
  AIServiceConfig config;
  config.provider = "chatgpt";
  config.openai_api_key = "test-key";

  auto service_or = CreateAIServiceStrict(config);

  ASSERT_TRUE(service_or.ok());
  EXPECT_EQ(service_or.value()->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, LmStudioAliasUsesOpenAIWithLocalBase) {
  AIServiceConfig config;
  config.provider = "lmstudio";
  config.openai_base_url = "http://localhost:1234";

  auto service_or = CreateAIServiceStrict(config);

  ASSERT_TRUE(service_or.ok());
  EXPECT_EQ(service_or.value()->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, OpenAILocalBaseAllowsEmptyKey) {
  AIServiceConfig config;
  config.provider = "openai";
  config.openai_base_url = "http://localhost:1234";

  auto service_or = CreateAIServiceStrict(config);

  ASSERT_TRUE(service_or.ok());
  EXPECT_EQ(service_or.value()->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, AutoDetectsLocalOpenAIBase) {
  AIServiceConfig config;
  config.provider = "auto";
  config.openai_base_url = "http://localhost:1234";

  auto service = CreateAIService(config);

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, AutoDetectsLocalOpenAIBaseBeforeAnthropicKey) {
  AIServiceConfig config;
  config.provider = "auto";
  config.openai_base_url = "http://localhost:1234";
  config.anthropic_api_key = "test-anthropic-key";

  auto service = CreateAIService(config);

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, AutoDetectsOpenAIBaseFromEnv) {
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "auto");
  absl::SetFlag(&FLAGS_ai_model, "");
  absl::SetFlag(&FLAGS_openai_base_url, "https://api.openai.com");
  absl::SetFlag(&FLAGS_ollama_host, "http://localhost:11434");
  absl::SetFlag(&FLAGS_gemini_api_key, "");
  absl::SetFlag(&FLAGS_anthropic_api_key, "");

  ScopedEnvVar clear_gemini("GEMINI_API_KEY", nullptr);
  ScopedEnvVar clear_anthropic("ANTHROPIC_API_KEY", nullptr);
  ScopedEnvVar clear_openai_key("OPENAI_API_KEY", nullptr);
  ScopedEnvVar clear_openai_api_base("OPENAI_API_BASE", nullptr);
  ScopedEnvVar clear_ollama_host("OLLAMA_HOST", nullptr);
  ScopedEnvVar clear_ollama_model("OLLAMA_MODEL", nullptr);
  ScopedEnvVar openai_base("OPENAI_BASE_URL", "http://localhost:1234");

  auto service = CreateAIService();

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, AutoDetectsOpenAIBaseFromEnvBeforeAnthropicKey) {
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "auto");
  absl::SetFlag(&FLAGS_ai_model, "");
  absl::SetFlag(&FLAGS_openai_base_url, "https://api.openai.com");
  absl::SetFlag(&FLAGS_ollama_host, "http://localhost:11434");
  absl::SetFlag(&FLAGS_gemini_api_key, "");
  absl::SetFlag(&FLAGS_anthropic_api_key, "");

  ScopedEnvVar clear_gemini("GEMINI_API_KEY", nullptr);
  ScopedEnvVar set_anthropic("ANTHROPIC_API_KEY", "test-anthropic-key");
  ScopedEnvVar clear_openai_key("OPENAI_API_KEY", nullptr);
  ScopedEnvVar clear_openai_api_base("OPENAI_API_BASE", nullptr);
  ScopedEnvVar clear_ollama_host("OLLAMA_HOST", nullptr);
  ScopedEnvVar clear_ollama_model("OLLAMA_MODEL", nullptr);
  ScopedEnvVar openai_base("OPENAI_BASE_URL", "http://localhost:1234");

  auto service = CreateAIService();

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "openai");
}

TEST(AIServiceFactoryTest, AutoDetectsOpenAIBaseFromApiBaseEnv) {
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "auto");
  absl::SetFlag(&FLAGS_ai_model, "");
  absl::SetFlag(&FLAGS_openai_base_url, "https://api.openai.com");
  absl::SetFlag(&FLAGS_ollama_host, "http://localhost:11434");
  absl::SetFlag(&FLAGS_gemini_api_key, "");
  absl::SetFlag(&FLAGS_anthropic_api_key, "");

  ScopedEnvVar clear_gemini("GEMINI_API_KEY", nullptr);
  ScopedEnvVar clear_anthropic("ANTHROPIC_API_KEY", nullptr);
  ScopedEnvVar clear_openai_key("OPENAI_API_KEY", nullptr);
  ScopedEnvVar clear_openai_base("OPENAI_BASE_URL", nullptr);
  ScopedEnvVar clear_ollama_host("OLLAMA_HOST", nullptr);
  ScopedEnvVar clear_ollama_model("OLLAMA_MODEL", nullptr);
  ScopedEnvVar openai_api_base("OPENAI_API_BASE", "http://localhost:1234/v1");

  auto service = CreateAIService();

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "openai");
}
#endif  // YAZE_WITH_JSON

#ifndef YAZE_WITH_JSON
TEST(AIServiceFactoryTest, ChatGptAliasRequiresJsonSupport) {
  AIServiceConfig config;
  config.provider = "chatgpt";
  config.openai_api_key = "test-key";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(service_or.status().message(), testing::HasSubstr("Unknown AI"));
}
#endif

TEST(AIServiceFactoryTest, AutoDetectsOllamaFromEnv) {
  ScopedFlagState flag_state;
  absl::SetFlag(&FLAGS_ai_provider, "auto");
  absl::SetFlag(&FLAGS_ai_model, "");
  absl::SetFlag(&FLAGS_openai_base_url, "https://api.openai.com");
  absl::SetFlag(&FLAGS_ollama_host, "http://localhost:11434");
  absl::SetFlag(&FLAGS_gemini_api_key, "");
  absl::SetFlag(&FLAGS_anthropic_api_key, "");

  ScopedEnvVar clear_gemini("GEMINI_API_KEY", nullptr);
  ScopedEnvVar clear_anthropic("ANTHROPIC_API_KEY", nullptr);
  ScopedEnvVar clear_openai_key("OPENAI_API_KEY", nullptr);
  ScopedEnvVar clear_openai_base("OPENAI_BASE_URL", nullptr);
  ScopedEnvVar clear_openai_api_base("OPENAI_API_BASE", nullptr);
  ScopedEnvVar set_ollama_host("OLLAMA_HOST", "http://localhost:11434");

  auto service = CreateAIService();

  ASSERT_NE(service, nullptr);
  EXPECT_EQ(service->GetProviderName(), "ollama");
}

#endif  // YAZE_AI_RUNTIME_AVAILABLE

// Test stub behavior when AI runtime is disabled
#ifndef YAZE_AI_RUNTIME_AVAILABLE

TEST(AIServiceFactoryTest, StubReturnsDisabledError) {
  AIServiceConfig config;
  config.provider = "openai";

  auto service_or = CreateAIServiceStrict(config);

  EXPECT_FALSE(service_or.ok());
  EXPECT_EQ(service_or.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(service_or.status().message(),
              testing::HasSubstr("AI runtime features are disabled"));
}

#endif  // !YAZE_AI_RUNTIME_AVAILABLE

}  // namespace
}  // namespace yaze::cli
