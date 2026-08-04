#include "app/editor/dungeon/minecart_track_source.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

void AppendDwValues(std::stringstream& stream, int first_value, int count,
                    std::string_view indent = "    ") {
  for (int i = 0; i < count; i += 4) {
    stream << indent << "dw ";
    for (int j = 0; j < 4 && i + j < count; ++j) {
      if (j > 0) {
        stream << ", ";
      }
      stream << absl::StrFormat("$%04X", first_value + i + j);
    }
    stream << "  ; values " << i << "\n";
  }
}

std::string MakeFlatSection(std::string_view label, int first_value,
                            int count = 32) {
  std::stringstream stream;
  stream << "  " << label << "\r\n";
  for (int i = 0; i < count; i += 4) {
    stream << "\tdw ";
    for (int j = 0; j < 4 && i + j < count; ++j) {
      if (j > 0) {
        stream << ",  ";
      }
      stream << absl::StrFormat("$%04X", first_value + i + j);
    }
    stream << " ; keep flat formatting\r\n";
  }
  return stream.str();
}

std::string MakeFlatSource(int room_count = 32) {
  return "; header stays byte-for-byte\r\n" +
         MakeFlatSection(".TrackStartingRooms", 0x0100, room_count) +
         "; between rooms and x\n" +
         MakeFlatSection(".TrackStartingX", 0x1000) +
         MakeFlatSection(".TrackStartingY", 0x2000);
}

std::string MakeGuardedSection(std::string_view label, int first_value,
                               int prefix_count = 4, int enabled_count = 28,
                               int disabled_count = 28) {
  std::stringstream stream;
  stream << "  " << label << "\n";
  AppendDwValues(stream, first_value, prefix_count, "  ");
  stream << "  if !ENABLE_MINECART_PLANNED_TRACK_TABLE == 1\n";
  AppendDwValues(stream, first_value + prefix_count, enabled_count);
  stream << "  else\n"
         << "    ; disabled branch must never be rewritten\n";
  AppendDwValues(stream, 0, disabled_count);
  stream << "  endif\n";
  return stream.str();
}

std::string MakeGuardedSource(int room_prefix = 4, int room_enabled = 28,
                              int room_disabled = 28) {
  return "; canonical guarded source\n" +
         MakeGuardedSection(".TrackStartingRooms", 0x0200, room_prefix,
                            room_enabled, room_disabled) +
         "\n; coordinate comments remain untouched\n" +
         MakeGuardedSection(".TrackStartingX", 0x1100) +
         MakeGuardedSection(".TrackStartingY", 0x2100);
}

bool IsWithinSpan(size_t offset, const MinecartTrackSourceTokenSpan& span) {
  return offset >= span.offset && offset < span.offset + span.length;
}

}  // namespace

TEST(MinecartTrackSourceTest, ParsesFlatTablesAndRetainsExactTokenSpans) {
  const std::string source = MakeFlatSource();
  auto document_or = MinecartTrackSourceDocument::Parse(source);

  ASSERT_TRUE(document_or.ok()) << document_or.status();
  EXPECT_FALSE(document_or->guarded());
  EXPECT_EQ(document_or->source_bytes(), source);
  ASSERT_EQ(document_or->tracks().size(), kMinecartTrackSlotCount);
  EXPECT_EQ(document_or->tracks().front(),
            (MinecartTrack{0, 0x0100, 0x1000, 0x2000}));
  EXPECT_EQ(document_or->tracks().back(),
            (MinecartTrack{31, 0x011F, 0x101F, 0x201F}));

  for (const auto& field : document_or->editable_token_spans()) {
    for (const MinecartTrackSourceTokenSpan span : field) {
      EXPECT_EQ(span.length, 5u);
      ASSERT_LE(span.offset + span.length, source.size());
      EXPECT_EQ(source.substr(span.offset, span.length).front(), '$');
    }
  }
}

TEST(MinecartTrackSourceTest,
     GuardedRenderChangesOnlySelectedPrefixAndEnabledTokens) {
  std::string source = MakeGuardedSource();
  source.replace(source.find("$0201"), 5, "$02a1");
  auto document_or = MinecartTrackSourceDocument::Parse(source);
  ASSERT_TRUE(document_or.ok()) << document_or.status();
  ASSERT_TRUE(document_or->guarded());

  std::vector<MinecartTrack> edited = document_or->tracks();
  edited[0].room_id = 0x0777;   // Four-entry prefix.
  edited[4].start_x = 0x1888;   // Enabled branch.
  edited[31].start_y = 0x2999;  // Last enabled token.
  auto rendered_or = document_or->Render(edited);
  ASSERT_TRUE(rendered_or.ok()) << rendered_or.status();
  ASSERT_EQ(rendered_or->size(), source.size());

  const std::array<MinecartTrackSourceTokenSpan, 3> changed_spans = {
      document_or->editable_token_spans()[0][0],
      document_or->editable_token_spans()[1][4],
      document_or->editable_token_spans()[2][31]};
  for (size_t offset = 0; offset < source.size(); ++offset) {
    if (source[offset] == (*rendered_or)[offset]) {
      continue;
    }
    EXPECT_TRUE(std::any_of(
        changed_spans.begin(), changed_spans.end(),
        [&](const auto& span) { return IsWithinSpan(offset, span); }))
        << "Unexpected byte change at offset " << offset;
  }
  EXPECT_EQ(rendered_or->substr(changed_spans[0].offset, 5), "$0777");
  EXPECT_EQ(rendered_or->substr(changed_spans[1].offset, 5), "$1888");
  EXPECT_EQ(rendered_or->substr(changed_spans[2].offset, 5), "$2999");
  EXPECT_NE(rendered_or->find("$02a1"), std::string::npos);

  auto reparsed_or = MinecartTrackSourceDocument::Parse(*rendered_or);
  ASSERT_TRUE(reparsed_or.ok()) << reparsed_or.status();
  EXPECT_EQ(reparsed_or->tracks(), edited);
}

TEST(MinecartTrackSourceTest, RejectsMalformedCountsTokensAndLayouts) {
  std::vector<std::string> invalid_sources = {
      MakeFlatSource(0),
      MakeFlatSource(31),
      MakeFlatSource(33),
      MakeGuardedSource(/*room_prefix=*/3),
      MakeGuardedSource(/*room_prefix=*/5),
      MakeGuardedSource(/*room_prefix=*/4, /*room_enabled=*/27),
      MakeGuardedSource(/*room_prefix=*/4, /*room_enabled=*/29),
      MakeGuardedSource(/*room_prefix=*/4, /*room_enabled=*/28,
                        /*room_disabled=*/27),
      MakeGuardedSource(/*room_prefix=*/4, /*room_enabled=*/28,
                        /*room_disabled=*/29),
  };

  std::string short_token = MakeFlatSource();
  short_token.replace(short_token.find("$0100"), 5, "$100");
  invalid_sources.push_back(std::move(short_token));
  std::string wide_token = MakeFlatSource();
  wide_token.replace(wide_token.find("$0100"), 5, "$00100");
  invalid_sources.push_back(std::move(wide_token));
  invalid_sources.push_back(MakeFlatSection(".TrackStartingRooms", 0x0100) +
                            MakeGuardedSection(".TrackStartingX", 0x1100) +
                            MakeGuardedSection(".TrackStartingY", 0x2100));
  invalid_sources.push_back(MakeFlatSection(".TrackStartingX", 0x1000) +
                            MakeFlatSection(".TrackStartingRooms", 0x0100) +
                            MakeFlatSection(".TrackStartingY", 0x2000));
  invalid_sources.push_back(MakeFlatSource() +
                            MakeFlatSection(".TrackStartingRooms", 0x3000));

  for (const std::string& source : invalid_sources) {
    SCOPED_TRACE(source);
    EXPECT_FALSE(MinecartTrackSourceDocument::Parse(source).ok());
  }
}

TEST(MinecartTrackSourceTest, RenderValidatesTrackCountIdsAndWords) {
  auto document_or = MinecartTrackSourceDocument::Parse(MakeFlatSource());
  ASSERT_TRUE(document_or.ok()) << document_or.status();

  std::vector<MinecartTrack> tracks = document_or->tracks();
  tracks.pop_back();
  EXPECT_TRUE(absl::IsInvalidArgument(document_or->Render(tracks).status()));

  tracks = document_or->tracks();
  tracks[7].id = 8;
  EXPECT_TRUE(absl::IsInvalidArgument(document_or->Render(tracks).status()));

  for (int MinecartTrack::* field :
       {&MinecartTrack::room_id, &MinecartTrack::start_x,
        &MinecartTrack::start_y}) {
    tracks = document_or->tracks();
    tracks[3].*field = -1;
    EXPECT_TRUE(absl::IsInvalidArgument(document_or->Render(tracks).status()));
    tracks[3].*field = 0x10000;
    EXPECT_TRUE(absl::IsInvalidArgument(document_or->Render(tracks).status()));
  }
}

}  // namespace yaze::editor
