#include "app/editor/agent/agent_chat.h"

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

  EXPECT_EQ(data["version"], 1);
  ASSERT_TRUE(data.contains("messages"));
  ASSERT_EQ(data["messages"].size(), 2);
  EXPECT_EQ(data["messages"][0]["sender"], "user");
  EXPECT_EQ(data["messages"][0]["message"], "Hello");
  EXPECT_EQ(data["messages"][1]["sender"], "agent");
  EXPECT_EQ(data["messages"][1]["message"], "Hi there");

  std::error_code ec;
  std::filesystem::remove(file_path, ec);
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
