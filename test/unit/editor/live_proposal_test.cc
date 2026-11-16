#include "app/editor/overworld/live_proposal.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {

using nlohmann::json;

json ValidPacket() {
  return {{"schema", "yaze.overworld_live_proposal.v1"},
          {"id", "woods-trail.a"},
          {"author", "Claude"},
          {"title", "Woods trail A"},
          {"why", "Connect the woods to the docks."},
          {"map_id", 0x28},
          {"cells",
           {{{"x", 0}, {"y", 0}, {"expected", 1}, {"proposed", 2}},
            {{"x", 31}, {"y", 31}, {"expected", 0x10}, {"proposed", 0xFFF}}}}};
}

absl::StatusOr<LiveProposal> Parse(const json& packet) {
  return ParseLiveProposal(packet.dump(), 0xA0, 0x1000);
}

TEST(LiveProposalTest, ParsesAValidPacket) {
  auto proposal = Parse(ValidPacket());
  ASSERT_TRUE(proposal.ok()) << proposal.status();
  EXPECT_EQ(proposal->id, "woods-trail.a");
  EXPECT_EQ(proposal->title, "Woods trail A");
  EXPECT_EQ(proposal->author, "Claude");
  EXPECT_EQ(proposal->map_id, 0x28);
  ASSERT_EQ(proposal->cells.size(), 2u);
  EXPECT_EQ(proposal->cells[1].x, 31);
  EXPECT_EQ(proposal->cells[1].proposed, 0xFFF);
  EXPECT_EQ(proposal->decision, LiveProposalDecision::kPending);
}

TEST(LiveProposalTest, RejectsInvalidPackets) {
  const auto rejects = [](const std::function<void(json&)>& edit) {
    json packet = ValidPacket();
    edit(packet);
    return !Parse(packet).ok();
  };
  EXPECT_TRUE(rejects([](json& p) { p["schema"] = "other"; }));
  EXPECT_TRUE(rejects([](json& p) { p["id"] = "../escape"; }));
  EXPECT_TRUE(rejects([](json& p) { p["id"] = ""; }));
  EXPECT_TRUE(rejects([](json& p) { p["id"] = std::string(65, 'a'); }));
  EXPECT_TRUE(rejects([](json& p) { p.erase("title"); }));
  EXPECT_TRUE(rejects([](json& p) { p["map_id"] = 0xA0; }));
  EXPECT_TRUE(rejects([](json& p) { p["map_id"] = "0x28"; }));
  EXPECT_TRUE(rejects([](json& p) { p["cells"] = json::array(); }));
  EXPECT_TRUE(rejects([](json& p) { p["cells"][0]["x"] = 32; }));
  EXPECT_TRUE(rejects([](json& p) { p["cells"][0]["proposed"] = 0x1000; }));
  EXPECT_TRUE(rejects([](json& p) { p["cells"][0]["expected"] = -1; }));
  EXPECT_TRUE(rejects([](json& p) { p["cells"][1] = p["cells"][0]; }));
  EXPECT_TRUE(rejects([](json& p) { p["cells"][0].erase("expected"); }));
  EXPECT_TRUE(rejects([](json& p) {
    p["cells"] = json::array();
    for (int i = 0; i <= kLiveProposalMaxCells; ++i)
      p["cells"].push_back({{"x", i % 32},
                            {"y", i / 32 % 32},
                            {"expected", 0},
                            {"proposed", 1}});
  }));
  EXPECT_FALSE(ParseLiveProposal("{not json", 0xA0, 0x1000).ok());
}

TEST(LiveProposalTest, ClassifiesReadyConflictAndAlreadyApplied) {
  LiveProposal proposal;
  proposal.cells = {{0, 0, 1, 2}, {1, 0, 1, 2}, {2, 0, 1, 2}};
  const auto result =
      ClassifyLiveProposal(proposal, [](int x, int) -> uint16_t {
        return x == 0 ? 1 : x == 1 ? 2 : 7;
      });
  EXPECT_EQ(result.ready, 1);
  EXPECT_EQ(result.already_applied, 1);
  EXPECT_EQ(result.conflicts, 1);
  EXPECT_EQ(result.states[0], LiveProposalCellState::kReady);
  EXPECT_EQ(result.states[1], LiveProposalCellState::kAlreadyApplied);
  EXPECT_EQ(result.states[2], LiveProposalCellState::kConflict);
  ASSERT_EQ(result.ready_cells.size(), 1u);
  EXPECT_EQ(result.ready_cells[0].x, 0);
  ASSERT_EQ(result.conflict_cells.size(), 1u);
  EXPECT_EQ(result.conflict_cells[0].found, 7);
}

TEST(LiveProposalTest, FeedbackKeepsHistoryForTheSameIdOnly) {
  LiveProposal proposal = *Parse(ValidPacket());
  proposal.packet_sha256 = std::string(64, 'a');
  LiveProposalFeedbackEntry comment{"commented", "move it one tile north"};
  const auto first =
      json::parse(BuildLiveProposalFeedback(proposal, comment, ""));
  EXPECT_EQ(first["schema"], "yaze.overworld_live_proposal_feedback.v1");
  EXPECT_EQ(first["status"], "pending");
  ASSERT_EQ(first["history"].size(), 1u);
  EXPECT_EQ(first["history"][0]["comment"], "move it one tile north");

  proposal.decision = LiveProposalDecision::kAccepted;
  LiveProposalFeedbackEntry accept{"accepted", "", 2, 0, {{1, 0, 1, 9}}};
  const auto second =
      json::parse(BuildLiveProposalFeedback(proposal, accept, first.dump()));
  EXPECT_EQ(second["status"], "accepted");
  ASSERT_EQ(second["history"].size(), 2u);
  EXPECT_EQ(second["history"][1]["cells_applied"], 2);
  EXPECT_EQ(second["history"][1]["conflicts"][0]["found"], 9);

  json other = first;
  other["id"] = "someone-else";
  const auto fresh =
      json::parse(BuildLiveProposalFeedback(proposal, accept, other.dump()));
  EXPECT_EQ(fresh["history"].size(), 1u);
  const auto garbage =
      json::parse(BuildLiveProposalFeedback(proposal, accept, "{broken"));
  EXPECT_EQ(garbage["history"].size(), 1u);
}

TEST(LiveProposalTest, WritesFeedbackBesideThePacket) {
  const auto dir =
      std::filesystem::temp_directory_path() /
      ("yaze_live_proposal_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(dir);
  LiveProposal proposal = *Parse(ValidPacket());
  EXPECT_FALSE(WriteLiveProposalFeedback(proposal, {"rejected"}).ok());
  proposal.packet_path = (dir / "proposal.json").string();
  EXPECT_EQ(LiveProposalFeedbackPath(proposal.packet_path),
            (dir / "feedback.json").string());
  ASSERT_TRUE(WriteLiveProposalFeedback(proposal, {"commented", "a"}).ok());
  ASSERT_TRUE(WriteLiveProposalFeedback(proposal, {"commented", "b"}).ok());
  std::ifstream in(dir / "feedback.json");
  const auto feedback = json::parse(std::string(
      std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()));
  ASSERT_EQ(feedback["history"].size(), 2u);
  EXPECT_EQ(feedback["history"][1]["comment"], "b");
  EXPECT_FALSE(std::filesystem::exists(dir / "feedback.json.tmp"));
  std::filesystem::remove_all(dir);
}

TEST(LiveProposalTest, InboxReofferReplacesAndResetsToPending) {
  LiveProposalInbox inbox;
  LiveProposal a = *Parse(ValidPacket());
  LiveProposal b = a;
  b.id = "other";
  EXPECT_EQ(inbox.Offer(a), 0);
  EXPECT_EQ(inbox.Offer(b), 1);
  inbox.Find("woods-trail.a")->decision = LiveProposalDecision::kRejected;
  EXPECT_EQ(inbox.PendingCount(), 1);
  a.title = "Woods trail A v2";
  EXPECT_EQ(inbox.Offer(a), 0);
  EXPECT_EQ(inbox.proposals().size(), 2u);
  EXPECT_EQ(inbox.Selected()->title, "Woods trail A v2");
  EXPECT_EQ(inbox.PendingCount(), 2);
  EXPECT_EQ(inbox.Find("missing"), nullptr);
}

TEST(LiveProposalTest, CountLabelUsesSingularOnlyForOne) {
  EXPECT_EQ(CountLabel(0, "conflict", "conflicts"), "0 conflicts");
  EXPECT_EQ(CountLabel(1, "conflict", "conflicts"), "1 conflict");
  EXPECT_EQ(CountLabel(2, "cell", "cells"), "2 cells");
}

}  // namespace
}  // namespace yaze::editor
