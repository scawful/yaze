#include "zelda3/dungeon/dungeon_tile_patterns.h"

#include <filesystem>
#include <fstream>

#include "absl/cleanup/cleanup.h"
#include "gtest/gtest.h"
#include "unique_temp_path.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/object_parser.h"

namespace yaze::zelda3 {
namespace {

TEST(DungeonTilePatternsTest, ParsesSparseLiteralWordsIncludingZeroAndFlips) {
  const auto parsed = ParseDungeonTilePatterns(
      "; Example\r\nlorom\r\n#obj02E8:\norg $009E3A\n"
      "dw $4890, $0000 ; zero is a real tile word\n"
      "next_label:\ndw $C890\norg $00A6B8\ndw $C89E\n");
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->words.size(), 4);
  EXPECT_EQ(parsed->words.at(0x1E3A), 0x4890);
  EXPECT_EQ(parsed->words.at(0x1E3C), 0);
  EXPECT_EQ(parsed->words.at(0x1E3E), 0xC890);
  EXPECT_EQ(parsed->words.at(0x26B8), 0xC89E);
}

TEST(DungeonTilePatternsTest, ValidatesLiteralProgramCounterAssertions) {
  const std::string source =
      "org $00A6B8\ndw $C89E,$489E\n"
      "assert pc() == $00A6BC, \"Corner size; unchanged\" ; comment\n"
      "org $009E3A\ndw $0890\nassert pc()==$009E3C\n"
      "org $009E1A\ndw $0892\n"
      "assert pc() == $009E1C, \"Left wall size\"\n";
  const auto parsed = ParseDungeonTilePatterns(source);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->words.size(), 4u);
  EXPECT_EQ(parsed->source_text, source);
  for (const auto& [pc, token] : parsed->tokens) {
    EXPECT_EQ(source.substr(token.offset, token.length),
              pc == 0x26B8   ? "$C89E"
              : pc == 0x26BA ? "$489E"
              : pc == 0x1E3A ? "$0890"
                             : "$0892");
  }
}

TEST(DungeonTilePatternsTest, RejectsFailedOrUnsupportedAssertions) {
  EXPECT_FALSE(
      ParseDungeonTilePatterns("assert pc() == $009E3A\norg $009E3A\ndw $0001")
          .ok());
  for (const char* assertion :
       {"assert pc() == $009E3A, \"Wrong size\"", "assert pc() != $009E3C",
        "assert 1", "assert pc() == end", "assert pc() == $009E3C + $0000",
        "assert pc() == $7E9E3C", "assert pc() == $009E3C, unquoted",
        "assert pc() == $009E3C, \"unterminated",
        "assert pc() == $009E3C, \"ok\" extra"}) {
    const auto parsed = ParseDungeonTilePatterns(
        std::string("org $009E3A\ndw $0001\n") + assertion);
    EXPECT_FALSE(parsed.ok()) << assertion;
    EXPECT_NE(parsed.status().message().find("line 3"), std::string::npos);
  }
}

TEST(DungeonTilePatternsTest, PublishingWordsPreservesAssertions) {
  const auto path = test::UniqueTempPath("asserted_wall_patterns.asm");
  const absl::Cleanup cleanup = [&] {
    std::filesystem::remove(path);
  };
  const std::string source =
      "org $009E3A\ndw $0890,$0896\n"
      "assert pc() == $009E3E, \"Top wall size\"\n";
  {
    std::ofstream file(path, std::ios::binary);
    file << source;
  }
  const auto parsed = LoadDungeonTilePatterns(path.string());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  ASSERT_TRUE(PublishDungeonTilePatterns(*parsed, {{0x1E3A, 0x4890}}).ok());
  const auto changed = LoadDungeonTilePatterns(path.string());
  ASSERT_TRUE(changed.ok()) << changed.status();
  auto expected = source;
  expected.replace(expected.find("$0890"), 5, "$4890");
  EXPECT_EQ(changed->source_text, expected);
  EXPECT_EQ(changed->words.at(0x1E3A), 0x4890);
}

TEST(DungeonTilePatternsTest,
     RejectsUnsupportedOrAmbiguousSourceWithoutPartialResult) {
  for (const char* text :
       {"org $009E3A\ndw $10000", "org $009E3A\ndw $1,",
        "org $009E3A\ndw $1 + $2", "dw $0001", "org $7E9E3A\ndw $0001",
        "org $019E3A\ndw $0001", "org $009E3B\ndw $0001",
        "org $008000\ndw $0001", "org $00FFFE\ndw $0001,$0002",
        "org $009E3A\ndw $0001\norg $009E3A\ndw $0002",
        "org $009E3A\ndw $0001\nif 0\ndw $0002\nendif",
        "org $009E3A\ndw $0001\nincsrc \"other.asm\"",
        "org $009E3A\nJSL $008000", "org $009E3A\ndb $01", "; no payload\n"}) {
    EXPECT_FALSE(ParseDungeonTilePatterns(text).ok()) << text;
  }
  EXPECT_FALSE(ParseDungeonTilePatterns(std::string(65537, ' ')).ok());
}

TEST(DungeonTilePatternsTest,
     ParserUsesOverlayWithoutChangingRomOrOrdinaryReads) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000)).ok());
  ASSERT_TRUE(rom.WriteShort(0x8002, 0x02E8).ok());
  ASSERT_TRUE(rom.WriteShort(0x1E3A, 0x0890).ok());
  const auto bytes = rom.vector();
  const auto parsed = ParseDungeonTilePatterns("org $009E3A\ndw $4890");
  ASSERT_TRUE(parsed.ok());
  const auto original = ObjectParser(&rom).ParseObject(1);
  const auto preview = ObjectParser(&rom, &*parsed).ParseObject(1);
  ASSERT_TRUE(original.ok());
  ASSERT_TRUE(preview.ok());
  EXPECT_FALSE(original->at(0).horizontal_mirror_);
  EXPECT_TRUE(preview->at(0).horizontal_mirror_);
  EXPECT_EQ(preview->at(0).id_, original->at(0).id_);
  EXPECT_EQ(rom.vector(), bytes);
  EXPECT_FALSE(ObjectParser(&rom).ParseObject(1)->at(0).horizontal_mirror_);
}

TEST(DungeonTilePatternsTest,
     SessionsReloadAndSnapshotsDoNotShareMutableSource) {
  auto& manager = CustomObjectManager::Get();
  const auto previous_context = manager.active_runtime_context_id();
  const auto previous_state = manager.SnapshotState();
  const auto root = test::UniqueTempPath("dungeon_patterns");
  std::filesystem::create_directories(root);
  const absl::Cleanup cleanup = [&] {
    manager.RemoveRuntimeContext(0xFFFFF101);
    manager.RemoveRuntimeContext(0xFFFFF102);
    if (previous_context) {
      manager.ActivateRuntimeContext(*previous_context, previous_state);
    } else {
      manager.ActivateStandaloneContext();
      manager.RestoreState(previous_state);
    }
    std::filesystem::remove_all(root);
  };
  const auto path = root / "walls.asm";
  {
    std::ofstream out(path);
    out << "org $009E3A\ndw $4890\n";
  }
  CustomObjectManager::State state;
  state.dungeon_tile_patterns = path.string();
  manager.ActivateRuntimeContext(0xFFFFF101, state);
  auto snapshot = manager.CaptureRenderSnapshot(false);
  ASSERT_TRUE(snapshot.tile_patterns().ok());
  ASSERT_NE(*snapshot.tile_patterns(), nullptr);
  EXPECT_EQ((*snapshot.tile_patterns())->words.at(0x1E3A), 0x4890);
  manager.ActivateRuntimeContext(0xFFFFF102, {});
  ASSERT_TRUE(manager.GetTilePatterns().ok());
  EXPECT_EQ(*manager.GetTilePatterns(), nullptr);
  {
    std::ofstream out(path);
    out << "org $009E3A\ndw $C890\n";
  }
  manager.ActivateRuntimeContext(0xFFFFF101, state);
  EXPECT_EQ((*manager.GetTilePatterns())->words.at(0x1E3A), 0x4890);
  const auto generation = manager.asset_generation();
  manager.ReloadAll();
  EXPECT_NE(manager.asset_generation(), generation);
  EXPECT_EQ((*manager.GetTilePatterns())->words.at(0x1E3A), 0xC890);
  EXPECT_EQ((*snapshot.tile_patterns())->words.at(0x1E3A), 0x4890);
  {
    std::ofstream out(path);
    out << "org $009E3A\ndw $C890\nincsrc \"bad.asm\"\n";
  }
  manager.ReloadAll();
  EXPECT_FALSE(manager.GetTilePatterns().ok());
  const auto failed = manager.CaptureRenderSnapshot(false);
  EXPECT_FALSE(failed.tile_patterns().ok());
  manager.SetTilePatternsSource("");
  ASSERT_TRUE(manager.GetTilePatterns().ok());
  EXPECT_EQ(*manager.GetTilePatterns(), nullptr);
  EXPECT_FALSE(failed.tile_patterns().ok());
}

}  // namespace
}  // namespace yaze::zelda3
