#include "app/editor/message/message_review.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "testing.h"

namespace yaze::editor {
namespace {

namespace fs = std::filesystem;

class ScopedTempDir {
 public:
  ScopedTempDir() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = fs::temp_directory_path() /
            ("yaze_message_review_" + std::to_string(nonce));
    fs::create_directories(path_);
  }
  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

constexpr char kRegistry[] = R"({
  "format": "oos-message-registry",
  "version": 1,
  "ranges": [
    {"start": "0x1BE", "end": "0x1BE", "status": "occupied",
     "owner": "ranch-girl", "purpose": "Ranch Girl powder reveal"},
    {"start": "0x18D", "end": "0x1BB", "status": "occupied",
     "owner": "legacy-expanded", "purpose": "Tingle and friends"},
    {"start": "0x1BD", "end": "0x1BD", "status": "placeholder-pool",
     "owner": "unallocated", "purpose": "Free slot"},
    {"start": "0x1CC", "end": "0x1D1", "status": "reserved-placeholder",
     "owner": "d3-prison", "purpose": "D3 prison sequence"}
  ]
})";

TEST(MessageReviewTest, RegistryRangesResolveOwnersAndReservedSlots) {
  auto ranges = ParseMessageRegistry(kRegistry);
  ASSERT_TRUE(ranges.ok()) << ranges.status();
  MessageCatalog catalog;
  catalog.SetRanges(*std::move(ranges));

  ASSERT_NE(catalog.RangeFor(0x18D), nullptr);
  EXPECT_EQ(catalog.RangeFor(0x1A0)->owner, "legacy-expanded");
  EXPECT_EQ(catalog.RangeFor(0x1BE)->owner, "ranch-girl");
  EXPECT_EQ(catalog.RangeFor(0x1BC), nullptr);
  EXPECT_EQ(catalog.RangeFor(0x18C), nullptr);
  EXPECT_EQ(catalog.RangeFor(0x1D2), nullptr);

  EXPECT_FALSE(catalog.IsReservedSlot(0x1BE));
  EXPECT_TRUE(catalog.IsReservedSlot(0x1BD));
  EXPECT_TRUE(catalog.IsReservedSlot(0x1CF));
  EXPECT_FALSE(catalog.IsReservedSlot(0x020));
}

TEST(MessageReviewTest, SceneLabelPrefersScriptTitleThenOwner) {
  MessageCatalog catalog;
  catalog.SetRanges(*ParseMessageRegistry(kRegistry));
  catalog.SetTitles({{0x1BE, "Ranch Girl Ocarina"}, {0x020, "Maku Tree"}});

  EXPECT_EQ(catalog.SceneLabel(0x1BE), "Ranch Girl Ocarina");
  EXPECT_EQ(catalog.SceneLabel(0x1CC), "d3-prison");
  EXPECT_EQ(catalog.SceneLabel(0x020), "Maku Tree");
  EXPECT_EQ(catalog.SceneLabel(0x1BD), "");
  EXPECT_EQ(catalog.SceneLabel(0x100), "");
}

TEST(MessageReviewTest, RegistryRejectsMalformedRanges) {
  EXPECT_FALSE(ParseMessageRegistry("[]").ok());
  EXPECT_FALSE(ParseMessageRegistry(R"({"ranges": 3})").ok());
  EXPECT_FALSE(
      ParseMessageRegistry(R"({"ranges": [{"start": "0x20", "end": "0x10"}]})")
          .ok());
  EXPECT_FALSE(
      ParseMessageRegistry(R"({"ranges": [{"start": "zz", "end": "0x10"}]})")
          .ok());
}

TEST(MessageReviewTest, OrgTitlesReadHeadingsAndKeepTheFirst) {
  const auto titles = ParseMessageOrgTitles(
      "* Oracle of Secrets English Dialogue\n"
      "** 0F - Skeleton Guard\r\n"
      "text line\n"
      "** 1CB - Village Elder  \n"
      "** 1CB - Duplicate\n"
      "*** 20 - Not a message heading\n"
      "** XYZ - Not hex\n");
  ASSERT_EQ(titles.size(), 2u);
  EXPECT_EQ(titles.at(0x0F), "Skeleton Guard");
  EXPECT_EQ(titles.at(0x1CB), "Village Elder");
}

TEST(MessageReviewTest, PlaceholderTextIsOnlyDotsOrEmpty) {
  EXPECT_TRUE(IsPlaceholderMessageText("..."));
  EXPECT_TRUE(IsPlaceholderMessageText(" ... \n"));
  EXPECT_TRUE(IsPlaceholderMessageText(""));
  EXPECT_FALSE(IsPlaceholderMessageText("...Hey![K]"));
  EXPECT_FALSE(IsPlaceholderMessageText("Hello"));
}

TEST(MessageReviewTest, ReviewRoundTripsAndDropsEmptyEntries) {
  MessageReviewFile review;
  review.reviewer = "Ridoyie";
  review.entries[0x1CB] = {ReviewStatus::kNeedsChanges, "Name the witch", "abc",
                           "2026-11-05T19:00:00Z"};
  review.entries[0x020] = {ReviewStatus::kApproved, "", "def", ""};
  review.entries[0x1BE] = {};

  const std::string json = SerializeMessageReview(review);
  EXPECT_NE(json.find("\"0x1CB\""), std::string::npos);
  EXPECT_EQ(json.find("\"0x1BE\""), std::string::npos);

  auto parsed = ParseMessageReview(json);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->reviewer, "Ridoyie");
  ASSERT_EQ(parsed->entries.size(), 2u);
  const auto* elder = parsed->Find(0x1CB);
  ASSERT_NE(elder, nullptr);
  EXPECT_EQ(elder->status, ReviewStatus::kNeedsChanges);
  EXPECT_EQ(elder->comment, "Name the witch");
  EXPECT_EQ(elder->text_sha256, "abc");
  EXPECT_EQ(parsed->Find(0x020)->status, ReviewStatus::kApproved);
}

TEST(MessageReviewTest, ReviewRejectsWrongFormatStatusAndDuplicates) {
  EXPECT_FALSE(ParseMessageReview(R"({"format":"other","version":1})").ok());
  EXPECT_FALSE(
      ParseMessageReview(R"({"format":"yaze-message-review","version":2})")
          .ok());
  EXPECT_FALSE(ParseMessageReview(R"({"format":"yaze-message-review",
    "version":1,"messages":[{"id":"0x20","status":"maybe"}]})")
                   .ok());
  EXPECT_FALSE(ParseMessageReview(R"({"format":"yaze-message-review",
    "version":1,"messages":[{"id":"0x20","status":"approved"},
                            {"id":32,"status":"approved"}]})")
                   .ok());
}

TEST(MessageReviewTest, SaveCreatesDirectoryAndLoadsBack) {
  ScopedTempDir temp;
  const fs::path path = temp.path() / "review" / "ridoyie.json";
  MessageReviewFile review;
  review.reviewer = "Ridoyie";
  review.entries[0x1CC] = {ReviewStatus::kApproved, "Good", "sha", "t"};
  ASSERT_OK(SaveMessageReviewFile(path, review));
  EXPECT_FALSE(fs::exists(fs::path(path.string() + ".tmp")));

  auto loaded = LoadMessageReviewFile(path);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->Find(0x1CC)->comment, "Good");

  review.entries[0x1CC].comment = "Better";
  ASSERT_OK(SaveMessageReviewFile(path, review));
  EXPECT_EQ(LoadMessageReviewFile(path)->Find(0x1CC)->comment, "Better");

  const auto files = ListMessageReviewFiles(temp.path() / "review");
  ASSERT_EQ(files.size(), 1u);
  EXPECT_EQ(files.front().filename(), "ridoyie.json");
  EXPECT_TRUE(ListMessageReviewFiles(temp.path() / "missing").empty());
}

TEST(MessageReviewTest, ReviewFileStemIsPortable) {
  EXPECT_EQ(ReviewFileStem("Ridoyie"), "ridoyie");
  EXPECT_EQ(ReviewFileStem("  scaw ful!! "), "scaw-ful");
  EXPECT_EQ(ReviewFileStem("../etc/passwd"), "etc-passwd");
  EXPECT_EQ(ReviewFileStem("a_b"), "a_b");
  EXPECT_EQ(ReviewFileStem("!!!"), "");
}

}  // namespace
}  // namespace yaze::editor
