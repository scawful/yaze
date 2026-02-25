#include "cli/service/agent/conversational_agent_service.h"

#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "absl/strings/match.h"
#include "absl/time/time.h"
#include "rom/rom.h"

namespace yaze::cli::agent {
namespace {

Rom BuildTestRom(std::string_view filename, size_t size_bytes) {
  Rom rom;
  std::vector<uint8_t> data(size_bytes, 0);
  EXPECT_TRUE(rom.LoadFromData(data).ok());
  rom.set_filename(filename);
  return rom;
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
