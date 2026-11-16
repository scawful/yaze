#include "app/editor/overworld/live_proposal.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "util/indexed_png.h"

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

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
  EXPECT_TRUE(rejects([](json& p) { p["id"] = "."; }));
  EXPECT_TRUE(rejects([](json& p) { p["id"] = ".."; }));
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

TEST(LiveProposalTest, FeedbackKeepsHistoryAndRefusesForeignOrBrokenFiles) {
  LiveProposal proposal = *Parse(ValidPacket());
  proposal.packet_sha256 = std::string(64, 'a');
  LiveProposalFeedbackEntry comment{"commented", "move it one tile north"};
  auto first_text = BuildLiveProposalFeedback(proposal, comment, "");
  ASSERT_TRUE(first_text.ok()) << first_text.status();
  const auto first = json::parse(*first_text);
  EXPECT_EQ(first["schema"], "yaze.overworld_live_proposal_feedback.v1");
  EXPECT_EQ(first["status"], "pending");
  ASSERT_EQ(first["history"].size(), 1u);
  EXPECT_EQ(first["history"][0]["comment"], "move it one tile north");

  proposal.decision = LiveProposalDecision::kAccepted;
  LiveProposalFeedbackEntry accept{"accepted", "", 2, 0, {{1, 0, 1, 9}}};
  auto second_text = BuildLiveProposalFeedback(proposal, accept, first.dump());
  ASSERT_TRUE(second_text.ok()) << second_text.status();
  const auto second = json::parse(*second_text);
  EXPECT_EQ(second["status"], "accepted");
  ASSERT_EQ(second["history"].size(), 2u);
  EXPECT_EQ(second["history"][1]["cells_applied"], 2);
  EXPECT_EQ(second["history"][1]["conflicts"][0]["found"], 9);

  // History is never dropped: another id or a broken file is refused.
  json other = first;
  other["id"] = "someone-else";
  EXPECT_FALSE(BuildLiveProposalFeedback(proposal, accept, other.dump()).ok());
  json no_id = first;
  no_id.erase("id");
  EXPECT_FALSE(BuildLiveProposalFeedback(proposal, accept, no_id.dump()).ok());
  json no_history = first;
  no_history.erase("history");
  EXPECT_FALSE(
      BuildLiveProposalFeedback(proposal, accept, no_history.dump()).ok());
  EXPECT_FALSE(BuildLiveProposalFeedback(proposal, accept, "{broken").ok());
  EXPECT_FALSE(BuildLiveProposalFeedback(proposal, accept, "[1,2]").ok());
}

// A fresh empty folder under the system temp directory.
class TempDir {
 public:
  TempDir() {
    static int counter = 0;
    path_ = std::filesystem::temp_directory_path() /
            ("yaze_live_proposal_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()) +
             "_" + std::to_string(counter++));
    std::filesystem::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const { return path_; }
  std::vector<std::string> Names() const {
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(path_))
      names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    return names;
  }

 private:
  std::filesystem::path path_;
};

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

json ReadJson(const std::filesystem::path& path) {
  return json::parse(ReadText(path));
}

TEST(LiveProposalTest, WritesFeedbackBesideThePacketNamedById) {
  TempDir dir;
  LiveProposal proposal = *Parse(ValidPacket());
  EXPECT_FALSE(WriteLiveProposalFeedback(proposal, {"rejected"}).ok());
  proposal.packet_path = (dir.path() / "proposal.json").string();
  EXPECT_EQ(LiveProposalFeedbackPath(proposal.packet_path, proposal.id),
            (dir.path() / "feedback-woods-trail.a.json").string());
  ASSERT_TRUE(WriteLiveProposalFeedback(proposal, {"commented", "a"}).ok());
  ASSERT_TRUE(WriteLiveProposalFeedback(proposal, {"commented", "b"}).ok());
  const auto feedback = ReadJson(dir.path() / "feedback-woods-trail.a.json");
  ASSERT_EQ(feedback["history"].size(), 2u);
  EXPECT_EQ(feedback["history"][1]["comment"], "b");
  // The unique temp file is renamed onto the target; nothing is left over.
  EXPECT_EQ(dir.Names(),
            std::vector<std::string>{"feedback-woods-trail.a.json"});
}

TEST(LiveProposalTest, TwoPacketsInOneFolderKeepSeparateFeedback) {
  TempDir dir;
  LiveProposal a = *Parse(ValidPacket());
  LiveProposal b = a;
  b.id = "woods-trail.b";
  a.packet_path = b.packet_path = (dir.path() / "proposal.json").string();
  ASSERT_TRUE(WriteLiveProposalFeedback(a, {"commented", "for a"}).ok());
  ASSERT_TRUE(WriteLiveProposalFeedback(b, {"rejected", "for b"}).ok());
  ASSERT_TRUE(WriteLiveProposalFeedback(a, {"commented", "a again"}).ok());
  const auto fa = ReadJson(dir.path() / "feedback-woods-trail.a.json");
  const auto fb = ReadJson(dir.path() / "feedback-woods-trail.b.json");
  EXPECT_EQ(fa["id"], "woods-trail.a");
  EXPECT_EQ(fb["id"], "woods-trail.b");
  EXPECT_EQ(fa["history"].size(), 2u);
  ASSERT_EQ(fb["history"].size(), 1u);
  EXPECT_EQ(fb["history"][0]["comment"], "for b");
}

TEST(LiveProposalTest, WriteRefusesAndKeepsAForeignOrBrokenFile) {
  TempDir dir;
  LiveProposal proposal = *Parse(ValidPacket());
  proposal.packet_path = (dir.path() / "proposal.json").string();
  const auto target = dir.path() / "feedback-woods-trail.a.json";
  for (const std::string existing :
       {std::string("{broken"), std::string(""),
        json({{"schema", "yaze.overworld_live_proposal_feedback.v1"},
              {"id", "someone-else"},
              {"history", json::array()}})
            .dump()}) {
    SCOPED_TRACE(existing);
    std::ofstream(target, std::ios::binary) << existing;
    EXPECT_FALSE(WriteLiveProposalFeedback(proposal, {"commented", "x"}).ok());
    EXPECT_EQ(ReadText(target), existing);
    EXPECT_EQ(dir.Names(),
              std::vector<std::string>{"feedback-woods-trail.a.json"});
  }
}

TEST(LiveProposalTest, RejectsCellsThatChangeNothingAndOutOfRangeIntegers) {
  json same = ValidPacket();
  same["cells"][0]["proposed"] = same["cells"][0]["expected"];
  EXPECT_FALSE(Parse(same).ok());
  // 2^32 + 0x28 would wrap to 0x28 if narrowed without a range check.
  json wrapped = ValidPacket();
  wrapped["map_id"] = 4294967336ull;
  EXPECT_FALSE(Parse(wrapped).ok());
  json negative = ValidPacket();
  negative["cells"][0]["x"] = -4294967296ll;
  EXPECT_FALSE(Parse(negative).ok());
  json big = ValidPacket();
  big["cells"][0]["proposed"] = 18446744073709551615ull;
  EXPECT_FALSE(Parse(big).ok());
}

// A fake 32x32 map behind LiveProposalHooks, with the feedback history kept
// in memory. `apply` mirrors ApplyGuardedCells: it re-checks every cell and
// writes all or none.
struct FakeEditor {
  std::map<std::pair<int, int>, uint16_t> tiles;
  std::vector<LiveProposalFeedbackEntry> history;
  int apply_calls = 0;
  absl::Status apply_error = absl::OkStatus();
  absl::Status write_error = absl::OkStatus();

  uint16_t At(int x, int y) const {
    auto it = tiles.find({x, y});
    return it == tiles.end() ? 0 : it->second;
  }

  LiveProposalHooks Hooks() {
    LiveProposalHooks hooks;
    hooks.read = [this](int, int x, int y) -> absl::StatusOr<uint16_t> {
      return At(x, y);
    };
    hooks.apply = [this](int, const std::vector<GuardedTileCell>& cells)
        -> absl::StatusOr<int> {
      ++apply_calls;
      if (!apply_error.ok())
        return apply_error;
      for (const auto& cell : cells) {
        if (At(cell.x, cell.y) != cell.expected)
          return absl::AbortedError("cell changed");
      }
      for (const auto& cell : cells)
        tiles[{cell.x, cell.y}] = cell.proposed;
      return static_cast<int>(cells.size());
    };
    hooks.write_feedback = [this](const LiveProposal&,
                                  const LiveProposalFeedbackEntry& entry) {
      if (!write_error.ok())
        return write_error;
      history.push_back(entry);
      return absl::OkStatus();
    };
    hooks.rom_path = "/private/test.sfc";
    hooks.at_utc = "2026-11-16T15:00:00Z";
    return hooks;
  }

  void Restore(const std::vector<GuardedTileCell>& cells) {
    for (const auto& cell : cells)
      tiles[{cell.x, cell.y}] = cell.expected;
  }
  void Redo(const std::vector<GuardedTileCell>& cells) {
    for (const auto& cell : cells)
      tiles[{cell.x, cell.y}] = cell.proposed;
  }
};

LiveProposal ThreeCellProposal() {
  LiveProposal proposal = *Parse(ValidPacket());
  proposal.cells = {{0, 0, 1, 2}, {1, 0, 1, 2}, {2, 0, 1, 2}};
  return proposal;
}

TEST(LiveProposalTest, AcceptSkipsACellEditedAfterTheOffer) {
  FakeEditor editor;
  editor.tiles = {{{0, 0}, 1}, {{1, 0}, 1}, {{2, 0}, 1}};
  LiveProposal proposal = ThreeCellProposal();
  // The person paints (1,0) between the offer and Accept; (2,0) already
  // holds the proposed tile.
  editor.tiles[{1, 0}] = 7;
  editor.tiles[{2, 0}] = 2;
  auto applied = AcceptLiveProposalWith(proposal, editor.Hooks());
  ASSERT_TRUE(applied.ok()) << applied.status();
  EXPECT_EQ(*applied, 1);
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kAccepted);
  EXPECT_EQ(editor.At(0, 0), 2);
  EXPECT_EQ(editor.At(1, 0), 7);  // the person's edit stays
  EXPECT_EQ(editor.At(2, 0), 2);
  ASSERT_EQ(editor.history.size(), 1u);
  EXPECT_EQ(editor.history[0].decision, "accepted");
  EXPECT_EQ(editor.history[0].cells_applied, 1);
  EXPECT_EQ(editor.history[0].already_applied, 1);
  ASSERT_EQ(editor.history[0].conflicts.size(), 1u);
  EXPECT_EQ(editor.history[0].conflicts[0].found, 7);
  ASSERT_EQ(proposal.applied_cells.size(), 1u);
  EXPECT_EQ(proposal.applied_cells[0].x, 0);
}

TEST(LiveProposalTest, AcceptWritesNoCellWhenFeedbackFailsAndCanRetry) {
  FakeEditor editor;
  editor.tiles = {{{0, 0}, 1}, {{1, 0}, 1}, {{2, 0}, 1}};
  LiveProposal proposal = ThreeCellProposal();
  editor.write_error = absl::PermissionDeniedError("read-only folder");
  EXPECT_FALSE(AcceptLiveProposalWith(proposal, editor.Hooks()).ok());
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kPending);
  EXPECT_EQ(editor.apply_calls, 0);
  EXPECT_EQ(editor.At(0, 0), 1);
  EXPECT_NE(proposal.last_result.find("read-only folder"), std::string::npos);

  editor.write_error = absl::OkStatus();
  auto retry = AcceptLiveProposalWith(proposal, editor.Hooks());
  ASSERT_TRUE(retry.ok()) << retry.status();
  EXPECT_EQ(*retry, 3);
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kAccepted);
  ASSERT_EQ(editor.history.size(), 1u);
  EXPECT_EQ(editor.history[0].decision, "accepted");
}

TEST(LiveProposalTest, AcceptThatFailsToWriteCellsStaysPending) {
  FakeEditor editor;
  editor.tiles = {{{0, 0}, 1}, {{1, 0}, 1}, {{2, 0}, 1}};
  LiveProposal proposal = ThreeCellProposal();
  editor.apply_error = absl::FailedPreconditionError("Lyra preview");
  EXPECT_FALSE(AcceptLiveProposalWith(proposal, editor.Hooks()).ok());
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kPending);
  EXPECT_TRUE(proposal.applied_cells.empty());
  ASSERT_EQ(editor.history.size(), 2u);
  EXPECT_EQ(editor.history[0].decision, "accepted");
  EXPECT_EQ(editor.history[1].decision, "accept_failed");
  EXPECT_EQ(editor.history[1].comment, "Lyra preview");
}

TEST(LiveProposalTest, AcceptRefusesWhenEveryCellConflicts) {
  FakeEditor editor;
  editor.tiles = {{{0, 0}, 5}, {{1, 0}, 5}, {{2, 0}, 5}};
  LiveProposal proposal = ThreeCellProposal();
  EXPECT_FALSE(AcceptLiveProposalWith(proposal, editor.Hooks()).ok());
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kPending);
  EXPECT_TRUE(editor.history.empty());
  EXPECT_EQ(editor.apply_calls, 0);
}

TEST(LiveProposalTest, UndoReturnsToPendingAndRedoAcceptsAgain) {
  FakeEditor editor;
  editor.tiles = {{{0, 0}, 1}, {{1, 0}, 1}, {{2, 0}, 7}};
  LiveProposal proposal = ThreeCellProposal();
  ASSERT_TRUE(AcceptLiveProposalWith(proposal, editor.Hooks()).ok());
  const auto written = proposal.applied_cells;
  ASSERT_EQ(written.size(), 2u);

  // Nothing changed on the map: no sync.
  EXPECT_FALSE(SyncLiveProposalWith(proposal, editor.Hooks()));

  editor.Restore(written);  // Undo
  ASSERT_TRUE(SyncLiveProposalWith(proposal, editor.Hooks()));
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kPending);
  EXPECT_TRUE(proposal.undone);
  EXPECT_EQ(editor.history.back().decision, "undone");

  editor.Redo(written);  // Redo
  ASSERT_TRUE(SyncLiveProposalWith(proposal, editor.Hooks()));
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kAccepted);
  EXPECT_FALSE(proposal.undone);
  EXPECT_EQ(editor.history.back().decision, "redone");
  EXPECT_EQ(editor.history.back().cells_applied, 2);

  // Undo again, then Accept from the panel instead of Redo.
  editor.Restore(written);
  ASSERT_TRUE(SyncLiveProposalWith(proposal, editor.Hooks()));
  auto again = AcceptLiveProposalWith(proposal, editor.Hooks());
  ASSERT_TRUE(again.ok()) << again.status();
  EXPECT_EQ(*again, 2);
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kAccepted);
  EXPECT_FALSE(proposal.undone);
  std::vector<std::string> decisions;
  for (const auto& entry : editor.history)
    decisions.push_back(entry.decision);
  EXPECT_EQ(decisions, (std::vector<std::string>{"accepted", "undone", "redone",
                                                 "undone", "accepted"}));
}

TEST(LiveProposalTest, UndoSyncFollowsTheMapEvenWhenFeedbackFails) {
  FakeEditor editor;
  editor.tiles = {{{0, 0}, 1}, {{1, 0}, 1}, {{2, 0}, 1}};
  LiveProposal proposal = ThreeCellProposal();
  ASSERT_TRUE(AcceptLiveProposalWith(proposal, editor.Hooks()).ok());
  editor.Restore(proposal.applied_cells);
  editor.write_error = absl::PermissionDeniedError("disk full");
  ASSERT_TRUE(SyncLiveProposalWith(proposal, editor.Hooks()));
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kPending);
  EXPECT_NE(proposal.last_result.find("disk full"), std::string::npos);
}

TEST(LiveProposalTest, RejectAndCommentAddToTheHistory) {
  FakeEditor editor;
  LiveProposal proposal = ThreeCellProposal();
  ASSERT_TRUE(
      CommentLiveProposalWith(proposal, "one tile north", editor.Hooks()).ok());
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kPending);
  EXPECT_FALSE(CommentLiveProposalWith(proposal, "", editor.Hooks()).ok());

  editor.write_error = absl::PermissionDeniedError("read-only folder");
  EXPECT_FALSE(
      RejectLiveProposalWith(proposal, "too wide", editor.Hooks()).ok());
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kPending);
  EXPECT_FALSE(CommentLiveProposalWith(proposal, "lost", editor.Hooks()).ok());
  EXPECT_NE(proposal.last_result.find("NOT written"), std::string::npos);

  editor.write_error = absl::OkStatus();
  ASSERT_TRUE(
      RejectLiveProposalWith(proposal, "too wide", editor.Hooks()).ok());
  EXPECT_EQ(proposal.decision, LiveProposalDecision::kRejected);
  EXPECT_FALSE(AcceptLiveProposalWith(proposal, editor.Hooks()).ok());
  ASSERT_EQ(editor.history.size(), 2u);
  EXPECT_EQ(editor.history[0].decision, "commented");
  EXPECT_EQ(editor.history[0].comment, "one tile north");
  EXPECT_EQ(editor.history[1].decision, "rejected");
  EXPECT_EQ(editor.history[1].comment, "too wide");
  EXPECT_EQ(editor.history[1].rom_path, "/private/test.sfc");
}

TEST(LiveProposalTest, DecisionsReachTheFeedbackFileOnDisk) {
  TempDir dir;
  FakeEditor editor;
  editor.tiles = {{{0, 0}, 1}, {{1, 0}, 1}, {{2, 0}, 1}};
  LiveProposal proposal = ThreeCellProposal();
  proposal.packet_path = (dir.path() / "proposal.json").string();
  auto hooks = editor.Hooks();
  hooks.write_feedback = WriteLiveProposalFeedback;
  ASSERT_TRUE(CommentLiveProposalWith(proposal, "ok", hooks).ok());
  ASSERT_TRUE(AcceptLiveProposalWith(proposal, hooks).ok());
  editor.Restore(proposal.applied_cells);
  ASSERT_TRUE(SyncLiveProposalWith(proposal, hooks));
  const auto feedback = ReadJson(dir.path() / "feedback-woods-trail.a.json");
  EXPECT_EQ(feedback["status"], "pending");
  ASSERT_EQ(feedback["history"].size(), 3u);
  EXPECT_EQ(feedback["history"][0]["decision"], "commented");
  EXPECT_EQ(feedback["history"][1]["decision"], "accepted");
  EXPECT_EQ(feedback["history"][1]["cells_applied"], 3);
  EXPECT_EQ(feedback["history"][2]["decision"], "undone");
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

TEST(LiveProposalTest, InboxReofferGetsANewSerialAndClearEmptiesIt) {
  LiveProposalInbox inbox;
  LiveProposal a = *Parse(ValidPacket());
  inbox.Offer(a);
  const int first = inbox.Find("woods-trail.a")->offer_serial;
  inbox.Offer(a);
  EXPECT_NE(inbox.Find("woods-trail.a")->offer_serial, first);
  inbox.Clear();
  EXPECT_TRUE(inbox.proposals().empty());
  EXPECT_EQ(inbox.Selected(), nullptr);
  EXPECT_EQ(inbox.selected_index(), -1);
}

const std::string kZeroSha(64, '0');

TEST(LiveProposalTest, ParsesOptionalAttachments) {
  json packet = ValidPacket();
  auto plain = Parse(packet);
  ASSERT_TRUE(plain.ok()) << plain.status();
  EXPECT_TRUE(plain->attachments.empty());
  packet["attachments"] = {{{"path", "attachments/before.png"},
                            {"sha256", kZeroSha},
                            {"caption", "Before"}},
                           {{"path", "after.PNG"}, {"sha256", kZeroSha}}};
  auto proposal = Parse(packet);
  ASSERT_TRUE(proposal.ok()) << proposal.status();
  ASSERT_EQ(proposal->attachments.size(), 2u);
  EXPECT_EQ(proposal->attachments[0].path, "attachments/before.png");
  EXPECT_EQ(proposal->attachments[0].caption, "Before");
  EXPECT_EQ(proposal->attachments[1].caption, "");
  EXPECT_EQ(proposal->attachments[1].rgba, nullptr);  // not loaded yet
}

TEST(LiveProposalTest, RejectsAttachmentsOutsideThePacketFolderOrMalformed) {
  const auto rejects = [](const std::function<void(json&)>& edit) {
    json packet = ValidPacket();
    packet["attachments"] = {{{"path", "a.png"}, {"sha256", kZeroSha}}};
    edit(packet["attachments"]);
    return !Parse(packet).ok();
  };
  EXPECT_FALSE(rejects([](json&) {}));
  EXPECT_TRUE(rejects([](json& a) { a[0]["path"] = "/tmp/a.png"; }));
  EXPECT_TRUE(rejects([](json& a) { a[0]["path"] = "../a.png"; }));
  EXPECT_TRUE(rejects([](json& a) { a[0]["path"] = "x/../../a.png"; }));
  EXPECT_TRUE(rejects([](json& a) { a[0]["path"] = "x\\a.png"; }));
  EXPECT_TRUE(rejects([](json& a) { a[0]["path"] = "a.jpg"; }));
  EXPECT_TRUE(rejects([](json& a) { a[0]["path"] = ""; }));
  EXPECT_TRUE(rejects([](json& a) { a[0]["path"] = 7; }));
  EXPECT_TRUE(rejects([](json& a) { a[0]["sha256"] = std::string(64, 'A'); }));
  EXPECT_TRUE(rejects([](json& a) { a[0]["sha256"] = std::string(63, '0'); }));
  EXPECT_TRUE(rejects([](json& a) { a[0].erase("sha256"); }));
  EXPECT_TRUE(
      rejects([](json& a) { a[0]["caption"] = std::string(201, 'c'); }));
  EXPECT_TRUE(rejects([](json& a) { a[0] = "a.png"; }));
  EXPECT_TRUE(rejects([](json& a) { a = "a.png"; }));
  EXPECT_TRUE(rejects([](json& a) {
    for (int i = 0; i < kLiveProposalMaxAttachments; ++i)
      a.push_back(a[0]);
  }));
}

// Writes `bytes` as `name` under `dir` and returns its sha256.
std::string WriteBytes(const std::filesystem::path& dir,
                       const std::string& name,
                       const std::vector<uint8_t>& bytes) {
  std::filesystem::create_directories((dir / name).parent_path());
  EXPECT_TRUE(util::WriteBinaryFile((dir / name).string(), bytes).ok());
  return core::ComputeSourceArtifactSha256(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

LiveProposal ProposalWithImage(const std::filesystem::path& dir,
                               const std::string& name,
                               const std::string& sha256) {
  LiveProposal proposal = *Parse(ValidPacket());
  proposal.packet_path = (dir / "proposal.json").string();
  proposal.attachments.push_back({name, sha256, "caption"});
  return proposal;
}

TEST(LiveProposalTest, LoadsAttachmentPixelsAfterCheckingTheHash) {
  TempDir dir;
  // 2x1 indexed PNG: a red pixel, then a half-transparent blue one.
  auto png = util::EncodeIndexedPng(2, 1, {0, 1},
                                    {{255, 0, 0, 255}, {0, 0, 255, 128}});
  ASSERT_TRUE(png.ok()) << png.status();
  const std::string sha = WriteBytes(dir.path(), "attachments/mock.png", *png);
  LiveProposal proposal =
      ProposalWithImage(dir.path(), "attachments/mock.png", sha);
  ASSERT_TRUE(LoadLiveProposalAttachments(proposal).ok());
  const auto& image = proposal.attachments[0];
  EXPECT_EQ(image.width, 2);
  EXPECT_EQ(image.height, 1);
  ASSERT_NE(image.rgba, nullptr);
  EXPECT_EQ(*image.rgba,
            (std::vector<uint8_t>{255, 0, 0, 255, 0, 0, 255, 128}));
  // Copies share the decoded pixels.
  LiveProposal copy = proposal;
  EXPECT_EQ(copy.attachments[0].rgba.get(), image.rgba.get());
}

TEST(LiveProposalTest, RefusesMissingChangedOrUnreadableAttachments) {
  TempDir dir;
  auto png = util::EncodeIndexedPng(1, 1, {0}, {{1, 2, 3, 255}});
  ASSERT_TRUE(png.ok());
  const std::string sha = WriteBytes(dir.path(), "a.png", *png);

  LiveProposal missing = ProposalWithImage(dir.path(), "b.png", sha);
  EXPECT_TRUE(absl::IsNotFound(LoadLiveProposalAttachments(missing)));

  LiveProposal changed = ProposalWithImage(dir.path(), "a.png", kZeroSha);
  EXPECT_TRUE(absl::IsFailedPrecondition(LoadLiveProposalAttachments(changed)));

  const std::string text_sha =
      WriteBytes(dir.path(), "text.png", {'n', 'o', 't', ' ', 'p', 'n', 'g'});
  LiveProposal text = ProposalWithImage(dir.path(), "text.png", text_sha);
  EXPECT_TRUE(absl::IsInvalidArgument(LoadLiveProposalAttachments(text)));

  const int wide = kLiveProposalMaxAttachmentSide + 1;
  auto big = util::EncodeIndexedPng(wide, 1, std::vector<uint8_t>(wide, 0),
                                    {{0, 0, 0, 255}});
  ASSERT_TRUE(big.ok());
  const std::string big_sha = WriteBytes(dir.path(), "big.png", *big);
  LiveProposal oversize = ProposalWithImage(dir.path(), "big.png", big_sha);
  EXPECT_TRUE(absl::IsInvalidArgument(LoadLiveProposalAttachments(oversize)));
}

TEST(LiveProposalTest, RefusesAttachmentsThatResolveOutsideThePacketFolder) {
  TempDir dir;
  TempDir outside;
  auto png = util::EncodeIndexedPng(1, 1, {0}, {{1, 2, 3, 255}});
  ASSERT_TRUE(png.ok());
  const std::string sha = WriteBytes(outside.path(), "secret.png", *png);
  std::error_code error;

  // A symlinked file that points outside, with the right hash.
  std::filesystem::create_symlink(outside.path() / "secret.png",
                                  dir.path() / "link.png", error);
  if (error)
    GTEST_SKIP() << "symlinks unavailable: " << error.message();
  LiveProposal file_link = ProposalWithImage(dir.path(), "link.png", sha);
  const auto file_status = LoadLiveProposalAttachments(file_link);
  EXPECT_TRUE(absl::IsInvalidArgument(file_status)) << file_status;
  EXPECT_EQ(file_link.attachments[0].rgba, nullptr);

  // A symlinked folder above the file.
  std::filesystem::create_directory_symlink(outside.path(),
                                            dir.path() / "attachments", error);
  ASSERT_FALSE(error) << error.message();
  LiveProposal dir_link =
      ProposalWithImage(dir.path(), "attachments/secret.png", sha);
  EXPECT_TRUE(absl::IsInvalidArgument(LoadLiveProposalAttachments(dir_link)));

  // A link that stays inside the folder is fine.
  const std::string inside_sha = WriteBytes(dir.path(), "real.png", *png);
  std::filesystem::create_symlink(dir.path() / "real.png",
                                  dir.path() / "alias.png", error);
  ASSERT_FALSE(error) << error.message();
  LiveProposal alias = ProposalWithImage(dir.path(), "alias.png", inside_sha);
  EXPECT_TRUE(LoadLiveProposalAttachments(alias).ok());
}

TEST(LiveProposalTest, RefusesNonRegularAttachmentsAndRelativePackets) {
  TempDir dir;
  std::filesystem::create_directories(dir.path() / "folder.png");
  LiveProposal folder = ProposalWithImage(dir.path(), "folder.png", kZeroSha);
  EXPECT_TRUE(absl::IsInvalidArgument(LoadLiveProposalAttachments(folder)));
#if !defined(_WIN32)
  ASSERT_EQ(::mkfifo((dir.path() / "pipe.png").c_str(), 0600), 0);
  LiveProposal pipe = ProposalWithImage(dir.path(), "pipe.png", kZeroSha);
  EXPECT_TRUE(absl::IsInvalidArgument(LoadLiveProposalAttachments(pipe)));
#endif
  LiveProposal relative = ProposalWithImage(dir.path(), "a.png", kZeroSha);
  relative.packet_path = "proposal.json";
  EXPECT_TRUE(
      absl::IsFailedPrecondition(LoadLiveProposalAttachments(relative)));
}

TEST(LiveProposalTest, RefusesHugePngHeaderBeforeDecoding) {
  TempDir dir;
  // Signature + an IHDR claiming 60000x60000 and nothing else. DecodePng
  // would call this "Incomplete PNG"; the header check must answer first.
  std::vector<uint8_t> bytes = {0x89, 'P',  'N', 'G', '\r', '\n', 0x1A, '\n', 0,
                                0,    0,    13,  'I', 'H',  'D',  'R',  0,    0,
                                0xEA, 0x60, 0,   0,   0xEA, 0x60, 8,    6,    0,
                                0,    0,    0,   0,   0,    0};
  const std::string sha = WriteBytes(dir.path(), "huge.png", bytes);
  LiveProposal huge = ProposalWithImage(dir.path(), "huge.png", sha);
  const auto status = LoadLiveProposalAttachments(huge);
  EXPECT_TRUE(absl::IsInvalidArgument(status));
  EXPECT_NE(std::string(status.message()).find("60000x60000"),
            std::string::npos)
      << status;
}

TEST(LiveProposalTest, CountLabelUsesSingularOnlyForOne) {
  EXPECT_EQ(CountLabel(0, "conflict", "conflicts"), "0 conflicts");
  EXPECT_EQ(CountLabel(1, "conflict", "conflicts"), "1 conflict");
  EXPECT_EQ(CountLabel(2, "cell", "cells"), "2 cells");
}

}  // namespace
}  // namespace yaze::editor
