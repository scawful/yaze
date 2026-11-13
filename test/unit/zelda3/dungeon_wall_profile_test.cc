#include "zelda3/dungeon/dungeon_wall_profile.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::zelda3 {
namespace {

using Json = nlohmann::json;

Json ValidSource() {
  return {{"schema_version", 1},
          {"profiles",
           {{{"id", "timber"},
             {"patterns",
              {{{"name", "upper_n"},
                {"pc_offset", 0x1E3A},
                {"words", {0x0890, 0x4890}}}}}}}},
          {"bindings", {{{"room", 0x104}, {"profile", "timber"}}}}};
}

TEST(DungeonWallProfileTest, ParsesImmutableSourceAndFullTileWords) {
  auto source = ValidSource();
  source["profiles"][0]["patterns"][0]["words"] = {0, 0xFFFF};
  auto parsed = ParseDungeonWallProfiles(source.dump());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ((*parsed)->profiles().size(), 1);
  EXPECT_EQ((*parsed)->profiles()[0].patterns[0].words,
            (std::vector<uint16_t>{0, 0xFFFF}));
  EXPECT_EQ((*parsed)->source_text(), source.dump());
  EXPECT_TRUE((*parsed)->source_path().empty());
}

TEST(DungeonWallProfileTest, RejectsDuplicateJsonKeysAtEveryObjectDepth) {
  for (
      const std::string& source :
      {R"({"schema_version":1,"schema_version":1,"profiles":[],"bindings":[]})",
       R"({"schema_version":1,"profiles":[{"id":"a","id":"a","patterns":[]}],"bindings":[]})",
       R"({"schema_version":1,"profiles":[{"id":"a","patterns":[{"name":"n","pc_offset":6994,"words":[0],"words":[1]}]}],"bindings":[]})",
       R"({"schema_version":1,"profiles":[{"id":"a","patterns":[{"name":"n","pc_offset":6994,"words":[0]}]}],"bindings":[{"room":0,"room":0,"profile":"a"}]})"}) {
    EXPECT_FALSE(ParseDungeonWallProfiles(source).ok()) << source;
  }
}

TEST(DungeonWallProfileTest, RejectsUnknownFieldsAndMissingRequiredFields) {
  for (int depth = 0; depth < 4; ++depth) {
    auto source = ValidSource();
    Json* object = depth == 0   ? &source
                   : depth == 1 ? &source["profiles"][0]
                   : depth == 2 ? &source["profiles"][0]["patterns"][0]
                                : &source["bindings"][0];
    (*object)["typo"] = 1;
    EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  }
  for (const auto* field : {"schema_version", "profiles", "bindings"}) {
    auto source = ValidSource();
    source.erase(field);
    EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  }
}

TEST(DungeonWallProfileTest, RequiresUnsignedIntegerFields) {
  for (const Json& bad : {Json(true), Json(1.0), Json(-1), Json("1")}) {
    for (int field = 0; field < 6; ++field) {
      auto source = ValidSource();
      if (field == 0)
        source["schema_version"] = bad;
      if (field == 1)
        source["profiles"][0]["patterns"][0]["pc_offset"] = bad;
      if (field == 2)
        source["profiles"][0]["patterns"][0]["words"][0] = bad;
      if (field == 3)
        source["bindings"][0]["room"] = bad;
      if (field == 4)
        source["bindings"][0]["world"] = bad;
      if (field == 5)
        source["bindings"][0]["entrance"] = bad;
      EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
    }
  }
}

TEST(DungeonWallProfileTest, RejectsBomAndNegativeZeroLexemes) {
  EXPECT_FALSE(ParseDungeonWallProfiles(std::string("\xEF\xBB\xBF") +
                                        ValidSource().dump())
                   .ok());
  EXPECT_FALSE(
      ParseDungeonWallProfiles(
          R"({"schema_version":1,"profiles":[{"id":"a","patterns":[{"name":"upper_n","pc_offset":6994,"words":[-0]}]}],"bindings":[]})")
          .ok());
}

TEST(DungeonWallProfileTest, EnforcesIdentifiersAndUniqueNames) {
  for (const std::string& bad :
       std::vector<std::string>{"", "Upper", "a-b", "1a", "a b", "a/b",
                                "\xC3\xA9", std::string(49, 'a')}) {
    auto source = ValidSource();
    source["profiles"][0]["id"] = bad;
    EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
    source = ValidSource();
    source["profiles"][0]["patterns"][0]["name"] = bad;
    EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  }
  auto source = ValidSource();
  source["profiles"].push_back(source["profiles"][0]);
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source = ValidSource();
  source["profiles"][0]["patterns"].push_back(
      source["profiles"][0]["patterns"][0]);
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
}

TEST(DungeonWallProfileTest, EnforcesPcAlignmentSpanAndWordBounds) {
  for (int pc : {0x1B50, 0x1B53, 0x8000}) {
    auto source = ValidSource();
    source["profiles"][0]["patterns"][0]["pc_offset"] = pc;
    EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  }
  auto source = ValidSource();
  source["profiles"][0]["patterns"][0]["pc_offset"] = 0x7FFE;
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source["profiles"][0]["patterns"][0]["words"] = {0};
  EXPECT_TRUE(ParseDungeonWallProfiles(source.dump()).ok());
  source["profiles"][0]["patterns"][0]["words"] = {0x10000};
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source["profiles"][0]["patterns"][0]["words"] = Json::array();
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
}

TEST(DungeonWallProfileTest, RejectsOverlappingSpansButAllowsAdjacentSpans) {
  auto source = ValidSource();
  source["profiles"][0]["patterns"].push_back(
      {{"name", "upper_s"}, {"pc_offset", 0x1E3C}, {"words", {0}}});
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source["profiles"][0]["patterns"][1]["pc_offset"] = 0x1E3E;
  EXPECT_TRUE(ParseDungeonWallProfiles(source.dump()).ok());
}

TEST(DungeonWallProfileTest, EnforcesSchemaCountsAndPerProfileWordBudget) {
  auto source = ValidSource();
  source["schema_version"] = 0;
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source = ValidSource();
  source["profiles"] = Json::array();
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source = ValidSource();
  source["profiles"][0]["patterns"] = Json::array();
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source = ValidSource();
  source["profiles"][0]["patterns"][0]["words"] = std::vector<int>(4096, 0);
  ASSERT_TRUE(ParseDungeonWallProfiles(source.dump()).ok());
  source["profiles"][0]["patterns"].push_back(
      {{"name", "extra"}, {"pc_offset", 0x5000}, {"words", {0}}});
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
}

TEST(DungeonWallProfileTest,
     ProfilesMayReuseBankAddressesBeyond32768TotalWords) {
  auto source = ValidSource();
  source["profiles"] = Json::array();
  for (int i = 0; i < 9; ++i) {
    source["profiles"].push_back({{"id", "profile_" + std::to_string(i)},
                                  {"patterns",
                                   {{{"name", "upper_n"},
                                     {"pc_offset", 0x1B52},
                                     {"words", std::vector<int>(4096, 0)}}}}});
  }
  source["bindings"] = Json::array();
  EXPECT_TRUE(ParseDungeonWallProfiles(source.dump()).ok());
}

TEST(DungeonWallProfileTest, EnforcesSourceAndCollectionItemLimits) {
  EXPECT_EQ(
      ParseDungeonWallProfiles(std::string(kWallProfileMaxSourceBytes + 1, ' '))
          .status()
          .code(),
      absl::StatusCode::kResourceExhausted);
  auto source = ValidSource();
  source["profiles"] = Json::array();
  for (int i = 0; i < 129; ++i) {
    auto profile = ValidSource()["profiles"][0];
    profile["id"] = "profile_" + std::to_string(i);
    source["profiles"].push_back(profile);
  }
  source["bindings"] = Json::array();
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source = ValidSource();
  source["bindings"] = Json::array();
  for (int i = 0; i < 4097; ++i) {
    source["bindings"].push_back({{"room", 0}, {"profile", "timber"}});
  }
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
}

TEST(DungeonWallProfileTest, EnforcesBindingRangesAndExistingProfiles) {
  for (const auto& field : {"room", "world", "entrance"}) {
    auto source = ValidSource();
    source["bindings"][0][field] = std::string(field) == "room"    ? 0x128
                                   : std::string(field) == "world" ? 2
                                                                   : 0x85;
    EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  }
  auto source = ValidSource();
  source["bindings"][0]["profile"] = "missing";
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
}

TEST(DungeonWallProfileTest, RejectsDuplicateAndCrossingBindingsDuringParse) {
  auto source = ValidSource();
  source["bindings"].push_back(source["bindings"][0]);
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source = ValidSource();
  source["bindings"] = {
      {{"room", 0x104}, {"profile", "timber"}, {"world", 0}},
      {{"room", 0x104}, {"profile", "timber"}, {"entrance", 4}}};
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source["bindings"].push_back(
      {{"room", 0x104}, {"profile", "timber"}, {"world", 0}, {"entrance", 4}});
  EXPECT_FALSE(ParseDungeonWallProfiles(source.dump()).ok());
  source["bindings"][1] = {
      {"room", 0x104}, {"profile", "timber"}, {"world", 1}};
  EXPECT_TRUE(ParseDungeonWallProfiles(source.dump()).ok());
}

TEST(DungeonWallProfileTest, MissingContextCannotSatisfyConstrainedBinding) {
  auto source = ValidSource();
  source["bindings"][0]["world"] = 0;
  source["bindings"][0]["entrance"] = 4;
  auto parsed = ParseDungeonWallProfiles(source.dump());
  ASSERT_TRUE(parsed.ok());
  for (auto context : {WallProfileContext{0x104}, WallProfileContext{0x104, 0},
                       WallProfileContext{0x104, std::nullopt, 4}}) {
    auto result = ResolveWallProfile(**parsed, context, {});
    ASSERT_TRUE(result.ok());
    EXPECT_FALSE(result->profile_id().has_value());
  }
  auto result = ResolveWallProfile(**parsed, {0x104, 0, 4}, {});
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->profile_id(), "timber");
}

TEST(DungeonWallProfileTest, MostSpecificBindingWinsIndependentOfOrder) {
  auto source = ValidSource();
  auto other = source["profiles"][0];
  other["id"] = "specific";
  source["profiles"].push_back(other);
  source["bindings"].push_back({{"room", 0x104},
                                {"profile", "specific"},
                                {"world", 1},
                                {"entrance", 0x84}});
  auto parsed = ParseDungeonWallProfiles(source.dump());
  ASSERT_TRUE(parsed.ok());
  auto result = ResolveWallProfile(**parsed, {0x104, 1, 0x84}, {});
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->profile_id(), "specific");
  std::reverse(source["bindings"].begin(), source["bindings"].end());
  parsed = ParseDungeonWallProfiles(source.dump());
  ASSERT_TRUE(parsed.ok());
  result = ResolveWallProfile(**parsed, {0x104, 1, 0x84}, {});
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->profile_id(), "specific");
}

TEST(DungeonWallProfileTest,
     UnselectedPreservesLegacyAndSelectedBlocksPublication) {
  auto parsed = ParseDungeonWallProfiles(ValidSource().dump());
  ASSERT_TRUE(parsed.ok());
  DungeonTilePatterns legacy;
  legacy.source_path = "/legacy/house_walls.asm";
  legacy.source_text = "org $009E3A\n dw $0001";
  legacy.tokens[0x1E3A] = {0, 4};
  legacy.words = {{0x1E3A, 1}, {0x1E4A, 2}};
  auto unselected = ResolveWallProfile(**parsed, {0x28}, legacy);
  ASSERT_TRUE(unselected.ok());
  EXPECT_FALSE(unselected->profile_id());
  EXPECT_EQ(unselected->patterns().source_path, legacy.source_path);
  EXPECT_EQ(unselected->patterns().source_text, legacy.source_text);
  EXPECT_EQ(unselected->patterns().tokens.size(), legacy.tokens.size());
  EXPECT_EQ(unselected->patterns().words, legacy.words);
  auto selected = ResolveWallProfile(**parsed, {0x104}, legacy);
  ASSERT_TRUE(selected.ok());
  EXPECT_EQ(selected->patterns().words.at(0x1E3A), 0x0890);
  EXPECT_EQ(selected->patterns().words.at(0x1E4A), 2);
  EXPECT_TRUE(selected->patterns().source_path.empty());
  EXPECT_TRUE(selected->patterns().source_text.empty());
  EXPECT_TRUE(selected->patterns().tokens.empty());
  EXPECT_EQ(legacy.words.at(0x1E3A), 1);
}

TEST(DungeonWallProfileTest, RejectsInvalidResolutionContext) {
  auto parsed = ParseDungeonWallProfiles(ValidSource().dump());
  ASSERT_TRUE(parsed.ok());
  for (auto context :
       {WallProfileContext{-1}, WallProfileContext{0x128},
        WallProfileContext{0, 2}, WallProfileContext{0, std::nullopt, 0x85}}) {
    EXPECT_FALSE(ResolveWallProfile(**parsed, context, {}).ok());
  }
}

TEST(DungeonWallProfileTest, LoadsExactSourceWithoutCreatingMissingFiles) {
  const auto unique = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto directory =
      std::filesystem::temp_directory_path() / ("yaze_wall_profile_" + unique);
  std::filesystem::create_directory(directory);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() { std::filesystem::remove_all(path); }
  } cleanup{directory};
  const auto path = directory / "profiles.json";
  EXPECT_EQ(LoadDungeonWallProfiles(path.string()).status().code(),
            absl::StatusCode::kNotFound);
  EXPECT_FALSE(std::filesystem::exists(path));
  const auto source = ValidSource().dump();
  {
    std::ofstream output(path);
    output << source;
  }
  auto parsed = LoadDungeonWallProfiles(path.string());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ((*parsed)->source_text(), source);
  EXPECT_EQ((*parsed)->source_path(),
            std::filesystem::canonical(path).string());
}

}  // namespace
}  // namespace yaze::zelda3
