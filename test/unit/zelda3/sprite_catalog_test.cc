#include "zelda3/sprite/sprite_catalog.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::zelda3 {
namespace {
namespace fs = std::filesystem;
using nlohmann::json;

fs::path FixturePath() {
  return fs::path(__FILE__)
             .parent_path()
             .parent_path()
             .parent_path()
             .parent_path() /
         "assets/sprite_catalogs/oracle_f0.json";
}

json Fixture() {
  std::ifstream file(FixturePath());
  return json::parse(file);
}

TEST(SpriteCatalogTest,
     FixtureResolvesCanonicalVariantsAndPreservesAllRawBytes) {
  auto catalog = SpriteCatalog::Load(FixturePath().string());
  ASSERT_TRUE(catalog.ok()) << catalog.status();
  ASSERT_EQ(catalog->families().size(), 1);
  const auto* family = catalog->FindFamily(0xF0);
  ASSERT_NE(family, nullptr);
  EXPECT_EQ(family->sources.size(), 2);
  for (int raw = 0; raw < 256; ++raw) {
    auto resolved = catalog->Resolve(0xF0, raw);
    ASSERT_NE(resolved.variant, nullptr);
    EXPECT_EQ(resolved.raw_subtype, raw);
    EXPECT_EQ(resolved.used_fallback, raw > 2);
    EXPECT_EQ(resolved.variant->key, raw == 1   ? "oracle.maple"
                                     : raw == 2 ? "oracle.librarian"
                                                : "oracle.mermaid");
  }
  const auto maple = catalog->Resolve(0xF0, 1);
  ASSERT_EQ(maple.variant->sources.size(), 2);
  EXPECT_EQ(maple.variant->sources[1].path, "Sprites/NPCs/maple.asm");
  EXPECT_EQ(maple.variant->sources[1].label, "MapleHandler");
}

TEST(SpriteCatalogTest, UnknownFamilyAndInvalidRawValuesAreNotNormalized) {
  auto catalog = SpriteCatalog::Parse(Fixture().dump());
  ASSERT_TRUE(catalog.ok());
  EXPECT_EQ(catalog->Resolve(0x01, 3).variant, nullptr);
  for (int raw : {-1, 256, 999}) {
    auto resolved = catalog->Resolve(0xF0, raw);
    EXPECT_EQ(resolved.variant, nullptr);
    EXPECT_EQ(resolved.raw_subtype, raw);
  }
}

TEST(SpriteCatalogTest, RejectsUnsupportedVersionProfileAndSelector) {
  auto document = Fixture();
  document["schema_version"] = 2;
  EXPECT_EQ(SpriteCatalog::Parse(document.dump()).status().code(),
            absl::StatusCode::kUnimplemented);
  document = Fixture();
  document["profile"] = "another_game";
  EXPECT_EQ(SpriteCatalog::Parse(document.dump()).status().code(),
            absl::StatusCode::kUnimplemented);
  document = Fixture();
  document["families"][0]["selector"]["kind"] = "contextual";
  EXPECT_EQ(SpriteCatalog::Parse(document.dump()).status().code(),
            absl::StatusCode::kUnimplemented);
  document = Fixture();
  document["families"][0]["selector"]["mask"] = 7;
  EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok());
}

TEST(SpriteCatalogTest, RejectsAmbiguityAndInvalidReferences) {
  auto document = Fixture();
  document["families"].push_back(document["families"][0]);
  EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok());
  document = Fixture();
  document["families"][0]["variants"][1]["authored_subtype"] = 0;
  EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok());
  document = Fixture();
  document["families"][0]["variants"][1]["key"] = "oracle.mermaid";
  EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok());
  document = Fixture();
  document["families"][0]["selector"]["fallback_variant"] = "missing";
  EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok());
}

TEST(SpriteCatalogTest, RejectsMalformedOrOutOfRangeInput) {
  EXPECT_FALSE(SpriteCatalog::Parse("{").ok());
  EXPECT_FALSE(SpriteCatalog::Parse("[]").ok());
  EXPECT_FALSE(
      SpriteCatalog::Parse(R"({"schema_version":1,"schema_version":2})").ok());
  auto document = Fixture();
  document["families"][0]["main_id"] = 0xF3;
  EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok());
  for (json invalid : {json(-1), json(32), json(1.5), json("1")}) {
    document = Fixture();
    document["families"][0]["variants"][1]["authored_subtype"] = invalid;
    EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok());
  }
  document = Fixture();
  document["families"][0]["variants"][1]["authored_subtype"] = 31;
  EXPECT_TRUE(SpriteCatalog::Parse(document.dump()).ok());
  document["families"] = json::array();
  EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok());
  EXPECT_FALSE(SpriteCatalog::Parse(std::string(1024 * 1024 + 1, ' ')).ok());
}

TEST(SpriteCatalogTest, RejectsUnsafeBindingPaths) {
  for (const auto* path : {"../outside.asm", "/outside.asm", "C:\\outside.asm",
                           "a/../../outside.asm"}) {
    auto document = Fixture();
    document["families"][0]["sources"][0]["path"] = path;
    EXPECT_FALSE(SpriteCatalog::Parse(document.dump()).ok()) << path;
  }
}

class SpriteCatalogSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("yaze_sprite_source_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root_ / "source");
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }
  void Write(const std::string& path, const std::string& contents) {
    std::ofstream(root_ / path, std::ios::binary) << contents;
  }
  fs::path root_;
};

TEST_F(SpriteCatalogSourceTest, FindsLabelWithoutChangingSource) {
  const std::string original =
      "; Target:\r\nNotTarget:\r\n  RTS\r\n Target: ; comment\r\n  RTL\r\n";
  Write("source/test.asm", original);
  auto source = ReadSpriteSource((root_ / "source").string(),
                                 {"draw", "test.asm", "Target"});
  ASSERT_TRUE(source.ok()) << source.status();
  EXPECT_EQ(source->label_line, 4);
  EXPECT_EQ(source->lines.size(), 5);
  std::ifstream stream(root_ / "source/test.asm", std::ios::binary);
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(stream), {}), original);
}

TEST_F(SpriteCatalogSourceTest, MissingRootFileAndLabelAreReported) {
  EXPECT_FALSE(ReadSpriteSource("", {"draw", "test.asm", "Target"}).ok());
  EXPECT_FALSE(ReadSpriteSource((root_ / "missing").string(),
                                {"draw", "test.asm", "Target"})
                   .ok());
  EXPECT_FALSE(ReadSpriteSource((root_ / "source").string(),
                                {"draw", "test.asm", "Target"})
                   .ok());
  Write("source/test.asm", "; Target:\nOther:\nRTL\n");
  EXPECT_FALSE(ReadSpriteSource((root_ / "source").string(),
                                {"draw", "test.asm", "Target"})
                   .ok());
  Write("source/test.asm", "Target:\nRTS\nTarget:\nRTL\n");
  EXPECT_FALSE(ReadSpriteSource((root_ / "source").string(),
                                {"draw", "test.asm", "Target"})
                   .ok());
}

TEST_F(SpriteCatalogSourceTest, RejectsTraversalAndSymlinkEscape) {
  Write("outside.asm", "Target:\nRTL\n");
  EXPECT_FALSE(ReadSpriteSource((root_ / "source").string(),
                                {"draw", "../outside.asm", "Target"})
                   .ok());
  EXPECT_FALSE(
      ReadSpriteSource((root_ / "source").string(),
                       {"draw", (root_ / "outside.asm").string(), "Target"})
          .ok());
  std::error_code ec;
  fs::create_symlink(root_ / "outside.asm", root_ / "source/link.asm", ec);
  if (ec)
    GTEST_SKIP() << "Symlink creation unavailable: " << ec.message();
  EXPECT_EQ(ReadSpriteSource((root_ / "source").string(),
                             {"draw", "link.asm", "Target"})
                .status()
                .code(),
            absl::StatusCode::kPermissionDenied);
}
}  // namespace
}  // namespace yaze::zelda3
