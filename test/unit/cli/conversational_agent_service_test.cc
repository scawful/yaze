#include "cli/service/agent/conversational_agent_service.h"

#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "absl/strings/match.h"
#include "absl/time/time.h"
#include "rom/rom.h"

namespace yaze::cli::agent {
class ConversationalAgentServiceTestPeer {
 public:
  static void SetAI(ConversationalAgentService& service,
                    std::unique_ptr<AIService> ai) {
    service.ai_service_ = std::move(ai);
  }
};
namespace {

Rom BuildTestRom(std::string_view filename, size_t size_bytes) {
  Rom rom;
  std::vector<uint8_t> data(size_bytes, 0);
  EXPECT_TRUE(rom.LoadFromData(data).ok());
  rom.set_filename(filename);
  return rom;
}

class ScriptedNativeProvider : public AIService {
 public:
  absl::StatusOr<AgentResponse> GenerateResponse(const std::string&) override {
    return absl::UnimplementedError("History required");
  }
  absl::StatusOr<AgentResponse> GenerateResponse(
      const std::vector<ChatMessage>& history) override {
    if (turn_++ == 0) {
      AgentResponse response;
      response.native_content =
          NativeAIContent{"gemini", "gemini-3.8-flash", "generateContent",
                          "offline-call-parts"};
      response.tool_calls = {
          {"dungeon-edit-inspect", {{"room", "0x25"}}, "inspect-success"},
          {"dungeon-edit-inspect", {{"room", "0x25"}}, "inspect-error"}};
      return response;
    }
    EXPECT_GE(history.size(), 4u);
    const size_t n = history.size();
    EXPECT_TRUE(history[n - 3].native_content);
    EXPECT_TRUE(history[n - 3].is_internal);
    EXPECT_EQ(history[n - 3].sender, ChatMessage::Sender::kAgent);
    EXPECT_TRUE(history[n - 2].native_tool_result);
    EXPECT_TRUE(history[n - 1].native_tool_result);
    if (history[n - 2].native_tool_result &&
        history[n - 1].native_tool_result) {
      EXPECT_EQ(history[n - 2].native_tool_result->call_id, "inspect-success");
      EXPECT_EQ(history[n - 2].native_tool_result->output, R"({"objects":53})");
      EXPECT_FALSE(history[n - 2].native_tool_result->is_error);
      EXPECT_EQ(history[n - 1].native_tool_result->call_id, "inspect-error");
      EXPECT_TRUE(history[n - 1].native_tool_result->is_error);
      EXPECT_NE(
          history[n - 1].native_tool_result->output.find("guard mismatch"),
          std::string::npos);
    }
    AgentResponse response;
    response.text_response =
        "Inspection returned one receipt and one guard mismatch. ROM not "
        "saved.";
    return response;
  }
  std::string GetProviderName() const override { return "scripted-offline"; }

 private:
  int turn_ = 0;
};

TEST(ConversationalAgentServiceTest,
     NativeCallsAndFailuresReachNextProviderTurn) {
  AgentConfig config;
  config.max_history_messages = 2;  // Deliberately smaller than the tool group.
  ConversationalAgentService service(config);
  ConversationalAgentServiceTestPeer::SetAI(
      service, std::make_unique<ScriptedNativeProvider>());
  Rom rom = BuildTestRom("offline-native.sfc", 0x200000);
  service.SetRomContext(&rom);
  int calls = 0;
  service.SetDungeonEditorToolCallback(
      [&calls](const ToolCall&, Rom*) -> absl::StatusOr<std::string> {
        if (++calls == 1)
          return std::string(R"({"objects":53})");
        return absl::FailedPreconditionError("guard mismatch");
      });
  const auto result = service.SendMessage("Inspect this room without saving.");
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(calls, 2);
  EXPECT_NE(result->message.find("guard mismatch"), std::string::npos);
  const auto& history = service.GetHistory();
  ASSERT_GE(history.size(), 5u);
  EXPECT_EQ(history.front().sender, ChatMessage::Sender::kUser);
  EXPECT_FALSE(history.front().is_internal);
}

TEST(ConversationalAgentServiceTest, ReplaceHistoryCountsMessagesAndProposals) {
  ConversationalAgentService service;

  std::vector<ChatMessage> history;
  ChatMessage user_msg;
  user_msg.sender = ChatMessage::Sender::kUser;
  user_msg.message = "status";
  user_msg.timestamp = absl::UnixEpoch();
  history.push_back(user_msg);

  ChatMessage agent_msg;
  agent_msg.sender = ChatMessage::Sender::kAgent;
  agent_msg.message = "response";
  agent_msg.timestamp = absl::UnixEpoch() + absl::Seconds(1);
  ChatMessage::ProposalSummary proposal;
  proposal.id = "proposal-1";
  agent_msg.proposal = proposal;
  history.push_back(agent_msg);

  ChatMessage agent_msg2 = agent_msg;
  agent_msg2.message = "follow-up";
  agent_msg2.proposal.reset();
  history.push_back(agent_msg2);

  service.ReplaceHistory(std::move(history));

  auto metrics = service.GetMetrics();
  EXPECT_EQ(metrics.total_user_messages, 1);
  EXPECT_EQ(metrics.total_agent_messages, 2);
  EXPECT_EQ(metrics.total_proposals, 1);
  EXPECT_EQ(metrics.total_tool_calls, 0);
  EXPECT_EQ(metrics.total_commands, 0);
  EXPECT_EQ(metrics.turn_index, 2);
}

TEST(ConversationalAgentServiceTest, ReplaceHistoryUsesSnapshotMetrics) {
  ConversationalAgentService service;

  std::vector<ChatMessage> history;
  ChatMessage user_msg;
  user_msg.sender = ChatMessage::Sender::kUser;
  user_msg.message = "hello";
  user_msg.timestamp = absl::UnixEpoch();
  history.push_back(user_msg);

  ChatMessage agent_msg;
  agent_msg.sender = ChatMessage::Sender::kAgent;
  agent_msg.message = "metrics snapshot";
  agent_msg.timestamp = absl::UnixEpoch() + absl::Seconds(1);
  ChatMessage::SessionMetrics snapshot;
  snapshot.turn_index = 4;
  snapshot.total_user_messages = 5;
  snapshot.total_agent_messages = 4;
  snapshot.total_tool_calls = 3;
  snapshot.total_commands = 2;
  snapshot.total_proposals = 1;
  snapshot.total_elapsed_seconds = 12.0;
  agent_msg.metrics = snapshot;
  history.push_back(agent_msg);

  service.ReplaceHistory(std::move(history));

  auto metrics = service.GetMetrics();
  EXPECT_EQ(metrics.turn_index, 4);
  EXPECT_EQ(metrics.total_user_messages, 5);
  EXPECT_EQ(metrics.total_agent_messages, 4);
  EXPECT_EQ(metrics.total_tool_calls, 3);
  EXPECT_EQ(metrics.total_commands, 2);
  EXPECT_EQ(metrics.total_proposals, 1);
  EXPECT_DOUBLE_EQ(metrics.total_elapsed_seconds, 12.0);
  EXPECT_DOUBLE_EQ(metrics.average_latency_seconds, 3.0);
}

TEST(ConversationalAgentServiceTest,
     SendMessageInjectsOracleHookForOracleDebugIntent) {
  ConversationalAgentService service;
  Rom rom = BuildTestRom("roms/oos168.sfc", 0x400000);
  service.SetRomContext(&rom);

  auto response_or = service.SendMessage("mesen oracle smoke status");
  ASSERT_TRUE(response_or.ok());

#ifdef Z3ED_AI
  const auto& history = service.GetHistory();
  ASSERT_GE(history.size(), 3u);
  EXPECT_EQ(history[0].sender, ChatMessage::Sender::kUser);
  EXPECT_TRUE(history[0].is_internal);
  EXPECT_TRUE(
      absl::StrContains(history[0].message, "[AUTO_ORACLE_STATE_HOOK]"));
  EXPECT_TRUE(
      absl::StrContains(history[0].message, "OracleRom: roms/oos168.sfc"));
  EXPECT_EQ(history[1].sender, ChatMessage::Sender::kUser);
  EXPECT_FALSE(history[1].is_internal);
  EXPECT_EQ(history[1].message, "mesen oracle smoke status");
#else
  const auto& history = service.GetHistory();
  ASSERT_GE(history.size(), 2u);
  EXPECT_EQ(history[0].sender, ChatMessage::Sender::kUser);
  EXPECT_FALSE(history[0].is_internal);
  EXPECT_EQ(history[0].message, "mesen oracle smoke status");
#endif
}

TEST(ConversationalAgentServiceTest,
     SendMessageDoesNotInjectOracleHookForNonOracleIntent) {
  ConversationalAgentService service;
  Rom rom = BuildTestRom("roms/oos168.sfc", 0x400000);
  service.SetRomContext(&rom);

  auto response_or = service.SendMessage("tell me a joke");
  ASSERT_TRUE(response_or.ok());

  const auto& history = service.GetHistory();
  ASSERT_GE(history.size(), 2u);
  EXPECT_EQ(history[0].sender, ChatMessage::Sender::kUser);
  EXPECT_FALSE(history[0].is_internal);
  EXPECT_EQ(history[0].message, "tell me a joke");
}

}  // namespace
}  // namespace yaze::cli::agent
