#include "app/editor/agent/agent_chat.h"
#include "app/editor/agent/agent_chat_history_codec.h"
#include "app/editor/agent/agent_editor.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze::editor {
namespace {

TEST(AgentChatTest, UpdateHarnessTelemetryKeepsLatestEntries) {
  AgentChat chat;
  for (int i = 0; i < 120; ++i) {
    AgentChat::AutomationTelemetry telemetry;
    telemetry.test_id = absl::StrFormat("test-%d", i);
    telemetry.name = "Automation";
    telemetry.status = "ok";
    telemetry.message = "finished";
    telemetry.updated_at = absl::Now();
    chat.UpdateHarnessTelemetry(telemetry);
  }

  const auto& history = chat.GetTelemetryHistory();
  ASSERT_EQ(history.size(), 100u);
  EXPECT_EQ(history.front().test_id, "test-20");
  EXPECT_EQ(history.back().test_id, "test-119");
}

TEST(AgentChatTest, SetLastPlanSummaryStoresLatest) {
  AgentChat chat;
  chat.SetLastPlanSummary("Plan A");
  EXPECT_EQ(chat.GetLastPlanSummary(), "Plan A");
}

TEST(AgentChatTest, ErrorSummaryKeepsReasonWithoutOpaqueProviderPayload) {
  const auto status = absl::InternalError(
      "No textual response. Raw response: {\"thoughtSignature\":\"opaque\"}");
  EXPECT_EQ(AgentChat::SummarizeError(status), "No textual response.");
  EXPECT_LE(
      AgentChat::SummarizeError(absl::InternalError(std::string(4000, 'x')))
          .size(),
      243u);
  EXPECT_EQ(AgentChat::SummarizeError(
                absl::InternalError("Permission denied\nprotocol details")),
            "Permission denied");
}

TEST(AgentChatTest, InternalStateHookAndModelProseAreNotToolReceipts) {
  cli::agent::ChatMessage message;
  message.sender = cli::agent::ChatMessage::Sender::kUser;
  message.is_internal = true;
  message.message = "[AUTO_ORACLE_STATE_HOOK]state[/AUTO_ORACLE_STATE_HOOK]";
  EXPECT_FALSE(AgentChat::DescribeToolResult(message));
  message.is_internal = false;
  message.message =
      "[TOOL RESULT for dungeon-edit-apply]\n{\"changed_object_count\":22}";
  EXPECT_FALSE(AgentChat::DescribeToolResult(message));
  // Native providers retain internal assistant parts too. A model-written
  // marker in those parts is not an editor receipt.
  message.is_internal = true;
  message.sender = cli::agent::ChatMessage::Sender::kAgent;
  message.native_content = cli::NativeAIContent{"gemini", "gemini-3.8-flash",
                                                "generateContent", "{}"};
  EXPECT_FALSE(AgentChat::DescribeToolResult(message));
}

#ifdef YAZE_WITH_JSON
TEST(AgentChatTest, SaveHistoryWritesChatMessages) {
  AgentChat chat;
  std::vector<cli::agent::ChatMessage> history;

  cli::agent::ChatMessage user_msg;
  user_msg.sender = cli::agent::ChatMessage::Sender::kUser;
  user_msg.message = "Hello";
  user_msg.timestamp = absl::UnixEpoch() + absl::Seconds(1);
  history.push_back(user_msg);

  cli::agent::ChatMessage agent_msg;
  agent_msg.sender = cli::agent::ChatMessage::Sender::kAgent;
  agent_msg.message = "Hi there";
  agent_msg.timestamp = absl::UnixEpoch() + absl::Seconds(2);
  history.push_back(agent_msg);

  chat.GetAgentService()->ReplaceHistory(std::move(history));

  auto file_path = std::filesystem::temp_directory_path() /
                   absl::StrCat("agent_chat_history_test_",
                                absl::ToUnixSeconds(absl::Now()), ".json");
  auto status = chat.SaveHistory(file_path.string());
  EXPECT_TRUE(status.ok()) << status.message();

  std::ifstream file(file_path);
  ASSERT_TRUE(file.is_open());

  nlohmann::json data;
  file >> data;

  EXPECT_EQ(data["version"], 4);
  ASSERT_TRUE(data.contains("messages"));
  ASSERT_EQ(data["messages"].size(), 2);
  EXPECT_EQ(data["messages"][0]["sender"], "user");
  EXPECT_EQ(data["messages"][0]["message"], "Hello");
  EXPECT_EQ(data["messages"][1]["sender"], "agent");
  EXPECT_EQ(data["messages"][1]["message"], "Hi there");

  std::error_code ec;
  std::filesystem::remove(file_path, ec);
}
TEST(AgentChatTest, NativeToolReceiptReportsEditUndoAndToolSaveOutcome) {
  cli::agent::ChatMessage message;
  message.sender = cli::agent::ChatMessage::Sender::kUser;
  message.is_internal = true;
  message.native_tool_result = cli::NativeToolResult{
      "gemini",
      "gemini-3.8-flash",
      "generateContent",
      "edit-1",
      "dungeon-edit-apply",
      R"({"session_id":"session-a","room_id":37,"changed_object_count":22,"undo_recorded":true,"rom_saved":false,"objects":[]})",
      false};
  auto result = AgentChat::DescribeToolResult(message);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->tool_name, "dungeon-edit-apply");
  EXPECT_EQ(
      result->summary,
      "22 objects changed | Undo recorded | No save performed by this tool");
  EXPECT_NE(result->details.find("session-a"), std::string::npos);
  // A failed receipt must never become a successful edit receipt, even if it
  // contains JSON-shaped data.
  message.native_tool_result->is_error = true;
  result = AgentChat::DescribeToolResult(message);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->summary.find("objects changed"), std::string::npos);
  EXPECT_EQ(result->summary.find("Tool failed:"), 0u);
}

TEST(AgentChatTest, LegacyInspectReceiptDoesNotClaimRomIsUnsaved) {
  cli::agent::ChatMessage message;
  message.is_internal = true;
  message.sender = cli::agent::ChatMessage::Sender::kUser;
  message.message =
      "[TOOL RESULT for dungeon-edit-inspect]\n"
      "The tool returned the following data:\n"
      R"({"objects":[{},{}],"rom_saved":false})"
      "\n\nPlease provide a text_response field in your JSON to summarize this "
      "information for the user.";
  const auto result = AgentChat::DescribeToolResult(message);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->summary, "2 room objects inspected");
  EXPECT_EQ(result->summary.find("Unsaved"), std::string::npos);
  EXPECT_EQ(result->details.find("Please provide"), std::string::npos);
}

#ifdef YAZE_AI_RUNTIME_AVAILABLE
class ScopedAgentDataDirectory {
 public:
  ScopedAgentDataDirectory()
      : path_(
            std::filesystem::temp_directory_path() /
            absl::StrCat("agent_profile_ui_", absl::ToUnixNanos(absl::Now()))) {
    if (const char* value = std::getenv("YAZE_APP_DATA_DIR"))
      original_ = value;
    Set(path_.string());
  }
  ~ScopedAgentDataDirectory() {
    if (original_) {
      Set(*original_);
    } else {
#ifdef _WIN32
      _putenv_s("YAZE_APP_DATA_DIR", "");
#else
      unsetenv("YAZE_APP_DATA_DIR");
#endif
    }
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

 private:
  void Set(const std::string& value) {
#ifdef _WIN32
    _putenv_s("YAZE_APP_DATA_DIR", value.c_str());
#else
    setenv("YAZE_APP_DATA_DIR", value.c_str(), 1);
#endif
  }
  std::filesystem::path path_;
  std::optional<std::string> original_;
};

TEST(AgentChatTest, ProfileLoadAndSelectionCarryAnthropicCredentialToChat) {
  ScopedAgentDataDirectory isolated_data;
  AgentEditor editor;
  AgentUIContext context;
  editor.SetContext(&context);
  auto profile = editor.GetCurrentProfile();
  profile.provider = cli::kProviderMock;  // No network or provider request.
  profile.name = "Offline credential regression";
  profile.anthropic_api_key = "offline-profile-key";
  editor.SetCurrentProfile(profile);
  EXPECT_EQ(editor.GetAgentChat()
                ->GetAgentService()
                ->provider_config()
                .anthropic_api_key,
            "offline-profile-key");

  ASSERT_TRUE(editor.SaveBotProfile(profile).ok());
  profile.anthropic_api_key = "different-active-key";
  editor.SetCurrentProfile(profile);
  ASSERT_TRUE(editor.LoadBotProfile("Offline credential regression").ok());
  EXPECT_EQ(editor.GetAgentChat()
                ->GetAgentService()
                ->provider_config()
                .anthropic_api_key,
            "offline-profile-key");
}
#endif

TEST(AgentChatTest, NativeProtocolHistoryRoundTripsWithoutCredentials) {
  AgentChatHistoryCodec::Snapshot snapshot;
  snapshot.agent_config.emplace();
  snapshot.agent_config->provider = "gemini";
  snapshot.agent_config->model = "gemini-3.8-flash";
  snapshot.agent_config->gemini_api_key = "offline-gemini-secret";
  snapshot.agent_config->anthropic_api_key = "offline-anthropic-secret";
  snapshot.agent_config->openai_api_key = "offline-openai-secret";
  cli::agent::ChatMessage call;
  call.sender = cli::agent::ChatMessage::Sender::kAgent;
  call.timestamp = absl::UnixEpoch();
  call.is_internal = true;
  call.native_content = cli::NativeAIContent{
      "gemini", "gemini-3.8-flash", "generateContent",
      R"({"role":"model","parts":[{"thoughtSignature":"offline-signature","functionCall":{"name":"dungeon-edit-inspect","id":"inspect-1","args":{"room":"0x25"}}}]})"};
  snapshot.history.push_back(call);
  cli::agent::ChatMessage result;
  result.sender = cli::agent::ChatMessage::Sender::kUser;
  result.timestamp = absl::UnixEpoch();
  result.is_internal = true;
  result.native_tool_result = cli::NativeToolResult{
      "gemini",    "gemini-3.8-flash",     "generateContent",
      "inspect-1", "dungeon-edit-inspect", R"({"objects":53})",
      false};
  snapshot.history.push_back(result);
  const auto path = std::filesystem::temp_directory_path() /
                    absl::StrCat("native_history_codec_",
                                 absl::ToUnixNanos(absl::Now()), ".json");
  ASSERT_TRUE(AgentChatHistoryCodec::Save(path, snapshot).ok());
  std::ifstream input(path);
  const std::string serialized((std::istreambuf_iterator<char>(input)), {});
  EXPECT_EQ(serialized.find("offline-gemini-secret"), std::string::npos);
  EXPECT_EQ(serialized.find("offline-anthropic-secret"), std::string::npos);
  EXPECT_EQ(serialized.find("offline-openai-secret"), std::string::npos);
  auto loaded = AgentChatHistoryCodec::Load(path);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->history.size(), 2u);
  ASSERT_TRUE(loaded->history[0].native_content);
  EXPECT_EQ(loaded->history[0].native_content->content_json,
            call.native_content->content_json);
  ASSERT_TRUE(loaded->history[1].native_tool_result);
  EXPECT_EQ(loaded->history[1].native_tool_result->call_id, "inspect-1");
  EXPECT_EQ(loaded->history[1].native_tool_result->output,
            result.native_tool_result->output);
  EXPECT_FALSE(loaded->history[1].native_tool_result->is_error);
  std::filesystem::remove(path);
}

TEST(AgentChatTest, LegacyHistoryCredentialsRemainReadableButAreNotReexported) {
  const auto path = std::filesystem::temp_directory_path() /
                    absl::StrCat("legacy_history_codec_",
                                 absl::ToUnixNanos(absl::Now()), ".json");
  nlohmann::json legacy = {
      {"version", 4},
      {"messages", nlohmann::json::array()},
      {"agent_config",
       {{"provider", "gemini"}, {"gemini_api_key", "legacy-offline-secret"}}}};
  {
    std::ofstream output(path);
    output << legacy;
  }
  auto loaded = AgentChatHistoryCodec::Load(path);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_TRUE(loaded->agent_config);
  EXPECT_EQ(loaded->agent_config->gemini_api_key, "legacy-offline-secret");
  ASSERT_TRUE(AgentChatHistoryCodec::Save(path, *loaded).ok());
  std::ifstream input(path);
  nlohmann::json rewritten;
  input >> rewritten;
  EXPECT_FALSE(rewritten["agent_config"].contains("gemini_api_key"));
  EXPECT_FALSE(rewritten["agent_config"].contains("anthropic_api_key"));
  EXPECT_FALSE(rewritten["agent_config"].contains("openai_api_key"));
  std::filesystem::remove(path);
}

#else
TEST(AgentChatTest, SaveHistoryRequiresJsonSupport) {
  AgentChat chat;
  auto status = chat.SaveHistory("agent_chat_history.json");
  EXPECT_EQ(status.code(), absl::StatusCode::kUnimplemented);
}
#endif

}  // namespace
}  // namespace yaze::editor
