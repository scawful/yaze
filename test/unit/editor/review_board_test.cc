#include "app/editor/agent/review_board.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

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
  EXPECT_EQ(board->id, "q195-001");
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

TEST(ReviewBoardTest, RejectsInvalidBoards) {
  const auto rejects = [](const std::function<void(json&)>& edit) {
    json board = ValidBoard();
    edit(board);
    return !ParseReviewBoard(board.dump(), "/b").ok();
  };
  EXPECT_TRUE(rejects([](json& b) { b["schema"] = "other"; }));
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
  EXPECT_EQ(one["schema"], "yaze.review_board_feedback.v1");
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
