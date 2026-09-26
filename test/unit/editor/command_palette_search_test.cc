#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "app/editor/system/commands/command_palette.h"
#include "app/editor/system/commands/command_palette_goto.h"
#include "app/editor/system/commands/command_palette_providers.h"
#include "app/editor/system/commands/shortcut_manager.h"

namespace yaze::editor {
namespace {

// ---------------------------------------------------------------------------
// Scorer
// ---------------------------------------------------------------------------

TEST(CommandPaletteScoreTest, TiersOrderExactPrefixWordStartSubstring) {
  const int exact = CommandPalette::ScoreText("Save", "save");
  const int prefix = CommandPalette::ScoreText("Save As", "save");
  const int word_start = CommandPalette::ScoreText("Quick Save", "save");
  const int inner = CommandPalette::ScoreText("Autosaver", "save");
  const int subsequence = CommandPalette::ScoreText("Show Advanced", "shad");
  EXPECT_GT(exact, prefix);
  EXPECT_GT(prefix, word_start);
  EXPECT_GT(word_start, inner);
  EXPECT_GT(inner, subsequence);
  EXPECT_GT(subsequence, 0);
  EXPECT_EQ(CommandPalette::ScoreText("Save", "xyz"), 0);
  EXPECT_EQ(CommandPalette::ScoreText("Save", ""), 0);
}

TEST(CommandPaletteScoreTest, WordStartBeatsConsecutive) {
  // Both are subsequence-only matches of "abg". Word-start alignment
  // ("Alpha Beta Gamma") must outrank adjacent letters ("xabxgx").
  const int word_starts = CommandPalette::ScoreText("Alpha Beta Gamma", "abg");
  const int consecutive = CommandPalette::ScoreText("Xabxgx", "abg");
  EXPECT_GT(word_starts, consecutive);
  EXPECT_GT(consecutive, 0);

  // A substring at a word start beats the same substring mid-word.
  EXPECT_GT(CommandPalette::ScoreText("Show Overworld", "over"),
            CommandPalette::ScoreText("Hangover Cure", "over"));
}

TEST(CommandPaletteScoreTest, ConsecutiveBeatsScattered) {
  const int consecutive = CommandPalette::ScoreText("xpalxettex", "palet");
  const int scattered = CommandPalette::ScoreText("xpxaxlxext", "palet");
  EXPECT_GT(consecutive, scattered);
  EXPECT_GT(scattered, 0);
}

TEST(CommandPaletteScoreTest, SubsequenceStaysBelowSubstring) {
  const int substring = CommandPalette::ScoreText("abcdef", "cde");
  const int subsequence = CommandPalette::ScoreText("A B C D E F", "abcdef");
  EXPECT_GT(substring, subsequence);
}

TEST(CommandPaletteScoreTest, MultiTokenQueryMatchesAnyOrder) {
  EXPECT_GT(
      CommandPalette::ScoreText("Dungeon: Open Room [04A]", "open dungeon"), 0);
  EXPECT_EQ(CommandPalette::ScoreText("Dungeon: Open Room", "open zzz"), 0);
}

TEST(CommandPaletteSearchTest, RecencyBoostsOnlyMatchingEntries) {
  CommandPalette palette;
  palette.AddCommand("Alpha Tool", "Misc", "", "", [] {});
  palette.AddCommand("Beta Tool", "Misc", "", "", [] {});
  palette.AddCommand("Unrelated", "Misc", "", "", [] {});
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(palette.RecordUsage("Unrelated"));
  }
  ASSERT_TRUE(palette.RecordUsage("Beta Tool"));

  const auto hits = palette.SearchCommands("tool");
  ASSERT_EQ(hits.size(), 2u);  // "Unrelated" must not ride its usage in.
  EXPECT_EQ(hits[0].name, "Beta Tool");  // recency reorders within the tier
  EXPECT_EQ(hits[1].name, "Alpha Tool");
}

TEST(CommandPaletteSearchTest, RecencyDoesNotJumpMatchTiers) {
  CommandPalette palette;
  palette.AddCommand("Room List", "Nav", "", "", [] {});
  // Subsequence-only match of "room" (R..O..O..M), heavily used.
  palette.AddCommand("Rear Orb Old Mirror", "Nav", "", "", [] {});
  for (int i = 0; i < 50; ++i)
    palette.RecordUsage("Rear Orb Old Mirror");
  const auto hits = palette.SearchCommands("room");
  ASSERT_EQ(hits.size(), 2u);
  EXPECT_EQ(hits[0].name, "Room List");
  EXPECT_EQ(hits[1].name, "Rear Orb Old Mirror");
}

TEST(CommandPaletteSearchTest, StableNameTiebreak) {
  CommandPalette palette;
  palette.AddCommand("Zeta Panel", "Panels", "", "", [] {});
  palette.AddCommand("Alpha Panel", "Panels", "", "", [] {});
  palette.AddCommand("Mid Panel", "Panels", "", "", [] {});
  const auto hits = palette.SearchCommands("panel");
  ASSERT_EQ(hits.size(), 3u);
  EXPECT_EQ(hits[0].name, "Alpha Panel");
  EXPECT_EQ(hits[1].name, "Mid Panel");
  EXPECT_EQ(hits[2].name, "Zeta Panel");
}

TEST(CommandPaletteSearchTest, EmptyQueryListsEverythingRecentFirst) {
  CommandPalette palette;
  palette.AddCommand("B", "c", "", "", [] {});
  palette.AddCommand("A", "c", "", "", [] {});
  palette.RecordUsage("B");
  const auto hits = palette.SearchCommands("");
  ASSERT_EQ(hits.size(), 2u);
  EXPECT_EQ(hits[0].name, "B");
}

TEST(CommandPaletteSearchTest, UsageSurvivesClearAndUnknownNamesIgnored) {
  CommandPalette palette;
  palette.AddCommand("Keep Me", "c", "", "", [] {});
  EXPECT_TRUE(palette.RecordUsage("Keep Me"));
  EXPECT_FALSE(palette.RecordUsage("Not A Command"));
  palette.Clear();
  palette.AddCommand("Keep Me", "c", "", "", [] {});
  const auto recent = palette.GetRecentCommands(5);
  ASSERT_EQ(recent.size(), 1u);
  EXPECT_EQ(recent[0].name, "Keep Me");
  EXPECT_EQ(recent[0].usage_count, 1);
}

// ---------------------------------------------------------------------------
// Dedupe / internal ids
// ---------------------------------------------------------------------------

TEST(CommandPaletteDedupeTest, NormalizesPunctuationAndAltSuffix) {
  EXPECT_EQ(CommandPalette::NormalizeCommandName("Switch to: Overworld Editor"),
            CommandPalette::NormalizeCommandName("Switch to Overworld Editor"));
  EXPECT_EQ(CommandPalette::NormalizeCommandName("Panel Browser (Alt)"),
            CommandPalette::NormalizeCommandName("Panel Browser"));
  EXPECT_NE(CommandPalette::NormalizeCommandName("Show: Foo"),
            CommandPalette::NormalizeCommandName("Hide: Foo"));
}

TEST(CommandPaletteDedupeTest, InternalIdsDetected) {
  EXPECT_TRUE(CommandPalette::IsInternalCommandId("switch.3"));
  EXPECT_TRUE(CommandPalette::IsInternalCommandId("graphics.tool.pencil"));
  EXPECT_TRUE(CommandPalette::IsInternalCommandId("focus_left"));
  EXPECT_TRUE(CommandPalette::IsInternalCommandId("save"));
  EXPECT_FALSE(CommandPalette::IsInternalCommandId("Save"));
  EXPECT_FALSE(CommandPalette::IsInternalCommandId("drawer: Next"));
  EXPECT_FALSE(CommandPalette::IsInternalCommandId("window: Room List"));
}

TEST(CommandPaletteDedupeTest, ShortcutDuplicatesMergeIntoPaletteEntry) {
  ShortcutManager shortcuts;
  int switched = 0;
  shortcuts.RegisterShortcut("switch.6", {ImGuiMod_Ctrl, ImGuiKey_1},
                             [&switched] { ++switched; });
  shortcuts.RegisterCommand("Switch to Overworld Editor",
                            [&switched] { ++switched; });
  shortcuts.RegisterShortcut("graphics.tool.pencil", {ImGuiKey_B}, [] {});
  shortcuts.RegisterShortcut("Save", {ImGuiMod_Ctrl, ImGuiKey_S}, [] {});
  shortcuts.RegisterShortcut("Unbound Action", std::vector<ImGuiKey>{});

  CommandPalette palette;
  palette.RegisterEditorCommands([](const std::string&) {});
  palette.RegisterProvider(
      std::make_unique<ShortcutCommandsProvider>(&shortcuts));

  const auto visible = palette.GetVisibleCommands();
  auto count_normalized = [&](const std::string& name) {
    const auto key = CommandPalette::NormalizeCommandName(name);
    return std::count_if(
        visible.begin(), visible.end(), [&](const CommandEntry& e) {
          return CommandPalette::NormalizeCommandName(e.name) == key;
        });
  };
  EXPECT_EQ(count_normalized("Switch to Overworld Editor"), 1);

  auto find = [&](const std::string& name) {
    return std::find_if(visible.begin(), visible.end(),
                        [&](const CommandEntry& e) { return e.name == name; });
  };
  // Palette-native name wins and inherits the live Ctrl+1 binding.
  auto overworld = find("Switch to: Overworld Editor");
  ASSERT_NE(overworld, visible.end());
  EXPECT_EQ(overworld->shortcut, PrintShortcut({ImGuiMod_Ctrl, ImGuiKey_1}));

  // Internal ids and callback-less bindings are hidden.
  for (const auto& entry : visible) {
    EXPECT_FALSE(CommandPalette::IsInternalCommandId(entry.name)) << entry.name;
  }
  EXPECT_EQ(find("graphics.tool.pencil"), visible.end());
  EXPECT_EQ(find("Unbound Action"), visible.end());

  auto save = find("Save");
  ASSERT_NE(save, visible.end());
  EXPECT_EQ(save->shortcut, LookupShortcutHint(&shortcuts, "Save"));

  // Usage recorded under either spelling counts once, on the shown entry.
  EXPECT_TRUE(palette.RecordUsage("Switch to Overworld Editor"));
  const auto recent = palette.GetRecentCommands(5);
  ASSERT_EQ(recent.size(), 1u);
  EXPECT_EQ(recent[0].name, "Switch to: Overworld Editor");
}

TEST(CommandPaletteDedupeTest, PanelAliasesMergeIntoWindowNames) {
  EXPECT_EQ(CommandPalette::NormalizeCommandName("Panel Browser"),
            CommandPalette::NormalizeCommandName("Window Browser"));
  EXPECT_EQ(CommandPalette::NormalizeCommandName("Show Dungeon Panels"),
            CommandPalette::NormalizeCommandName("Show Dungeon Windows"));
  EXPECT_NE(CommandPalette::NormalizeCommandName("Panelist Tools"),
            CommandPalette::NormalizeCommandName("Window Tools"));

  ShortcutManager shortcuts;
  // Alias without a chord sorts first by name; the Window name owns the keys.
  shortcuts.RegisterCommand("Panel Browser", [] {});
  shortcuts.RegisterShortcut(
      "Window Browser", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_B}, [] {});
  shortcuts.RegisterShortcut("Panel Browser (Alt)", {ImGuiMod_Ctrl, ImGuiKey_E},
                             [] {});
  shortcuts.RegisterShortcut("Window Browser (Alt)",
                             {ImGuiMod_Ctrl, ImGuiKey_W}, [] {});

  CommandPalette palette;
  palette.RegisterProvider(
      std::make_unique<ShortcutCommandsProvider>(&shortcuts));
  const auto visible = palette.GetVisibleCommands();
  ASSERT_EQ(visible.size(), 1u);
  EXPECT_EQ(visible[0].name, "Window Browser");
  EXPECT_EQ(visible[0].shortcut,
            PrintShortcut({ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_B}));
}

TEST(CommandPaletteDedupeTest, ShortcutHintIsLive) {
  ShortcutManager shortcuts;
  shortcuts.RegisterShortcut("Save", {ImGuiMod_Ctrl, ImGuiKey_S}, [] {});
  EXPECT_EQ(LookupShortcutHint(&shortcuts, "Save"),
            PrintShortcut({ImGuiMod_Ctrl, ImGuiKey_S}));
  ASSERT_TRUE(
      shortcuts.UpdateShortcutKeys("Save", {ImGuiMod_Ctrl, ImGuiKey_W}));
  EXPECT_EQ(LookupShortcutHint(&shortcuts, "Save"),
            PrintShortcut({ImGuiMod_Ctrl, ImGuiKey_W}));
  EXPECT_EQ(LookupShortcutHint(&shortcuts, "Missing"), "");
  EXPECT_EQ(LookupShortcutHint(nullptr, "Save"), "");
}

// ---------------------------------------------------------------------------
// Go-to parser
// ---------------------------------------------------------------------------

TEST(CommandPaletteGotoTest, IdFormats) {
  EXPECT_EQ(ParseGotoId("4A"), 0x4A);
  EXPECT_EQ(ParseGotoId("4a"), 0x4A);
  EXPECT_EQ(ParseGotoId("0x4A"), 0x4A);
  EXPECT_EQ(ParseGotoId("0X4a"), 0x4A);
  EXPECT_EQ(ParseGotoId("$4A"), 0x4A);
  EXPECT_EQ(ParseGotoId("4Ah"), 0x4A);
  EXPECT_EQ(ParseGotoId("4AH"), 0x4A);
  EXPECT_EQ(ParseGotoId("#74"), 74);
  EXPECT_EQ(ParseGotoId("10"), 0x10);  // bare digits are hex
  EXPECT_EQ(ParseGotoId("#0"), 0);

  EXPECT_FALSE(ParseGotoId("").has_value());
  EXPECT_FALSE(ParseGotoId("0x").has_value());
  EXPECT_FALSE(ParseGotoId("$").has_value());
  EXPECT_FALSE(ParseGotoId("#").has_value());
  EXPECT_FALSE(ParseGotoId("#4A").has_value());  // decimal rejects hex digits
  EXPECT_FALSE(ParseGotoId("zz").has_value());
  EXPECT_FALSE(ParseGotoId("h").has_value());
  EXPECT_FALSE(ParseGotoId("1234567").has_value());  // too long
  EXPECT_FALSE(ParseGotoId("-1").has_value());
}

TEST(CommandPaletteGotoTest, KeywordsAndKinds) {
  struct Case {
    const char* text;
    GotoKind kind;
    int id;
  };
  const Case cases[] = {
      {"room 4A", GotoKind::kRoom, 0x4A},
      {"r 0x04a", GotoKind::kRoom, 0x4A},
      {"rm:$127", GotoKind::kRoom, 0x127},
      {"  Room   #74  ", GotoKind::kRoom, 74},
      {"go to room 12", GotoKind::kRoom, 0x12},
      {"goto map 1B", GotoKind::kOverworldMap, 0x1B},
      {"ow 9fh", GotoKind::kOverworldMap, 0x9F},
      {"msg 1C", GotoKind::kMessage, 0x1C},
      {"message #300", GotoKind::kMessage, 300},
      {"sprite $E3", GotoKind::kSprite, 0xE3},
      {"spr 0", GotoKind::kSprite, 0},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.text);
    auto parsed = ParseGotoQuery(c.text);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->kind, c.kind);
    EXPECT_EQ(parsed->status, GotoQuery::Status::kOk);
    EXPECT_EQ(parsed->id, c.id);
  }
}

TEST(CommandPaletteGotoTest, RangesAndMissingIds) {
  auto out_room = ParseGotoQuery("room 128");
  ASSERT_TRUE(out_room.has_value());
  EXPECT_EQ(out_room->status, GotoQuery::Status::kOutOfRange);

  auto out_map = ParseGotoQuery("map A0");
  ASSERT_TRUE(out_map.has_value());
  EXPECT_EQ(out_map->status, GotoQuery::Status::kOutOfRange);

  auto out_sprite = ParseGotoQuery("sprite 100");
  ASSERT_TRUE(out_sprite.has_value());
  EXPECT_EQ(out_sprite->status, GotoQuery::Status::kOutOfRange);

  auto missing = ParseGotoQuery("room ");
  ASSERT_TRUE(missing.has_value());
  EXPECT_EQ(missing->status, GotoQuery::Status::kMissingId);
  auto missing_colon = ParseGotoQuery("map:");
  ASSERT_TRUE(missing_colon.has_value());
  EXPECT_EQ(missing_colon->status, GotoQuery::Status::kMissingId);
}

TEST(CommandPaletteGotoTest, NonGotoQueriesFallThrough) {
  EXPECT_FALSE(ParseGotoQuery("").has_value());
  EXPECT_FALSE(ParseGotoQuery("room").has_value());        // no separator yet
  EXPECT_FALSE(ParseGotoQuery("r4a").has_value());         // no separator
  EXPECT_FALSE(ParseGotoQuery("map editor").has_value());  // not a number
  EXPECT_FALSE(ParseGotoQuery("room 4a extra").has_value());
  EXPECT_FALSE(ParseGotoQuery("rooms 4a").has_value());
  EXPECT_FALSE(ParseGotoQuery("drawer: next").has_value());
  EXPECT_FALSE(ParseGotoQuery("window: room").has_value());
}

TEST(CommandPaletteGotoTest, BuildEntryStates) {
  auto ok = BuildGotoEntry(*ParseGotoQuery("room 4a"), 0);
  EXPECT_TRUE(ok.enabled);
  EXPECT_TRUE(static_cast<bool>(ok.callback));
  EXPECT_EQ(ok.name.rfind("Go to room 0x04A", 0), 0u) << ok.name;

  auto map = BuildGotoEntry(*ParseGotoQuery("map 1b"), 0);
  EXPECT_TRUE(map.enabled);
  EXPECT_EQ(map.name.rfind("Go to map 0x1B", 0), 0u) << map.name;

  auto sprite = BuildGotoEntry(*ParseGotoQuery("sprite 4a"), 0);
  EXPECT_FALSE(sprite.enabled);
  EXPECT_FALSE(static_cast<bool>(sprite.callback));
  EXPECT_FALSE(sprite.note.empty());

  auto out = BuildGotoEntry(*ParseGotoQuery("room 200"), 0);
  EXPECT_FALSE(out.enabled);
  EXPECT_NE(out.note.find("Out of range"), std::string::npos);

  auto missing = BuildGotoEntry(*ParseGotoQuery("msg "), 0);
  EXPECT_FALSE(missing.enabled);
  EXPECT_FALSE(missing.note.empty());
}

}  // namespace
}  // namespace yaze::editor
