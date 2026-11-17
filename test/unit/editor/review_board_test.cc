#include "app/editor/agent/review_board.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "absl/time/time.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "util/indexed_png.h"

namespace yaze::editor {
namespace {

using nlohmann::json;
namespace fs = std::filesystem;

const std::string kSha(64, 'a');

json ValidBoard(const std::string& id = "q195-001") {
  return {{"schema", "yaze.review_board.v1"},
          {"id", id},
          {"title", "Docks boat, pick A"},
          {"why", "Boat glyphs from sheets E4/E5/E6."},
          {"author", "codex"},
          {"group", "docks-boat"},
          {"status", "current"},
          {"order", 3},
          {"subject", {{"kind", "map"}, {"id", "$4B"}, {"label", "Docks"}}},
          {"supersedes", {"q195-000"}},
          {"details", {{{"label", "Build"}, {"text", "b58"}}}},
          {"media",
           {{{"path", "media/1.png"},
             {"sha256", kSha},
             {"kind", "image"},
             {"caption", "Before"}}}},
          {"comments",
           {{{"id", "c1"},
             {"text", "Move the boat up"},
             {"source", "dashboard"},
             {"pin", {{"x", 0.25}, {"y", 0.5}}}}}}};
}

json ValidBoardV2(const std::string& id = "q237-sky-8c") {
  return {
      {"schema", "yaze.review_board.v2"},
      {"id", id},
      {"title", "Sky Island $8C cliff style"},
      {"ask", "Which cliff style and bridge offset for $8C?"},
      {"priority", "high"},
      {"why", "Compare b58 brown rock against vanilla mountain trim."},
      {"author", "Gemini-4-Carbon"},
      {"group", "sky-islands"},
      {"created_at", "2026-11-17T10:00:00Z"},
      {"status", "current"},
      {"order", 55},
      {"subject", {{"kind", "map"}, {"id", "$8C"}, {"label", "Sky Island SW"}}},
      {"media",
       {{{"path", "media/a.png"},
         {"sha256", kSha},
         {"kind", "image"},
         {"caption", "Option A"}},
        {{"path", "media/b.png"},
         {"sha256", kSha},
         {"kind", "image"},
         {"caption", "Option B"}}}},
      {"questions",
       {{{"id", "cliff_style"},
         {"prompt", "Which cliff style for $8C?"},
         {"kind", "single"},
         {"required", true},
         {"options",
          {{{"id", "a"},
            {"label", "Brown b58 rock"},
            {"detail", "one style"},
            {"media", 0},
            {"recommended", true}},
           {{"id", "b"},
            {"label", "Vanilla trim"},
            {"detail", "zero extra tile8"},
            {"media", 1},
            {"recommended", false}}}},
         {"allow_other", true}},
        {{"id", "pick"},
         {"prompt", "Which detail passes stay?"},
         {"kind", "multi"},
         {"required", true},
         {"options",
          {{{"id", "a"}, {"label", "Cloud fringe"}},
           {{"id", "c"}, {"label", "Rope posts"}}}},
         {"allow_other", true},
         {"default", json::array({"a"})}},
        {{"id", "notes"},
         {"prompt", "Bridge notes"},
         {"kind", "text"},
         {"required", false},
         {"placeholder", "Optional bridge offset"}},
        {{"id", "bridge_shift"},
         {"prompt", "East bridge shift"},
         {"kind", "number"},
         {"required", true},
         {"min", -4},
         {"max", 4},
         {"default", nullptr}},
        {{"id", "ship_now"},
         {"prompt", "Port onto $8C immediately?"},
         {"kind", "yes_no"},
         {"required", true},
         {"default", nullptr}}}}};
}

class TempDir {
 public:
  TempDir() {
    static int counter = 0;
    path_ = fs::temp_directory_path() /
            ("yaze_review_board_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()) +
             "_" + std::to_string(counter++));
    fs::create_directories(path_);
    path_ = fs::canonical(path_);
  }
  ~TempDir() {
    std::error_code ignored;
    fs::remove_all(path_, ignored);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

void WriteText(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << text;
}

void WriteBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

std::string ReadText(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

std::string Sha(const std::vector<uint8_t>& bytes) {
  return core::ComputeSourceArtifactSha256(std::string_view(
      reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

TEST(ReviewBoardTest, ParsesAValidBoard) {
  auto board = ParseReviewBoard(ValidBoard().dump(), "/boards/q195-001");
  ASSERT_TRUE(board.ok()) << board.status();
  EXPECT_EQ(board->schema, "yaze.review_board.v1");
  EXPECT_EQ(board->id, "q195-001");
  EXPECT_EQ(board->priority, "normal");
  EXPECT_TRUE(board->ask.empty());
  EXPECT_TRUE(board->questions.empty());
  EXPECT_EQ(board->subject_kind, "map");
  EXPECT_EQ(board->subject_id, "$4B");
  EXPECT_EQ(board->order, 3);
  ASSERT_EQ(board->supersedes.size(), 1u);
  ASSERT_EQ(board->details.size(), 1u);
  EXPECT_EQ(board->details[0].second, "b58");
  ASSERT_EQ(board->media.size(), 1u);
  EXPECT_EQ(board->media[0].caption, "Before");
  ASSERT_EQ(board->comments.size(), 1u);
  EXPECT_TRUE(board->comments[0].has_pin);
  EXPECT_FLOAT_EQ(board->comments[0].pin_x, 0.25f);
  EXPECT_EQ(board->folder, "/boards/q195-001");
  EXPECT_EQ(board->board_sha256.size(), 64u);
}

TEST(ReviewBoardTest, ParsesValidV2BoardAndQuestions) {
  auto board = ParseReviewBoard(ValidBoardV2().dump(), "/boards/q237-sky-8c");
  ASSERT_TRUE(board.ok()) << board.status();
  EXPECT_EQ(board->schema, "yaze.review_board.v2");
  EXPECT_EQ(board->ask, "Which cliff style and bridge offset for $8C?");
  EXPECT_EQ(board->priority, "high");
  EXPECT_TRUE(board->question_warnings.empty());
  ASSERT_EQ(board->questions.size(), 5u);

  const auto& q0 = board->questions[0];
  EXPECT_EQ(q0.id, "cliff_style");
  EXPECT_EQ(q0.kind, "single");
  EXPECT_TRUE(q0.required);
  EXPECT_TRUE(q0.allow_other);
  ASSERT_EQ(q0.options.size(), 2u);
  EXPECT_TRUE(q0.options[0].recommended);
  ASSERT_TRUE(q0.options[0].media.has_value());
  EXPECT_EQ(*q0.options[0].media, 0);

  const auto& q1 = board->questions[1];
  EXPECT_EQ(q1.kind, "multi");
  ASSERT_TRUE(
      std::holds_alternative<std::vector<std::string>>(q1.default_value));
  EXPECT_EQ(std::get<std::vector<std::string>>(q1.default_value),
            std::vector<std::string>({"a"}));

  const auto& q3 = board->questions[3];
  EXPECT_EQ(q3.kind, "number");
  ASSERT_TRUE(q3.min.has_value());
  ASSERT_TRUE(q3.max.has_value());
  EXPECT_DOUBLE_EQ(*q3.min, -4.0);
  EXPECT_DOUBLE_EQ(*q3.max, 4.0);
}

TEST(ReviewBoardTest,
     DropsMalformedQuestionsWithWarningsInsteadOfFailingBoard) {
  json packet = ValidBoardV2();
  // Inject malformed questions alongside the 5 valid ones.
  packet["questions"].push_back(
      {{"id", "Bad-Upper"}, {"prompt", "Bad id"}, {"kind", "text"}});
  packet["questions"].push_back(
      {{"id", "cliff_style"}, {"prompt", "Duplicate"}, {"kind", "text"}});
  packet["questions"].push_back(
      {{"id", "bad_kind"}, {"prompt", "Unknown kind"}, {"kind", "slider"}});
  packet["questions"].push_back(
      {{"id", "bad_media"},
       {"prompt", "Out of bounds thumb"},
       {"kind", "single"},
       {"options", {{{"id", "x"}, {"label", "X"}, {"media", 99}}}}});
  packet["questions"].push_back({{"id", "too_many_opts"},
                                 {"prompt", "13 options"},
                                 {"kind", "single"},
                                 {"options", json::array()}});
  for (int i = 0; i < 13; ++i) {
    packet["questions"].back()["options"].push_back(
        {{"id", "o" + std::to_string(i)}, {"label", "Opt"}});
  }

  auto board = ParseReviewBoard(packet.dump(), "/boards/q237-sky-8c");
  ASSERT_TRUE(board.ok()) << board.status();
  EXPECT_EQ(board->questions.size(), 5u);
  EXPECT_EQ(board->question_warnings.size(), 5u);

  // Exceeding 12 valid questions keeps the first 12 and emits a warning.
  json many = ValidBoardV2();
  many["questions"] = json::array();
  for (int i = 0; i < 14; ++i) {
    many["questions"].push_back({{"id", "q_" + std::to_string(i)},
                                 {"prompt", "Question " + std::to_string(i)},
                                 {"kind", "yes_no"}});
  }
  auto capped = ParseReviewBoard(many.dump(), "/boards/q237-sky-8c");
  ASSERT_TRUE(capped.ok()) << capped.status();
  EXPECT_EQ(capped->questions.size(), 12u);
  ASSERT_EQ(capped->question_warnings.size(), 1u);
}

TEST(ReviewBoardTest, RejectsInvalidBoards) {
  const auto rejects = [](const std::function<void(json&)>& edit) {
    json board = ValidBoard();
    edit(board);
    return !ParseReviewBoard(board.dump(), "/b").ok();
  };
  EXPECT_TRUE(rejects([](json& b) { b["schema"] = "other"; }));
  EXPECT_TRUE(rejects([](json& b) { b["priority"] = "urgent"; }));
  EXPECT_TRUE(rejects([](json& b) { b["id"] = "../up"; }));
  EXPECT_TRUE(rejects([](json& b) { b["id"] = ".."; }));
  EXPECT_TRUE(rejects([](json& b) { b["title"] = ""; }));
  EXPECT_TRUE(rejects([](json& b) { b["media"] = json::array(); }));
  EXPECT_TRUE(rejects([](json& b) { b["media"][0]["path"] = "../x.png"; }));
  EXPECT_TRUE(rejects([](json& b) { b["media"][0]["path"] = "/abs.png"; }));
  EXPECT_TRUE(rejects([](json& b) { b["media"][0]["path"] = "a\\b.png"; }));
  EXPECT_TRUE(rejects([](json& b) { b["media"][0]["sha256"] = "ABC"; }));
  EXPECT_TRUE(rejects([](json& b) { b["media"][0]["kind"] = "audio"; }));
  EXPECT_TRUE(rejects([](json& b) { b["media"][0]["path"] = "a.jpg"; }));
  EXPECT_TRUE(rejects([](json& b) { b["title"] = 5; }));
  EXPECT_TRUE(rejects([](json& b) {
    for (int i = 0; i < 9; ++i)
      b["media"].push_back(b["media"][0]);
  }));
  EXPECT_FALSE(rejects([](json& b) {
    b["media"][0] = {
        {"path", "media/clip.mp4"}, {"sha256", kSha}, {"kind", "video"}};
  }));
  // A pin outside 0..1 is kept as a comment without a pin.
  json board = ValidBoard();
  board["comments"][0]["pin"]["x"] = 1.5;
  auto parsed = ParseReviewBoard(board.dump(), "/b");
  ASSERT_TRUE(parsed.ok());
  EXPECT_FALSE(parsed->comments[0].has_pin);
}

TEST(ReviewBoardTest, ScanSortsCurrentFirstAndListsBadBoards) {
  TempDir dir;
  json stale = ValidBoard("b-stale");
  stale["status"] = "stale_landed";
  stale["order"] = 0;
  json late = ValidBoard("b-late");
  late["order"] = 9;
  json early = ValidBoard("b-early");
  early["order"] = 1;
  json old = ValidBoard("b-old");
  old["status"] = "superseded";
  old["order"] = 0;
  WriteText(dir.path() / "b-stale" / "board.json", stale.dump());
  WriteText(dir.path() / "b-late" / "board.json", late.dump());
  WriteText(dir.path() / "b-early" / "board.json", early.dump());
  WriteText(dir.path() / "b-old" / "board.json", old.dump());
  WriteText(dir.path() / "b-broken" / "board.json", "{not json");
  WriteText(dir.path() / "b-renamed" / "board.json",
            ValidBoard("other-id").dump());
  fs::create_directories(dir.path() / "empty-folder");

  const auto scan = ScanReviewBoards(dir.path());
  ASSERT_EQ(scan.boards.size(), 4u);
  EXPECT_EQ(scan.boards[0].id, "b-early");
  EXPECT_EQ(scan.boards[1].id, "b-late");
  EXPECT_EQ(scan.boards[2].id, "b-stale");
  EXPECT_EQ(scan.boards[3].id, "b-old");
  EXPECT_EQ(scan.errors.size(), 2u);
  EXPECT_TRUE(ScanReviewBoards(dir.path() / "missing").boards.empty());
}

TEST(ReviewBoardTest, FeedbackKeepsHistoryAndRefusesForeignFiles) {
  auto board = ParseReviewBoard(ValidBoard().dump(), "/b");
  ASSERT_TRUE(board.ok());
  ReviewBoardFeedbackEntry good{"looks_good", "general", "", "t1"};
  auto first = BuildReviewBoardFeedback(*board, good, "");
  ASSERT_TRUE(first.ok());
  const json one = json::parse(*first);
  EXPECT_EQ(one["schema"], "yaze.review_board_feedback.v2");
  EXPECT_EQ(one["status"], "looks_good");
  ASSERT_EQ(one["history"].size(), 1u);
  EXPECT_EQ(one["history"][0]["board_sha256"], board->board_sha256);

  ReviewBoardFeedbackEntry note{"comment", "layout", "Path is narrow", "t2"};
  auto second = BuildReviewBoardFeedback(*board, note, *first);
  ASSERT_TRUE(second.ok());
  const json two = json::parse(*second);
  EXPECT_EQ(two["status"], "looks_good");  // a comment keeps the decision
  ASSERT_EQ(two["history"].size(), 2u);

  json foreign = one;
  foreign["id"] = "someone-else";
  EXPECT_FALSE(BuildReviewBoardFeedback(*board, note, foreign.dump()).ok());
  EXPECT_FALSE(BuildReviewBoardFeedback(*board, note, "[]").ok());
  EXPECT_FALSE(BuildReviewBoardFeedback(*board, note, "{bad").ok());
}

TEST(ReviewBoardTest, FeedbackV2RoundTripAllKindsAndLegacyV1Read) {
  TempDir dir;
  const fs::path v1_folder = dir.path() / "legacy-v1";
  WriteText(v1_folder / "board.json", ValidBoard("legacy-v1").dump());
  const json legacy_fb = {{"schema", "yaze.review_board_feedback.v1"},
                          {"id", "legacy-v1"},
                          {"status", "looks_good"},
                          {"history",
                           {{{"at", "2026-11-16T12:00:00Z"},
                             {"board_sha256", kSha},
                             {"decision", "looks_good"},
                             {"category", "general"},
                             {"comment", "Legacy approval"}}}}};
  WriteText(v1_folder / "feedback-legacy-v1.json", legacy_fb.dump());

  const fs::path v2_folder = dir.path() / "q237-sky-8c";
  WriteText(v2_folder / "board.json", ValidBoardV2("q237-sky-8c").dump());

  auto scan = ScanReviewBoards(dir.path());
  ASSERT_EQ(scan.boards.size(), 2u);
  ReviewBoard* v1_board = nullptr;
  ReviewBoard* v2_board = nullptr;
  for (auto& b : scan.boards) {
    if (b.id == "legacy-v1")
      v1_board = &b;
    if (b.id == "q237-sky-8c")
      v2_board = &b;
  }
  ASSERT_NE(v1_board, nullptr);
  ASSERT_NE(v2_board, nullptr);
  EXPECT_TRUE(v1_board->feedback_error.empty());
  EXPECT_EQ(ReviewBoardDecision(*v1_board), "looks_good");
  EXPECT_FALSE(ReviewBoardWaitingOnUser(*v1_board));
  EXPECT_TRUE(ReviewBoardWaitingOnUser(*v2_board));

  // Required gating: missing required answers are identified in order and
  // refused on write.
  ReviewBoardAnswerMap answers = InitialReviewBoardAnswers(*v2_board);
  const auto* missing1 = FirstUnansweredRequiredQuestion(*v2_board, answers);
  ASSERT_NE(missing1, nullptr);
  EXPECT_EQ(missing1->id, "cliff_style");
  EXPECT_FALSE(
      WriteReviewBoardFeedback(*v2_board, {"looks_good", "general", "",
                                           "2026-11-17T11:00:00Z", answers})
          .ok());

  // Answer every kind: single, multi+other, text, number (out-of-range then
  // in-range), yes_no.
  answers["cliff_style"] = ReviewBoardAnswer{std::string("a"), ""};
  answers["pick"] =
      ReviewBoardAnswer{std::vector<std::string>{"a", "c"}, "mix"};
  answers["notes"] =
      ReviewBoardAnswer{std::string("move the bridge 2 tiles"), ""};
  answers["bridge_shift"] = ReviewBoardAnswer{9.0, ""};  // > max (4)
  answers["ship_now"] = ReviewBoardAnswer{true, ""};
  const auto* missing_range =
      FirstUnansweredRequiredQuestion(*v2_board, answers);
  ASSERT_NE(missing_range, nullptr);
  EXPECT_EQ(missing_range->id, "bridge_shift");

  answers["bridge_shift"] = ReviewBoardAnswer{2.0, ""};
  EXPECT_EQ(FirstUnansweredRequiredQuestion(*v2_board, answers), nullptr);
  EXPECT_EQ(CountAnsweredQuestions(*v2_board, answers), 5);

  ASSERT_TRUE(WriteReviewBoardFeedback(*v2_board,
                                       {"looks_good", "layout", "Ready to port",
                                        "2026-11-17T11:05:00Z", answers})
                  .ok());
  EXPECT_FALSE(ReviewBoardWaitingOnUser(*v2_board));

  // Verify JSON on disk matches SPEC.md #2 exactly and round-trips on rescan.
  const json disk_fb =
      json::parse(ReadText(v2_folder / "feedback-q237-sky-8c.json"));
  EXPECT_EQ(disk_fb["schema"], "yaze.review_board_feedback.v2");
  EXPECT_EQ(disk_fb["answers"]["cliff_style"]["value"], "a");
  EXPECT_EQ(disk_fb["answers"]["pick"]["value"], json::array({"a", "c"}));
  EXPECT_EQ(disk_fb["answers"]["pick"]["other"], "mix");
  EXPECT_EQ(disk_fb["answers"]["notes"]["value"], "move the bridge 2 tiles");
  EXPECT_EQ(disk_fb["answers"]["bridge_shift"]["value"], 2);
  EXPECT_EQ(disk_fb["answers"]["ship_now"]["value"], true);

  auto rescan = ScanReviewBoards(dir.path());
  const ReviewBoard* reloaded = nullptr;
  for (const auto& b : rescan.boards) {
    if (b.id == "q237-sky-8c")
      reloaded = &b;
  }
  ASSERT_NE(reloaded, nullptr);
  EXPECT_EQ(reloaded->latest_answers, answers);
  EXPECT_EQ(InitialReviewBoardAnswers(*reloaded), answers);
  EXPECT_FALSE(ReviewBoardWaitingOnUser(*reloaded));
}

TEST(ReviewBoardTest, SortFilterSearchAndAgeHelpers) {
  auto b_old = ParseReviewBoard(ValidBoard("b-old-20261113").dump(), "/b1");
  auto b_mid = ParseReviewBoard(ValidBoard("b-mid-20261115").dump(), "/b2");
  auto b_new = ParseReviewBoard(ValidBoardV2("b-new-20261117").dump(), "/b3");
  ASSERT_TRUE(b_old.ok() && b_mid.ok() && b_new.ok());

  b_old->created_at = "2026-11-13T08:00:00+00:00";
  b_old->priority = "low";
  b_old->order = 1;

  b_mid->created_at = "2026-11-15T12:30:00.500000+00:00";
  b_mid->priority = "normal";
  b_mid->order = 20;

  b_new->created_at = "2026-11-17T09:00:00Z";
  b_new->priority = "high";
  b_new->order = 55;

  std::vector<ReviewBoard> boards = {*b_old, *b_mid, *b_new};

  const auto newest =
      SortedReviewBoardIndices(boards, ReviewBoardSort::kNewest);
  EXPECT_EQ(boards[newest[0]].id, "b-new-20261117");
  EXPECT_EQ(boards[newest[1]].id, "b-mid-20261115");
  EXPECT_EQ(boards[newest[2]].id, "b-old-20261113");

  const auto oldest =
      SortedReviewBoardIndices(boards, ReviewBoardSort::kOldest);
  EXPECT_EQ(boards[oldest[0]].id, "b-old-20261113");
  EXPECT_EQ(boards[oldest[2]].id, "b-new-20261117");

  const auto by_prio =
      SortedReviewBoardIndices(boards, ReviewBoardSort::kPriority);
  EXPECT_EQ(boards[by_prio[0]].id, "b-new-20261117");
  EXPECT_EQ(boards[by_prio[2]].id, "b-old-20261113");

  // Search covers title, why, ask, id, subject id/label, and group.
  EXPECT_TRUE(MatchesReviewBoardQuery(*b_new, "bridge offset"));  // ask
  EXPECT_TRUE(MatchesReviewBoardQuery(*b_new, "$8c"));            // subject.id
  EXPECT_TRUE(MatchesReviewBoardQuery(*b_new, "sky-islands"));    // group
  EXPECT_FALSE(MatchesReviewBoardQuery(*b_new, "glacia"));
  EXPECT_TRUE(
      MatchesReviewBoardQuery(*b_new, "", "sky-islands", "Gemini-4-Carbon"));
  EXPECT_FALSE(MatchesReviewBoardQuery(*b_new, "", "other-group", ""));

  // Relative age formatting.
  const auto now = ParseReviewBoardTimestamp("2026-11-17T12:00:00Z");
  ASSERT_TRUE(now.has_value());
  EXPECT_EQ(FormatReviewBoardAge("2026-11-17T09:00:00Z", *now), "3h");
  EXPECT_EQ(FormatReviewBoardAge("2026-11-15T12:00:00Z", *now), "2d");
}

TEST(ReviewBoardTest, IncrementalWatcherReparsesOnlyChangedFoldersAndMarksNew) {
  TempDir dir;
  WriteText(dir.path() / "b-one" / "board.json", ValidBoard("b-one").dump());

  ReviewBoardScan scan;
  ReviewBoardWatcherState state;
  EXPECT_TRUE(RescanReviewBoardsIncremental(dir.path(), scan, state));
  ASSERT_EQ(scan.boards.size(), 1u);
  EXPECT_EQ(state.board_parse_count, 1);
  EXPECT_TRUE(state.new_ids.empty());

  // Second scan with no disk changes reparses zero board.json files.
  EXPECT_FALSE(RescanReviewBoardsIncremental(dir.path(), scan, state));
  EXPECT_EQ(state.board_parse_count, 1);

  // Adding a second board parses only the new folder and marks it `new`.
  WriteText(dir.path() / "b-two" / "board.json", ValidBoardV2("b-two").dump());
  EXPECT_TRUE(RescanReviewBoardsIncremental(dir.path(), scan, state));
  ASSERT_EQ(scan.boards.size(), 2u);
  EXPECT_EQ(state.board_parse_count, 2);
  EXPECT_FALSE(state.new_ids.contains("b-one"));
  EXPECT_TRUE(state.new_ids.contains("b-two"));
}

TEST(ReviewBoardTest, WritesFeedbackAndScanReadsItBack) {
  TempDir dir;
  const fs::path folder = dir.path() / "q195-001";
  WriteText(folder / "board.json", ValidBoard().dump());
  auto scan = ScanReviewBoards(dir.path());
  ASSERT_EQ(scan.boards.size(), 1u);
  ReviewBoard& board = scan.boards[0];
  EXPECT_EQ(ReviewBoardDecision(board), "");

  EXPECT_FALSE(
      WriteReviewBoardFeedback(board, {"needs_changes", "general", "", "t"})
          .ok());  // needs a comment
  EXPECT_FALSE(
      WriteReviewBoardFeedback(board, {"maybe", "general", "x", "t"}).ok());
  ASSERT_TRUE(WriteReviewBoardFeedback(
                  board, {"needs_changes", "layout", "Too wide", "t1"})
                  .ok());
  ASSERT_TRUE(
      WriteReviewBoardFeedback(board, {"looks_good", "general", "", "t2"})
          .ok());
  EXPECT_EQ(ReviewBoardDecision(board), "looks_good");

  const fs::path path = folder / "feedback-q195-001.json";
  ASSERT_TRUE(fs::exists(path));
  const json saved = json::parse(ReadText(path));
  EXPECT_EQ(saved["history"].size(), 2u);
  // Only board.json and the feedback file: no temp files left behind.
  int files = 0;
  for (const auto& entry : fs::directory_iterator(folder)) {
    (void)entry;
    ++files;
  }
  EXPECT_EQ(files, 2);

  auto rescan = ScanReviewBoards(dir.path());
  ASSERT_EQ(rescan.boards.size(), 1u);
  EXPECT_EQ(rescan.boards[0].feedback.size(), 2u);
  EXPECT_EQ(ReviewBoardDecision(rescan.boards[0]), "looks_good");

  // A broken feedback file is left untouched and reported.
  WriteText(path, "{broken");
  auto broken = ScanReviewBoards(dir.path());
  ASSERT_EQ(broken.boards.size(), 1u);
  EXPECT_FALSE(broken.boards[0].feedback_error.empty());
  EXPECT_FALSE(WriteReviewBoardFeedback(broken.boards[0],
                                        {"comment", "general", "x", "t3"})
                   .ok());
  EXPECT_EQ(ReadText(path), "{broken");
}

TEST(ReviewBoardTest, LoadsImagePixelsAfterCheckingTheHash) {
  TempDir dir;
  const fs::path folder = dir.path() / "q195-001";
  auto png = util::EncodeIndexedPng(2, 1, {0, 1},
                                    {{10, 20, 30, 255}, {40, 50, 60, 128}});
  ASSERT_TRUE(png.ok());
  WriteBytes(folder / "media" / "1.png", *png);
  json packet = ValidBoard();
  packet["media"][0]["sha256"] = Sha(*png);
  WriteText(folder / "board.json", packet.dump());
  auto scan = ScanReviewBoards(dir.path());
  ASSERT_EQ(scan.boards.size(), 1u);

  auto image = LoadReviewBoardImage(scan.boards[0], 0);
  ASSERT_TRUE(image.ok()) << image.status();
  EXPECT_EQ(image->width, 2);
  EXPECT_EQ(image->height, 1);
  const std::vector<uint8_t> expected = {10, 20, 30, 255, 40, 50, 60, 128};
  EXPECT_EQ(*image->rgba, expected);
  EXPECT_FALSE(LoadReviewBoardImage(scan.boards[0], 1).ok());

  // Changed bytes fail the hash check.
  auto other = util::EncodeIndexedPng(2, 1, {1, 0},
                                      {{10, 20, 30, 255}, {40, 50, 60, 128}});
  ASSERT_TRUE(other.ok());
  WriteBytes(folder / "media" / "1.png", *other);
  EXPECT_FALSE(LoadReviewBoardImage(scan.boards[0], 0).ok());
}

#if !defined(_WIN32)
TEST(ReviewBoardTest, RefusesImagesThatResolveOutsideTheBoardFolder) {
  TempDir dir;
  TempDir outside;
  auto png = util::EncodeIndexedPng(1, 1, {0}, {{1, 2, 3, 255}});
  ASSERT_TRUE(png.ok());
  WriteBytes(outside.path() / "secret.png", *png);
  const fs::path folder = dir.path() / "q195-001";
  fs::create_directories(folder / "media");
  fs::create_symlink(outside.path() / "secret.png", folder / "media" / "1.png");
  json packet = ValidBoard();
  packet["media"][0]["sha256"] = Sha(*png);
  WriteText(folder / "board.json", packet.dump());
  auto scan = ScanReviewBoards(dir.path());
  ASSERT_EQ(scan.boards.size(), 1u);
  auto image = LoadReviewBoardImage(scan.boards[0], 0);
  ASSERT_FALSE(image.ok());
  EXPECT_NE(std::string(image.status().message()).find("outside"),
            std::string::npos);

  // A relative board folder is refused.
  ReviewBoard relative = scan.boards[0];
  relative.folder = "relative/q195-001";
  EXPECT_FALSE(LoadReviewBoardImage(relative, 0).ok());
}
#endif

}  // namespace
}  // namespace yaze::editor
